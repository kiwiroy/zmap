/*  File: zmapWindowDump.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
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
 * Description: Contains functions to output window contents in various
 *              formats (postscript, PNG etc.) to a file, i.e. a screen "dump".
 *
 * Exported functions: See ZMap/zmapWindow.h
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.hpp>


/*
 * THIS CODE IS ESSENTIALLY COMMENTED OUT BECAUSE THE g2 LIBRARY CALLS
 * ARE BEING REPLACED WITH THE GtkPrint FUNCTIONS.
 *
 * OPTIONS THAT CALL THIS CODE WILL BE NO-OPS.....
 *
 *
 */




/* PLEASE READ THIS: there are some problems with understanding the coord/origin system, some
 * of which are of our own making (e.g. the blank border at top/bottom of the zmap affects the
 * scaling factor of the canvas which in turn affects stuff here.. but that is not all, the gv
 * postscript viewer is not working consistently making it hard to judge output from this
 * program...sigh...encapuslated and image output looks fine.... */

/* MH17:
 * There's something odd going on w/ coordinates and g2
 * - If we don't have origin as 0.0, 0.0 then we get blank documents
 * - x coords need to be offset by x1 but y coords do not
 * So to make it work I offset the x-coordinate only for all features
 * - y coords are ok due to being inverted relative to y2 so become based from 0 regardless
 */

#include <string.h>
#include <math.h>
#include <glib.h>

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
#include <g2.h>
#include <g2_PS.h>
#include <g2_gd.h>
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

#include <ZMap/zmapUtils.hpp>
#include <zmapWindow_P.hpp>

//#include <zmapWindowCanvas.h>
#include <zmapWindowContainers.hpp>
#include <zmapWindowCanvasItem.hpp>


/* dump data/options. */
typedef enum {EXTENT_WHOLE, EXTENT_VISIBLE} DumpExtent ;

typedef enum {DUMP_EPSF, DUMP_PS, DUMP_PNG, DUMP_JPEG} DumpFormat ;

typedef enum {CUSTOM_BEST_FIT, CUSTOM_CUSTOMISE} DumpCustomise ;

typedef enum {ORIENTATION_PORTRAIT, ORIENTATION_LANDSCAPE} DumpOrientation ;

typedef enum {ASPECT_AS_IS, ASPECT_FITPAGE} DumpAspect ;



typedef struct
{
  int g2_id ;
  GHashTable *ink_colours ;                                    /* hash of printing colours. */

  double x1, y1, x2, y2 ;                                    /* Bounds of displayed objects. */

  /* If filename == NULL then channel is used. */
  char *filename ;
  GIOChannel *channel ;

  DumpExtent extent ;
  DumpFormat format ;
  DumpCustomise customise ;
  DumpOrientation orientation ;
  DumpAspect aspect ;

  ZMapWindow window ;

  GdkColor *current_background_colour;

  const char *id;       // for debugging
  char *levels[10];
  char *names[10];

} DumpOptionsStruct, *DumpOptions ;

/* Notes on GdkColor *current_background_colour; in struct above:
 * --------------------------------------------------------------
 * This is a fix for RT ticket # 59775.
 *
 * We need to hold onto the current background colour as the foo
 * canvas does nothing to retrieve the real colour for transparent
 * items. As a result, those items that were not created with a
 * fill colour will have an incorrect value for their ->fill_colour
 * as in rects and polygons.  This isn't an issue for the foo canvas
 * as when nothing has been set no gdk_draw_rectangle occurs...
 * Any the fix is to temporarily set the colour on the items to be
 * that of the item below (good job we have a hierarchy), get the
 * colour from the item, set the ink in g2, and reset the fill back
 * to NULL.  A long winded way, but it works
 */

/* Callback data for dump dialog. */
typedef struct
{
  DumpOptions dump_opts ;

  GtkWidget *options_frame ;

  GtkWidget *customise_frame ;

} dialogCBStruct, *dialogCB ;



/* We need to unpack the colours in the foocanvas item structs....these macros do this. */

/* Convert Shifted Int to Scaled double */
#define S_I2S_D(INT, SHIFT) \
  ( ((double)( ((INT) << (32 - ((SHIFT) + 8))) >> 24 )) / 255.0 )

/* Get all three colours. */
#define FROM_FOO_CANVAS_COLOR(COMPOSITE, RED, GREEN, BLUE)         \
  ( ((RED)   =  S_I2S_D(COMPOSITE, 24)),                           \
    ((GREEN) =  S_I2S_D(COMPOSITE, 16)),                           \
    ((BLUE)  =  S_I2S_D(COMPOSITE,  8)) )


