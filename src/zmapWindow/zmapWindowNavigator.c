/*  File: zmapWindowNav.c
 *  Author: Roy Storey (rds@sanger.ac.uk)
 *  Copyright (c) 2006-2010: Genome Research Ltd.
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
 *      Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *        Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk,
 *   Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *
 * Description: Implements the separate scale/scrolling window widget which
 *              also holds loci which can be searched/filtered.
 *
 * Exported functions: See ZMap/zmapNavigator.h
 *
 * HISTORY:
 * Last edited: Feb 22 08:31 2011 (edgrif)
 * Created: Wed Sep  6 11:22:24 2006 (rds)
 * CVS info:   $Id: zmapWindowNavigator.c,v 1.70 2011-04-05 13:29:15 mh17 Exp $
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.h>

#include <math.h>
#include <string.h>
#include <ZMap/zmapUtilsGUI.h>
#include <ZMap/zmapUtils.h>
#include <zmapWindowNavigator_P.h>

#ifdef RDS_WITH_STIPPLE
#include <ZMap/zmapNavigatorStippleG.xbm> /* bitmap... */
#endif

#include <zmapWindowContainerFeatureSet_I.h>


/* Return the widget! */
#define NAVIGATOR_WIDGET(navigate) GTK_WIDGET(fetchCanvas(navigate))


typedef struct
{
  /* inputs */
  ZMapWindowNavigator navigate;
  ZMapFeatureContext  context;
  GHashTable              *styles;

  /* The current features in the recursion */
  ZMapFeatureAlignment current_align;
  ZMapFeatureBlock     current_block;
  ZMapFeatureSet       current_set;
  /* The current containers in the recursion */
  ZMapWindowContainerGroup container_block;
  ZMapWindowContainerGroup container_strand;
  ZMapWindowContainerGroup container_feature_set;
  double current;

} NavigateDrawStruct, *NavigateDraw;

typedef struct
{
  double start, end;
  ZMapStrand strand;
  ZMapFeature feature;
} LocusEntryStruct, *LocusEntry;


/* We need this because the locator is drawn as a foo_canvas_rect with
 * a transparent background, that will not receive events! Therefore as
 * a work around we set up a handler on the root background and test
 * whether we're within the bounds of the locator. */
typedef struct
{
  ZMapWindowNavigator navigate;
  gboolean locator_click;
  double   click_correction;
}TransparencyEventStruct, *TransparencyEvent;


typedef struct
{
  ZMapWindowNavigator navigate;
  FooCanvasItem *item;
  ZMapWindowTextPositioner positioner;
  double wheight;
}RepositionTextDataStruct, *RepositionTextData;


static void repositionText(ZMapWindowNavigator navigate);

static void container_group_add_highlight_area_item(ZMapWindowNavigator navigate,
						    ZMapWindowContainerGroup container);

/* draw some features... */
static ZMapFeatureContextExecuteStatus drawContext(GQuark key,
                                                   gpointer data,
                                                   gpointer user_data,
                                                   char **err_out);
static void createColumnCB(gpointer data, gpointer user_data);

static void clampCoords(ZMapWindowNavigator navigate,
                        double pre_scale, double post_scale,
                        double *c1_inout, double *c2_inout);
static void clampScaled(ZMapWindowNavigator navigate,
                        double *s1_inout, double *s2_inout);
static void clampWorld2Scaled(ZMapWindowNavigator navigate,
                              double *w1_inout, double *w2_inout);
static void updateLocatorDragger(ZMapWindowNavigator navigate, double button_y, double size);
static gboolean rootBGEventCB(FooCanvasItem *item, GdkEvent *event, gpointer data);
static gboolean columnBackgroundEventCB(FooCanvasItem *item, GdkEvent *event, gpointer data);

static FooCanvas *fetchCanvas(ZMapWindowNavigator navigate);

static gboolean initialiseScaleIfNotExists(ZMapFeatureBlock block);
static gboolean drawScaleRequired(NavigateDraw draw_data);
static void drawScale(NavigateDraw draw_data);
static void navigateDrawFunc(NavigateDraw nav_draw, GtkWidget *widget);


static void expose_handler_disconn_cb(gpointer user_data, GClosure *unused);
static gboolean nav_draw_expose_handler(GtkWidget *widget,
					GdkEventExpose *expose,
					gpointer user_data);

static gboolean navCanvasItemEventCB(FooCanvasItem *item, GdkEvent *event, gpointer data);
static gboolean navCanvasItemDestroyCB(FooCanvasItem *feature_item, gpointer data);

static gboolean factoryItemHandler(FooCanvasItem       *new_item,
				   ZMapFeatureStack     feature_stack,
				   gpointer             handler_data);

static gboolean factoryFeatureSizeReq(ZMapFeature feature,
                                      double *limits_array,
                                      double *points_array_inout,
                                      gpointer handler_data);

static void customiseFactory(ZMapWindowNavigator navigator);

static GHashTable *zmapWindowNavigatorLDHCreate(void);
static LocusEntry zmapWindowNavigatorLDHFind(GHashTable *hash, GQuark key);
static LocusEntry zmapWindowNavigatorLDHInsert(GHashTable *hash,
                                               ZMapFeature feature,
                                               double start, double end);
static void zmapWindowNavigatorLDHDestroy(GHashTable **destroy);
static void destroyLocusEntry(gpointer data);

static void get_filter_list_up_to(GList **filter_out, int max);
static void available_locus_names_filter(GList **filter_out);
static void default_locus_names_filter(GList **filter_out);
static gint strcmp_list_find(gconstpointer list_data, gconstpointer user_data);

/* The container update hooks */
static gboolean highlight_locator_area_cb(ZMapWindowContainerGroup container, FooCanvasPoints *points,
					  ZMapContainerLevelType level, gpointer user_data);

static gboolean positioning_cb(ZMapWindowContainerGroup container, FooCanvasPoints *points,
			       ZMapContainerLevelType level, gpointer user_data);

static gboolean container_draw_locator(ZMapWindowContainerGroup container, FooCanvasPoints *points,
				       ZMapContainerLevelType level, gpointer user_data);

/* create the items which get updated during hooks */
static void container_group_add_locator(ZMapWindowNavigator navigate,
					ZMapWindowContainerGroup container);
static void container_group_add_highlight_area_item(ZMapWindowNavigator navigate,
						    ZMapWindowContainerGroup container);


/*
 *                     globals
 */

static gboolean locator_debug_G = FALSE;
static gboolean navigator_debug_containers_xml_G = FALSE;
static gboolean navigator_debug_containers_G = FALSE;




/*
 * Mail from Jon:
 * If you eliminate MGC:, SK:, MIT:, GD:, WU: then I think the
 * problem will be largely solved. However, I myself would also
 * get rid of (i.e. not display) ENSEST... but keep
 * ENSG... since the former seems to often mask HAVANA objects.
 * Mail from Jane:
 * I'd like to see CCDS:
 */
/* Keep the enum and array in step please... */
enum
  {
    LOCUS_NAMES_MGC,
    LOCUS_NAMES_SK,
    LOCUS_NAMES_MIT,
    LOCUS_NAMES_GD,
    LOCUS_NAMES_WU,
    LOCUS_NAMES_INT,
    LOCUS_NAMES_KO,
    LOCUS_NAMES_ERI,
    LOCUS_NAMES_JGI,
    LOCUS_NAMES_OLD_MIT,
    LOCUS_NAMES_MPI,
    LOCUS_NAMES_RI,
    LOCUS_NAMES_GC,
    LOCUS_NAMES_BCM,
    LOCUS_NAMES_C22,

    /* Before this one the loci names are hidden by default. */
    LOCUS_NAMES_SEPARATOR,
    /* After they will be shown... */

    LOCUS_NAMES_ENSEST,
    LOCUS_NAMES_ENSG,
    LOCUS_NAMES_CCDS,

    LOCUS_NAMES_LENGTH
  };

static char *locus_names_filter_G[] = {
  "MGC:",			/* LOCUS_NAMES_MGC */
  "SK:",
  "MIT:",
  "GD:",
  "WU:",
  "INT:",
  "KO:",
  "ERI:",
  "JGI:",
  "OLD_MIT:",
  "MPI:",
  "RI:",
  "GC:",
  "BCM:",
  "C22:",
  "-empty-",			/* LOCUS_NAMES_SEPARATOR */
  "ENSEST",
  "ENSG",
  "CCDS:",
  "-empty-"			/* LOCUS_NAMES_LENGTH */
};


