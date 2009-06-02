/*  File: zmapWindowCanvas.c
 *  Author: Roy Storey (rds@sanger.ac.uk)
 *  Copyright (c) 2009: Genome Research Ltd.
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
 * Last edited: Jun  1 10:13 2009 (rds)
 * Created: Wed Apr 29 14:42:41 2009 (rds)
 * CVS info:   $Id: zmapWindowCanvas.c,v 1.1 2009-06-02 11:20:23 rds Exp $
 *-------------------------------------------------------------------
 */

#include <zmapWindowCanvas_I.h>

enum
  {
    CANVAS_PROP_0,		/* zero is invalid property id */
    CANVAS_PIXELS_PER_UNIT,	/*! pixels per unit in both axis */
    CANVAS_PIXELS_PER_UNIT_X,	/*! pixels per unit in x axis */
    CANVAS_PIXELS_PER_UNIT_Y,	/*! pixels per unit in y axis */
    CANVAS_MAX_ZOOM_X,		/*! maximum pixels per unit in x axis */
    CANVAS_MAX_ZOOM_Y,		/*! maximum pixels per unit in y axis */
  };

enum
  {
    ZMAP_WINDOW_MAX_WINDOW = 30000
  };

static void zmap_window_canvas_class_init  (ZMapWindowCanvasClass canvas_class);
static void zmap_window_canvas_init        (ZMapWindowCanvas      canvas);
static void zmap_window_canvas_set_property(GObject               *object, 
					    guint                  param_id,
					    const GValue          *value,
					    GParamSpec            *pspec);
static void zmap_window_canvas_get_property(GObject               *object,
					    guint                  param_id,
					    GValue                *value,
					    GParamSpec            *pspec);
#ifdef REQUIRE_DESTROY
static void zmap_window_canvas_destroy     (GObject *object);
#endif /* REQUIRE_DESTROY */
static gint zmap_window_canvas_expose(GtkWidget      *widget, 
				      GdkEventExpose *event);

/* Long Items Functions */
static ZMapWindowLong_Items zmap_window_canvas_long_items_create(ZMapWindowCanvas canvas);
static void                 zmap_window_canvas_long_items_set_max_zoom(ZMapWindowCanvas canvas);
static void                 zmap_window_canvas_long_items_check(ZMapWindowCanvas canvas, 
								FooCanvasItem   *item, 
								double start, double end);
static gboolean             zmap_window_canvas_long_items_crop(ZMapWindowCanvas canvas);
static gboolean             zmap_window_canvas_long_items_item_is_long(ZMapWindowCanvas canvas, 
								       FooCanvasItem   *item);
static gboolean             zmap_window_canvas_long_items_get_coords(ZMapWindowCanvas canvas, 
								     FooCanvasItem   *item,
								     double *x1, double *y1,
								     double *x2, double *y2);
static gboolean             zmap_window_canvas_long_items_remove(ZMapWindowCanvas canvas,
								 FooCanvasItem   *item);
static void                 zmap_window_canvas_long_items_free(ZMapWindowCanvas canvas);
static void                 zmap_window_canvas_long_items_destroy(ZMapWindowCanvas canvas);
/* Long Items Functions */


static GtkWidgetClass *parent_widget_class_G = NULL;
static gboolean window_canvas_expose_crops_G = TRUE;
static gboolean window_canvas_meticulous_check_G = TRUE; /* This is _very_ slow! */

GType zMapWindowCanvasGetType (void)
{
  static GType canvas_type = 0;
  
  if (canvas_type == 0) 
    {
      static const GTypeInfo canvas_info = {
	sizeof (zmapWindowCanvasClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) zmap_window_canvas_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data */
	sizeof (zmapWindowCanvas),
	0,              /* n_preallocs */
	(GInstanceInitFunc) zmap_window_canvas_init,
	NULL
      };
      
      canvas_type = g_type_register_static (foo_canvas_get_type (),
					    ZMAP_WINDOW_CANVAS_NAME,
					    &canvas_info,
					    0);
    }
  
  return canvas_type;
}

/* The idea of the busy/unbusy routines is to stop _all_ redrawing, by
 * not calling the foo_canvas_expose() while busy, and to unset this 
 * switch and expose the current region when unbusy. */