/* Trivial position invertor. */
#define COORDINVERT(COORD, SEQ_END)                                   \
  (COORD) = (SEQ_END) - (COORD) + 1


static gboolean dumpWindow(DumpOptions dump_opts) ;
static void dumpCB(ZMapWindowContainerGroup container_parent, FooCanvasPoints *points,
                   ZMapContainerLevelType level, gpointer user_data);
static void itemCB(gpointer data, gpointer user_data) ;
static void dumpFeatureCB(gpointer data, gpointer user_data);


static int openPS(DumpOptions dump_opts) ;
static int openEPSF(DumpOptions dump_opts) ;
static int openGD(DumpOptions dump_opts) ;
static int setScalingPS(DumpOptions dump_opts) ;

static gboolean chooseDump(DumpOptions dump_opts) ;
static void setSensitive(GtkWidget *button, gpointer cb_data, gboolean active) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
static void printCB(GtkButton *button, gpointer user_data) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


#if NOT_USED
static int getInkColour(int g2_id, GHashTable *ink_colours, guint composite_colour) ;
#endif
static void scale2Canvas(DumpOptions dump_opts,
                         double *width, double *height,
                         double *x_origin, double *y_origin, double *x_mul, double *y_mul) ;



/* Just this function for now, can give other options as required. */
gboolean zMapWindowDump(ZMapWindow window)
{
  gboolean result = FALSE ;
  DumpOptionsStruct dump_opts = {0} ;

  dump_opts.window = window ;

  if ((result = chooseDump(&dump_opts)))
    {
      result = dumpWindow(&dump_opts) ;

      g_free(dump_opts.filename) ;
    }

  return result ;
}


/* This is internal to zmapWindow currently but could be exposed. */
gboolean zmapWindowDumpFile(ZMapWindow window, char *filename)
{
  gboolean result = FALSE ;
  DumpOptionsStruct dump_opts = {0} ;

  dump_opts.window = window ;

  dump_opts.filename = filename ;
  dump_opts.extent = EXTENT_VISIBLE ;
  dump_opts.format = DUMP_PS ;
  dump_opts.customise = CUSTOM_BEST_FIT ;
  dump_opts.orientation = ORIENTATION_PORTRAIT ;
  dump_opts.aspect = ASPECT_AS_IS ;

  result = dumpWindow(&dump_opts) ;

  return result ;
}



/*
 *                    Internal routines
 */


static gboolean dumpWindow(DumpOptions dump_opts)
{
  gboolean result = TRUE ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  if (chooseDump(dump_opts))
    {
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

      dump_opts->ink_colours = g_hash_table_new(NULL, NULL) ;

      if (dump_opts->extent == EXTENT_WHOLE)
        {
          /* should I be getting the whole thing here including borders ???? */

          zMapWindowMaxWindowPos(dump_opts->window, &dump_opts->x1, &dump_opts->y1, &dump_opts->x2, &dump_opts->y2) ;

        }
      else
        {
          zMapWindowCurrWindowPos(dump_opts->window, &dump_opts->x1, &dump_opts->y1, &dump_opts->x2, &dump_opts->y2) ;

// this looks equivalent...
//      void zmapWindowItemGetVisibleWorld(ZMapWindow window,double *wx1, double *wy1,double *wx2, double *wy2)

        }
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
printf("dump exent: %d %f,%f %f,%f\n",dump_opts->extent,dump_opts->x1,dump_opts->y1,dump_opts->x2,dump_opts->y2);
#endif

      /* Round boundaries of canvas down/up to make sure features at the very edge of the canvas
       * are not clipped. */
      dump_opts->x1 = floor(dump_opts->x1) ;
      dump_opts->y1 = floor(dump_opts->y1) ;
      dump_opts->x2 = ceil(dump_opts->x2) ;
      dump_opts->y2 = ceil(dump_opts->y2) ;


      /* Open the correct context. */
      switch (dump_opts->format)
        {
        case DUMP_PS:
          dump_opts->g2_id = openPS(dump_opts) ;
          break ;
        case DUMP_EPSF:
          dump_opts->g2_id = openEPSF(dump_opts) ;
          break ;
        case DUMP_PNG:
        case DUMP_JPEG:
          dump_opts->g2_id = openGD(dump_opts) ;
          break ;
        }


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      g2_set_font_size(dump_opts->g2_id,30.0);
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


      /* Could turn off autoflush here for performance, see how it goes....see p.16 in docs... */

      zmapWindowContainerUtilsExecute(dump_opts->window->feature_root_group,
                                 ZMAPCONTAINER_LEVEL_FEATURESET,
                                 dumpCB, dump_opts);


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      g2_close(dump_opts->g2_id) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


      g_hash_table_destroy(dump_opts->ink_colours) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
    }
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



  return result ;
}