/* create */
ZMapWindowNavigator zMapWindowNavigatorCreate(GtkWidget *canvas_widget)
{
  ZMapWindowNavigator navigate = NULL ;
  FooCanvas *canvas = NULL ;
  FooCanvasGroup *root = NULL ;

  zMapAssert(FOO_IS_CANVAS(canvas_widget)) ;

  navigate = g_new0(ZMapWindowNavigatorStruct, 1) ;

  navigate->ftoi_hash = zmapWindowFToICreate();



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  navigate->context_map.column_2_styles = zMap_g_hashlist_create() ;
  navigate->context_map.columns = NULL ;		    /* FOR NOW !! */

  fset_col   = zMapConfigIniGetFeatureset2Column(context,fset_col);
  if(g_hash_table_size(fset_col))
    view->context_map.featureset_2_column = fset_col;
  else
    g_hash_table_destroy(fset_col);

  src2src = g_hash_table_new(NULL,NULL);
  view->context_map.source_2_sourcedata = src2src;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



  navigate->locus_display_hash = zmapWindowNavigatorLDHCreate();
  navigate->locus_id = g_quark_from_string("locus");

  if(USE_BACKGROUNDS)
    {
      gdk_color_parse(ROOT_BACKGROUND,   &(navigate->root_background));
      gdk_color_parse(ALIGN_BACKGROUND,  &(navigate->align_background));
      gdk_color_parse(BLOCK_BACKGROUND,  &(navigate->block_background));
      gdk_color_parse(STRAND_BACKGROUND, &(navigate->strand_background));
      gdk_color_parse(COLUMN_BACKGROUND, &(navigate->column_background));
    }
  else
    {
      GdkColor *canvas_color = NULL;
      int gdkcolor_size = 0;
      gdkcolor_size = sizeof(GdkColor);
      canvas_color  = &(gtk_widget_get_style(canvas_widget)->bg[GTK_STATE_NORMAL]);
      memcpy(&(navigate->root_background),
	     canvas_color,
	     gdkcolor_size);
      memcpy(&(navigate->align_background),
	     canvas_color,
	     gdkcolor_size);
      memcpy(&(navigate->block_background),
	     canvas_color,
	     gdkcolor_size);
      memcpy(&(navigate->strand_background),
	     canvas_color,
	     gdkcolor_size);
      memcpy(&(navigate->column_background),
	     canvas_color,
	     gdkcolor_size);
    }

  gdk_color_parse(LOCATOR_BORDER,    &(navigate->locator_border_gdk));
  gdk_color_parse(LOCATOR_DRAG,      &(navigate->locator_drag_gdk));
  gdk_color_parse(LOCATOR_FILL,      &(navigate->locator_fill_gdk));
  gdk_color_parse(LOCATOR_HIGHLIGHT, &(navigate->locator_highlight));

  navigate->scaling_factor      = 0.0;
  navigate->locator_bwidth      = LOCATOR_LINE_WIDTH;
  navigate->locator_x_coords.x1 = 0.0;
  navigate->locator_x_coords.x2 = LOCATOR_LINE_WIDTH * 10.0;

#ifdef RDS_WITH_STIPPLE
  navigate->locator_stipple = gdk_bitmap_create_from_data(NULL, &zmapNavigatorStippleG_bits[0],
							  zmapNavigatorStippleG_width,
							  zmapNavigatorStippleG_height);
#endif

  /* create the root container */
  canvas = FOO_CANVAS(canvas_widget);
  root   = FOO_CANVAS_GROUP(foo_canvas_root(canvas));


  g_object_set_data(G_OBJECT(canvas), ZMAP_WINDOW_POINTER, navigate->current_window) ;


  navigate->container_root = zmapWindowContainerGroupCreateFromFoo(root, ZMAPCONTAINER_LEVEL_ROOT,
								   ROOT_CHILD_SPACING,
								   &(navigate->root_background), NULL);

  g_object_set_data(G_OBJECT(navigate->container_root), ZMAP_WINDOW_POINTER, navigate->current_window) ;


  /* add it to the hash. */
  zmapWindowFToIAddRoot(navigate->ftoi_hash, (FooCanvasGroup *)(navigate->container_root));

  /* lower to bottom so that everything else works... */
  foo_canvas_item_lower_to_bottom(FOO_CANVAS_ITEM(navigate->container_root));

  container_group_add_locator(navigate, navigate->container_root);

  zmapWindowContainerGroupAddUpdateHook(navigate->container_root,
					container_draw_locator,
					navigate);

  g_object_set(G_OBJECT(navigate->container_root),
	       "debug-xml", navigator_debug_containers_xml_G, NULL);
  g_object_set(G_OBJECT(navigate->container_root),
	       "debug", navigator_debug_containers_G, NULL);


  customiseFactory(navigate);

  default_locus_names_filter(&(navigate->hide_filter));

  available_locus_names_filter(&(navigate->available_filters));

      /* this is to allow other files in this module to access this module's data */
      /* yes really */
  zMapWindowNavigatorSetWindowNavigator(canvas_widget,navigate);

  return navigate ;
}




void zMapWindowNavigatorSetStrand(ZMapWindowNavigator navigate, gboolean revcomped)
{
  navigate->is_reversed = revcomped;
  return ;
}

void zMapWindowNavigatorReset(ZMapWindowNavigator navigate)
{
  gulong expose_id = 0;

  if((expose_id = navigate->draw_expose_handler_id) != 0)
    {
      FooCanvas *canvas;

      canvas = fetchCanvas(navigate);

      g_signal_handler_disconnect(G_OBJECT(canvas), expose_id);

      navigate->draw_expose_handler_id = 0;
    }

  /* Keep pointers in step and recreate what was destroyed */
  if(navigate->container_align != NULL)
    {
      gtk_object_destroy(GTK_OBJECT(navigate->container_align));
      navigate->container_align = NULL;
    }

  /* The hash contains invalid pointers so destroy and recreate. */
  zmapWindowFToIDestroy(navigate->ftoi_hash);
  navigate->ftoi_hash = zmapWindowFToICreate();

  zmapWindowNavigatorLDHDestroy(&(navigate->locus_display_hash));
  navigate->locus_display_hash = zmapWindowNavigatorLDHCreate();

  zmapWindowFToIFactoryClose(navigate->item_factory);
  customiseFactory(navigate);

  return ;
}

void zMapWindowNavigatorFocus(ZMapWindowNavigator navigate,
                              gboolean true_eq_focus)
{
  FooCanvasItem *root;

  root = FOO_CANVAS_ITEM(navigate->container_root);

  if(true_eq_focus)
    {
      foo_canvas_item_show(root);
    }
  else
    {
      foo_canvas_item_hide(root);
    }

  return ;
}

void zMapWindowNavigatorSetCurrentWindow(ZMapWindowNavigator navigate, ZMapWindow window)
{
  zMapAssert(navigate && window);

  /* We should be updating the locator here too.  In the case of
   * unlocked windows the locator becomes out of step on focusing
   * until the visibility change handler is called */

 /* mh17: should also twiddle revcomped? but as revcomp applies to the view
     in violation of MVC then all the windows will follow
  */
  navigate->current_window = window;



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* Try sticking this in here....it looks to me like quite a few things should be updated
   * each time we change window... */
  navigate->current_window->context_map->featureset_2_column
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



  return ;
}

void zMapWindowNavigatorMergeInFeatureSetNames(ZMapWindowNavigator navigate,
                                               GList *navigator_sets)
{
  /* This needs to do more that just concat!! ha, it'll break something down the line ... column ordering at least */
  navigator_sets = g_list_copy(navigator_sets);
  navigate->feature_set_names = g_list_concat(navigate->feature_set_names, navigator_sets);

  return ;
}

/* draw features */
void zMapWindowNavigatorDrawFeatures(ZMapWindowNavigator navigate,
                                     ZMapFeatureContext full_context,
				     GHashTable *styles)
{
  FooCanvas *canvas = NULL;
  NavigateDrawStruct draw_data = {NULL};

  draw_data.navigate  = navigate;
  draw_data.context   = full_context;
  draw_data.styles    = styles;

  navigate->full_span.x1 = full_context->master_align->sequence_span.x1;
  navigate->full_span.x2 = full_context->master_align->sequence_span.x2 + 1.0;

  navigate->scaling_factor = NAVIGATOR_SIZE / (navigate->full_span.x2 - navigate->full_span.x1 + 1.0);

  canvas = fetchCanvas(navigate);

  if(!GTK_WIDGET_MAPPED(GTK_WIDGET(canvas)))
    {
      gulong expose_id = 0;

      NavigateDraw draw_data_cpy = g_new0(NavigateDrawStruct, 1);

      memcpy(draw_data_cpy, &draw_data, sizeof(NavigateDrawStruct));

      if((expose_id = navigate->draw_expose_handler_id) == 0)
	{
	  expose_id = g_signal_connect_data(G_OBJECT(canvas), "expose_event",
					    G_CALLBACK(nav_draw_expose_handler), (gpointer)draw_data_cpy,
					    (GClosureNotify)(expose_handler_disconn_cb), 0) ;

	  navigate->draw_expose_handler_id = expose_id;
	}
      /* else { let the current expose handler do it... } */
    }
  else
    {
      gulong expose_id;

      if((expose_id = navigate->draw_expose_handler_id) == 0)
	{
	  navigateDrawFunc(&draw_data, GTK_WIDGET(canvas));
	}
      /* else { let the current expose handler do it... } */
    }
#if MH17_DEBUG_NAV_FOOBAR
print_foo("draw features");
#endif

  return ;
}

