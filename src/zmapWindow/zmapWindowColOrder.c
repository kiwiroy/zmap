/*  File: zmapWindowColOrder.c
 *  Author: Roy Storey (rds@sanger.ac.uk)
 *  Copyright (c) 2006: Genome Research Ltd.
 *-------------------------------------------------------------------
 * ZMap is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * or see the on-line version at http://www.gnu.org/copyleft/gpl.txt
 *-------------------------------------------------------------------
 * This file is part of the ZMap genome database package
 * originally written by:
 *
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *      Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk
 *
 * Description: 
 *
 * Exported functions: See XXXXXXXXXXXXX.h
 * HISTORY:
 * Last edited: Dec  5 15:09 2006 (rds)
 * Created: Tue Dec  5 14:48:45 2006 (rds)
 * CVS info:   $Id: zmapWindowColOrder.c,v 1.1 2006-12-05 16:13:55 rds Exp $
 *-------------------------------------------------------------------
 */

#include <ZMap/zmapUtils.h>
#include <ZMap/zmapGLibUtils.h>
#include <zmapWindow_P.h>
#include <zmapWindowContainer.h>

typedef struct 
{
  int current_list_number;      /* zero based. */
  GQuark feature_set_name;
  ZMapWindow window;
  int group_length;
  int three_frame_position;
  ZMapStrand strand;
  ZMapFrame frame;
  GList *item_list2order;
  GQuark three_frame_position_quark;
} OrderColumnsDataStruct, *OrderColumnsData;

static gint columnWithName(gconstpointer list_data, gconstpointer user_data);
static gint columnWithFrame(gconstpointer list_data, gconstpointer user_data);
static gint columnWithNameAndFrame(gconstpointer list_data, gconstpointer user_data);
static gint matchFrameSensitiveColCB(gconstpointer list_data, gconstpointer user_data);
static void sortByFSNList(gpointer list_data, gpointer user_data);
static void orderColumnsForFrame(OrderColumnsData order_data, GList *list, ZMapFrame frame);
static void orderColumnsCB(FooCanvasGroup *data, FooCanvasPoints *points, 
                           ZMapContainerLevelType level, gpointer user_data);

static gboolean order_debug_G = TRUE;


void zmapWindowColOrderColumns(ZMapWindow window)
{
  FooCanvasGroup *super_root ;
  OrderColumnsDataStruct order_data = {0} ;

  super_root = FOO_CANVAS_GROUP(zmapWindowFToIFindItemFull(window->context_to_item,
							   0,0,0,
							   ZMAPSTRAND_NONE, ZMAPFRAME_NONE,
							   0)) ;
  zMapAssert(super_root) ;

  order_data.window = window;
  order_data.strand = ZMAPSTRAND_FORWARD; /* makes things simpler */

  zmapWindowContainerExecute(FOO_CANVAS_GROUP(super_root),
                             ZMAPCONTAINER_LEVEL_STRAND,
                             orderColumnsCB, &order_data);

  /* If we've reversed the feature_set_names list, and left it like that, re-reverse. */
  if(order_data.strand == ZMAPSTRAND_REVERSE)
    window->feature_set_names = g_list_reverse(window->feature_set_names);

  return ;
}


static gint columnWithName(gconstpointer list_data, gconstpointer user_data)
{
  FooCanvasGroup *parent = FOO_CANVAS_GROUP(list_data);
  ZMapFeatureAny feature_any;
  GQuark name_data = GPOINTER_TO_UINT(user_data);
  gint match = -1;

  if((feature_any = (ZMapFeatureAny)(g_object_get_data(G_OBJECT(parent), ITEM_FEATURE_DATA))))
    {
      if(name_data == feature_any->unique_id)
        match = 0;
    }

  return match;
}