static void add_dump_format_options(GtkWidget *vbox, gpointer user_data)
{
  GtkWidget *frame = NULL, *hbox_top = NULL, *hbox = NULL;
  ZMapGUIRadioButtonStruct extent_buttons[] = {
    {EXTENT_VISIBLE, "Visible Features", NULL},
    {EXTENT_WHOLE,   "All Features",     NULL},
    {-1,             NULL,               NULL}};
  ZMapGUIRadioButtonStruct type_buttons[] = {
    {DUMP_EPSF, "Encaps. Postscript", NULL},
    {DUMP_PS,   "Postscript",         NULL},
    {DUMP_PNG,  "PNG",                NULL},
    {DUMP_JPEG, "JPEG",               NULL},
    {-1,        NULL,                 NULL}};
  ZMapGUIRadioButtonStruct custom_buttons[] = {
    {CUSTOM_BEST_FIT,  "Best Fit",  NULL},
    {CUSTOM_CUSTOMISE, "Customise", NULL},
    {-1, NULL, NULL}};
  ZMapGUIRadioButtonStruct orient_buttons[] = {
    {ORIENTATION_PORTRAIT,  "Portrait",  NULL},
    {ORIENTATION_LANDSCAPE, "Landscape", NULL},
    {-1,                    NULL,        NULL}};
  ZMapGUIRadioButtonStruct aspect_buttons[] = {
    {ASPECT_AS_IS,   "As is",       NULL},
    {ASPECT_FITPAGE, "Fit to Page", NULL},
    {-1,             NULL,          NULL}};

  dialogCB cb_data = (dialogCB)user_data ;

  /* Add extent box. */
  frame = gtk_frame_new(" Extent ") ;
  gtk_container_border_width(GTK_CONTAINER(frame), 5);
  gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, TRUE, 0);

  hbox = gtk_hbox_new(FALSE, 10) ;

  zMapGUICreateRadioGroup(hbox,
                          &extent_buttons[0],
                          EXTENT_VISIBLE,
                          (int *)&(cb_data->dump_opts->extent),
                          NULL, NULL);

  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10) ;
  gtk_container_add(GTK_CONTAINER(frame), hbox) ;

  /* Add file format box. */
  frame = gtk_frame_new(" Type ") ;
  gtk_container_border_width(GTK_CONTAINER(frame), 5);
  gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, TRUE, 0);

  hbox = gtk_hbox_new(FALSE, 10) ;
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10) ;
  gtk_container_add(GTK_CONTAINER(frame), hbox) ;

  zMapGUICreateRadioGroup(hbox,
                          &type_buttons[0],
                          DUMP_EPSF, (int *)&(cb_data->dump_opts->format),
                          setSensitive, cb_data);

  /* Add a format options box. */
  cb_data->options_frame = frame = gtk_frame_new(" Format Options ") ;
  gtk_container_border_width(GTK_CONTAINER(frame), 5);
  gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, TRUE, 0);


  hbox_top = gtk_hbox_new(FALSE, 10) ;
  gtk_container_set_border_width(GTK_CONTAINER (hbox_top), 10) ;
  gtk_container_add(GTK_CONTAINER(frame), hbox_top) ;


  frame = gtk_frame_new(NULL) ;
  gtk_container_border_width(GTK_CONTAINER(frame), 5);
  gtk_box_pack_start(GTK_BOX(hbox_top), frame, FALSE, TRUE, 0);


  hbox = gtk_hbox_new(FALSE, 10) ;
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10) ;
  gtk_container_add(GTK_CONTAINER(frame), hbox) ;

  zMapGUICreateRadioGroup(hbox,
                          &custom_buttons[0],
                          CUSTOM_BEST_FIT,
                          (int *)&(cb_data->dump_opts->customise),
                          setSensitive, cb_data);

  cb_data->customise_frame = frame = gtk_frame_new(NULL) ;
  gtk_container_border_width(GTK_CONTAINER(frame), 5);
  gtk_box_pack_start(GTK_BOX(hbox_top), frame, FALSE, TRUE, 0);


  hbox_top = gtk_hbox_new(FALSE, 10) ;
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 10) ;
  gtk_container_add(GTK_CONTAINER(frame), hbox_top) ;


  frame = gtk_frame_new(" Orientation ") ;
  gtk_container_border_width(GTK_CONTAINER(frame), 5);
  gtk_box_pack_start(GTK_BOX(hbox_top), frame, FALSE, TRUE, 0);

  hbox = gtk_hbox_new(FALSE, 10) ;
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10) ;
  gtk_container_add(GTK_CONTAINER(frame), hbox) ;

  zMapGUICreateRadioGroup(hbox,
                          &orient_buttons[0],
                          ORIENTATION_PORTRAIT,
                          (int *)&(cb_data->dump_opts->orientation),
                          NULL, NULL);

  frame = gtk_frame_new(" Aspect Ratio ") ;
  gtk_container_border_width(GTK_CONTAINER(frame), 5);
  gtk_box_pack_start(GTK_BOX(hbox_top), frame, FALSE, TRUE, 0);

  hbox = gtk_hbox_new(FALSE, 10) ;
  gtk_container_set_border_width(GTK_CONTAINER (hbox), 10) ;
  gtk_container_add(GTK_CONTAINER(frame), hbox) ;

  zMapGUICreateRadioGroup(hbox,
                          &aspect_buttons[0],
                          ASPECT_AS_IS,
                          (int *)&(cb_data->dump_opts->aspect),
                          NULL, NULL);

  setSensitive(NULL, cb_data, FALSE) ; /* Set which buttons are active. */
}