void zmapWindowNavigatorLocusRedraw(ZMapWindowNavigator navigate)
{
      /* NOTE this can get called before we even drew in the first place */

  if(navigate)
      repositionText(navigate);
  return ;
}

/* draw locator */
void zMapWindowNavigatorDrawLocator(ZMapWindowNavigator navigate,
                                    double raw_top, double raw_bot)
{
  zMapAssert(navigate);

  /* Always set these... */
  navigate->locator_y_coords.x1 = raw_top;
  navigate->locator_y_coords.x2 = raw_bot;

  if(navigate->draw_expose_handler_id == 0)
    zmapWindowContainerRequestReposition(navigate->container_root);

  return ;
}

void zmapWindowNavigatorPositioning(ZMapWindowNavigator navigate)
{
  zmapWindowContainerRequestReposition(navigate->container_root);

  return ;
}

/* destroy */
void zMapWindowNavigatorDestroy(ZMapWindowNavigator navigate)
{

  gtk_object_destroy(GTK_OBJECT(navigate->container_root)); // remove our item.

  zmapWindowFToIDestroy(navigate->ftoi_hash);
  zmapWindowFToIFactoryClose(navigate->item_factory);
  zmapWindowNavigatorLDHDestroy(&(navigate->locus_display_hash));

  g_free(navigate);             /* possibly not enough ... */

  return ;
}




/*
 *                     Internal routines
 */



static FooCanvas *fetchCanvas(ZMapWindowNavigator navigate)
{
  FooCanvas *canvas = NULL;
  FooCanvasGroup *g = NULL;

  g = (FooCanvasGroup *)(navigate->container_root);

  zMapAssert(g);

  canvas = FOO_CANVAS(FOO_CANVAS_ITEM(g)->canvas);

  return canvas;
}

static void expose_handler_disconn_cb(gpointer user_data, GClosure *unused)
{
  g_free(user_data);
  return ;
}

static gboolean nav_draw_expose_handler(GtkWidget *widget, GdkEventExpose *expose, gpointer user_data)
{
  NavigateDraw draw_data = (NavigateDraw)user_data;
  gulong expose_id;

  expose_id = draw_data->navigate->draw_expose_handler_id;

  if(draw_data->navigate->current_window)
    {
      g_signal_handler_block(G_OBJECT(widget), expose_id);

      g_signal_handler_disconnect(G_OBJECT(widget), expose_id);

      /* zero this before any other code. */
      draw_data->navigate->draw_expose_handler_id = 0;
      /* navigateDrawFunc can end up in nav_focus|locator_expose_handler */
      navigateDrawFunc(draw_data, widget);
    }

  return FALSE;                 /* lets others run. */
}

static void locus_gh_func(gpointer hash_key, gpointer hash_value, gpointer user_data)
{
  RepositionTextData data = (RepositionTextData)user_data;
  ZMapFeature feature = NULL;
  FooCanvasItem *item = NULL;
  LocusEntry locus_data = (LocusEntry)hash_value;
  double text_height, start, end, mid, draw_here, dummy_x = 0.0;
  double iy1, iy2, wy1, wy2;
  int cx = 0, cy1, cy2;

      // mh17: start end are as feature but limited to block ??? (i'm guessing)
  feature = locus_data->feature;
  start   = locus_data->start;
  end     = locus_data->end;

  if((item = zmapWindowFToIFindFeatureItem(data->navigate->current_window,
					   data->navigate->ftoi_hash,
                                           locus_data->strand, ZMAPFRAME_NONE,
                                           feature)))
    {
      FooCanvasItem *line_item;
      GList *hide_list;
      double x1, x2, y1, y2;

      text_height = data->wheight;
      mid         = start + ((end - start + 1.0) / 2.0);
      draw_here   = mid - (text_height / 2.0);

      iy1 = wy1 = start;
      iy2 = wy2 = start + text_height;

      foo_canvas_item_get_bounds(item, &x1, &y1, &x2, &y2);

      /* move to the start of the locus... */
      foo_canvas_item_move(item, 0.0, start - y1);

      foo_canvas_item_get_bounds(item, &x1, &(iy1), &x2, &(iy2));

      wy1 = iy1;
      wy2 = iy2;

      foo_canvas_item_i2w(item, &dummy_x, &(wy1));
      foo_canvas_item_i2w(item, &dummy_x, &(wy2));

      foo_canvas_w2c(item->canvas, dummy_x, wy1, &cx, &(cy1));
      foo_canvas_w2c(item->canvas, dummy_x, wy2, &cx, &(cy2));

      /* This is where we hide/show text matching the filter */
      foo_canvas_item_show(item); /* _always_ show the item */

      /* If this a redraw, there may be aline item.  If there
       * is. Destroy it.  It'll get recreated... */

      if((line_item = g_object_get_data(G_OBJECT(item), ZMAPWINDOWTEXT_ITEM_TO_LINE)))
	{
	  gboolean move_back_to_zero = TRUE;

	  gtk_object_destroy(GTK_OBJECT(line_item));
	  line_item = NULL;
	  g_object_set_data(G_OBJECT(item), ZMAPWINDOWTEXT_ITEM_TO_LINE, line_item);

	  if(move_back_to_zero)
	    {
	      double x1, x2, y1, y2;
	      foo_canvas_item_get_bounds(item, &x1, &y1, &x2, &y2);
	      x1 = 0 - x1;
	      foo_canvas_item_move(item, x1, 0.0);
	    }
	}

      if((hide_list = data->navigate->hide_filter))
	{
	  GList *match;
	  char *text = NULL;

	  g_object_get(G_OBJECT(item),
		       "text", &text,
		       NULL);

	  /* This filters the loci on prefixes in hide_list. strcmp_list_find does the prefix check */
	  if(text && (match = g_list_find_custom(hide_list, text, strcmp_list_find)))
	    {
	      foo_canvas_item_hide(item);
	    }
	  else
	    zmapWindowTextPositionerAddItem(data->positioner, item);

	  if(text)
	    g_free(text);
	}
      else
	zmapWindowTextPositionerAddItem(data->positioner, item);
    }

  return ;
}

static void repositionText(ZMapWindowNavigator navigate)
{
  RepositionTextDataStruct repos_data = {NULL};

  if(navigate->locus_display_hash)
    {
      FooCanvas *canvas = NULL;

      canvas = fetchCanvas(navigate);

      repos_data.navigate = navigate;
      repos_data.positioner = zmapWindowTextPositionerCreate(
            navigate->full_span.x1 * navigate->scaling_factor,
            navigate->full_span.x2 * navigate->scaling_factor);

      zmapWindowNavigatorTextSize(GTK_WIDGET(canvas),
                                  NULL, &(repos_data.wheight));

      g_hash_table_foreach(navigate->locus_display_hash,
                           locus_gh_func,
                           &repos_data);

      zmapWindowTextPositionerUnOverlap(repos_data.positioner, TRUE);
      zmapWindowTextPositionerDestroy(repos_data.positioner);
    }

  return ;
}

#if MH17_DEBUG_NAV_FOOBAR
FooCanvasItem *text_foo = NULL;
void print_foo(char * where)
{
      double x1,x2,y1,y2;
      if(!text_foo)
            return;

      foo_canvas_item_get_bounds(text_foo,&x1,&y1,&x2,&y2);
      printf("foo (%s) %f -> %f\n",where,y1,y2);
}
#endif

static void navigateDrawFunc(NavigateDraw nav_draw, GtkWidget *widget)
{
  ZMapWindowNavigator navigate = nav_draw->navigate;
  /* We need this! */
  zMapAssert(navigate->current_window);

  /* Everything to get a context drawn, raised to top and visible. */
  zMapFeatureContextExecuteComplete((ZMapFeatureAny)(nav_draw->context),
                                    ZMAPFEATURE_STRUCT_FEATURE,
                                    drawContext,
                                    NULL, nav_draw);

  repositionText(navigate);

#if MH17_DEBUG_NAV_FOOBAR
print_foo("nav draw 1");
#endif

#warning MH17: NOTE this function does some very complex and bizarre stuff and removing it appears to have no effect.
//  zmapWindowContainerRequestReposition(navigate->container_root);
// (it gets called some time later from somewhere obscure)

  zmapWindowNavigatorSizeRequest(widget, navigate->width, navigate->height,
      (double) navigate->full_span.x1 * navigate->scaling_factor,
      (double) navigate->full_span.x2 * navigate->scaling_factor);

  zmapWindowNavigatorFillWidget(widget);
#if MH17_DEBUG_NAV_FOOBAR
print_foo("nav draw 2");
#endif

  return ;
}



