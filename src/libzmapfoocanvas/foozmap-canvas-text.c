/*  File: foozmap-canvas-text.c
 *  Author: Roy Storey (rds@sanger.ac.uk)
 *  Copyright (c) 2006-2014: Genome Research Ltd.
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
 *-------------------------------------------------------------------
 */

#include <config.h>
#include <math.h>
#include <string.h>
#include "foo-canvas-text.h"
#include "foozmap-canvas-text.h"

#include "foo-canvas-util.h"
#include "foo-canvas-i18n.h"

/* Object argument IDs */
enum {
	PROP_0,

	PROP_WRAP_MODE,

	PROP_TEXT_ALLOCATE_FUNC,
	PROP_TEXT_FETCH_TEXT_FUNC,
	PROP_TEXT_CALLBACK_DATA,

	PROP_TEXT_REQUESTED_WIDTH,
	PROP_TEXT_REQUESTED_HEIGHT
};

enum 
  {
    NO_NOTIFY_MASK            = 0,
    ZOOM_NOTIFY_MASK          = 1,
    SCROLL_REGION_NOTIFY_MASK = 2,
    KEEP_WITHIN_SCROLL_REGION = 4
  };

struct _FooCanvasZMapTextPrivate {
  /* a fixed size array buffer_size long */
  char                     *buffer;
  gint                      buffer_size;

  double                    requested_width;
  double                    requested_height;
  /* unsure why these two are here, remove?? */
  double                    zx,zy;

  /* a cache of the drawing extents/scroll region etc... */
  ZMapTextDrawDataStruct    update_cache;
  ZMapTextDrawDataStruct    draw_cache;
  /* the extents that should be used instead of the  */
  PangoRectangle            char_extents;
  FooCanvasZMapAllocateCB   allocate_func;
  FooCanvasZMapFetchTextCB  fetch_text_func;
  gpointer                  callback_data; /* user data for the allocate_func & fetch_text_func */
  unsigned int              flags        : 3;
  unsigned int              allocated    : 1;
  unsigned int              text_fetched : 1;
};

static void foo_canvas_zmap_text_class_init (FooCanvasZMapTextClass *class);
static void foo_canvas_zmap_text_init (FooCanvasZMapText *text);
static void foo_canvas_zmap_text_destroy (GtkObject *object);
static void foo_canvas_zmap_text_set_property (GObject            *object,
					    guint               param_id,
					    const GValue       *value,
					    GParamSpec         *pspec);
static void foo_canvas_zmap_text_get_property (GObject            *object,
					    guint               param_id,
					    GValue             *value,
					    GParamSpec         *pspec);

static void foo_canvas_zmap_text_realize   (FooCanvasItem  *item);
static void foo_canvas_zmap_text_unrealize (FooCanvasItem  *item);
static void foo_canvas_zmap_text_draw      (FooCanvasItem  *item,
					    GdkDrawable    *drawable,
					    GdkEventExpose *expose);
static void foo_canvas_zmap_text_update(FooCanvasItem *item, 
					double         i2w_dx, 
					double         i2w_dy, 
					int            flags);
static void foo_canvas_zmap_text_bounds (FooCanvasItem *item, 
					 double *x1, double *y1, 
					 double *x2, double *y2);

/*  */
static void allocate_buffer_size(FooCanvasZMapText *zmap);
static void run_update_cycle_loop(FooCanvasItem *item);
static gboolean canvas_has_changed(FooCanvasItem *item, ZMapTextDrawData draw_data);
static gboolean invoke_get_text(FooCanvasItem  *item);
static void invoke_allocate_width_height(FooCanvasItem *item);
static void save_origin(FooCanvasItem *item);
static void sync_data(ZMapTextDrawData from, ZMapTextDrawData to);

/* print out cached data functions */
static void print_private_data(FooCanvasZMapTextPrivate *private_data);
static void print_draw_data(ZMapTextDrawData draw_data);
static void print_calculate_buffer_data(FooCanvasZMapText *zmap,
					ZMapTextDrawData   draw_data,
					int cx1, int cy1, 
					int cx2, int cy2);


/* ************** our gdk drawing functions *************** */
static PangoRenderer *zmap_get_renderer(GdkDrawable *drawable,
					GdkGC       *gc,
					GdkColor    *foreground,
					GdkColor    *background);
static void zmap_release_renderer (PangoRenderer *renderer);
static void zmap_pango_renderer_draw_layout_get_size (PangoRenderer    *renderer,
						      PangoLayout      *layout,
						      int               x,
						      int               y,
						      GdkEventExpose   *expose,
						      GdkRectangle     *size_in_out);
static void zmap_gdk_draw_layout_get_size_within_expose(GdkDrawable    *drawable,
							GdkGC          *gc,
							int             x,
							int             y,
							PangoLayout    *layout,
							GdkEventExpose *expose,
							int            *width_max_in_out,
							int            *height_max_in_out);
/* ******************************************************* */

static gboolean debug_allocate  = FALSE;
static gboolean debug_functions = FALSE;
static gboolean debug_area      = FALSE;
static gboolean debug_table     = FALSE;

static FooCanvasItemClass *parent_class_G;


/**
 * foo_canvas_zmap_text_get_type:
 * @void:
 *
 * Registers the &FooCanvasZMapText class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the &FooCanvasZMapText class.
 **/