gboolean zMapWindowCanvasBusy(ZMapWindowCanvas canvas)
{
  GQueue *queue;
  gboolean result = TRUE;

  g_return_val_if_fail(ZMAP_IS_CANVAS(canvas), FALSE);

  queue = canvas->busy_queue;

  /* mark long items as busy */
  g_queue_push_tail(queue, GUINT_TO_POINTER(TRUE));

  /* and also mark canvas as busy */
  canvas->canvas_busy = TRUE;

  return result;
}

/*
 * If zMapWindowCanvasUnBusy() finds no pending busy-ness it will
 * invalidate the rectangle of the current area and make the window
 * redraw.
 *
 */

/*
 * comments for
 * gdk_window_invalidate_rect () & gdk_window_invalidate_region ()

 * Adds region to the update area for window. The update area is the
 * region that needs to be redrawn, or "dirty region." The call
 * gdk_window_process_updates() sends one or more expose events to the
 * window, which together cover the entire update area. An application
 * would normally redraw the contents of window in response to those
 * expose events.

 * GDK will call gdk_window_process_all_updates() on your behalf
 * whenever your program returns to the main loop and becomes idle, so
 * normally there's no need to do that manually, you just need to
 * invalidate regions that you know should be redrawn.

 * The invalidate_children parameter controls whether the region of
 * each child window that intersects region will also be
 * invalidated. If FALSE, then the update area for child windows will
 * remain unaffected. See gdk_window_invalidate_maybe_recurse if you
 * need fine grained control over which children are invalidated.
 */

gboolean zMapWindowCanvasUnBusy(ZMapWindowCanvas canvas)
{
  GQueue *queue = NULL;
  gboolean result = TRUE;
  gboolean need_redraw = FALSE;

  g_return_val_if_fail(ZMAP_IS_CANVAS(canvas), FALSE);

  queue = canvas->busy_queue;

  if(g_queue_get_length(queue) != 0)
    {
      need_redraw = GPOINTER_TO_UINT( g_queue_pop_tail(queue) );
    }

  if(need_redraw && (g_queue_get_length(queue) == 0))
    {
      FooCanvas *foo_canvas;
      int x1, y1, x2, y2;

      canvas->canvas_busy = FALSE;

      foo_canvas = FOO_CANVAS(canvas);

      x1 = y1 = x2 = y2 = 0;

      foo_canvas_request_redraw(foo_canvas, x1, y1, x2, y2);

      gdk_window_process_updates(foo_canvas->layout.bin_window, TRUE);
    }

  return result;
}

void zMapWindowCanvasLongItemCheck(ZMapWindowCanvas canvas, FooCanvasItem *item,
				   double start, double end)
{
  zmap_window_canvas_long_items_check(canvas, item, start, end);

  return ;
}

void zMapWindowCanvasLongItemRemove(ZMapWindowCanvas canvas, FooCanvasItem *item_to_remove)
{
  zmap_window_canvas_long_items_remove(canvas, item_to_remove);

  return ;
}

GtkWidget *zMapWindowCanvasNew(double max_zoom)
{
  GtkWidget *canvas = NULL;
  GObject *object = NULL;

  object = g_object_new(ZMAP_TYPE_CANVAS,
			"max-zoom-x", max_zoom,
			"max-zoom-y", max_zoom,
			NULL);
  if(object)
    {
      canvas = GTK_WIDGET(object);
    }
  
  return canvas;
}


/*  */


static void visit_all_items_cb(FooCanvasItem *this_item, GFunc each_item, gpointer user_data)
{
  if(FOO_IS_CANVAS_GROUP(this_item))
    {
      FooCanvasGroup *this_group;
      FooCanvasItem *item;
      GList *list;
      
      this_group = FOO_CANVAS_GROUP(this_item);

      if((list = g_list_first(this_group->item_list)))
	{
	  do
	    {
	      item = FOO_CANVAS_ITEM(list->data);
	      visit_all_items_cb(item, each_item, user_data);
	    }
	  while((list = list->next));
	}
    }
  else
    {
      (each_item)(this_item, user_data);
    }

  return ;
}

static void visit_all_items(ZMapWindowCanvas canvas, GFunc each_item, gpointer user_data)
{
  FooCanvasItem *root;

  g_return_if_fail(canvas != NULL);
  g_return_if_fail(each_item != NULL);

  root = FOO_CANVAS(canvas)->root;

  visit_all_items_cb(root, each_item, user_data);

  return ;
}