/* Create the print window for user to select options. */
static gboolean chooseDump(DumpOptions dump_opts)
{
  gboolean status = FALSE ;
  dialogCBStruct cb_data = {NULL} ;
  char *window_title ;

  cb_data.dump_opts = dump_opts ;

  window_title = zMapGUIMakeTitleString("Export Window", "Please select options") ;

  char *filename = zmapGUIFileChooserFull(NULL, window_title, NULL, NULL,
                                          add_dump_format_options, &cb_data) ;

  g_free(window_title) ;

  if (filename)
    {
      dump_opts->filename = g_strdup(filename) ;

      status = TRUE ;
    }

  return status ;
}


static void setSensitive(GtkWidget *button, gpointer data, gboolean active)
{
  dialogCB cb_data = (dialogCB)data;

//  if (cb_data->dump_opts->format == DUMP_PNG || cb_data->dump_opts->format == DUMP_JPEG)
  if (cb_data->dump_opts->format != DUMP_PS)      // we don't scale EPS
    gtk_widget_set_sensitive(cb_data->options_frame, FALSE) ;
  else
    gtk_widget_set_sensitive(cb_data->options_frame, TRUE) ;

  if (cb_data->dump_opts->customise == CUSTOM_CUSTOMISE)
    gtk_widget_set_sensitive(cb_data->customise_frame, TRUE) ;
  else
    gtk_widget_set_sensitive(cb_data->customise_frame, FALSE) ;

  return ;
}


/* Make a raw postscript file. */
static int openPS(DumpOptions dump_opts)
{
  int g2_id = 0 ;

  g2_id = setScalingPS(dump_opts) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  double a4_width = 595.0, a4_height = 842.0 ;
  double pixels_per_unit_x;
  double pixels_per_unit_y;
  double x1, y1, x2, y2 ;
  double canvas_width, canvas_height ;
  double x_origin, y_origin ;
  double paper_mul, x_mul, y_mul ;

  x1 = dump_opts->x1 ;
  y1 = dump_opts->y1 ;
  x2 = dump_opts->x2 ;
  y2 = dump_opts->y2 ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  g2_id = g2_open_PS(dump_opts->filename, g2_A4, g2_PS_port) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  x_origin = x1 ;
  y_origin = y1 ;


  pixels_per_unit_x = dump_opts->window->canvas->pixels_per_unit_x ;

  pixels_per_unit_y = dump_opts->window->canvas->pixels_per_unit_y ;

  if (dump_opts->extent == EXTENT_WHOLE)
    pixels_per_unit_y = zMapWindowGetZoomMin(dump_opts->window) ;


  canvas_width = ((x2 - x1 + 1) * pixels_per_unit_x) ;
  canvas_height = ((y2 - y1 + 1) * pixels_per_unit_y) ;


  /* redo this scaling stuff to be more accurate once I have everything working.... */
  if (canvas_width > canvas_height)
    paper_mul = a4_width / canvas_width ;
  else
    paper_mul = a4_height / canvas_height ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  x_mul = a4_width / (x2 - x1 + 1) ;
  y_mul = a4_height / (y2 - y1 + 1) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  x_mul = (a4_width / (x2 - x1 + 1)) * paper_mul ;
  y_mul = (a4_height / (y2 - y1 + 1)) * paper_mul ;


  g2_set_coordinate_system(dump_opts->g2_id, x_origin, y_origin, x_mul, y_mul) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  return g2_id ;
}