static ZMapFeatureContextExecuteStatus drawContext(GQuark key_id,
                                                   gpointer data,
                                                   gpointer user_data,
                                                   char **error_out)
{
  ZMapFeatureAny feature_any = (ZMapFeatureAny)data;
  ZMapFeatureBlock     feature_block = NULL;
  ZMapFeatureSet       feature_set   = NULL;
  ZMapFeatureStructType feature_type = ZMAPFEATURE_STRUCT_INVALID;
  ZMapFeatureContextExecuteStatus status = ZMAP_CONTEXT_EXEC_STATUS_OK;
  NavigateDraw draw_data = (NavigateDraw)user_data;
  ZMapWindowNavigator navigate = NULL;
  gboolean hash_status = FALSE;

  feature_type = feature_any->struct_type;

  navigate = draw_data->navigate;


  switch(feature_type)
    {
    case ZMAPFEATURE_STRUCT_CONTEXT:
      break;
    case ZMAPFEATURE_STRUCT_ALIGN:
      {
	ZMapWindowContainerFeatures container_features;

        draw_data->current_align = (ZMapFeatureAlignment)feature_any;
        container_features = zmapWindowContainerGetFeatures(navigate->container_root);
        if(!navigate->container_align)
          {
            navigate->container_align = zmapWindowContainerGroupCreate(container_features, ZMAPCONTAINER_LEVEL_ALIGN,
								       ALIGN_CHILD_SPACING,
								       &(navigate->align_background), NULL);

	    g_object_set_data(G_OBJECT(navigate->container_align), ZMAP_WINDOW_POINTER, navigate->current_window) ;

	    container_group_add_highlight_area_item(navigate, navigate->container_align);
#ifdef NOPE
	    zmapWindowContainerGroupAddUpdateHook(navigate->container_align,
						  positioning_cb,
						  navigate);
#endif
	    zmapWindowContainerGroupAddUpdateHook(navigate->container_align,
						  highlight_locator_area_cb,
						  navigate);

            hash_status = zmapWindowFToIAddAlign(navigate->ftoi_hash, key_id, (FooCanvasGroup *)(navigate->container_align));

            zMapAssert(hash_status);
          }
      }
      break;
    case ZMAPFEATURE_STRUCT_BLOCK:
      {
        ZMapWindowContainerFeatures features = NULL;
//        int block_start, block_end;

        draw_data->current_block = feature_block = (ZMapFeatureBlock)feature_any;

#if MH17_DEBUG_NAV_FOOBAR
/* NOTE the navigator features get scaled but the block is not as these don't appear to
 * have any coordinates set anywhere.
 * in the item factory features get offset by a block start coordinate
 * which also has to be scaled
 */
        block_start = feature_block->block_to_sequence.block.x1;
        block_end   = feature_block->block_to_sequence.block.x2;

printf("nav draw block %d %d\n",block_start,block_end);
#endif

        /* create the block and add the item to the hash */
        features    = zmapWindowContainerGetFeatures(draw_data->navigate->container_align);
        draw_data->container_block = zmapWindowContainerGroupCreate(features, ZMAPCONTAINER_LEVEL_BLOCK,
								    BLOCK_CHILD_SPACING,
								    &(navigate->block_background), NULL);

	g_object_set_data(G_OBJECT(draw_data->container_block),
			  ZMAP_WINDOW_POINTER, draw_data->navigate->current_window) ;

	container_group_add_highlight_area_item(navigate, draw_data->container_block);

	zmapWindowContainerGroupAddUpdateHook(draw_data->container_block,
					      highlight_locator_area_cb,
					      draw_data->navigate);

	g_object_set_data(G_OBJECT(draw_data->container_block), ITEM_FEATURE_STATS,
			  zmapWindowStatsCreate((ZMapFeatureAny)draw_data->current_block)) ;

        hash_status = zmapWindowFToIAddBlock(navigate->ftoi_hash, draw_data->current_align->unique_id,
                                             key_id, (FooCanvasGroup *)(draw_data->container_block));
        zMapAssert(hash_status);

        /* we're only displaying one strand... create it ... */
        features = zmapWindowContainerGetFeatures(draw_data->container_block);

        /* The strand container doesn't get added to the hash! */
        draw_data->container_strand = zmapWindowContainerGroupCreate(features, ZMAPCONTAINER_LEVEL_STRAND,
								     STRAND_CHILD_SPACING,
								     &(navigate->strand_background), NULL);

	g_object_set_data(G_OBJECT(draw_data->container_strand),
			  ZMAP_WINDOW_POINTER, draw_data->navigate->current_window) ;

	container_group_add_highlight_area_item(navigate, draw_data->container_strand);

	zmapWindowContainerGroupAddUpdateHook(draw_data->container_strand,
					      highlight_locator_area_cb,
					      draw_data->navigate);

        /* create a column per set ... */
        initialiseScaleIfNotExists(draw_data->current_block);

        g_list_foreach(draw_data->navigate->feature_set_names, createColumnCB, (gpointer)draw_data);

        if(drawScaleRequired(draw_data))
          drawScale(draw_data);
      }
      break;
    case ZMAPFEATURE_STRUCT_FEATURESET:
      {
        FooCanvasItem *item = NULL;
        feature_set = (ZMapFeatureSet)feature_any;

#if MH17_FToIHash_does_this_mapping
        if(!feature_set->column_id)
	  {
            ZMapFeatureSetDesc gffset;

            gffset = g_hash_table_lookup(draw_data->navigate->current_window->context_map->featureset_2_column,
					 GUINT_TO_POINTER(feature_set->unique_id));
            if (gffset)
	      feature_set->column_id = gffset->column_id;
	  }
#endif

        draw_data->current_set = feature_set;

        status = ZMAP_CONTEXT_EXEC_STATUS_DONT_DESCEND;

	/* play safe: only look up the item if it's displayed */
        if(/*feature_set->column_id && */
	   (item = zmapWindowFToIFindSetItem(draw_data->navigate->current_window,
					     draw_data->navigate->ftoi_hash,
					     feature_set,
					     ZMAPSTRAND_NONE, ZMAPFRAME_NONE)))
          {
	    ZMapWindowContainerFeatureSet container_feature_set;
            FooCanvasGroup *group_feature_set;
	    ZMapStyleBumpMode bump_mode;

            group_feature_set = FOO_CANVAS_GROUP(item);
	    container_feature_set = (ZMapWindowContainerFeatureSet)item;

            zmapWindowFToIFactoryRunSet(draw_data->navigate->item_factory,
                                        feature_set,
                                        group_feature_set, ZMAPFRAME_NONE);

	    if ((bump_mode = zmapWindowContainerFeatureSetGetBumpMode(container_feature_set)) != ZMAPBUMP_UNBUMP)
	      {
		zmapWindowContainerFeatureSetSortFeatures(container_feature_set, 0);

		zmapWindowColumnBumpRange(item, bump_mode, ZMAPWINDOW_COMPRESS_ALL) ;
	      }
          }
        else
          {
            /* do not draw, the vast majority, of the feature context. */
          }
      }
      break;
    case ZMAPFEATURE_STRUCT_FEATURE:
      /* FeatureSet draws it all ;) */
      break;
    case ZMAPFEATURE_STRUCT_INVALID:
    default:
      status = ZMAP_CONTEXT_EXEC_STATUS_ERROR;
      zMapAssertNotReached();
      break;
    }

  return status;
}

static gboolean initialiseScaleIfNotExists(ZMapFeatureBlock block)
{
  ZMapFeatureSet scale;
  char *scale_id = ZMAP_FIXED_STYLE_SCALE_NAME;
  gboolean got_initialised = FALSE;

  if(!(scale = zMapFeatureBlockGetSetByID(block, g_quark_from_string(scale_id))))
    {
      scale = zMapFeatureSetCreate(scale_id, NULL);

      zMapFeatureBlockAddFeatureSet(block, scale);

      got_initialised = TRUE;
    }

  return got_initialised;
}

