/*  File: zmapWindowState.c
 *  Author: Roy Storey (rds@sanger.ac.uk)
 *  Copyright (c) 2007: Genome Research Ltd.
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
 * Last edited: Nov 13 08:16 2007 (rds)
 * Created: Mon Jun 11 09:49:16 2007 (rds)
 * CVS info:   $Id: zmapWindowState.c,v 1.5 2007-11-13 10:54:29 rds Exp $
 *-------------------------------------------------------------------
 */

#include <string.h>
#include <ZMap/zmapUtils.h>
#include <zmapWindow_P.h>
#include <zmapWindowContainer.h>
#include <zmapWindowState.h>

typedef struct
{
  GQuark align_id, block_id, set_id, feature_id;
  ZMapFrame frame;
  ZMapStrand strand;
  double x1, x2, y1, y2;
  gboolean rev_comp_state;
} ZMapWindowMarkSerialStruct;


typedef struct
{
  double scroll_x1, scroll_x2;
  double scroll_y1, scroll_y2;

  GtkAdjustment v_adjuster;	/* Be very careful with this! */
  GtkAdjustment h_adjuster;	/* Be very careful with this! */

  int scroll_offset_x, scroll_offset_y;
  gboolean rev_comp_state;
} ZMapWindowPositionStruct, ZMapWindowPosition;

/* _NOTHING_ in this struct (ZMapWindowStateStruct) should refer 
 * to dynamic memory elsewhere. This means ...
 * - No FooCanvasItems
 * - No ZMapWindows
 * - No ZMapViews
 * Everything should be serialised to enable reverse complements,
 * window splitting, zooming and any other action where state needs 
 * to be kept, but where features might be destroyed, altered or
 * logically be the wrong object to restore state to.
 *************************************/
typedef struct _ZMapWindowStateStruct
{
  ZMapMagic   magic;

  /* state of stuff ... */
  double       zoom_factor;
  ZMapWindowMarkSerialStruct mark;
  ZMapWindowPositionStruct position;

  gboolean rev_comp_state;

  /* flags to say which states are set/unset */
  unsigned int zoom_set   : 1 ;
  unsigned int mark_set   : 1 ;
  unsigned int position_set : 1 ;
  unsigned int in_state_restore : 1 ;
} ZMapWindowStateStruct;

static void state_mark_restore(ZMapWindow window, ZMapWindowMark mark, ZMapWindowMarkSerialStruct *serialized);
static void state_position_restore(ZMapWindow window, ZMapWindowPositionStruct *position);
static void print_position(ZMapWindowPositionStruct *position, char *from);

gboolean queue_doing_update(ZMapWindowStateQueue queue);
void mark_queue_updating(ZMapWindowStateQueue queue, gboolean update_flag);

ZMAP_MAGIC_NEW(window_state_magic_G, ZMapWindowStateStruct) ;

static gboolean window_state_debug_G = FALSE;

ZMapWindowState zmapWindowStateCreate(void)
{
  ZMapWindowState state = NULL;

  if(!(state = g_new0(ZMapWindowStateStruct, 1)))
    zMapAssertNotReached();
  else
    state->magic = window_state_magic_G;

  return state;
}

void zmapWindowStateRestore(ZMapWindowState state, ZMapWindow window)
{
  mark_queue_updating(window->history, TRUE);

  if(state->mark_set)
    {
      state_mark_restore(window, window->mark, &(state->mark));
    }

  if(state->position_set)
    {
      state_position_restore(window, &(state->position));
    }

  if(state->zoom_set)
    {
      double current, target, factor;
      current = zMapWindowGetZoomFactor(window);
      target  = state->zoom_factor;
      factor  = target / current;
      zMapWindowZoom(window, factor);
    }

  mark_queue_updating(window->history, FALSE);

  return ;
}

ZMapWindowState zmapWindowStateCopy(ZMapWindowState state)
{
  ZMapWindowState new_state = NULL;

  zMapAssert(state && ZMAP_MAGIC_IS_VALID(window_state_magic_G, state->magic)) ;

  new_state = zmapWindowStateCreate();
  
  memcpy(new_state, state, sizeof(ZMapWindowStateStruct));

  return new_state;
}