/*
 * Make an encapsulated postscript file
 * clipped to window size but not scaled to paper
 * EPS can be scaled when included in a document
 */

static int openEPSF(DumpOptions dump_opts)
{
  int g2_id = 0 ;
  double canvas_width, canvas_height ;
  double x_origin, y_origin ;
  double x_mul, y_mul ;

  scale2Canvas(dump_opts, &canvas_width, &canvas_height, &x_origin, &y_origin, &x_mul, &y_mul) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  g2_id = g2_open_EPSF_CLIP(dump_opts->filename, canvas_width, canvas_height) ;

  g2_set_coordinate_system(dump_opts->g2_id, x_origin, y_origin, x_mul, y_mul) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  return g2_id ;
}




/* Set up scaling for postscript. */
static int setScalingPS(DumpOptions dump_opts)
{
  int g2_id = 0 ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  enum g2_PS_orientation orientation ;
  double a4_width = 595.0, a4_height = 842.0 ;
  double pixels_per_unit_x;
  double pixels_per_unit_y;
  double x1, y1, x2, y2 ;
  double canvas_width, canvas_height ;
  double x_origin, y_origin ;
  double paper_mul, x_mul, y_mul ;


  /* Get coords of section to be exported, could be whole alignment or just visible window. */
  x1 = dump_opts->x1 ;
  y1 = dump_opts->y1 ;
  x2 = dump_opts->x2 ;
  y2 = dump_opts->y2 ;


  /* Get current zoom level, override with min zoom level if user wants to print whole alignment. */
  pixels_per_unit_x = dump_opts->window->canvas->pixels_per_unit_x ;
  pixels_per_unit_y = dump_opts->window->canvas->pixels_per_unit_y ;
  if (dump_opts->extent == EXTENT_WHOLE)
    pixels_per_unit_y = zMapWindowGetZoomMin(dump_opts->window) ;


  /* Get scaled size. */
  canvas_width = ((x2 - x1 + 1) * pixels_per_unit_x) ;
  canvas_height = ((y2 - y1 + 1) * pixels_per_unit_y) ;


  /* If user chose simple best fit then make window fit the page as best as possible while
   * maintaining its aspect ratio. */
  if (dump_opts->customise == CUSTOM_BEST_FIT)
    {
      if (canvas_width > canvas_height)
        dump_opts->orientation = ORIENTATION_LANDSCAPE ;
      else
        dump_opts->orientation = ORIENTATION_PORTRAIT ;

      dump_opts->aspect = ASPECT_AS_IS ;
    }


  if (dump_opts->orientation == ORIENTATION_PORTRAIT)
    {
      orientation = g2_PS_port ;
      a4_width = 595.0 ;
      a4_height = 842.0 ;
    }
  else
    {
      orientation = g2_PS_land ;
      a4_width = 842.0 ;
      a4_height = 595.0 ;
    }

  g2_id = g2_open_PS(dump_opts->filename, g2_A4, orientation) ;



  if (dump_opts->aspect == ASPECT_AS_IS)
    {

      if (canvas_width > canvas_height)
        paper_mul = a4_width / canvas_width ;
      else
        paper_mul = a4_height / canvas_height ;



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      x_mul = (a4_width / (x2 - x1 + 1)) * paper_mul ;
      y_mul = (a4_height / (y2 - y1 + 1)) * paper_mul ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

      x_mul = pixels_per_unit_x * paper_mul ;
      y_mul = pixels_per_unit_y * paper_mul ;


    }
  else
    {
      x_mul = a4_width / (x2 - x1 + 1) ;
      y_mul = a4_height / (y2 - y1 + 1) ;
    }


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  if (dump_opts->orientation == ORIENTATION_PORTRAIT)
    {
      x_origin = x1 ;
      y_origin = y1 * y_mul ;
    }
  else
    {
      x_origin = x1 ;
      y_origin = y1 * y_mul ;
    }
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  x_origin = y_origin = 0.0;

  g2_set_coordinate_system(g2_id, x_origin, y_origin, x_mul, y_mul) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  return g2_id ;
}