static gboolean drawScaleRequired(NavigateDraw draw_data)
{
  FooCanvasItem *item;
  gboolean required = FALSE;
  GQuark scale_id;

  scale_id = g_quark_from_string(ZMAP_FIXED_STYLE_SCALE_NAME);

  if((item = zmapWindowFToIFindItemFull(draw_data->navigate->current_window,
                                        draw_data->navigate->ftoi_hash,
                                        draw_data->current_align->unique_id,
                                        draw_data->current_block->unique_id,
                                        scale_id, ZMAPSTRAND_NONE, ZMAPFRAME_NONE, 0)))
    {
      /* if block size changes then the scale will start breaking... */
      required = !(zmapWindowContainerHasFeatures(ZMAP_CONTAINER_GROUP(item)));
    }

  return required;
}

static void drawScale(NavigateDraw draw_data)
{
  FooCanvasItem *item = NULL;

  GQuark scale_id = 0;
  int min, max;

  /* HACK...  */
  scale_id = g_quark_from_string(ZMAP_FIXED_STYLE_SCALE_NAME);
  /* less of a hack ... */
  if((item = zmapWindowFToIFindItemFull(draw_data->navigate->current_window,
                                        draw_data->navigate->ftoi_hash,
                                        draw_data->current_align->unique_id,
                                        draw_data->current_block->unique_id,
                                        scale_id, ZMAPSTRAND_NONE, ZMAPFRAME_NONE, 0)))
    {
      FooCanvasGroup *scale_group = NULL;
      FooCanvasGroup *features    = NULL;

      scale_group = FOO_CANVAS_GROUP(item);

      features = (FooCanvasGroup *)zmapWindowContainerGetFeatures((ZMapWindowContainerGroup)scale_group);

      min = draw_data->context->master_align->sequence_span.x1;
      max = draw_data->context->master_align->sequence_span.x2;
#if MH17_DEBUG_NAV_FOOBAR
printf("nav draw scale %d %d\n",min,max);
#endif
      zmapWindowRulerGroupDraw(features, draw_data->navigate->scaling_factor,
                                draw_data->navigate->is_reversed,
                                (double)min, (double)max);
    }

  return ;
}


/* data is a GQuark, user_data is a NavigateDraw */
static void createColumnCB(gpointer data, gpointer user_data)
{
  GQuark set_id = GPOINTER_TO_UINT(data);
  NavigateDraw draw_data = (NavigateDraw)user_data;
  ZMapWindowContainerFeatures features;
  ZMapWindowContainerBackground container_background = NULL;
  ZMapFeatureTypeStyle style;
  gboolean status = FALSE;
  GQuark set_unique_id;

  /* We need the mapping stuff so navigator can use windowsearch calls and other stuff. */
  /* for the navigator styles are hard coded?? and there's no featureset_2_colum mapping ?
     style = zMapWindowGetColumnStyle(draw_data->navigate->current_window,set_id);
  */

      /* mh17: need to canonicalise the set name to find the style */
  set_unique_id = zMapStyleCreateID((char *) g_quark_to_string(set_id));
  style = zMapFindStyle(draw_data->styles,set_unique_id);
  draw_data->current_set = zMapFeatureBlockGetSetByID(draw_data->current_block, set_unique_id);


  if(!style)
    {
      zMapLogWarning("Failed to find style for navigator featureset '%s'", g_quark_to_string(set_id));
    }
  else if(draw_data->current_set)
    {
      ZMapWindowContainerFeatureSet container_set;

      features = zmapWindowContainerGetFeatures(ZMAP_CONTAINER_GROUP(draw_data->container_strand));

      draw_data->container_feature_set = zmapWindowContainerGroupCreate(features,
									ZMAPCONTAINER_LEVEL_FEATURESET,
									SET_CHILD_SPACING,
									&(draw_data->navigate->column_background),
									NULL);

      g_object_set_data(G_OBJECT(draw_data->container_feature_set),
			ZMAP_WINDOW_POINTER, draw_data->navigate->current_window) ;

      container_group_add_highlight_area_item(draw_data->navigate, draw_data->container_feature_set);


      zmapWindowContainerGroupAddUpdateHook(draw_data->container_feature_set,
					    highlight_locator_area_cb,
					    draw_data->navigate);

      status = zmapWindowFToIAddSet(draw_data->navigate->ftoi_hash,
                                    draw_data->current_align->unique_id,
                                    draw_data->current_block->unique_id,
                                    set_unique_id, ZMAPSTRAND_NONE, ZMAPFRAME_NONE,
                                    (FooCanvasGroup *)draw_data->container_feature_set);
      zMapAssert(status);

      container_set = (ZMapWindowContainerFeatureSet)draw_data->container_feature_set;

      zmapWindowContainerFeatureSetAugment(container_set,
					   draw_data->navigate->current_window,
					   draw_data->current_align->unique_id,
					   draw_data->current_block->unique_id,
					   set_id, set_unique_id, style,
					   ZMAPSTRAND_FORWARD, ZMAPFRAME_NONE);

      zmapWindowContainerAttachFeatureAny(draw_data->container_feature_set, (ZMapFeatureAny) draw_data->current_set);

      zmapWindowContainerSetVisibility(FOO_CANVAS_GROUP(draw_data->container_feature_set), TRUE);

      container_background = zmapWindowContainerGetBackground(draw_data->container_feature_set);

      zmapWindowContainerGroupBackgroundSize(draw_data->container_feature_set,
		(draw_data->current_block->block_to_sequence.block.x2 -
             draw_data->current_block->block_to_sequence.block.x1)
            * draw_data->navigate->scaling_factor);

      /* scale doesn't need this. */
      if(set_id != g_quark_from_string(ZMAP_FIXED_STYLE_SCALE_NAME))
	g_signal_connect(G_OBJECT(container_set), "event",
			 G_CALLBACK(columnBackgroundEventCB),
			 (gpointer)draw_data->navigate);
    }
  else
    zMapLogWarning("Failed to find navigator featureset '%s'\n", g_quark_to_string(set_id));

  return ;
}


static void clampCoords(ZMapWindowNavigator navigate,
                        double pre_scale, double post_scale,
                        double *c1_inout, double *c2_inout)
{
  double top, bot;
  double min, max;

  min = (double)(navigate->full_span.x1) * pre_scale;
  max = (double)(navigate->full_span.x2) * pre_scale;

  top = *c1_inout;
  bot = *c2_inout;

  zMapGUICoordsClampSpanWithLimits(min, max, &top, &bot);

  *c1_inout = top * post_scale;
  *c2_inout = bot * post_scale;

  return ;
}


static void clampScaled(ZMapWindowNavigator navigate, double *s1_inout, double *s2_inout)
{
  clampCoords(navigate,
              navigate->scaling_factor,
              1.0,
              s1_inout,
              s2_inout);

  return ;
}

static void clampWorld2Scaled(ZMapWindowNavigator navigate, double *w1_inout, double *w2_inout)
{
  clampCoords(navigate,
              1.0,
              navigate->scaling_factor,
              w1_inout,
              w2_inout);
  return ;
}

static void updateLocatorDragger(ZMapWindowNavigator navigate, double button_y, double size)
{
  double a, b;

  a = button_y;
  b = button_y + size - 1.0;

  clampScaled(navigate, &a, &b);

  if(locator_debug_G)
    {
      double sf = navigate->scaling_factor;
      double top, bot;
      double start, end;
      start = a;
      end   = b;

      top = start / sf;
      bot = end   / sf;

      printf("%s: [nav] %f -> %f (%f) = [wrld] %f -> %f (%f)\n",
	     "updateLocatorDragger",
	     start, end, end - start + 1.0,
	     top, bot, bot - top + 1.0);
    }

  foo_canvas_item_set(FOO_CANVAS_ITEM(navigate->locator_drag),
		      "y1", a,
		      "y2", b,
		      NULL);

  return ;
}