GtkType
foo_canvas_zmap_text_get_type (void)
{
  static GtkType text_type = 0;
  
  if (!text_type) {
    /* FIXME: Convert to gobject style.  */
    static const GtkTypeInfo text_info = {
      (char *)"FooCanvasZMapText",
      sizeof (FooCanvasZMapText),
      sizeof (FooCanvasZMapTextClass),
      (GtkClassInitFunc) foo_canvas_zmap_text_class_init,
      (GtkObjectInitFunc) foo_canvas_zmap_text_init,
      NULL, /* reserved_1 */
      NULL, /* reserved_2 */
      (GtkClassInitFunc) NULL
    };
    
    text_type = gtk_type_unique (foo_canvas_text_get_type (), &text_info);
  }
  
  return text_type;
}

void foo_canvas_pango2item(FooCanvas *canvas, int px, int py, double *ix, double *iy)
{
  double zoom_x, zoom_y;

  g_return_if_fail (FOO_IS_CANVAS (canvas));
  
  zoom_x = canvas->pixels_per_unit_x;
  zoom_y = canvas->pixels_per_unit_y;
  
  if (ix)
    *ix = ((((double)px) / PANGO_SCALE) / zoom_x);
  if (iy)
    *iy = ((((double)py) / PANGO_SCALE) / zoom_y);

  return ;
}

gboolean foo_canvas_item_text_index2item(FooCanvasItem *item, 
					 int index, 
					 double *item_coords_out)
{
  FooCanvasZMapText *zmap;
  gboolean index_found = FALSE ;

  if (FOO_IS_CANVAS_ZMAP_TEXT(item) && (zmap = FOO_CANVAS_ZMAP_TEXT(item)))
    {
      FooCanvasZMapTextPrivate *private_data;
      FooCanvasGroup *parent_group;
      ZMapTextDrawData draw_data;
      double w, h;
      int row_idx, row, width;
      double x1, x2, y1, y2;

      foo_canvas_item_get_bounds(item, &x1, &y1, &x2, &y2);
      
      parent_group = FOO_CANVAS_GROUP(item->parent);

#ifdef NOT_OUR_RESPONSIBILITY
      /* We are actually only drawing from parent_group->ypos so index must be reduced. */
      index -= parent_group->ypos;
#endif /* NOT_OUR_RESPONSIBILITY */

      private_data = zmap->priv;
      draw_data    = &(private_data->update_cache);
      
      if(draw_data->table.truncated)
	width = draw_data->table.untruncated_width;
      else
	width = draw_data->table.width;
      
      row_idx = index % width;
      row     = (index - row_idx) / width;

      if (row_idx == 0)
	{
	  row--;
	  row_idx = draw_data->table.width;
	}

      if (row_idx > draw_data->table.width)
	row_idx = draw_data->table.width;
      
      row_idx--;		/* zero based. */
      
      if (item_coords_out)
	{
	  w = (draw_data->table.ch_width / draw_data->zx);
	  h = ((draw_data->table.ch_height + draw_data->table.spacing) / draw_data->zy);
	  (item_coords_out[0]) = row_idx * w;
	  (item_coords_out[1]) = row     * h;
	  row++;
	  row_idx++;
	  (item_coords_out[2]) = row_idx * w;
	  (item_coords_out[3]) = row     * h;

	  index_found = TRUE;
	}
    }

  return index_found ;
}

int foo_canvas_item_world2text_index(FooCanvasItem *item, double x, double y)
{
  int index = -1;

  foo_canvas_item_w2i(item, &x, &y);

  index = foo_canvas_item_item2text_index(item, x, y);

  return index;
}

int foo_canvas_item_item2text_index(FooCanvasItem *item, double x, double y)
{
  FooCanvasZMapText *zmap;
  int index = -1;

  if(FOO_IS_CANVAS_ZMAP_TEXT(item) && 
     (zmap = FOO_CANVAS_ZMAP_TEXT(item)))
    {
      double x1, x2, y1, y2;
      gboolean min = FALSE, max = FALSE;
      foo_canvas_item_get_bounds(item, &x1, &y1, &x2, &y2);
      
      min = (x < x1);
      max = (x > x2);

      if(!(y < y1 || y > y2))
	{
	  FooCanvasZMapTextPrivate *private_data;
	  ZMapTextDrawData draw_data;
	  double ix, iy, w, h;
	  int row_idx, row, width;

	  ix = x - x1;
	  iy = y - y1;

	  private_data = zmap->priv;
	  draw_data    = &(private_data->update_cache);

	  if(draw_data->table.truncated)
	    width = draw_data->table.untruncated_width;
	  else
	    width = draw_data->table.width;

	  w = (draw_data->table.ch_width / draw_data->zx);
	  h = ((draw_data->table.ch_height + draw_data->table.spacing) / draw_data->zy);

	  row_idx = (int)(ix / w);
	  row     = (int)(iy / h);

	  if(min){ row_idx = 0; }
	  if(max){ row_idx = width - 1; }

	  index   = row * width + row_idx;
	}
    }
  else
    g_warning("Item _must_ be a FOO_CANVAS_ZMAP_TEXT item.");

  return index;
}