static gint columnWithFrame(gconstpointer list_data, gconstpointer user_data)
{
  FooCanvasGroup *parent = FOO_CANVAS_GROUP(list_data);
  ZMapFrame frame_data = GPOINTER_TO_UINT(user_data);
  ZMapWindowItemFeatureSetData set_data;
  gint match = -1;

  if((set_data = g_object_get_data(G_OBJECT(parent), ITEM_FEATURE_SET_DATA)))
    {
      if(set_data->frame == frame_data)
        match = 0;
    }

  return match;
}

static gint columnWithNameAndFrame(gconstpointer list_data, gconstpointer user_data)
{
  OrderColumnsData order_data = (OrderColumnsData)user_data;
  gint match = -1;

  if(!columnWithName(list_data, GUINT_TO_POINTER(order_data->feature_set_name)))
    {
      if(order_data->frame == ZMAPFRAME_NONE)
        match = 0;
      else if(!columnWithFrame(list_data, GUINT_TO_POINTER(order_data->frame)))
        match = 0;
    }
  
  return match;
}

static gint matchFrameSensitiveColCB(gconstpointer list_data, gconstpointer user_data)
{
  FooCanvasGroup *col_group   = FOO_CANVAS_GROUP(list_data);
  ZMapWindowItemFeatureSetData set_data;
  ZMapFeatureAny feature_any;
  ZMapFeatureTypeStyle style;
  gint match = -1;

  if((set_data = g_object_get_data(G_OBJECT(col_group), ITEM_FEATURE_SET_DATA)) &&
     (feature_any = (ZMapFeatureAny)(g_object_get_data(G_OBJECT(col_group), ITEM_FEATURE_DATA))))
    {
      style = set_data->style;
      /* style = zmapWindowStyleTableFind(set_data->style_table, feature_any->unique_id); */
      
      if(style->opts.frame_specific)
        {
          match = 0;
          if(order_debug_G)
            printf("%s: %s is frame sensitive\n", __PRETTY_FUNCTION__,
                   g_quark_to_string(feature_any->unique_id));
        }
    }

  return match;
}

static void sortByFSNList(gpointer list_data, gpointer user_data)
{
  OrderColumnsData order_data = (OrderColumnsData)user_data;
  GList *column;
  int position, positions_to_move;

  order_data->feature_set_name = GPOINTER_TO_UINT(list_data);

  if((column = g_list_find_custom(order_data->item_list2order,
                                  order_data, columnWithNameAndFrame)))
    {
      position = g_list_length(column);

      if(order_debug_G)
        printf("%s: %s\n\tlist_length = %d\n\tgroup_length = %d\n\tposition = %d\n\ttarget = %d\n",
               __PRETTY_FUNCTION__,
               g_quark_to_string(order_data->feature_set_name),
               position,
               order_data->group_length,
               order_data->group_length - position,
               order_data->current_list_number);

      position = order_data->group_length - position;

      if(position != order_data->current_list_number)
        {
          positions_to_move = order_data->current_list_number - position;

          if(order_debug_G)
            printf("%s: moving %d\n", __PRETTY_FUNCTION__, positions_to_move);

          if(positions_to_move > 0)
            foo_canvas_item_raise(column->data, positions_to_move);
          else if(positions_to_move < 0)
            foo_canvas_item_lower(column->data, positions_to_move * -1);
          
          order_data->item_list2order = g_list_first(order_data->item_list2order);
        }

      order_data->current_list_number++;
    }

  return ;
}

static void orderColumnsForFrame(OrderColumnsData order_data, GList *list, ZMapFrame frame)
{
  /* simple function so that all this get remembered... */
  order_data->current_list_number = 
    order_data->feature_set_name  = 0;
  order_data->item_list2order     = list;

  order_data->group_length = g_list_length(list);
  order_data->frame        = ZMAPFRAME_NONE;

  g_list_foreach(order_data->window->feature_set_names, sortByFSNList, order_data);

  return ;
}