static gboolean rootBGEventCB(FooCanvasItem *item, GdkEvent *event, gpointer data)
{
  TransparencyEvent transp_data = (TransparencyEvent)data;
  ZMapWindowNavigator navigate = NULL;
  gboolean event_handled = FALSE;

  navigate = transp_data->navigate;

  switch(event->type)
    {
    case GDK_BUTTON_PRESS:
      {
        GdkEventButton *button = (GdkEventButton *)event;
        zMapAssert(navigate->locator_drag);

        if(button->button  == 1)
          {
            double locator_y1, locator_y2, locator_size, y_coord, x = 0.0;
            gboolean within_fuzzy = FALSE;
            int fuzzy_dist = 6;

            foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(navigate->locator), NULL, &locator_y1, NULL, &locator_y2);
            y_coord = (double)button->y;

            if( (y_coord > locator_y1) && (y_coord < locator_y2) )
              within_fuzzy = TRUE;
            else if(fuzzy_dist > 1 && fuzzy_dist < 10)
              {
                int cx, cy1, cy2, cbut;
                FooCanvas *canvas = FOO_CANVAS(FOO_CANVAS_ITEM(navigate->locator)->canvas);
                foo_canvas_w2c(canvas, x, locator_y1, &cx, &cy1);
                foo_canvas_w2c(canvas, x, locator_y2, &cx, &cy2);
                foo_canvas_w2c(canvas, x, y_coord,    &cx, &cbut);

                if(cy2 - cy1 <= fuzzy_dist)
                  {
                    fuzzy_dist /= 2;
                    cy1 -= fuzzy_dist;
                    cy2 += fuzzy_dist;
                    if(cbut < cy2 && cbut > cy1)
                      within_fuzzy = TRUE;
                  }
              }

            if(within_fuzzy == TRUE)
              {
                transp_data->click_correction = y_coord - locator_y1;

                locator_y1  += LOCATOR_LINE_WIDTH / 2.0;
                locator_y2  -= LOCATOR_LINE_WIDTH / 2.0;

                locator_size = locator_y2 - locator_y1 + 1.0;

                foo_canvas_item_show(navigate->locator_drag);
                foo_canvas_item_lower_to_bottom(navigate->locator_drag);

                updateLocatorDragger(navigate, y_coord - transp_data->click_correction, locator_size);

                event_handled = transp_data->locator_click = TRUE;
              }
          }
      }
      break;
    case GDK_BUTTON_RELEASE:
      {
        double locator_y1, locator_y2, origin_y1, origin_y2, dummy;
        zMapAssert(navigate->locator_drag);

        if(transp_data->locator_click == TRUE)
          {
            foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(navigate->locator_drag), NULL, &locator_y1, NULL, &locator_y2);
            foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(navigate->locator), NULL, &origin_y1, NULL, &origin_y2);

            locator_y1 += LOCATOR_LINE_WIDTH / 2.0;
            locator_y2 -= LOCATOR_LINE_WIDTH / 2.0;

            origin_y1 += LOCATOR_LINE_WIDTH / 2.0;
            origin_y2 -= LOCATOR_LINE_WIDTH / 2.0;

            foo_canvas_item_i2w(FOO_CANVAS_ITEM(navigate->locator_drag), &dummy, &locator_y1);
            foo_canvas_item_i2w(FOO_CANVAS_ITEM(navigate->locator_drag), &dummy, &locator_y2);

            foo_canvas_item_i2w(FOO_CANVAS_ITEM(navigate->locator), &dummy, &origin_y1);
            foo_canvas_item_i2w(FOO_CANVAS_ITEM(navigate->locator), &dummy, &origin_y2);

            locator_y1 /= navigate->scaling_factor;
            locator_y2 /= navigate->scaling_factor;

            origin_y1 /= navigate->scaling_factor;
            origin_y2 /= navigate->scaling_factor;

            foo_canvas_item_hide(navigate->locator_drag);

            zMapWindowNavigatorDrawLocator(navigate, locator_y1, locator_y2);

            if(locator_y1 != origin_y1 && locator_y2 != origin_y2)
              zmapWindowNavigatorValueChanged(NAVIGATOR_WIDGET(navigate), locator_y1, locator_y2);

            transp_data->locator_click = FALSE;

            event_handled = TRUE;
          }
      }
      break;
    case GDK_MOTION_NOTIFY:
      {
        double button_y = 0.0, locator_y1,
          locator_y2, locator_size = 0.0;
        zMapAssert(navigate->locator_drag);

        if(transp_data->locator_click == TRUE)
          {
            button_y = (double)event->button.y - transp_data->click_correction;

            foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(navigate->locator), NULL, &locator_y1, NULL, &locator_y2);

            locator_y1 += LOCATOR_LINE_WIDTH / 2.0;
            locator_y2 -= LOCATOR_LINE_WIDTH / 2.0;

            locator_size = locator_y2 - locator_y1 + 1.0;

            updateLocatorDragger(navigate, button_y, locator_size);

            event_handled = TRUE;
          }
      }
      break;
    default:
      {
        /* By default we _don't_handle events. */
        event_handled = FALSE ;
        break ;
      }

    }

  return event_handled;
}



static void makeMenuFromCanvasItem(GdkEventButton *button, FooCanvasItem *item, gpointer data)
{
  static ZMapGUIMenuItemStruct separator[] =
    {
      {ZMAPGUI_MENU_SEPARATOR, NULL, 0, NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL, 0, NULL, NULL}
    } ;
  NavigateMenuCBData menu_data = NULL;
  ZMapFeatureAny feature_any = NULL;
  GList *menu_sets = NULL;
  char *menu_title = NULL;
  gboolean bumping_works = TRUE;

  feature_any = zmapWindowItemGetFeatureAny(item);

  menu_title  = zMapFeatureName(feature_any);

  if((menu_data = g_new0(NavigateMenuCBDataStruct, 1)))
    {
      ZMapWindowContainerFeatureSet container = NULL;
      ZMapStyleBumpMode bump_mode = ZMAPBUMP_UNBUMP;
      menu_data->item     = item;
      menu_data->navigate = (ZMapWindowNavigator)data;



      if (feature_any->struct_type == ZMAPFEATURE_STRUCT_FEATURE)
        {
          menu_data->item_cb  = TRUE;
          /* This is the variant filter... Shouldn't be in the menu code too! */


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
          if (feature->feature.transcript.locus_id != 0)
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

            {
              menu_sets = g_list_append(menu_sets, zmapWindowNavigatorMakeMenuLocusOps(NULL, NULL, menu_data));
              menu_sets = g_list_append(menu_sets, zmapWindowNavigatorMakeMenuLocusColumnOps(NULL, NULL, menu_data));
              menu_sets = g_list_append(menu_sets, separator);
            }

	  container = (ZMapWindowContainerFeatureSet)zmapWindowContainerCanvasItemGetContainer(item);
        }
      else
        {
          /* get set_data->style */

	  if (ZMAP_IS_CONTAINER_FEATURESET(item))
	    {
	      container = ZMAP_CONTAINER_FEATURESET(item);

	      if(container->unique_id == menu_data->navigate->locus_id)
		{
		  menu_sets = g_list_append(menu_sets, zmapWindowNavigatorMakeMenuLocusColumnOps(NULL, NULL, menu_data));
		  menu_sets = g_list_append(menu_sets, separator);
		}
	    }

          menu_data->item_cb  = FALSE;
        }

      if (bumping_works && container)
        {
	  bump_mode = zmapWindowContainerFeatureSetGetBumpMode(container);

          menu_sets = g_list_append(menu_sets, zmapWindowNavigatorMakeMenuBump(NULL, NULL, menu_data, bump_mode));
          menu_sets = g_list_append(menu_sets, separator);
        }

      menu_sets = g_list_append(menu_sets, zmapWindowNavigatorMakeMenuColumnOps(NULL, NULL, menu_data));

    }

  zMapGUIMakeMenu(menu_title, menu_sets, button);

  return ;
}

static gboolean columnBackgroundEventCB(FooCanvasItem *item, GdkEvent *event, gpointer data)
{
#ifdef __DATA_UNUSED__
  ZMapWindowNavigator navigate = (ZMapWindowNavigator)data;
#endif /* __DATA_UNUSED__ */

  gboolean event_handled = FALSE;

  switch(event->type)
    {
    case GDK_BUTTON_PRESS:
      {
        GdkEventButton *button = (GdkEventButton *)event;

        if(button->button == 3)
          {
            makeMenuFromCanvasItem(button, item, data);
            event_handled = TRUE;
          }
      }
      break;
    default:
      break;
    }

  return event_handled;
}

static gboolean navCanvasItemEventCB(FooCanvasItem *item, GdkEvent *event, gpointer data)
{
  gboolean event_handled = FALSE;
  ZMapWindowNavigator navigate = (ZMapWindowNavigator)data;
  ZMapFeature feature = NULL;
  static guint32 last_but_press = 0 ;			    /* Used for double clicks... */

  switch(event->type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
      {
	GdkEventButton *button = (GdkEventButton *)event ;

        /* Retrieve the feature item info from the canvas item. */
        feature = zmapWindowItemGetFeature(item);
        zMapAssert(feature) ;

#if MH17_DEBUG_NAV_FOOBAR
{ double x1,x2,y1,y2;
foo_canvas_item_get_bounds(item,&x1,&y1,&x2,&y2);
printf("navCanvasItemEventCB %f,%f: %s, %p at %f -> %f\n", button->x,button->y,g_quark_to_string(feature->original_id), item, y1, y2);
}
#endif
        if(button->type == GDK_BUTTON_PRESS)
          {
            if (button->time - last_but_press > 250)
              {
                if(button->button == 1)
                  {
                    /* ignore ATM */
                  }
                else if(button->button == 3)
                  {
                    /* make item menu */
                    makeMenuFromCanvasItem(button, item, navigate);
                  }
              }
            last_but_press = button->time ;

	    event_handled = TRUE ;
          }
        else
          {
            if (button->button == 1  /* && feature->feature.transcript.locus_id != 0) */
                  && feature->type == ZMAPSTYLE_MODE_TEXT)
              {
                  /*used to get a transcript feature? no idea how, but the locus feature is a text item */
                zmapWindowNavigatorGoToLocusExtents(navigate, item);
                event_handled = TRUE;
              }
          }
      }
      break;
    default:
      break;
    }

  return event_handled;
}