ZMapWindowState zmapWindowStateDestroy(ZMapWindowState state)
{
  zMapAssert(state && ZMAP_MAGIC_IS_VALID(window_state_magic_G, state->magic)) ;

  /* Anything that needs freeing */


  /* ZERO AND FREE */
  ZMAP_PTR_ZERO_FREE(state, ZMapWindowStateStruct);

  return state;
}

gboolean zmapWindowStateSavePosition(ZMapWindowState state, ZMapWindow window)
{
  GtkAdjustment *v_adjuster, *h_adjuster;
  FooCanvas *canvas;
  GtkScrolledWindow *scrolled_window;

  if(window->canvas && FOO_IS_CANVAS(window->canvas))
    {
      canvas          = FOO_CANVAS(window->canvas);
      scrolled_window = GTK_SCROLLED_WINDOW(window->scrolled_window);

      if((v_adjuster  = gtk_scrolled_window_get_vadjustment(scrolled_window)))
	{
	  state->position.v_adjuster = *v_adjuster; /* n.b. struct copy! */
	}

      if((h_adjuster  = gtk_scrolled_window_get_hadjustment(scrolled_window)))
	{
	  state->position.h_adjuster = *h_adjuster; /* n.b. struct copy! */      
	}
      
      foo_canvas_get_scroll_offsets(canvas, &(state->position.scroll_offset_x), &(state->position.scroll_offset_y));
      
      foo_canvas_get_scroll_region(canvas, 
				   &(state->position.scroll_x1), 
				   &(state->position.scroll_y1),
				   &(state->position.scroll_x2), 
				   &(state->position.scroll_y2));

      state->rev_comp_state = state->position.rev_comp_state = window->revcomped_features;

      if(window_state_debug_G)
	print_position(&(state->position), "save_position");

      state->position_set = TRUE;
    }

  return state->position_set;
}

gboolean zmapWindowStateSaveZoom(ZMapWindowState state, double zoom_factor)
{
  zMapAssert(state && ZMAP_MAGIC_IS_VALID(window_state_magic_G, state->magic)) ;

  state->zoom_set    = 1;
  state->zoom_factor = zoom_factor;

  return state->zoom_set;
}

gboolean zmapWindowStateSaveMark(ZMapWindowState state, ZMapWindow window)
{
  ZMapWindowMark mark = window->mark;

  zMapAssert(state && ZMAP_MAGIC_IS_VALID(window_state_magic_G, state->magic)) ;

  if((state->mark_set = zmapWindowMarkIsSet(mark)))
    {
      FooCanvasItem *mark_item;
      FooCanvasGroup *mark_item_cont;
      if((mark_item = zmapWindowMarkGetItem(mark)))
	{
	  ZMapFeature feature = g_object_get_data(G_OBJECT(mark_item), ITEM_FEATURE_DATA);

	  if((mark_item_cont = zmapWindowContainerGetFromItem(mark_item)))
	    {
	      ZMapWindowItemFeatureSetData set_data;

	      if((set_data = zmapWindowContainerGetData(mark_item_cont, ITEM_FEATURE_SET_DATA)))
		{
		  state->mark.strand     = set_data->strand;
		  state->mark.frame      = set_data->frame;
		  state->mark.align_id   = feature->parent->parent->parent->unique_id;
		  state->mark.block_id   = feature->parent->parent->unique_id;
		  state->mark.set_id     = feature->parent->unique_id;
		  state->mark.feature_id = feature->unique_id;
		}
	      else
		state->mark_set = FALSE;
	    }
	}

      /* Always get the world range, so that if feature can not be found on restore, we can still remark. */
      zmapWindowMarkGetWorldRange(mark, 
				  &(state->mark.x1), &(state->mark.y1),
				  &(state->mark.x2), &(state->mark.y2));

      state->rev_comp_state = state->mark.rev_comp_state = window->revcomped_features;
    }

  return state->mark_set;
}


/* INTERNAL */

static void rev_comp_about_origin(int origin, double *a, double *b)
{
  double tp;			/* temp */
  if(a && b)
    {
      *a = origin - *a;
      *b = origin - *b;
      tp = *a;
      *a = *b;
      *b = tp;
    }
  return ;
}