/* Class initialization function for the text item */
static void foo_canvas_zmap_text_class_init (FooCanvasZMapTextClass *class)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  FooCanvasItemClass *item_class;
  FooCanvasItemClass *parent_class;
  
  gobject_class = (GObjectClass *) class;
  object_class = (GtkObjectClass *) class;
  item_class = (FooCanvasItemClass *) class;
  
  parent_class_G = gtk_type_class(foo_canvas_text_get_type());
  parent_class   = gtk_type_class(foo_canvas_text_get_type());
  
  gobject_class->set_property = foo_canvas_zmap_text_set_property;
  gobject_class->get_property = foo_canvas_zmap_text_get_property;
  
  g_object_class_install_property(gobject_class,
				  PROP_TEXT_ALLOCATE_FUNC,
				  g_param_spec_pointer("allocate-func",
						       _("Size Allocate text area."),
						       _("User function to allocate the correct table cell dimensions (not-implemented)"),
						       G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class,
				  PROP_TEXT_FETCH_TEXT_FUNC,
				  g_param_spec_pointer("fetch-text-func",
						       _("Get Text Function"),
						       _("User function to copy text to buffer"),
						       G_PARAM_READWRITE));
  
  g_object_class_install_property(gobject_class,
				  PROP_TEXT_CALLBACK_DATA,
				  g_param_spec_pointer("callback-data",
						       _("Get Text Function Data"),
						       _("User function data to use to copy text to buffer"),
						       G_PARAM_READWRITE));
  
  g_object_class_install_property(gobject_class,
				  PROP_TEXT_REQUESTED_WIDTH,
				  g_param_spec_double("full-width",
						      NULL, NULL,
						      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
						      G_PARAM_READWRITE));
  g_object_class_install_property(gobject_class,
				  PROP_TEXT_REQUESTED_HEIGHT,
				  g_param_spec_double("full-height",
						      NULL, NULL,
						      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
						      G_PARAM_READWRITE));
  g_object_class_install_property(gobject_class,
				  PROP_WRAP_MODE,
				  g_param_spec_enum ("wrap-mode", NULL, NULL,
						     PANGO_TYPE_WRAP_MODE, PANGO_WRAP_WORD,
						     G_PARAM_READWRITE));
  
  object_class->destroy = foo_canvas_zmap_text_destroy;
  
  item_class->realize   = foo_canvas_zmap_text_realize;
  item_class->unrealize = foo_canvas_zmap_text_unrealize;
  item_class->draw      = foo_canvas_zmap_text_draw;
  item_class->update    = foo_canvas_zmap_text_update;
  item_class->bounds    = foo_canvas_zmap_text_bounds;

  return ;
}

/* Object initialization function for the text item */
static void foo_canvas_zmap_text_init (FooCanvasZMapText *text)
{
  text->priv = g_new0 (FooCanvasZMapTextPrivate, 1);
  text->priv->flags = (ZOOM_NOTIFY_MASK | 
		       SCROLL_REGION_NOTIFY_MASK | 
		       KEEP_WITHIN_SCROLL_REGION);
  return ;
}

/* Destroy handler for the text item */
static void foo_canvas_zmap_text_destroy (GtkObject *object)
{
  FooCanvasZMapText *text;
  
  g_return_if_fail (FOO_IS_CANVAS_ZMAP_TEXT (object));
  
  text = FOO_CANVAS_ZMAP_TEXT (object);
  
  /* remember, destroy can be run multiple times! */
  
  g_free (text->priv);
  text->priv = NULL;
  
  if (GTK_OBJECT_CLASS (parent_class_G)->destroy)
    (* GTK_OBJECT_CLASS (parent_class_G)->destroy) (object);

  return ;
}