static gboolean navCanvasItemDestroyCB(FooCanvasItem *feature_item, gpointer data)
{
  //ZMapWindowNavigator navigate = (ZMapWindowNavigator)data;
  if(locator_debug_G)
    printf("item got destroyed\n");

  return FALSE;
}

static gboolean factoryItemHandler(FooCanvasItem       *new_item,
				   ZMapFeatureStack     feature_stack,
				   gpointer             handler_data)
{
  g_signal_connect(GTK_OBJECT(new_item), "event",
                   GTK_SIGNAL_FUNC(navCanvasItemEventCB), handler_data);
  g_signal_connect(GTK_OBJECT(new_item), "destroy",
                   GTK_SIGNAL_FUNC(navCanvasItemDestroyCB), handler_data);


  //g_object_set(G_OBJECT(new_item), "debug", TRUE, NULL);

  return TRUE;
}

static gboolean factoryFeatureSizeReq(ZMapFeature feature,
                                      double *limits_array,
                                      double *points_array_inout,
                                      gpointer handler_data)
{
  ZMapWindowNavigator navigate = (ZMapWindowNavigator)handler_data;
  gboolean outside = FALSE;
  double scale_factor = 1.0;    /* get from handler_data! */
  double *x1_inout = &(points_array_inout[1]);
  double *x2_inout = &(points_array_inout[3]);
  int start_end_crossing = 0;
  double block_start, block_end;
  LocusEntry hash_entry = NULL;

  scale_factor = navigate->scaling_factor;

  block_start  = limits_array[1] * scale_factor;
  block_end    = limits_array[3] * scale_factor;

  *x1_inout = *x1_inout * scale_factor;
  *x2_inout = *x2_inout * scale_factor;

  if(feature && navigate->locus_id == feature->parent->unique_id)
    {
      points_array_inout[0] += 20;
      points_array_inout[2] += 20;

      if((hash_entry = zmapWindowNavigatorLDHFind(navigate->locus_display_hash,
                                                  feature->original_id)))
        {
          /* we only ever draw the first one of these. */
          outside = TRUE;
        }
      else if((hash_entry = zmapWindowNavigatorLDHInsert(navigate->locus_display_hash, feature,
                                                         *x1_inout, *x2_inout)))
        {
          outside = FALSE;
        }
      else
        {
          zMapAssertNotReached();
        }
    }

  /* shift according to how we cross, like this for ease of debugging, not speed */
  start_end_crossing |= ((*x1_inout < block_start) << 1);
  start_end_crossing |= ((*x2_inout > block_end)   << 2);
  start_end_crossing |= ((*x1_inout > block_end)   << 3);
  start_end_crossing |= ((*x2_inout < block_start) << 4);

  /* Now check whether we cross! */
  if(start_end_crossing & 8 ||
     start_end_crossing & 16) /* everything is out of range don't display! */
    outside = TRUE;

  if(start_end_crossing & 2)
    *x1_inout = block_start;
  if(start_end_crossing & 4)
    *x2_inout = block_end;

  if(hash_entry)
    {
      /* extend the values in the hash list */
      if(hash_entry->start > *x1_inout)
        hash_entry->start  = *x1_inout;
      if(hash_entry->end < *x2_inout)
        hash_entry->end  = *x2_inout;
    }

  return outside;
}



static void customiseFactory(ZMapWindowNavigator navigate)
{
  ZMapWindowFToIFactoryProductionTeamStruct factory_helpers = {NULL};

  /* create a factory and set up */
  navigate->item_factory = zmapWindowFToIFactoryOpen(navigate->ftoi_hash, NULL);
  factory_helpers.feature_size_request = factoryFeatureSizeReq;
  factory_helpers.top_item_created     = factoryItemHandler;
  zmapWindowFToIFactorySetup(navigate->item_factory, 1, /* line_width hardcoded for now. */
                             &factory_helpers, (gpointer)navigate);

  return ;
}



/* mini package for the locus_display_hash ... */
/*
 * \brief Create a hash for the locus display.
 *
 * Hash is keyed on GUINT_TO_POINTER(feature->original_id)
 * Contains LocusEntry items.
 */
static GHashTable *zmapWindowNavigatorLDHCreate(void)
{
  GHashTable *hash = NULL;

  hash = g_hash_table_new_full(NULL, NULL, NULL, destroyLocusEntry);

  return hash;
}

/*
 * \brief finds the entry.
 */
static LocusEntry zmapWindowNavigatorLDHFind(GHashTable *hash, GQuark key)
{
  LocusEntry hash_entry = NULL;

  hash_entry = g_hash_table_lookup(hash, GUINT_TO_POINTER(key));

  return hash_entry;
}
/*
 * \brief creates a new entry from feature, start and end (scaled) and returns it.
 */
static LocusEntry zmapWindowNavigatorLDHInsert(GHashTable *hash,
                                               ZMapFeature feature,
                                               double start, double end)
{
  LocusEntry hash_entry = NULL;

  if((hash_entry = g_new0(LocusEntryStruct, 1)))
    {
      hash_entry->start   = start;
      hash_entry->end     = end;
      hash_entry->strand  = feature->strand;
      hash_entry->feature = feature; /* So we can find the item */
      g_hash_table_insert(hash,
                          GUINT_TO_POINTER(feature->original_id),
                          (gpointer)hash_entry);
    }
  else
    zMapAssertNotReached();

  return hash_entry;
}
/*
 * \brief Destroy the hash **! NULL's for you.
 */
static void zmapWindowNavigatorLDHDestroy(GHashTable **destroy_me)
{
  zMapAssert(destroy_me && *destroy_me);

  g_hash_table_destroy(*destroy_me);

  *destroy_me = NULL;

  return ;
}

static void destroyLocusEntry(gpointer data)
{
  LocusEntry locus_entry = (LocusEntry)data;

  zMapAssert(data);

  locus_entry->feature = NULL;

  g_free(locus_entry);

  return ;
}


static void get_filter_list_up_to(GList **filter_out, int max)
{
  if(max <= LOCUS_NAMES_LENGTH)
    {
      int i;
      char **names_ptr = locus_names_filter_G;
      GList *filter = NULL;

      for(i = 0; i < LOCUS_NAMES_LENGTH && names_ptr; i++, names_ptr++)
	{
	  if(i < max && i != LOCUS_NAMES_SEPARATOR)
	    {
	      filter = g_list_append(filter, *names_ptr);
	    }
	}
      if(filter_out)
	*filter_out = filter;
    }

  return ;
}

static void available_locus_names_filter(GList **filter_out)
{
  GList *filter = NULL;

  if(filter_out)
    {
      get_filter_list_up_to(&filter, LOCUS_NAMES_LENGTH);
      *filter_out = filter;
    }

  return ;
}

static void default_locus_names_filter(GList **filter_out)
{
  GList *filter = NULL;

  if(filter_out)
    {
      get_filter_list_up_to(&filter, LOCUS_NAMES_SEPARATOR);
      *filter_out = filter;
    }

  return ;
}

/* compares the two strings for a list find */
static gint strcmp_list_find(gconstpointer list_data, gconstpointer user_data)
{
  gint result = -1;

  result = strncmp(list_data, user_data, strlen(list_data));

  return result;
}