static void state_mark_restore(ZMapWindow window, ZMapWindowMark mark, ZMapWindowMarkSerialStruct *serialized)
{
  ZMapWindowMarkSerialStruct restore = {};
  restore = *serialized;	/* n.b. struct copy */

  if(window->revcomped_features != restore.rev_comp_state)
    {
      double seq_length;
      seq_length = window->seqLength + 1; /* seqToExtent */
      rev_comp_about_origin(seq_length, &(restore.y1), &(restore.y2));
    }
  
  if(serialized->align_id != 0 && serialized->feature_id != 0)
    {
      FooCanvasItem *mark_item;
      GList *possible_mark_items;

      /* We're not completely accurate here.  SubPart features are not remarked correctly. */
      if((mark_item = zmapWindowFToIFindItemFull(window->context_to_item,
						 restore.align_id,
						 restore.block_id,
						 restore.set_id,
						 restore.strand,
						 restore.frame,
						 restore.feature_id)))
	{
	  zmapWindowMarkSetItem(mark, mark_item);
	}
      else if((possible_mark_items = zmapWindowFToIFindItemSetFull(window->context_to_item,
								   restore.align_id,
								   restore.block_id,
								   restore.set_id,
								   "*",	/* reverse complement... */
								   "*",	/* laziness */
								   restore.feature_id,
								   NULL, NULL)))
	{
	  zmapWindowMarkSetItem(mark, possible_mark_items->data);
	}
      else
	zmapWindowMarkSetWorldRange(mark, 
				    restore.x1, restore.y1, 
				    restore.x2, restore.y2);

    }
  else
    zmapWindowMarkSetWorldRange(mark, 
				restore.x1, restore.y1, 
				restore.x2, restore.y2);
  
  return ;
}

static void state_position_restore(ZMapWindow window, ZMapWindowPositionStruct *position)
{
  FooCanvas *canvas;
  GtkAdjustment *v_adjuster, *h_adjuster;
  GtkScrolledWindow *scrolled_window;
  ZMapWindowPositionStruct new_position = {};

  if(window->canvas && FOO_IS_CANVAS(window->canvas))
    {
      canvas = window->canvas;
      
      scrolled_window = GTK_SCROLLED_WINDOW(window->scrolled_window);
      v_adjuster      = gtk_scrolled_window_get_vadjustment(scrolled_window);
      h_adjuster      = gtk_scrolled_window_get_hadjustment(scrolled_window);
      /* I'm copying this, so as not to alter the state->position version. */
      /* Accidentally or otherwise doing a state restore more than once should be ok. */
      new_position = *position;	/* n.b. struct copy! */

      if(window_state_debug_G)
	print_position(position, "state_position_restore input");

      if(window->revcomped_features != position->rev_comp_state)
	{
	  int seq_length;
	  double tmp;
	  /* we need to swap positions in the position struct... */
	  seq_length = window->seqLength + 1; /* seqToExtent */

	  if(window_state_debug_G)
	    print_position(position, "state_position_restore rev-comp status switched! reversing position...");

	  rev_comp_about_origin(seq_length, &(new_position.scroll_y1), &(new_position.scroll_y2));
#ifdef RDS_DONT_INCLUDE
	  new_position.scroll_y1 = seq_length - position->scroll_y1;
	  new_position.scroll_y2 = seq_length - position->scroll_y2;

	  tmp                    = new_position.scroll_y1;
	  new_position.scroll_y1 = new_position.scroll_y2;
	  new_position.scroll_y2 = tmp;
#endif
	  tmp                           = new_position.v_adjuster.value + new_position.v_adjuster.page_size;
	  new_position.v_adjuster.value = new_position.v_adjuster.upper - tmp;
	  new_position.scroll_offset_y  = new_position.v_adjuster.upper - tmp;
	}

      zmapWindowSetScrollRegion(window, 
				&(new_position.scroll_x1), &(new_position.scroll_y1),
				&(new_position.scroll_x2), &(new_position.scroll_y2));
      
      gtk_adjustment_set_value(v_adjuster, new_position.v_adjuster.value);
      gtk_adjustment_set_value(h_adjuster, new_position.h_adjuster.value);

      if(window_state_debug_G)
	print_position(&new_position, "state_position_restore input");
    }

  return ;
}