/* Make either a jpeg or png image. */
static int openGD(DumpOptions dump_opts)
{
  int g2_id = 0 ;
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  double canvas_width, canvas_height ;
  double x_origin, y_origin ;
  double x_mul, y_mul ;



  scale2Canvas(dump_opts, &canvas_width, &canvas_height, &x_origin, &y_origin, &x_mul, &y_mul) ;

  if (dump_opts->format == DUMP_PNG)
    g2_id = g2_open_gd(dump_opts->filename, canvas_width, canvas_height, g2_gd_png) ;
  else
    g2_id = g2_open_gd(dump_opts->filename, canvas_width, canvas_height, g2_gd_jpeg) ;

  g2_set_coordinate_system(dump_opts->g2_id, x_origin, y_origin, x_mul, y_mul) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  return g2_id ;
}


static gboolean dumpItemIsVisible(FooCanvasItem *item, DumpOptions dump_options)
{
  double x1,y1,x2,y2;
  gboolean ret = FALSE;

      // need to convert iten coords to world

  foo_canvas_window_to_world(item->canvas,item->x1,item->y1,&x1,&y1);
  foo_canvas_window_to_world(item->canvas,item->x2,item->y2,&x2,&y2);

  if(zmapWindowItemIsShown(item))
    {
      if(!((x1 >= dump_options->x2) || (x2 <= dump_options->x1)) &&
            !((y1 >= dump_options->y2) || (y2 <= dump_options->y1)))
          ret = TRUE;
    }

  return(ret);
}


//#define ED_G_NEVER_INCLUDE_THIS_CODE      1
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
#include <sys/time.h>
#include <time.h>
char * tstamp()
{
      struct timeval time;
      struct timezone tz = {0,0};
      static char tbuf[64];
      long start = 0;

      gettimeofday(&time,&tz);
      if(!start) start = time.tv_sec;
      time.tv_usec /= 1000;
      sprintf(tbuf,"%d.%d",time.tv_sec,time.tv_usec);
      return(tbuf);
}
#endif

static void dumpCB(ZMapWindowContainerGroup container_parent, FooCanvasPoints *points,
                   ZMapContainerLevelType level,  gpointer user_data)
{
  DumpOptions cb_data = (DumpOptions)user_data ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  {
    FooCanvasItem *foo = FOO_CANVAS_ITEM(container_parent);
    ZMapFeatureAny any ;
    char *name = "no feature";
    char *lev = "";
    char *prevn = "";
    char *prevl = "";
    static char  buf[20]; // will always only be used on strand level

    printf("DumpCB starts %s\n",tstamp());

    if(level > 1)
    {
      prevn = cb_data->names[level-1];
      prevl = cb_data->levels[level-1];
    }
    if ((any = zmapWindowItemGetFeatureAny(foo)))     // we get a log warning if this is a strand
      {
        name = (char *) g_quark_to_string(any->original_id) ;
      }
    else if(ZMAP_IS_CONTAINER_STRAND(container_parent))
    {
      sprintf(buf,"%d",zmapWindowContainerGetStrand(container_parent));
      name = buf;
    }

    lev = (char *)G_OBJECT_TYPE_NAME(foo);
    printf("Dump: %s level %d %s/%s -> %s/%s\n",tstamp(), level, prevl,prevn,lev,name);
    cb_data->levels[level] = lev;
    cb_data->names[level] = name;
  }
#endif

      // add existing visible foo canvas items to the g2 canvas
      switch(level)
      {
        case ZMAPCONTAINER_LEVEL_ROOT:
        case ZMAPCONTAINER_LEVEL_ALIGN:
          break;

        case ZMAPCONTAINER_LEVEL_BLOCK:
          // paint background?
          break;


        case ZMAPCONTAINER_LEVEL_FEATURESET:

//printf("DumpCB featureset 1 %s\n",tstamp());
         {
//printf("DumpCB featureset 2 %s\n",tstamp());
            ZMapWindowContainerFeatures features;
            if ((features = zmapWindowContainerGetFeatures(container_parent)))
              {
                FooCanvasGroup *features_group;
                features_group = (FooCanvasGroup *)features;
                cb_data->id = "Features";
//printf("DumpCB featureset 3 %s, %d features in set\n",tstamp(),g_list_length(features_group->item_list));
                g_list_foreach(features_group->item_list, itemCB, user_data) ;
              }
            break;
          }

          default:
            zMapWarnIfReached();
            break;
      }
//printf("DumpCB ends %s\n",tstamp());
}