/* Set_arg handler for the text item */
static void foo_canvas_zmap_text_set_property (GObject      *object,
					       guint         param_id,
					       const GValue *value,
					       GParamSpec   *pspec)
{
  FooCanvasItem *item;
  FooCanvasZMapText *text;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (FOO_IS_CANVAS_ZMAP_TEXT (object));
  
  text = FOO_CANVAS_ZMAP_TEXT (object);
  item = FOO_CANVAS_ITEM(object);
  
  switch (param_id) 
    {
    case PROP_TEXT_ALLOCATE_FUNC:
      text->priv->allocate_func    = g_value_get_pointer(value);
      break;
    case PROP_TEXT_FETCH_TEXT_FUNC:
      text->priv->fetch_text_func  = g_value_get_pointer(value);
      break;
    case PROP_TEXT_CALLBACK_DATA:
      text->priv->callback_data    = g_value_get_pointer(value);
      break;
    case PROP_TEXT_REQUESTED_WIDTH:
      text->priv->requested_width  = g_value_get_double(value);
      break;
    case PROP_TEXT_REQUESTED_HEIGHT:
      text->priv->requested_height = g_value_get_double(value);
      break;
    case PROP_WRAP_MODE:
      {
	FooCanvasText *parent_text = FOO_CANVAS_TEXT(object);
	int mode = g_value_get_enum(value);

	if(!parent_text->layout)
	  parent_text->layout = 
	    gtk_widget_create_pango_layout(GTK_WIDGET(item->canvas), NULL);

	pango_layout_set_wrap(parent_text->layout, mode);
      }	
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
  
  foo_canvas_item_request_update (item);

  return ;
}

/* Get_arg handler for the text item */
static void foo_canvas_zmap_text_get_property (GObject    *object,
					       guint       param_id,
					       GValue     *value,
					       GParamSpec *pspec)
{
  FooCanvasZMapText *text;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (FOO_IS_CANVAS_ZMAP_TEXT (object));
  
  text = FOO_CANVAS_ZMAP_TEXT (object);
  
  switch (param_id) 
    {
    case PROP_TEXT_ALLOCATE_FUNC:
      g_value_set_pointer(value, text->priv->allocate_func);
      break;
    case PROP_TEXT_FETCH_TEXT_FUNC:
      g_value_set_pointer(value, text->priv->fetch_text_func);
      break;
    case PROP_TEXT_CALLBACK_DATA:
      g_value_set_pointer(value, text->priv->callback_data);
      break;
    case PROP_TEXT_REQUESTED_WIDTH:
      g_value_set_double(value, text->priv->requested_width);
      break;
    case PROP_TEXT_REQUESTED_HEIGHT:
      g_value_set_double(value, text->priv->requested_height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }

  return ;
}

/* Realize handler for the text item */
static void foo_canvas_zmap_text_realize (FooCanvasItem *item)
{
  FooCanvasZMapText *text;
  PangoLayout *layout = FOO_CANVAS_TEXT(item)->layout;
  PangoLayoutIter *iter = NULL;
  const char *current = NULL;
  int l = 0;

  text = FOO_CANVAS_ZMAP_TEXT (item);

#ifdef __GNUC__
  if(debug_functions)
    printf("%s\n", __PRETTY_FUNCTION__);
#endif /* __GNUC__ */

  /* First time it's realized  */
  save_origin(item);

  if((current = pango_layout_get_text(layout)) && (l = strlen(current)) == 0)
    {
      current = NULL;
      pango_layout_set_text(layout, "0", 1);
    }
  
  if((iter = pango_layout_get_iter(layout)))
    {
      pango_layout_iter_get_char_extents(iter, &(text->priv->char_extents));
      
      pango_layout_iter_free(iter);
    }

  if(!current)
    pango_layout_set_text(layout, "", 0);
  
  allocate_buffer_size(text);

  if (parent_class_G->realize)
    (* parent_class_G->realize) (item);

  return ;
}

/* Unrealize handler for the text item */
static void foo_canvas_zmap_text_unrealize (FooCanvasItem *item)
{
  FooCanvasZMapText *text;
  text = FOO_CANVAS_ZMAP_TEXT (item);

#ifdef __GNUC__
  if(debug_functions)
    printf("%s\n", __PRETTY_FUNCTION__);
#endif /* __GNUC__ */

  /* make sure it's all zeros */
  if(text->priv && text->priv->buffer)
    memset(text->priv->buffer, 0, text->priv->buffer_size);
  
  if (parent_class_G->unrealize)
    (* parent_class_G->unrealize) (item);

  return ;
}

/* Draw handler for the text item */
static void foo_canvas_zmap_text_draw (FooCanvasItem  *item, 
				       GdkDrawable    *drawable,
				       GdkEventExpose *expose)
{
  FooCanvasZMapText *zmap;
  FooCanvasText     *text;
  GdkRectangle       rect;

  zmap = FOO_CANVAS_ZMAP_TEXT (item);
  text = FOO_CANVAS_TEXT(item);

#ifdef __GNUC__
  if(debug_functions)
    printf("%s\n", __PRETTY_FUNCTION__);
#endif /* __GNUC__ */

  /* from here is identical logic to parent, but calls internal
   * zmap_gdk_draw_layout_to_expose instead of gdk_draw_layout */

  if (text->clip) 
    {
      rect.x = text->clip_cx;
      rect.y = text->clip_cy;
      rect.width  = text->clip_cwidth;
      rect.height = text->clip_cheight;
      
      gdk_gc_set_clip_rectangle (text->gc, &rect);
    }
  
  if (text->stipple)
    foo_canvas_set_stipple_origin (item->canvas, text->gc);
  
  {
    gboolean text_changed = FALSE;

    text_changed = invoke_get_text(item);

    /* This is faster than the gdk_draw_layout as it filters based on
     * the intersection of lines within the expose region. */
    zmap_gdk_draw_layout_get_size_within_expose (drawable, text->gc, 
						 text->cx, text->cy, 
						 text->layout, 
						 expose,
						 &(text->max_width),
						 &(text->height));
  }  

  if (text->clip)
    gdk_gc_set_clip_rectangle (text->gc, NULL);



  return ;
}

static void foo_canvas_zmap_text_update(FooCanvasItem *item, 
					double         i2w_dx, 
					double         i2w_dy, 
					int            flags)
{
  FooCanvasZMapText *zmap;

  zmap = FOO_CANVAS_ZMAP_TEXT(item);

#ifdef __GNUC__
  if(debug_functions)
    printf("%s\n", __PRETTY_FUNCTION__);
#endif /* __GNUC__ */

  run_update_cycle_loop(item);

  if(parent_class_G->update)
    (*parent_class_G->update)(item, i2w_dx, i2w_dy, flags);

  return ;
}


static void foo_canvas_zmap_text_bounds (FooCanvasItem *item, 
					 double *x1, double *y1, 
					 double *x2, double *y2)
{
  FooCanvasZMapText *zmap = NULL;
  FooCanvasText *text = NULL;

  zmap = FOO_CANVAS_ZMAP_TEXT(item);
  text = FOO_CANVAS_TEXT(item);

#ifdef __GNUC__
  if(debug_functions)
    printf("%s\n", __PRETTY_FUNCTION__);
#endif /* __GNUC__ */

  run_update_cycle_loop(item);

  if(parent_class_G->bounds)
    (*parent_class_G->bounds)(item, x1, y1, x2, y2);

#ifdef __GNUC__
  if(debug_area)
    printf("%s: %f %f %f %f\n", __PRETTY_FUNCTION__, *x1, *y1, *x2, *y2);
#endif /* __GNUC__ */

  return ;
}

static void print_draw_data(ZMapTextDrawData draw_data)
{

  printf("  zoom x, y  : %f, %f\n", draw_data->zx, draw_data->zy);
  printf("  world x, y : %f, %f\n", draw_data->wx, draw_data->wy);
  printf("  scr x1,x2,r: %f, %f, %f\n", draw_data->x1, draw_data->x2, draw_data->x2 - draw_data->x1 + 1);
  printf("  scr y1,y2,r: %f, %f, %f\n", draw_data->y1, draw_data->y2, draw_data->y2 - draw_data->y1 + 1);

  return ;
}

static void print_private_data(FooCanvasZMapTextPrivate *private_data)
{
  if(debug_area || debug_table)
    {
      printf("Private Data\n");
      printf(" buffer size: %d\n",     private_data->buffer_size);
      printf(" req height : %f\n",     private_data->requested_height);
      printf(" req width  : %f\n",     private_data->requested_width);
      printf(" zx, zy     : %f, %f\n", private_data->zx, private_data->zy);
      printf(" char w, h  : %d, %d\n", private_data->char_extents.width, private_data->char_extents.height);
      
      printf("Update draw data\n");
      print_draw_data(&(private_data->update_cache));
      
      printf("Draw draw data\n");
      print_draw_data(&(private_data->draw_cache));
    }

  return ;
}

static void print_calculate_buffer_data(FooCanvasZMapText *zmap,
					ZMapTextDrawData   draw_data,
					int cx1, int cy1, 
					int cx2, int cy2)
{
  if(debug_table)
    {
      printf("foo y1,y2,r: %d, %d, %d\n", 
	     cy1, cy2, cy2 - cy1 + 1);
      printf("world range/canvas range = %f\n", 
	     ((draw_data->y2 - draw_data->y1 + 1) / (cy2 - cy1 + 1)));
      printf("world range (txt)/canvas range = %f\n", 
	     (((draw_data->y2 - draw_data->y1 + 1) * draw_data->table.ch_height) / ((cy2 - cy1 + 1))));
    }

  return ;
}

int foo_canvas_zmap_text_calculate_zoom_buffer_size(FooCanvasItem   *item,
						    ZMapTextDrawData draw_data,
						    int              max_buffer_size)
{
  FooCanvasZMapText *zmap = FOO_CANVAS_ZMAP_TEXT(item);
  int buffer_size = max_buffer_size;
  int char_per_line, line_count;
  int cx1, cy1, cx2, cy2, cx, cy; /* canvas coords of scroll region and item */
  int table;
  int width, height;
  double world_range, raw_chars_per_line, raw_lines;
  int real_chars_per_line, canvas_range;
  int real_lines, real_lines_space;

  if(draw_data->y1 < 0.0)
    {
      double t = 0.0 - draw_data->y1;
      draw_data->y1  = 0.0;
      draw_data->y2 -= t;
    }
  
  foo_canvas_w2c(item->canvas, draw_data->x1, draw_data->y1, &cx1, &cy1);
  foo_canvas_w2c(item->canvas, draw_data->x2, draw_data->y2, &cx2, &cy2);
  foo_canvas_w2c(item->canvas, draw_data->wx, draw_data->wy, &cx,  &cy);

  /* We need to know the extents of a character */
  width  = draw_data->table.ch_width;
  height = draw_data->table.ch_height;

  /* possibly print out some debugging info */
  print_calculate_buffer_data(zmap, draw_data, cx1, cy1, cx2, cy2);
  print_private_data(zmap->priv);

  /* world & canvas range to work out rough chars per line */
  world_range        = (draw_data->y2 - draw_data->y1 + 1);
  canvas_range       = (cy2 - cy1 + 1);
  raw_chars_per_line = ((world_range * height) / canvas_range);
  
  if((double)(real_chars_per_line = (int)raw_chars_per_line) < raw_chars_per_line)
    real_chars_per_line++;
  
  raw_lines = world_range / real_chars_per_line;
  
  if((double)(real_lines = (int)raw_lines) < raw_lines)
    real_lines++;
  
  real_lines_space = real_lines * height;
  
  if(real_lines_space > canvas_range)
    {
      real_lines--;
      real_lines_space = real_lines * height;
    }
  
  if(real_lines_space < canvas_range)
    {
      double spacing_dbl = canvas_range;
      double delta_factor = 0.15;
      int spacing;
      spacing_dbl -= (real_lines * height);
      spacing_dbl /= real_lines;
      
      /* need a fudge factor here! We want to round up if we're
       * within delta factor of next integer */
      
      if(((double)(spacing = (int)spacing_dbl) < spacing_dbl) &&
	 ((spacing_dbl + delta_factor) > (double)(spacing + 1)))
	spacing_dbl = (double)(spacing + 1);
      
      draw_data->table.spacing = (int)(spacing_dbl * PANGO_SCALE);
      draw_data->table.spacing = spacing_dbl;
    }
  
  line_count = real_lines;
  
  char_per_line = (int)(zmap->priv->requested_width);

  draw_data->table.untruncated_width = real_chars_per_line;

  if(real_chars_per_line <= char_per_line)
    {
      char_per_line = real_chars_per_line;
      draw_data->table.truncated = FALSE;
    }
  else
    {
      draw_data->table.truncated = TRUE;
    }

  draw_data->table.width  = char_per_line;
  draw_data->table.height = line_count;

  table = char_per_line * line_count;

  buffer_size = MIN(buffer_size, table);

  return buffer_size;
}

static void allocate_buffer_size(FooCanvasZMapText *zmap)
{
  FooCanvasZMapTextPrivate *private_data;

  private_data = zmap->priv;

  if(private_data->buffer_size == 0)
    {
      FooCanvasText *parent_text = FOO_CANVAS_TEXT(zmap);
      int char_per_line, line_count;
      int pixel_size_x, pixel_size_y;
      int spacing, max_canvas;

      /* work out how big the buffer should be. */
      pango_layout_get_pixel_size(parent_text->layout, 
				  &pixel_size_x, &pixel_size_y);
      spacing = pango_layout_get_spacing(parent_text->layout);

      char_per_line = pixel_size_y + spacing;
      line_count    = 1 << 15;	/* 32768 Maximum canvas size ever */
      max_canvas    = 1 << 15;

      /* This is on the side of caution, but possibly by a factor of 10. */
      private_data->buffer_size = char_per_line * line_count;

      /* The most that can really be displayed is the requested width
       * when the zoom allows drawing this at exactly at the height of
       * the text. The canvas will never be bigger than 32768 so that
       * means 32768 / pixel_size_y * requested_width 
       */
      private_data->buffer_size = max_canvas / char_per_line * private_data->requested_width;

      private_data->buffer = g_new0(char, private_data->buffer_size + 1);

#ifdef __GNUC__
      if(debug_allocate)
	printf("%s: allocated %d.\n", __PRETTY_FUNCTION__, private_data->buffer_size);
#endif /* __GNUC__ */
    }

  return ;
}

static void run_update_cycle_loop(FooCanvasItem *item)
{
  FooCanvasZMapText *zmap;
  FooCanvasZMapTextPrivate *private_data;
  gboolean canvas_changed = FALSE;

  zmap           = FOO_CANVAS_ZMAP_TEXT(item);
  private_data   = zmap->priv;
  canvas_changed = canvas_has_changed(item, &(private_data->update_cache));

  if(canvas_changed)  
    {
      PangoLayout *layout = FOO_CANVAS_TEXT(item)->layout;
      ZMapTextDrawDataStruct clear = {0};

      /* This simple bit of code speeds everything up ;) */
      pango_layout_set_text(layout, "", 0);

      invoke_allocate_width_height(item);

      /* clear the draw cache... to absolutely guarantee the draw
       * code sees the canvas has changed! */
      private_data->draw_cache = clear;
    }

  return ;
}

static void invoke_allocate_width_height(FooCanvasItem *item)
{
  FooCanvasZMapText *zmap;
  FooCanvasZMapTextPrivate *private_data;
  ZMapTextDrawData draw_data;
  ZMapTextDrawDataStruct draw_data_stack = {};
  FooCanvasText *text;
  int spacing, width, height, actual_size;

  zmap = FOO_CANVAS_ZMAP_TEXT(item);
  text = FOO_CANVAS_TEXT(item);

  private_data = zmap->priv;
  draw_data    = &(private_data->update_cache);

  draw_data_stack = *draw_data; /* struct copy! */
  
  /* We need to know the extents of a character */
  draw_data_stack.table.ch_width  = PANGO_PIXELS(zmap->priv->char_extents.width);
  draw_data_stack.table.ch_height = PANGO_PIXELS(zmap->priv->char_extents.height);

  if(private_data->allocate_func)
    actual_size = (private_data->allocate_func)(item, &draw_data_stack, 
						(int)(private_data->requested_width),
						private_data->buffer_size,
						private_data->callback_data);
  else
    actual_size = foo_canvas_zmap_text_calculate_zoom_buffer_size(item,
								  &draw_data_stack,
								  private_data->buffer_size);

  /* The only part we allow to be copied back... */
  draw_data->table = draw_data_stack.table;	/* struct copy */

  if(actual_size > 0)
    {
      double ztmp;
      if(actual_size != (draw_data->table.width * draw_data->table.height))
	g_warning("Allocated size of %d does not match table allocation of %d x %d.",
		  actual_size, draw_data->table.width, draw_data->table.height);
      
      if(private_data->buffer_size <= (draw_data->table.width * draw_data->table.height))
	{
	  g_warning("Allocated size of %d is _too_ big. Buffer is only %d long!", 
		    actual_size, private_data->buffer_size);

	  draw_data->table.height = private_data->buffer_size / draw_data->table.width;
	  actual_size = draw_data->table.width * draw_data->table.height;
	  g_warning("Reducing table size to %d by reducing rows to %d. Please fix %p.", 
		    actual_size, draw_data->table.height, private_data->allocate_func);	  
	}
      
      if(debug_table)
	printf("spacing %f = %f\n", 
	       draw_data_stack.table.spacing, 
	       draw_data_stack.table.spacing * PANGO_SCALE);
      
      spacing = (int)(draw_data_stack.table.spacing   * PANGO_SCALE);
      width   = (int)(draw_data_stack.table.ch_width  * 
		      draw_data_stack.table.width     * PANGO_SCALE);
      
      /* Need to add the spacing otherwise we lose the last bits as the update/bounds
       * calculation which group_draw filters on will not draw the extra bit between
       * (height * rows) and ((height + spacing) * rows).
       */
      ztmp    = (draw_data_stack.zy > 1.0 ? draw_data_stack.zy : 1.0);
	
      height  = (int)((draw_data_stack.table.ch_height + 
		       draw_data_stack.table.spacing) * ztmp *
		      draw_data_stack.table.height    * PANGO_SCALE);
      
      
      /* I'm seriously unsure about the width & height calculations here.
       * ( mainly the height one as zoom is always fixed in x for us ;) )
       * Without the conditional (zoom factor @ high factors) text doesn't get drawn 
       * below a certain point, but including the zoom factor when < 1.0 has the same
       * effect @ lower factors.  I haven't investigated enough to draw a good conclusion
       * and there doesn't seem to be any affect on speed. So it's been left in!
       *
       * Thoughts are it could be to do with scroll offsets or rounding or some
       * interaction with the floating group.
       * RDS
       */

      pango_layout_set_spacing(text->layout, spacing);
      
      pango_layout_set_width(text->layout, width);

      width = PANGO_PIXELS(width);
      height= PANGO_PIXELS(height);
      text->max_width = width;
      text->height    = height;
    }

  return ;
}

static gboolean invoke_get_text(FooCanvasItem *item)
{
  FooCanvasText *parent_text;
  FooCanvasZMapText *zmap;
  FooCanvasZMapTextPrivate *private_data;
  gboolean got_text = FALSE;
  gboolean changed = FALSE;

  parent_text  = FOO_CANVAS_TEXT(item);
  zmap         = FOO_CANVAS_ZMAP_TEXT(item);
  private_data = zmap->priv;

  run_update_cycle_loop(item);

  /* has the canvas changed according to the last time we drew */
  if(private_data->fetch_text_func &&
     (changed = canvas_has_changed(item, &(private_data->draw_cache))))
    {
      ZMapTextDrawData draw_data;
      ZMapTextDrawDataStruct draw_data_stack = {};
      int actual_size = 0;
      
      sync_data(&(private_data->update_cache),
		&(private_data->draw_cache));

      draw_data       = &(private_data->draw_cache);
      draw_data_stack = *draw_data; /* struct copy! */
      
      /* item coords. */
      draw_data_stack.wx = parent_text->x;
      draw_data_stack.wy = parent_text->y;
      
      /* translate to world... */
      foo_canvas_item_i2w(item, 
			  &(draw_data_stack.wx),
			  &(draw_data_stack.wy));

      /* try and get the text from the user specified callback. */
      actual_size = (private_data->fetch_text_func)(item, 
						    &draw_data_stack,
						    private_data->buffer, 
						    private_data->buffer_size,
						    private_data->callback_data);

      draw_data->table = draw_data_stack.table;	/* struct copy */
      
      if(actual_size > 0 && private_data->buffer[0] != '\0')
	{
	  /* If actual size is too big Pango will be slower than it needs to be. */
	  if(actual_size > (draw_data->table.width * draw_data->table.height))
	    g_warning("Returned size of %d is _too_ big. Text table is only %d.", 
		      actual_size, (draw_data->table.width * draw_data->table.height));
	  if(actual_size > private_data->buffer_size)
	    g_warning("Returned size of %d is _too_ big. Buffer is only %d long!", 
		      actual_size, private_data->buffer_size);

	  /* So that foo_canvas_zmap_text_get_property() returns text. */
	  parent_text->text = private_data->buffer;
	  /* Set the text in the pango layout */
	  pango_layout_set_text(parent_text->layout, private_data->buffer, actual_size);

	  foo_canvas_item_request_update(item);
 	}
    }

  if(parent_text->text)
    got_text = TRUE;

  return got_text;
}

static gboolean canvas_has_changed(FooCanvasItem   *item, 
				   ZMapTextDrawData draw_data)
{
  FooCanvasText *parent_text;
  FooCanvasZMapText *zmap;
  FooCanvasZMapTextPrivate *private_data;
  gboolean canvas_changed = FALSE;
  int no_change = 0;

  zmap         = FOO_CANVAS_ZMAP_TEXT(item);
  parent_text  = FOO_CANVAS_TEXT(item);
  private_data = zmap->priv;

  if((private_data->flags & ZOOM_NOTIFY_MASK))
    {
      if(draw_data->zy == item->canvas->pixels_per_unit_y &&
	 draw_data->zx == item->canvas->pixels_per_unit_x)
	{
	  no_change |= ZOOM_NOTIFY_MASK;
	}
      else
	{
	  draw_data->zx = item->canvas->pixels_per_unit_x;
	  draw_data->zy = item->canvas->pixels_per_unit_y;
	}
    }

  if((private_data->flags & SCROLL_REGION_NOTIFY_MASK))
    {
      double sx1, sy1, sx2, sy2;

      foo_canvas_get_scroll_region(item->canvas, &sx1, &sy1, &sx2, &sy2);

      if((draw_data->x1 != sx1) ||
	 (draw_data->y1 != sy1) ||
	 (draw_data->x2 != sx2) ||
	 (draw_data->y2 != sy2))
	{
	  draw_data->x1 = sx1;
	  draw_data->y1 = sy1;
	  draw_data->x2 = sx2;
	  draw_data->y2 = sy2;
	}
      else
	no_change |= SCROLL_REGION_NOTIFY_MASK;
    }

  /* if no change in zoom and scroll region */
  if((no_change & (ZOOM_NOTIFY_MASK)) && 
     (no_change & (SCROLL_REGION_NOTIFY_MASK)))
    canvas_changed = FALSE;
  else
    canvas_changed = TRUE;

  return canvas_changed;
}


static void save_origin(FooCanvasItem *item)
{
  FooCanvasZMapText *text;
  FooCanvasText *parent;

  text   = FOO_CANVAS_ZMAP_TEXT (item);
  parent = FOO_CANVAS_TEXT(item);

  text->priv->update_cache.wx = parent->x;
  text->priv->update_cache.wy = parent->y;

  foo_canvas_get_scroll_region(item->canvas, 
			       &(text->priv->update_cache.x1),
			       &(text->priv->update_cache.y1),
			       &(text->priv->update_cache.x2),
			       &(text->priv->update_cache.y2));

  sync_data(&(text->priv->update_cache),
	    &(text->priv->draw_cache));

  return ;
}

static void sync_data(ZMapTextDrawData from, ZMapTextDrawData to)
{
  /* Straight forward copy... */
  to->wx = from->wx;
  to->wy = from->wy;

  to->x1 = from->x1;
  to->y1 = from->y1;
  to->x2 = from->x2;
  to->y2 = from->y2;

  to->table = from->table;	/* struct copy */

  return ;
}

/* ******** faster gdk drawing code. ******** */

/* direct copy of get_renderer from gdkpango.c */
static PangoRenderer *zmap_get_renderer(GdkDrawable *drawable,
					GdkGC       *gc,
					GdkColor    *foreground,
					GdkColor    *background)
{
  GdkScreen *screen = gdk_drawable_get_screen (drawable);
  PangoRenderer *renderer = gdk_pango_renderer_get_default (screen);
  GdkPangoRenderer *gdk_renderer = GDK_PANGO_RENDERER (renderer);
 
  gdk_pango_renderer_set_drawable (gdk_renderer, drawable);
  gdk_pango_renderer_set_gc (gdk_renderer, gc);  
 
  gdk_pango_renderer_set_override_color (gdk_renderer,
					 PANGO_RENDER_PART_FOREGROUND,
					 foreground);
  gdk_pango_renderer_set_override_color (gdk_renderer,
					 PANGO_RENDER_PART_UNDERLINE,
					 foreground);
  gdk_pango_renderer_set_override_color (gdk_renderer,
					 PANGO_RENDER_PART_STRIKETHROUGH,
					 foreground);
 
  gdk_pango_renderer_set_override_color (gdk_renderer,
					 PANGO_RENDER_PART_BACKGROUND,
					 background);
 
  pango_renderer_activate (renderer);
 
  return renderer;
}

/* direct copy of release_renderer from gdkpango.c */
static void zmap_release_renderer (PangoRenderer *renderer)
{
  GdkPangoRenderer *gdk_renderer = GDK_PANGO_RENDERER (renderer);
   
  pango_renderer_deactivate (renderer);
   
  gdk_pango_renderer_set_override_color (gdk_renderer,
					 PANGO_RENDER_PART_FOREGROUND,
					 NULL);
  gdk_pango_renderer_set_override_color (gdk_renderer,
					 PANGO_RENDER_PART_UNDERLINE,
					 NULL);
  gdk_pango_renderer_set_override_color (gdk_renderer,
					 PANGO_RENDER_PART_STRIKETHROUGH,
					 NULL);
  gdk_pango_renderer_set_override_color (gdk_renderer,
					 PANGO_RENDER_PART_BACKGROUND,
					 NULL);
   
  gdk_pango_renderer_set_drawable (gdk_renderer, NULL);
  gdk_pango_renderer_set_gc (gdk_renderer, NULL);
  
  return ;
}

/* based on pango_renderer_draw_layout, but additionally filters based
 * on the expose region */
static void zmap_pango_renderer_draw_layout_get_size (PangoRenderer    *renderer,
						      PangoLayout      *layout,
						      int               x,
						      int               y,
						      GdkEventExpose   *expose,
						      GdkRectangle     *size_in_out)
{
  PangoLayoutIter *iter;
  g_return_if_fail (PANGO_IS_RENDERER (renderer));
  g_return_if_fail (PANGO_IS_LAYOUT (layout));

  /* some missing code here that deals with matrices, but as none is
   * used for the canvas, its removed. */
   
  pango_renderer_activate (renderer);
 
  iter = pango_layout_get_iter (layout);

   do
     {
       GdkRectangle     line_rect;
       GdkOverlapType   overlap;
       PangoRectangle   logical_rect;
       PangoLayoutLine *line;
       int              baseline;
       
       line = pango_layout_iter_get_line (iter);
       
       pango_layout_iter_get_line_extents (iter, NULL, &logical_rect);
       baseline = pango_layout_iter_get_baseline (iter);
       
       line_rect.x      = PANGO_PIXELS(x + logical_rect.x);
       line_rect.y      = PANGO_PIXELS(y + logical_rect.y);
       line_rect.width  = PANGO_PIXELS(logical_rect.width);
       line_rect.height = PANGO_PIXELS(logical_rect.height);

       /* additionally in this code we want to get the size of the text. */
       if(size_in_out)
	 gdk_rectangle_union(&line_rect, size_in_out, size_in_out);

       if((overlap = gdk_region_rect_in(expose->region, &line_rect)) != GDK_OVERLAP_RECTANGLE_OUT)
	 {
	   pango_renderer_draw_layout_line (renderer,
					    line,
					    x + logical_rect.x,
					    y + baseline);
	 }
     }
   while (pango_layout_iter_next_line (iter));
 
   pango_layout_iter_free (iter);
   
   pango_renderer_deactivate (renderer);

   return ;
}

/* similar to gdk_draw_layout/gdk_draw_layout_with_colours, but calls
 * the functions above. */
static void zmap_gdk_draw_layout_get_size_within_expose(GdkDrawable    *drawable,
							GdkGC          *gc,
							int             x,
							int             y,
							PangoLayout    *layout,
							GdkEventExpose *expose,
							int            *width_max_in_out,
							int            *height_max_in_out)
{
  GdkRectangle size = {0}, *size_ptr = NULL;
  PangoRenderer *renderer;
  GdkColor *foreground = NULL;
  GdkColor *background = NULL;

  /* gdk_draw_layout just calls gdk_draw_layout_with_colours, which is
   * the source of this function. */

  /* get_renderer */
  renderer = zmap_get_renderer (drawable, gc, foreground, background);

  if(width_max_in_out && height_max_in_out)
    {
      size.x      = x;
      size.y      = y;
      size.width  = *width_max_in_out;
      size.height = *height_max_in_out;
      size_ptr = &size;
    }

  /* draw_lines of text */
  zmap_pango_renderer_draw_layout_get_size(renderer, layout, 
					   x * PANGO_SCALE, 
					   y * PANGO_SCALE, 
					   expose,
					   size_ptr);
   
  if(size_ptr)
    {
      *width_max_in_out  = size_ptr->width;
      *height_max_in_out = size_ptr->height;
    }

  /* release renderer */
  zmap_release_renderer (renderer);

  return ;
}
