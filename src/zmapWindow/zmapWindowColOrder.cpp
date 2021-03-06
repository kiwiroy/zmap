/*  File: zmapWindowColOrder.c
 *  Author: Roy Storey (rds@sanger.ac.uk), Malcolm Hinsley (mh17@sanger.ac.uk)
 *  Copyright (c) 2006-2017: Genome Research Ltd.
 *-------------------------------------------------------------------
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------
 * This file is part of the ZMap genome database package
 * originally written by:
 * 
 *      Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk
 *        Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk
 *   Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *       Gemma Guest (Sanger Institute, UK) gb10@sanger.ac.uk
 *      Steve Miller (Sanger Institute, UK) sm23@sanger.ac.uk
 *  
 * Description:
 *
 * Exported functions: See zmapWindow_P.h
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.hpp>

#include <ZMap/zmapUtils.hpp>
#include <ZMap/zmapGLibUtils.hpp>
#include <zmapWindow_P.hpp>
#include <zmapWindowContainers.hpp>

using namespace std ;


typedef struct OrderColumnsDataStructType
{
  ZMapWindow window;            /* The window */

  ZMapStrand strand;            /* which strand group we're sorting...
                                 * direction of window->feature_set_names */
  //  GList *names_list;
  GHashTable *names_hash;      // featureset unique id -> position

  int three_frame_position;

  int n_names;
} OrderColumnsDataStruct, *OrderColumnsData;



static void orderPositionColumns(ZMapWindow window);
static void orderColumnsCB(ZMapWindowContainerGroup container, FooCanvasPoints *points,
                           ZMapContainerLevelType level, gpointer user_data);
static gint qsortColumnsCB(gconstpointer colA, gconstpointer colB, gpointer user_data);
static gboolean isFrameSensitive(gconstpointer col_data);
//static int columnFSNListPosition(gconstpointer col_data, GList *feature_set_names);

static void printCB(gpointer data, gpointer user_data) ;


/*
 *                Globals
 */

static gboolean order_debug_G = FALSE ;



/*
 *                Package routines
 */


/* void zmapWindowColOrderColumns(ZMapWindow window)
 *
 * Ordering the columns in line with the contents of
 * window->feature_set_names. The foocanvas
 */
void zmapWindowColOrderColumns(ZMapWindow window)
{
  orderPositionColumns(window);
}




/*
 *               Internal routines
 */

static void orderPositionColumns(ZMapWindow window)
{
  OrderColumnsDataStruct order_data = {NULL} ;
  guint i = 0;
  GList *names = NULL ;

  order_data.window = window;
  order_data.strand = ZMAPSTRAND_FORWARD; /* makes things simpler */

  if(order_debug_G)
    printf("%s: starting column ordering\n", __PRETTY_FUNCTION__);

  // Get the current order
  list<ZMapFeatureColumn> orig_columns ;

  if (window->context_map)
    orig_columns = window->context_map->getOrderedColumnsList() ;

  // make a hash table of positions for easy lookup
  order_data.names_hash = g_hash_table_new(NULL,NULL);
  GQuark threeframe = zMapStyleCreateID(ZMAP_FIXED_STYLE_3FRAME);

  // start at 1 to let us catch 0 == not found
  for(names = window->feature_set_names, i = 1; names; names = names->next, ++i)
    {
      GQuark id = zMapFeatureSetCreateID((char *) g_quark_to_string(GPOINTER_TO_UINT(names->data)));

      g_hash_table_insert(order_data.names_hash,GUINT_TO_POINTER(id), GUINT_TO_POINTER(i));
      if(id == threeframe)
        order_data.three_frame_position = i;

      if(order_debug_G)
        printf("WFSN %s = %d\n",g_quark_to_string(GPOINTER_TO_UINT(names->data)),i);
    }

  // Now loop through the original ordered list and append any that weren't in the feature_set_names to
  // the end.
  for (auto col_iter = orig_columns.begin(); col_iter != orig_columns.end(); ++col_iter)
    {
      ZMapFeatureColumn column = *col_iter ;

      // look up this column id and see if the order is set in the names hash
      guint order = GPOINTER_TO_INT(g_hash_table_lookup(order_data.names_hash, GINT_TO_POINTER(column->unique_id))) ;

      // order should never be 0, so 0 means it was null and therefore not found
      if (order)
        {
          // Update the struct with the order number that has already been set
          column->order = order ;
        }
      else
        {
          // Append this column name to the hash and update the column with the new order
          g_hash_table_insert(order_data.names_hash, GINT_TO_POINTER(column->unique_id), GUINT_TO_POINTER(i)) ;
          column->order = i ;
          ++i ;
        }
    }

  // Append 0 to indicate the end of the list. Anything else will be added after this.
  g_hash_table_insert(order_data.names_hash,GUINT_TO_POINTER(0), GUINT_TO_POINTER(i));
  order_data.n_names = i;


  if(!order_data.three_frame_position)
    order_data.three_frame_position = i;           // off the end


  zmapWindowContainerUtilsExecuteFull(window->feature_root_group,
                                      ZMAPCONTAINER_LEVEL_BLOCK,
                                      orderColumnsCB, &order_data,
                                      NULL, NULL);

  /* If we've reversed the feature_set_names list, and left it like that, re-reverse. */
  if(order_data.strand == ZMAPSTRAND_REVERSE)
    window->feature_set_names = g_list_reverse(window->feature_set_names);

  g_hash_table_destroy(order_data.names_hash);

  if (order_debug_G)
    {
      printf("%s: columns should now be in order\n", __PRETTY_FUNCTION__);
    }

  return ;
}