static void itemCB(gpointer data, gpointer user_data)
{
  FooCanvasItem *item = (FooCanvasItem *)data ;
  DumpOptions dump_options = (DumpOptions)user_data ;

    if(FOO_IS_CANVAS_GROUP(item))         // complex object
      {
        if(dumpItemIsVisible(item,dump_options))
          {
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
            ZMapFeature feature ;

            if ((feature = zmapWindowItemGetFeature(item)))
              {
                printf("group feature %s/ %s at %f,%f,%f,%f: %s\n",
                  G_OBJECT_TYPE_NAME(item), g_quark_to_string(feature->original_id),
                  item->x1,item->y1,item->x2,item->y2,
                  ZMAP_IS_CANVAS_ITEM(item)? "CanvasGroup": "");
              }
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

        }
      }
    else
      {
         dumpFeatureCB(item, user_data) ;
      }

  return ;
}





/* Sadly the g2 package doesn't really allow relative drawing in any consistent way,
 * e.g. you can do lines and points but not any other shapes..sigh...
 * so we have to convert to world coords...sigh... */
static void dumpFeatureCB(gpointer data, gpointer user_data)
{
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  FooCanvasItem *item = FOO_CANVAS_ITEM(data);
  DumpOptions cb_data = (DumpOptions)user_data ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  printf("Paint %s @ %f,%f %f,%f\n", G_OBJECT_TYPE_NAME(item),item->x1,item->y1,item->x2,item->y2);
#endif

    {

      guint composite ;
      int fill_colour = 0 ;                                    /* default to white. */
      int outline_colour = 1 ;                            /* default to black. */


      if (FOO_IS_CANVAS_RE(item))
        {
          dumpRectangle(cb_data, FOO_CANVAS_RE(item), TRUE) ;
        }
      else if (FOO_IS_CANVAS_LINE(item))
        {
          FooCanvasLine *line_item = FOO_CANVAS_LINE(item) ;
          int i ;
          double *line, *point_x, *point_y ;
          int bytes_to_copy ;

          bytes_to_copy = line_item->num_points * 2 * sizeof(double) ;
          line = (double *)g_malloc(bytes_to_copy) ;
          memcpy(line, line_item->coords, bytes_to_copy) ;

          for (i = 0, point_x = line, point_y = line + 1 ;
               i < line_item->num_points ;
               i++, point_x += 2, point_y += 2)
            {
              foo_canvas_item_i2w(item, point_x, point_y) ;
            *point_x -= cb_data->x1;

              COORDINVERT(*point_y, cb_data->y2) ;
            }

          composite = line_item->fill_rgba ;

          fill_colour = getInkColour(cb_data->g2_id, cb_data->ink_colours, composite) ;

          g2_pen(cb_data->g2_id, fill_colour) ;
          g2_poly_line(cb_data->g2_id, line_item->num_points, line) ;

          g_free(line) ;
        }
      else if (FOO_IS_CANVAS_POLYGON(item))
        {
          FooCanvasPolygon *polygon_item = FOO_CANVAS_POLYGON(item) ;
          int i ;
          double *line, *point_x, *point_y ;
          int bytes_to_copy ;
          gboolean fill_set;

          bytes_to_copy = polygon_item->num_points * 2 * sizeof(double) ;
          line = (double *)g_malloc(bytes_to_copy) ;
          memcpy(line, polygon_item->coords, bytes_to_copy) ;

          for (i = 0, point_x = line, point_y = line + 1 ;
               i < polygon_item->num_points ;
               i++, point_x += 2, point_y += 2)
            {
              foo_canvas_item_i2w(item, point_x, point_y) ;
            *point_x -= cb_data->x1;

              COORDINVERT(*point_y, cb_data->y2) ;
            }

          if(!(fill_set = polygon_item->fill_set))
            foo_canvas_item_set(FOO_CANVAS_ITEM(polygon_item),
                                "fill_color_gdk", cb_data->current_background_colour,
                                NULL);

          composite = polygon_item->fill_color ;
          fill_colour = getInkColour(cb_data->g2_id, cb_data->ink_colours, composite) ;

          if(!fill_set)
              foo_canvas_item_set(FOO_CANVAS_ITEM(polygon_item),
                                  "fill_color_gdk", NULL,
                                  NULL);

          composite = polygon_item->outline_color ;
          outline_colour = getInkColour(cb_data->g2_id, cb_data->ink_colours, composite) ;

          g2_pen(cb_data->g2_id, fill_colour) ;
          g2_filled_polygon(cb_data->g2_id, polygon_item->num_points, line) ;

          g2_pen(cb_data->g2_id, outline_colour) ;
          g2_polygon(cb_data->g2_id, polygon_item->num_points, line) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
          printf("polygon: ") ;
          for (i = 0, point_x = line ; i < polygon_item->num_points ; i++, point_x += 2)
            {
              printf("%f %f ", *point_x, *(point_x + 1)) ;
            }
          printf("\n") ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

          g_free(line) ;
        }
      else if (FOO_IS_CANVAS_TEXT(item))
        {
          FooCanvasText *text_item = FOO_CANVAS_TEXT(item) ;
          double x, y ;

// so I can look at them in the debugger
//ZMapWindowTextItem zwt = ZMAP_WINDOW_TEXT_ITEM(text_item);
// "GLib-GObject-WARNING **: invalid cast from `ZMapWindowTextItem' to `FooCanvasZMapText'"
//FooCanvasZMapText *fzt = FOO_CANVAS_ZMAP_TEXT(text_item);

#ifdef MH17_XY_IS_ZERO
          x = text_item->x ;
          y = text_item->y ;
#else
        x = text_item->item.x1;
        y = text_item->item.y1;
#endif
          foo_canvas_item_i2w(item, &x, &y) ;
        x -= cb_data->x1;

          COORDINVERT(y, cb_data->y2) ;

          y = y - (text_item->clip_height / 2) ;            /* Correct for text placement... */

          composite = text_item->rgba ;
          fill_colour = getInkColour(cb_data->g2_id, cb_data->ink_colours, composite) ;

        //seem to have the whole dna seq here
// printf("text <%.20s> @ %f,%f (%f,%f) rgba = %x\n",text_item->text,x,y,text_item->item.x1,text_item->item.y1,composite);
          g2_pen(cb_data->g2_id, fill_colour) ;
        g2_set_font_size(cb_data->g2_id,20.0);
          g2_string(cb_data->g2_id, x, y, text_item->text) ;
        }
      else
        {
          zMapLogMessage("Unexpected item [%s]", G_OBJECT_TYPE_NAME(item));
          zMapWarnIfReached() ;
        }
    }
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  return ;
}