/* container update hooks */
static gboolean highlight_locator_area_cb(ZMapWindowContainerGroup container, FooCanvasPoints *points,
					  ZMapContainerLevelType level, gpointer user_data)
{
  ZMapWindowNavigator navigate = (ZMapWindowNavigator)user_data;
  GList *item_list;
  FooCanvasGroup *container_underlay;


  switch(level)
    {
    case ZMAPCONTAINER_LEVEL_ALIGN:
    case ZMAPCONTAINER_LEVEL_BLOCK:
    case ZMAPCONTAINER_LEVEL_STRAND:
    case ZMAPCONTAINER_LEVEL_FEATURESET:

#if MH17_DEBUG_NAV_FOOBAR
{
char *name = "none";
if(container->feature_any) name = zMapFeatureName(container->feature_any);
printf("highlight locator %d %s\n", level,name);
print_foo("");
}
#endif
      container_underlay = (FooCanvasGroup *)zmapWindowContainerGetUnderlay(container);

      if((item_list = g_list_first(container_underlay->item_list)))
	{
	  FooCanvasItem *item;

	  do
	    {
	      if(FOO_IS_CANVAS_RE(item_list->data))
		{
		  item = FOO_CANVAS_ITEM(item_list->data);

		  foo_canvas_item_set(item,
				      "x1", points->coords[0],
				      "x2", points->coords[2],
/* MH17 NOTE: when displayed this does not get scaled */
				      "y1", (double)(navigate->locator_y_coords.x1 * navigate->scaling_factor),
				      "y2", (double)(navigate->locator_y_coords.x2 * navigate->scaling_factor),
				      NULL);
#if MH17_DEBUG_NAV_FOOBAR
("highlight_locator_area_cb %f %f\n",
(double)(navigate->locator_y_coords.x1),
(double)(navigate->locator_y_coords.x2));
#endif
		}
	    }
	  while((item_list = g_list_next(item_list)));
	}

      break;
    default:
      zMapAssertNotReached();
      break;
    }

#if MH17_DEBUG_NAV_FOOBAR
print_foo("highlight locator");
#endif

  return TRUE;
}

static gboolean idle_resize_widget_cb(gpointer navigate_data)
{
  ZMapWindowNavigator navigate = (ZMapWindowNavigator)navigate_data;
  GtkWidget *widget;

  g_return_val_if_fail(navigate->locator != NULL, FALSE);

  widget = GTK_WIDGET( navigate->locator->canvas );
#if MH17_DEBUG_NAV_FOOBAR
print_foo("idle resize 1");
#endif

  zmapWindowNavigatorSizeRequest(widget, navigate->width, navigate->height,
      (double) navigate->full_span.x1 * navigate->scaling_factor,
      (double) navigate->full_span.x2 * navigate->scaling_factor);

  zmapWindowNavigatorFillWidget(widget);
#if MH17_DEBUG_NAV_FOOBAR
print_foo("idle resize 2");
#endif

  return FALSE;			/* only run once! */
}

static gboolean positioning_cb(ZMapWindowContainerGroup container, FooCanvasPoints *points,
			       ZMapContainerLevelType level, gpointer user_data)
{
  ZMapWindowNavigator navigate = (ZMapWindowNavigator)user_data;
  FooCanvasItem *item;
  GtkWidget *widget;
  double width, height;

  item   = FOO_CANVAS_ITEM( container );
  widget = GTK_WIDGET( item->canvas );


/* MH17 NOTE:
* + 1 does not give a width or height but 1 more
* and removing it causes the navigator to be scrolled up by about 20 pixels
* whenever this function is called the y coords in points are zero so WTH is going on ?
*/
  width  = points->coords[2] - points->coords[0] + 1.0;
  height = points->coords[3] - points->coords[1] + 1.0;

  navigate->width  = width;
  navigate->height = height;
#if MH17_DEBUG_NAV_FOOBAR
("positioning_cb %f - %f = %f\n",points->coords[1],points->coords[3],height);
#endif

  g_idle_add(idle_resize_widget_cb, navigate);
#ifdef RDS_DONT_INCLUDE_UNUSED
  zmapWindowNavigatorSizeRequest(widget, width, height,
      (double) navigate->full_span.x1 * navigate->scaling_factor,
      (double) navigate->full_span.x2 * navigate->scaling_factor);

  zmapWindowNavigatorFillWidget(widget);
#endif

  return TRUE;
}


static gboolean container_draw_locator(ZMapWindowContainerGroup container, FooCanvasPoints *points,
				       ZMapContainerLevelType level, gpointer user_data)
{
  ZMapWindowNavigator navigate = (ZMapWindowNavigator)user_data;
  FooCanvasGroup *overlay;
  GList *list;
  double x1, x2, y1, y2;

  g_return_val_if_fail(navigate != NULL, FALSE);
  g_return_val_if_fail(navigate->locator != NULL, FALSE);

  if(GTK_WIDGET_MAPPED(GTK_WIDGET(navigate->locator->canvas)))
    {

      x1 = points->coords[0];
      x2 = points->coords[2];

      y1 = navigate->locator_y_coords.x1;
      y2 = navigate->locator_y_coords.x2;

      clampWorld2Scaled(navigate, &y1, &y2);

      overlay = (FooCanvasGroup *)zmapWindowContainerGetOverlay(container);

      if((list = g_list_first(overlay->item_list)))
	{
	  do
	    {
	      foo_canvas_item_set(FOO_CANVAS_ITEM(list->data),
				  "x1", x1,
				  "x2", x2,
				  NULL);

	      if(list->data == navigate->locator ||
		 list->data == navigate->locator_drag)
		foo_canvas_item_set(FOO_CANVAS_ITEM(list->data),
				    "y1", y1,
				    "y2", y2,
				    NULL);
	    }
	  while((list = list->next));
	}



      foo_canvas_item_raise_to_top(navigate->locator);
      foo_canvas_item_show(navigate->locator);

      positioning_cb(container, points, level, user_data);
    }

  return TRUE;
}



static void container_group_add_highlight_area_item(ZMapWindowNavigator navigate,
						    ZMapWindowContainerGroup container)
{
  ZMapWindowContainerUnderlay underlay;
  FooCanvasGroup *underlay_group;

  underlay = zmapWindowContainerGetUnderlay(container);

  underlay_group = (FooCanvasGroup *)underlay;

  foo_canvas_item_new(underlay_group, FOO_TYPE_CANVAS_RECT,
		      "fill_color_gdk", &(navigate->locator_highlight),
		      "width_pixels",   0,
		      NULL);

  return ;
}


static void container_group_add_locator(ZMapWindowNavigator navigate,
					ZMapWindowContainerGroup container)
{
  ZMapWindowContainerOverlay overlay;
  FooCanvasGroup *overlay_group;
  TransparencyEvent transp_data = NULL;
  gboolean k_markers = FALSE;

  FooCanvasItem *locator_drag = NULL;
  FooCanvasItem *locator      = NULL;

  overlay = zmapWindowContainerGetOverlay(container);

  overlay_group = (FooCanvasGroup *)overlay;

  if(!(locator_drag = navigate->locator_drag))
    {
      locator_drag = foo_canvas_item_new(overlay_group,
					 FOO_TYPE_CANVAS_RECT,
					 "x1", 0.0,
					 "y1", 0.0,
					 "x2", 100.0,
					 "y2", 100.0,
					 "outline_color_gdk", &(navigate->locator_drag_gdk),
					 "fill_color_gdk",    (GdkColor *)(NULL),
					 "width_pixels",      navigate->locator_bwidth,
					 NULL);
      navigate->locator_drag = locator_drag;
    }

  if(!(locator = navigate->locator))
    {
      locator = foo_canvas_item_new(overlay_group,
				    FOO_TYPE_CANVAS_RECT,
				    "x1", 0.0,
				    "y1", 0.0,
				    "x2", 100.0,
				    "y2", 100.0,
				    "outline_color_gdk", &(navigate->locator_border_gdk),
				    "fill_color_gdk",    (GdkColor *)(NULL),
				    "width_pixels",      navigate->locator_bwidth,
				    NULL);
      navigate->locator = locator;
    }

  if(k_markers)
    {
      int i;
      GdkColor blue;
      gdk_color_parse("lightblue", &blue);

      /* These are just some markers to draw that show where the
       * 1000 "scaled" world coords are. i.e. 1000 world coords
       * of _this_ canvas, which does not equal the feature
       * coords here... They get updated in x by the
       * container_draw_locator update hook. */

      for(i = 0; i < 26; i+=2)
	{
	  foo_canvas_item_new(overlay_group,
			      FOO_TYPE_CANVAS_RECT,
			      "x1", 0.0,
			      "y1", (i * 1000.0),
			      "x2", 100.0,
			      "y2", (i + 1) * 1000.0,
			      "outline_color_gdk", &blue,
			      "fill_color_gdk",    (GdkColor *)(NULL),
			      "width_pixels",      navigate->locator_bwidth,
			      NULL);
	}
    }

  /* Need something to do for the event capture. */
  if((transp_data =  g_new0(TransparencyEventStruct, 1)))
    {
      FooCanvasItem *root_bg = NULL;

      root_bg = (FooCanvasItem *)(navigate->container_root);

      transp_data->navigate      = navigate;
      transp_data->locator_click = FALSE;
      transp_data->click_correction = 0.0;

      g_signal_connect(GTK_OBJECT(root_bg), "event",
		       GTK_SIGNAL_FUNC(rootBGEventCB), transp_data);
    }

  return ;
}