/*  */

static void zmap_window_canvas_class_init  (ZMapWindowCanvasClass canvas_class)
{
  GObjectClass   *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  
  gobject_class = (GObjectClass *)   canvas_class;
  object_class  = (GtkObjectClass *) canvas_class;
  widget_class  = (GtkWidgetClass *) canvas_class;

  parent_widget_class_G = g_type_class_peek_parent(canvas_class);
  
  gobject_class->set_property = zmap_window_canvas_set_property;
  gobject_class->get_property = zmap_window_canvas_get_property;

  widget_class->expose_event  = zmap_window_canvas_expose;
  
  g_object_class_install_property(gobject_class, CANVAS_MAX_ZOOM_X,
				  g_param_spec_double("max-zoom-x", "max zoom x",
						      "The maximum zoom pixels per unit for x axis",
						      0.0, G_MAXDOUBLE, 0.0, 
						      ZMAP_PARAM_STATIC_RW));

  g_object_class_install_property(gobject_class, CANVAS_MAX_ZOOM_Y,
				  g_param_spec_double("max-zoom-y", "max zoom y",
						      "The maximum zoom pixels per unit for y axis",
						      0.0, G_MAXDOUBLE, 0.0, 
						      ZMAP_PARAM_STATIC_RW));  

  g_object_class_install_property(gobject_class, CANVAS_PIXELS_PER_UNIT,
				  g_param_spec_double("pixels-per-unit", "pixels per unit",
						      "The pixels per unit for both axis",
						      0.0, G_MAXDOUBLE, 0.0, 
						      ZMAP_PARAM_STATIC_RW));

  g_object_class_install_property(gobject_class, CANVAS_PIXELS_PER_UNIT_X,
				  g_param_spec_double("pixels-per-unit-x", "pixels per unit x",
						      "The pixels per unit for x axis",
						      0.0, G_MAXDOUBLE, 0.0, 
						      ZMAP_PARAM_STATIC_RW));

  g_object_class_install_property(gobject_class, CANVAS_PIXELS_PER_UNIT_Y,
				  g_param_spec_double("pixels-per-unit-y", "pixels per unit y",
						      "The pixels per unit for y axis",
						      0.0, G_MAXDOUBLE, 0.0, 
						      ZMAP_PARAM_STATIC_RW));


  return ;
}

static void zmap_window_canvas_init        (ZMapWindowCanvas      canvas)
{
  canvas->long_items  = zmap_window_canvas_long_items_create(canvas);

  canvas->busy_queue  = g_queue_new();

  canvas->busy_cursor = gdk_cursor_new(GDK_WATCH);

  return ;
}

static void zmap_window_canvas_set_property(GObject               *object, 
					    guint                  param_id,
					    const GValue          *value,
					    GParamSpec            *pspec)
{
  ZMapWindowCanvas canvas;

  canvas = ZMAP_CANVAS(object);

  switch(param_id)
    {
    case CANVAS_MAX_ZOOM_X:
    case CANVAS_MAX_ZOOM_Y:
      {
	double max_zoom;

	max_zoom = g_value_get_double(value);

	if(param_id == CANVAS_MAX_ZOOM_Y)
	  canvas->max_zoom_y = max_zoom;

	if(param_id == CANVAS_MAX_ZOOM_X)
	  canvas->max_zoom_x = max_zoom;

	zmap_window_canvas_long_items_set_max_zoom(canvas);
      }
      break;
    case CANVAS_PIXELS_PER_UNIT:
    case CANVAS_PIXELS_PER_UNIT_X:
    case CANVAS_PIXELS_PER_UNIT_Y:
      break;
    default:
      break;
    }

  return ;
}

static void zmap_window_canvas_get_property(GObject               *object,
					    guint                  param_id,
					    GValue                *value,
					    GParamSpec            *pspec)
{

  return ;
}

#ifdef REQUIRE_DESTROY
static void zmap_window_canvas_destroy     (GObject *object)
{

  return ;
}
#endif /* REQUIRE_DESTROY */