static void orderColumnsCB(FooCanvasGroup *data, FooCanvasPoints *points, 
                           ZMapContainerLevelType level, gpointer user_data)
{
  OrderColumnsData order_data = (OrderColumnsData)user_data;
  FooCanvasGroup *strand_group;
  int interface_check;
  ZMapStrand strand;
  ZMapWindow window = order_data->window;
  GList *frame_list = NULL, 
    *sensitive_list = NULL, 
    *sensitive_tmp  = NULL;

  if(level == ZMAPCONTAINER_LEVEL_STRAND)
    {
      /* Get Features */
      strand_group = zmapWindowContainerGetFeatures(data);
      zMapAssert(strand_group);

      /* check that g_list_length doesn't do g_list_first! */
      interface_check = g_list_length(strand_group->item_list_end);
      //zMapAssert(interface_check == 1);

      strand = zmapWindowContainerGetStrand(data);

      if(strand == ZMAPSTRAND_REVERSE)
        {
          if(order_data->strand == ZMAPSTRAND_FORWARD)
            window->feature_set_names =
              g_list_reverse(window->feature_set_names);
          order_data->strand = ZMAPSTRAND_REVERSE;
        }
      else if(strand == ZMAPSTRAND_FORWARD)
        {
          if(order_data->strand == ZMAPSTRAND_REVERSE)
            window->feature_set_names =
              g_list_reverse(window->feature_set_names);
          order_data->strand = ZMAPSTRAND_FORWARD;
        }
      else
        zMapAssertNotReached(); /* What! */

      if(window->display_3_frame)
        {
          if((frame_list = g_list_find(window->feature_set_names, 
                                       GUINT_TO_POINTER(zMapStyleCreateID(ZMAP_FIXED_STYLE_3FRAME)))))
            {
              /* grep out the group->item_list frame sensitive columns */
              sensitive_list = zMap_g_list_grep(&(strand_group->item_list),
                                                order_data,
                                                matchFrameSensitiveColCB);
              sensitive_tmp = sensitive_list;
            }
        }

      if(order_debug_G)
        printf("%s: \n", __PRETTY_FUNCTION__);

      /* order the non frame sensitive ones */
      orderColumnsForFrame(order_data, strand_group->item_list, ZMAPFRAME_NONE);

      if(sensitive_list)
        {
          GList *tmp_list;
          int position = 0;

          /* order the frame sensitive ones */
          orderColumnsForFrame(order_data, sensitive_list, ZMAPFRAME_0);
          
          /* need to move the place sensitive_list points to ... the first
           * one of next strand... MUST keep hold of the list
           * though!!!! */
          sensitive_list = tmp_list = g_list_first(sensitive_list);
          if((sensitive_list = g_list_find_custom(sensitive_list, 
                                                  GUINT_TO_POINTER(ZMAPFRAME_1),
                                                  columnWithFrame)))
            orderColumnsForFrame(order_data, sensitive_list, ZMAPFRAME_1);
          else
            sensitive_list = tmp_list;

          /* need to move the place sensitive_list points to */
          sensitive_list = tmp_list = g_list_first(sensitive_list);
          if((sensitive_list = g_list_find_custom(sensitive_list, 
                                                  GUINT_TO_POINTER(ZMAPFRAME_2),
                                                  columnWithFrame)))
            orderColumnsForFrame(order_data, sensitive_list, ZMAPFRAME_2);
          else
            sensitive_list = tmp_list;

          sensitive_list = g_list_first(sensitive_tmp);

          /* find where to insert the sensitive list. */
          while(frame_list)
            {
              if((tmp_list = g_list_find_custom(strand_group->item_list,
                                                frame_list->data,
                                                columnWithName)))
                {
                  position = g_list_position(strand_group->item_list, tmp_list);
                  break;
                }
              frame_list = frame_list->prev;
            }

          /* insert the frame sensitive ones into the non frame sensitive list */
          strand_group->item_list = zMap_g_list_insert_list_after(strand_group->item_list, 
                                                                  sensitive_list, position);
        }

      /* update foo_canvas list cache. */
      strand_group->item_list_end = g_list_last(strand_group->item_list);
    }
  
  return ;
}