static void print_position(ZMapWindowPositionStruct *position, char *from)
{
  printf("%s: position is\n", from);

  printf("\tscroll-region:\t%f, %f, %f, %f\n", 
	 position->scroll_x1, position->scroll_y1, 
	 position->scroll_x2, position->scroll_y2);

  printf("\tv-adjuster: value = %f, page-size = %f, lower = %f, upper = %f\n",
	 position->v_adjuster.value, position->v_adjuster.page_size,
	 position->v_adjuster.lower, position->v_adjuster.upper);

  printf("\tscroll-offsets: x = %d, y = %d\n",
	 position->scroll_offset_x, position->scroll_offset_y);
  
  return ;
}


/* Queue code */

ZMapWindowStateQueue zmapWindowStateQueueCreate(void)
{
  ZMapWindowStateQueue queue = NULL;

  if(!(queue = g_queue_new()))
    zMapAssertNotReached();

  if(queue)
    {
      ZMapWindowState head_state;
      if((head_state = zmapWindowStateCreate()))
	{
	  head_state->in_state_restore = FALSE;
	  g_queue_push_head(queue, head_state);
	}
    }

  return queue;
}

int zmapWindowStateQueueLength(ZMapWindowStateQueue queue)
{
  int size = 0;
  
  if((size = g_queue_get_length(queue)) > 0)
    size--;
  else if(size == 0)
    zMapLogWarning("%s", "Queue has zero size. This is unexpected!");

  return size;
}

gboolean zMapWindowHasHistory(ZMapWindow window)
{
  if(zmapWindowStateQueueLength(window->history))
    return TRUE;
  return FALSE;
}

gboolean zmapWindowStateGetPrevious(ZMapWindow window, ZMapWindowState *state_out, gboolean pop)
{
  ZMapWindowState state = NULL;
  gboolean got_state = FALSE;
  
  if(zMapWindowHasHistory(window))
    {
      if(pop)
	state = g_queue_pop_tail(window->history);
      else
	state = g_queue_peek_tail(window->history);
    }

  if(state_out && state)
    {
      got_state  = TRUE;
      *state_out = state;
    }
  
  return got_state;
}

gboolean zmapWindowStateQueueStore(ZMapWindow window, ZMapWindowState state_in, gboolean clear_current)
{
  ZMapWindowStateQueue queue = window->history;
  gboolean stored = FALSE;

  if((stored = !queue_doing_update(queue)) == TRUE)
    {
      if(clear_current)
	zmapWindowStateQueueClear(queue);
      
      g_queue_push_tail(queue, state_in);
    }

  return stored;
}


void zmapWindowStateQueueClear(ZMapWindowStateQueue queue)
{
  ZMapWindowState state = NULL;

  while(zmapWindowStateQueueLength(queue) &&
	(state = g_queue_pop_tail(queue)))
    {
      state = zmapWindowStateDestroy(state);
    }

  return ;
}

ZMapWindowStateQueue zmapWindowStateQueueDestroy(ZMapWindowStateQueue queue)
{
  ZMapWindowState head_state;

  zmapWindowStateQueueClear(queue);

  if((head_state = g_queue_pop_head(queue)))
    zmapWindowStateDestroy(head_state);

  g_queue_free(queue);

  queue = NULL;

  return queue;
}

/* Queue internals */

gboolean queue_doing_update(ZMapWindowStateQueue queue)
{
  ZMapWindowState head_state;
  gboolean doing_update = FALSE;

  if((head_state = g_queue_peek_head(queue)))
    {
      doing_update = head_state->in_state_restore;
    }

  return doing_update;
}

void mark_queue_updating(ZMapWindowStateQueue queue, gboolean update_flag)
{
  ZMapWindowState head_state;

  if((head_state = g_queue_peek_head(queue)))
    {
      head_state->in_state_restore = update_flag;
    }

  return ;
}