static void orderColumnsCB(ZMapWindowContainerGroup container, FooCanvasPoints *points,
                           ZMapContainerLevelType level, gpointer user_data)
{
  OrderColumnsData order_data = (OrderColumnsData)user_data;
  ZMapWindowContainerFeatures container_features;
  FooCanvasGroup *features_group;


  if(level == ZMAPCONTAINER_LEVEL_BLOCK)
    {
      /* Get Features */
      container_features = zmapWindowContainerGetFeatures(container);

      /* Cast to FooCanvasGroup */
      features_group = (FooCanvasGroup *) container_features;

      zMapWindowContainerGroupSortByLayer((FooCanvasGroup *) container_features);

#if 1
      /* hack: background appears at the front of the list and we don't implement overlay on block
         so trim the front, sort and put it back together
      */
      {
        GList *back, *feat;

        back = feat = features_group->item_list;
        while(feat && !ZMAP_IS_CONTAINER_GROUP(feat->data))
          {
            feat = feat->next;
          }

        if(back != feat && feat)
          {
            feat->prev->next = NULL;
            feat->prev = NULL;
          }
        else
          {
            back = NULL;
          }

        feat = g_list_sort_with_data(feat, qsortColumnsCB, order_data) ;

        if (order_debug_G)
          {
            printf("order of columns:\n") ;

            g_list_foreach(feat, printCB, NULL) ;
          }


        features_group->item_list = feat ;

        if (back)
          features_group->item_list = g_list_concat(back, feat) ;
      }
#else
      features_group->item_list = g_list_sort_with_data(features_group->item_list, qsortColumnsCB, order_data);
#endif


      /* update foo_canvas list cache. joy. */
      features_group->item_list_end = g_list_last(features_group->item_list);
    }

  return ;
}


static gboolean isFrameSensitive(gconstpointer col_data)
{
  ZMapWindowContainerFeatureSet container = ZMAP_CONTAINER_FEATURESET(col_data);
  gboolean frame_sensitive = FALSE ;
  ZMapFrame frame;

  frame = zmapWindowContainerFeatureSetGetFrame(container);

  if(frame != ZMAPFRAME_NONE)
    {
      frame_sensitive = zmapWindowContainerFeatureSetIsFrameSpecific(container, NULL);
    }

  return frame_sensitive;
}


static GQuark columnFSNid(gconstpointer col_data)
{
  GQuark id = 0;

#if MH17_COL_ID_WAS_FEATURE_ID
  FooCanvasGroup *col_group = FOO_CANVAS_GROUP(col_data);
  ZMapFeatureAny feature_any;

  if((feature_any = zmapWindowItemGetFeatureAny(col_group)))
    id = feature_any->unique_id;
#else
  ZMapWindowContainerFeatureSet container  = (ZMapWindowContainerFeatureSet) col_data;

  id = zmapWindowContainerFeaturesetGetColumnUniqueId(container) ;
#endif

  return id;
}