#if NOT_USED
/* Looks for a colour in our hash table of colours and returns the corresponding ink id.
 * There is a strict limit of 256 colours in g2's interface to the libgd package so
 * we keep a hash of colours so that don't allocate the same colour twice.
 *
 * NOTE: you cannot simply clear the palette and then reallocate as this completely messes
 * up the gd output.
 *  */
static int getInkColour(int g2_id, GHashTable *ink_colours, guint composite_colour)
{
  int ink = 0;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  if (!(ink = GPOINTER_TO_UINT(g_hash_table_lookup(ink_colours, GUINT_TO_POINTER(composite_colour)))))
    {
      double red = 0.0, green = 0.0, blue = 0.0 ;

      FROM_FOO_CANVAS_COLOR(composite_colour, red, green, blue) ;

      ink = g2_ink(g2_id, red, green, blue) ;

      g_hash_table_insert(ink_colours, GUINT_TO_POINTER(composite_colour),
                          GUINT_TO_POINTER(ink)) ;

    }
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  return ink ;
}
#endif


static void scale2Canvas(DumpOptions dump_opts,
                         double *canvas_width, double *canvas_height,
                         double *x_origin, double *y_origin, double *x_mul, double *y_mul)
{
  double pixels_per_unit_x;
  double pixels_per_unit_y;
  double x1, y1, x2, y2 ;

  x1 = dump_opts->x1 ;
  y1 = dump_opts->y1 ;
  x2 = dump_opts->x2 ;
  y2 = dump_opts->y2 ;

  *x_origin = *y_origin = 0.0 ;

  pixels_per_unit_x = dump_opts->window->canvas->pixels_per_unit_x ;
  pixels_per_unit_y = dump_opts->window->canvas->pixels_per_unit_y ;

  *canvas_width = ((x2 - x1 + 1) * pixels_per_unit_x) ;
  *canvas_height = ((y2 - y1 + 1) * pixels_per_unit_y) ;

  *x_mul = (*canvas_width / (x2 - x1 + 1)) ;
  *y_mul = (*canvas_height / (y2 - y1 + 1)) ;

  return ;
}