static gboolean zmap_window_canvas_requires_crop(ZMapWindowCanvas canvas, 
						 double *ppux, double *ppuy,
						 double *x1,   double *y1,
						 double *x2,   double *y2)
{
  FooCanvas *foo_canvas;
  gboolean crop_required = TRUE;

  foo_canvas = FOO_CANVAS(canvas);

  foo_canvas_get_scroll_region(foo_canvas, x1, y1, x2, y2);

  if(ppux)
    *ppux = foo_canvas->pixels_per_unit_x;

  if(ppuy)
    *ppuy = foo_canvas->pixels_per_unit_y;

  if((canvas->last_cropped_region.ppuy != *ppuy) ||
     (canvas->last_cropped_region.y1 != *y1)     ||
     (canvas->last_cropped_region.y2 != *y2))
    {
      crop_required = TRUE;
    }
  else
    crop_required = FALSE;

  canvas->last_cropped_region.x1 = *x1;
  canvas->last_cropped_region.x2 = *x2;
  canvas->last_cropped_region.y1 = *y1;
  canvas->last_cropped_region.y2 = *y2;
  canvas->last_cropped_region.ppux = *ppux;
  canvas->last_cropped_region.ppuy = *ppuy;

  return crop_required;
}

static gboolean zmap_window_canvas_get_visible_area(ZMapWindowCanvas canvas,
						    GdkRectangle *rectangle_in_out)
{
  GdkRectangle rect = {};
  GtkAdjustment *hadj, *vadj;

  hadj = GTK_LAYOUT(canvas)->hadjustment;
  vadj = GTK_LAYOUT(canvas)->vadjustment;

  rect.x = hadj->value ;
  rect.y = vadj->value ;
  
  rect.width  = hadj->page_size;
  rect.height = vadj->page_size;

  if(rectangle_in_out)
    *rectangle_in_out = rect;	/* struct copy */

  return TRUE;
}

static void meticulous_check(FooCanvasItem *item, ZMapWindowCanvas canvas)
{
  FooCanvas *foo_canvas;
  double x1, y1, x2, y2, pixel_extent;
  gboolean too_big = FALSE;
  gboolean drawn = FALSE;

  foo_canvas_item_get_bounds(item, &x1, &y1, &x2, &y2);
  foo_canvas = FOO_CANVAS(canvas);

  if((pixel_extent = (y2 - y1 + 1.0) * foo_canvas->pixels_per_unit_y) > ZMAP_WINDOW_MAX_WINDOW)
    {
      double scroll_x1, scroll_y1, scroll_x2, scroll_y2, dummy_x = 0.0;

      too_big = TRUE;
      
      scroll_x1 = foo_canvas->scroll_x1;
      scroll_y1 = foo_canvas->scroll_y1;
      scroll_x2 = foo_canvas->scroll_x2;
      scroll_y2 = foo_canvas->scroll_y2;

      foo_canvas_item_i2w(item, &dummy_x, &y1);
      foo_canvas_item_i2w(item, &dummy_x, &y2);

      if (!(y2 < scroll_y1) && !(y1 > scroll_y2) && ((y1 < scroll_y1) || (y2 > scroll_y2)))
	{
	  //drawn = TRUE;
	}
    }

  if(too_big && drawn)
    {
      const char *type_name, *parent_type_name, *colour = "#FF0099";
      GdkColor awful_pink;

      type_name = G_OBJECT_TYPE_NAME(G_OBJECT(item));
      
      parent_type_name = G_OBJECT_TYPE_NAME(G_OBJECT(item->parent));

      gdk_color_parse(colour, &awful_pink);

      if(window_canvas_expose_crops_G && 0)
	foo_canvas_item_set(item,
			    "fill_color_gdk", &awful_pink,
			    NULL);

      g_warning("Item (Type='%s',Parent='%s') is too big! Coords (y1,y2)=(%f, %f) @ %f=%f. Colour now='%s'", 
		type_name, parent_type_name, y1, y2, foo_canvas->pixels_per_unit_y, pixel_extent, colour);

    }

  return ;
}

static void meticulous_foreach_check(gpointer foo_canvas_item_or_group, gpointer user_data)
{
  FooCanvasItem *item = FOO_CANVAS_ITEM(foo_canvas_item_or_group);
  FooCanvasGroup *group;

  if(FOO_IS_CANVAS_GROUP(item) && (group = FOO_CANVAS_GROUP(item)))
    {
      g_list_foreach(group->item_list, meticulous_foreach_check, user_data);
    }
  else
    {
      meticulous_check(item, ZMAP_CANVAS(user_data));
    }
}