/* Sort so reverse strand columns come first and then forward strand cols, within
 * cols the order is given by that specified in the config file but note that
 * for reverse strand the order is reversed so that cols are a mirror image of
 * forward strand.
 *
 *
 * -1 if a is before b
 * +1 if a is after  b
 *  0 if equal
 */
static gint qsortColumnsCB(gconstpointer colA, gconstpointer colB, gpointer user_data)
{
  gint order = 0 ;
  OrderColumnsData order_data = (OrderColumnsData)user_data ;
  ZMapFrame frame_a, frame_b ;
  ZMapStrand strand_a, strand_b ;
  int pos_a, pos_b ;
  gboolean sens_a, sens_b ;
  GQuark idA, idB ;

  if(colA == colB)
    return order ;

  strand_a = zmapWindowContainerFeatureSetGetStrand((ZMapWindowContainerFeatureSet)colA) ;
  strand_b = zmapWindowContainerFeatureSetGetStrand((ZMapWindowContainerFeatureSet)colB) ;

  if(strand_a < strand_b)/* these are right to left */
    return(1) ;

  if(strand_a > strand_b)/* these are right to left */
    return(-1) ;

  idA = columnFSNid(colA);
  idB = columnFSNid(colB);

  sens_a = isFrameSensitive(colA);
  sens_b = isFrameSensitive(colB);

  pos_a = GPOINTER_TO_UINT(g_hash_table_lookup(order_data->names_hash,GUINT_TO_POINTER(idA)));
  pos_b = GPOINTER_TO_UINT(g_hash_table_lookup(order_data->names_hash,GUINT_TO_POINTER(idB)));

  if(!pos_a && !pos_b)  // both not in the list: put at the end and order by featureset id
    {
      pos_a = idA;
      pos_b = idB;
    }
  // else put unknown columns at the end
  else if(!pos_a)
    pos_a = order_data->n_names + 1;
  else if(!pos_b)
    pos_b = order_data->n_names + 1;

  if (order_debug_G)
    printf("compare %s/%d=%d %s/%d=%d", g_quark_to_string(idA),(int) sens_a,pos_a, g_quark_to_string(idB),(int) sens_b, pos_b);


  if (sens_a && sens_b)
    {
      /* Both are frame sensitive */
      frame_a = zmapWindowContainerFeatureSetGetFrame((ZMapWindowContainerFeatureSet) colA);
      frame_b = zmapWindowContainerFeatureSetGetFrame((ZMapWindowContainerFeatureSet) colB);
    }

  if (sens_a && sens_b)
    {
      /* If frames are equal rely on position */
      if (frame_a == frame_b)
        {
          if(pos_a > pos_b)
            order = 1;
          else if(pos_a < pos_b)
            order = -1;
          else
            order = 0;
        }
      else
        {
          /* else use frame */
          if(frame_a > frame_b)
            order = 1;
          else if(frame_a < frame_b)
            order = -1;
          else
            order = 0;
        }
    }
  else
    {
      if (sens_a)
        {
          /* only a is frame sensitive. Does b come before or after the 3 Frame section */
          pos_a = order_data->three_frame_position;
        }
      else if(sens_b)
        {
          /* only b is frame sensitive. Does a come before or after the 3 Frame section */
          pos_b = order_data->three_frame_position;
        }

      if (pos_a > pos_b)
        order = 1;
      else if(pos_a < pos_b)
        order = -1;
      else
        order = 0;
    }

  /* reverse strand needs to be reverse order so cols are mirror image of forward strand. */
  if (strand_a == strand_b && strand_a == ZMAPSTRAND_REVERSE)
    {
      if (order == 1)
        order = -1 ;
      else if (order == -1)
        order = 1 ;
    }


  if(order_debug_G)
    printf("order=  %d\n", order);

  return order;
}





static void printCB(gpointer data, gpointer user_data)
{
  ZMapWindowContainerFeatureSet featureset = (ZMapWindowContainerFeatureSet)data ;
  const char *col_name ;
  ZMapStrand strand ;

  col_name = zmapWindowContainerFeaturesetGetColumnName(featureset) ;
  strand = zmapWindowContainerFeatureSetGetStrand(featureset) ;

  printf("col strand/name: %c %s\n",
         (strand == ZMAPSTRAND_REVERSE ? '-' : '+'),
         col_name) ;

  return ;
}