static void zmap_window_canvas_meticulous_long_item_check(ZMapWindowCanvas canvas, gboolean cropped)
{
  FooCanvas *foo_canvas;
  
  foo_canvas = FOO_CANVAS(canvas);

  meticulous_foreach_check(foo_canvas->root, canvas);

  return ;
}

static gint zmap_window_canvas_expose(GtkWidget      *widget, 
				      GdkEventExpose *event)
{
  ZMapWindowCanvas window_canvas;
  FooCanvas *canvas;
  gboolean disable_draw;
  gboolean cropped = FALSE;
  int result = 0;

  canvas        = FOO_CANVAS(widget);
  window_canvas = ZMAP_CANVAS(widget);

  if(window_canvas_expose_crops_G)
    {
      cropped = zmap_window_canvas_long_items_crop(window_canvas);
    }
  
  if(window_canvas_meticulous_check_G)
    {
      zmap_window_canvas_meticulous_long_item_check(window_canvas, cropped);
    }

  disable_draw = window_canvas->canvas_busy;

  if(!disable_draw && !cropped)
    {
      gdk_window_set_cursor(canvas->layout.bin_window, window_canvas->busy_cursor);

      result = (GTK_WIDGET_CLASS(parent_widget_class_G)->expose_event)(widget, event);

      gdk_window_set_cursor(canvas->layout.bin_window, NULL);
    }
  else if(cropped)
    {
      gdk_window_invalidate_region(canvas->layout.bin_window, event->region, FALSE);
    }

  return result;
}



/* LongItems Code */


static ZMapWindowLong_Items zmap_window_canvas_long_items_create(ZMapWindowCanvas canvas)
{
  ZMapWindowLong_Items long_items = NULL;

  long_items = zmapWindowLong_ItemCreate(canvas->max_zoom_x, canvas->max_zoom_y);

  return long_items;
}

static void zmap_window_canvas_long_items_set_max_zoom(ZMapWindowCanvas canvas)
{

  zmapWindowLong_ItemSetMaxZoom(canvas->long_items, canvas->max_zoom_x, canvas->max_zoom_y);

  return ;
}

static void zmap_window_canvas_long_items_check(ZMapWindowCanvas canvas, 
						FooCanvasItem *item, 
						double start, double end)
{
  zmapWindowLong_ItemCheck(canvas->long_items, item, start, end);

  return ;
}

static gboolean zmap_window_canvas_long_items_crop(ZMapWindowCanvas canvas)
{
  double x1, x2, y1, y2, ppux, ppuy;
  gboolean crop_required;

  crop_required = zmap_window_canvas_requires_crop(canvas, &ppux, &ppuy, 
						   &x1, &y1, &x2, &y2);
  
  if(crop_required)
    zmapWindowLong_ItemCrop(canvas->long_items, ppux, ppuy, x1, y1, x2, y2);

  return crop_required;
}

static gboolean zmap_window_canvas_long_items_item_is_long(ZMapWindowCanvas canvas, 
							   FooCanvasItem   *item)
{
  gboolean is_long_item = FALSE;

  is_long_item = zmap_window_canvas_long_items_get_coords(canvas, item, NULL, NULL, NULL, NULL);

  return is_long_item;
}

static gboolean zmap_window_canvas_long_items_get_coords(ZMapWindowCanvas canvas, 
							 FooCanvasItem   *item,
							 double *x1, double *y1,
							 double *x2, double *y2)
{
  double start, end;
  gboolean found;

  if( x1 || y1 || x2 || y2 )
    foo_canvas_item_get_bounds(item, x1, y1, x2, y2);

  if((found = zmapWindowLong_ItemCoords(canvas->long_items, item, &start, &end)))
    {
      if(y1)
	*y1 = start;
      if(y2)
	*y2 = end;
    }

  return found;
}

static gboolean zmap_window_canvas_long_items_remove(ZMapWindowCanvas canvas,
						     FooCanvasItem   *item)
{
  gboolean result;

  result = zmapWindowLong_ItemRemove(canvas->long_items, item);

  return result;
}

static void zmap_window_canvas_long_items_free(ZMapWindowCanvas canvas)
{
  zmapWindowLong_ItemFree(canvas->long_items);

  return ;
}

static void zmap_window_canvas_long_items_destroy(ZMapWindowCanvas canvas)
{
  zmapWindowLong_ItemDestroy(canvas->long_items);

  canvas->long_items = NULL;

  return ;
}



