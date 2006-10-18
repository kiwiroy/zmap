/*  File: zmapControlNavigator.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) Sanger Institute, 2004
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
 * originated by
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk
 *
 * Description: Implements the navigator window in a zmap.
 *              We tried using a scale widget but you can't set the size
 *              of the thumb with this widget which is useless so for
 *              now we are using the scrollbar which looks naff...sigh.
 *              
 * Exported functions: See zmapControl_P.h
 * HISTORY:
 * Last edited: Oct 18 14:54 2006 (rds)
 * Created: Thu Jul  8 12:54:27 2004 (edgrif)
 * CVS info:   $Id: zmapControlNavigator.c,v 1.25 2006-10-18 15:10:54 rds Exp $
 *-------------------------------------------------------------------
 */

#include <math.h>
#include <gtk/gtk.h>
#include <zmapNavigator_P.h>


#define TOPTEXT_NO_SCALE "<no data>"
#define BOTTEXT_NO_SCALE ""


static void valueCB(GtkAdjustment *adjustment, gpointer user_data) ;
static void paneNotifyPositionCB(GObject *pane, GParamSpec *scroll, gpointer user_data);
static void canvasValueCB(gpointer user_data, double top, double bottom) 
{
  ZMapNavigator navigator = (ZMapNavigator)user_data;

  if (navigator->cb_func)
    (*(navigator->cb_func))(navigator->user_data, top, bottom) ;

  return ;
}


/* Create an instance of the navigator, this currently has two scroll bars,
 * one to show the position of the region in its parent assembly and 
 * one to show the position of the window within the canvas.
 * The function returns the navigator instance but also its top container
 * widget in top_widg_out. */
ZMapNavigator zMapNavigatorCreate(GtkWidget **top_widg_out, GtkWidget **canvas_out)
{
  ZMapNavigator navigator ;
  GtkWidget *pane ;
  GtkObject *adjustment ;
  GtkWidget *label ;

  navigator = g_new0(ZMapNavStruct, 1);

  navigator->pane = pane = gtk_hpaned_new() ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  gtk_widget_set_size_request(pane, 100, -1) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  /* Construct the region locator. */

  /* Need a vbox so we can add a label with sequence size at the bottom later,
   * we set it to a fixed width so that the text is always visible. */
  navigator->navVBox = gtk_vbox_new(FALSE, 0);
  gtk_paned_add1(GTK_PANED(pane), navigator->navVBox) ;

  label = gtk_label_new("Region") ;
  gtk_box_pack_start(GTK_BOX(navigator->navVBox), label, FALSE, TRUE, 0);

  navigator->topLabel = gtk_label_new(TOPTEXT_NO_SCALE) ;
  gtk_box_pack_start(GTK_BOX(navigator->navVBox), navigator->topLabel, FALSE, TRUE, 0);


  /* Make the navigator with a default, "blank" adjustment obj. */
  adjustment = gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0) ;

  navigator->navVScroll = gtk_vscrollbar_new(GTK_ADJUSTMENT(adjustment)) ;
  gtk_box_pack_start(GTK_BOX(navigator->navVBox), navigator->navVScroll, TRUE, TRUE, 0) ;

  /* Note how we pack the label at the end of the vbox and set "expand" to FALSE so that it
   * remains small and the vscale expands to fill the rest of the box. */
  navigator->botLabel = gtk_label_new(BOTTEXT_NO_SCALE) ;
  gtk_box_pack_end(GTK_BOX(navigator->navVBox), navigator->botLabel, FALSE, TRUE, 0);


  /* Construct the window locator. Note that we set the update policy to discontinuous so if
   * user drags this scroll bar the position is only reported on button release. This is
   * necessary because the underlying canvas window cannot be remapped rapidly. */
  navigator->wind_vbox = gtk_vbox_new(FALSE, 0);

  {
    GtkWidget *locator_frame = NULL,
      *locator_canvas = NULL,
      *locator_pane   = NULL, 
      *locator_vbox   = NULL,
      *locator_label  = NULL,
      *event_box      = NULL,
      *event_label    = NULL;
    ZMapWindowNavigatorCallbackStruct cbs = {NULL};

#ifdef RDS_USE_SCROLL
    locator_pane = gtk_hpaned_new();
    gtk_paned_add2(GTK_PANED(pane), locator_pane) ; /* add this pane to main pane */

    gtk_paned_add1(GTK_PANED(locator_pane), navigator->wind_vbox) ; /* add scroll to this pane */
#endif /* RDS_USE_SCROLL */
    
    locator_vbox = gtk_vbox_new(FALSE, 0);

#ifdef RDS_USE_SCROLL
    gtk_paned_add2(GTK_PANED(locator_pane), locator_vbox) ; /* add locusnav to this pane */
#else
    gtk_paned_add2(GTK_PANED(pane), locator_vbox) ; /* add locusnav to this pane */
#endif /* RDS_USE_SCROLL */

    locator_label = gtk_label_new("Navigator") ;
    gtk_box_pack_start(GTK_BOX(locator_vbox), locator_label, FALSE, TRUE, 0);

    locator_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(locator_frame), GTK_SHADOW_NONE);
    gtk_box_pack_start(GTK_BOX(locator_vbox), locator_frame, TRUE, TRUE, 0);

    cbs.valueCB    = canvasValueCB;
    locator_canvas = navigator->locator_widget = 
      zMapWindowNavigatorCreateCanvas(&cbs, navigator);
    
    gtk_container_add(GTK_CONTAINER(locator_frame), locator_canvas);
    
    event_box = gtk_event_box_new();
    /* gtk_widget_set_name(event_box, "zmap-control-infopanel"); */
    gtk_box_pack_start(GTK_BOX(locator_vbox), event_box, FALSE, FALSE, 2);
    
    event_label = gtk_label_new("drag box to navigate");
    gtk_label_set_selectable(GTK_LABEL(event_label), TRUE);
    gtk_container_add(GTK_CONTAINER(event_box), event_label);
#ifdef RDS_USE_SCROLL
    g_object_connect(G_OBJECT(pane), 
                     "signal::notify::position", 
                     G_CALLBACK(paneNotifyPositionCB), 
                     (gpointer)navigator,
                     NULL);
#endif
    if(canvas_out)
      *canvas_out = locator_canvas;
    
  }

#ifdef RDS_USE_SCROLL
  label = gtk_label_new("Scroll") ;
  gtk_box_pack_start(GTK_BOX(navigator->wind_vbox), label, FALSE, TRUE, 0);

  navigator->wind_top_label = gtk_label_new(TOPTEXT_NO_SCALE) ;
  gtk_box_pack_start(GTK_BOX(navigator->wind_vbox), navigator->wind_top_label, FALSE, TRUE, 0);

  adjustment = gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0) ; /* "blank" adjustment obj. */

#ifdef RDS_DONT_INCLUDE_UNUSED
  gtk_signal_connect(GTK_OBJECT(adjustment), "value-changed",
                     GTK_SIGNAL_FUNC(valueCB), (void *)navigator) ;
#endif /* RDS_DONT_INCLUDE_UNUSED */

  navigator->wind_scroll = gtk_vscrollbar_new(GTK_ADJUSTMENT(adjustment)) ;
  gtk_range_set_update_policy(GTK_RANGE(navigator->wind_scroll), GTK_UPDATE_DISCONTINUOUS) ;
  gtk_box_pack_start(GTK_BOX(navigator->wind_vbox), navigator->wind_scroll, TRUE, TRUE, 0) ;


  /* Note how we pack the label at the end of the vbox and set "expand" to FALSE so that it
   * remains small and the vscale expands to fill the rest of the box. */
  navigator->wind_bot_label = gtk_label_new(BOTTEXT_NO_SCALE) ;
  gtk_box_pack_end(GTK_BOX(navigator->wind_vbox), navigator->wind_bot_label, FALSE, TRUE, 0);
#endif /* RDS_USE_SCROLL */

  navigator->wind_top = navigator->wind_bot = 0.0 ;

  /* Set left hand (region view) pane closed by default. */
  gtk_paned_set_position(GTK_PANED(pane), 0) ;

  *top_widg_out = pane ;

  return navigator ;
}


/* Set function + data that Navigator will call each time the position of the region window
 * is changed. */
void zMapNavigatorSetWindowCallback(ZMapNavigator navigator,
				    ZMapNavigatorScrollValue cb_func, void *user_data)
{
  navigator->cb_func = cb_func ;
  navigator->user_data = user_data ;

  return ;
}

/* zmapControl.c:388 calls this! */

/* Set the window adjuster to match the Max Window size of the underlying canvas. This is needed
 * because with a long sequence it is not possible to zoom the whole sequence down to the level
 * of bases without exceeding the maximum window size of X Windows.
 * We need to watch for when the scroll bar shrinks and at this point open the window
 * slider so the user sees it.....then they can use this to navigate when we can't show the whole
 * sequence in one window.
 * Returns an integer which is the width in pixels of the window scrollbar. */
int zMapNavigatorSetWindowPos(ZMapNavigator navigator, double top_pos, double bot_pos)
{
  int pane_width = 0 ;
  GtkAdjustment *window_adjuster ;
  double w, h;
  enum {PANED_WINDOW_GUTTER_SIZE = 10} ;		    /* There is no simple way to get this. */

#ifdef RDS_USE_SCROLL
  /* Set the new scrollbar size. */
  window_adjuster = (GtkAdjustment *)gtk_range_get_adjustment(GTK_RANGE(navigator->wind_scroll)) ;

  window_adjuster->value = top_pos ;
  window_adjuster->page_size = (gdouble)(fabs(bot_pos - top_pos) + 1) ;

  gtk_adjustment_changed(window_adjuster) ;

  /* find out if page_size is smaller than max pane size....if so then return width of window
     navigator box, otherwise return 0 so the window navigator will not be visible. */
  //  if (window_adjuster->page_size < (window_adjuster->upper - window_adjuster->lower + 1))
    {
      GtkRequisition box_size ;

      gtk_widget_size_request(GTK_WIDGET(navigator->wind_vbox), &box_size) ;

      pane_width = box_size.width + PANED_WINDOW_GUTTER_SIZE ;
    }
#endif /* RDS_USE_SCROLL */
    
    {
      zMapWindowNavigatorPackDimensions(navigator->locator_widget, &w, &h);
      pane_width = (int)fabs(w);
      if(pane_width > 0)
        pane_width += PANED_WINDOW_GUTTER_SIZE;
    }

  return pane_width;
}



/* updates size/range to coords of the new view. */
void zMapNavigatorSetView(ZMapNavigator navigator, ZMapFeatureContext features,
			  double top, double bottom)
{
  GtkObject *region_adjuster, *window_adjuster ;
  gchar *region_top_str, *region_bot_str, *window_top_str, *window_bot_str ;
  region_adjuster = (GtkObject *)gtk_range_get_adjustment(GTK_RANGE(navigator->navVScroll)) ;
#ifdef RDS_USE_SCROLL
  window_adjuster = (GtkObject *)gtk_range_get_adjustment(GTK_RANGE(navigator->wind_scroll)) ;
#endif /* RDS_USE_SCROLL */

  /* May be called with no sequence to parent mapping so must set default navigator for this. */
  if (features)
    {
      navigator->parent_span = features->parent_span ;	    /* n.b. struct copy. */
      navigator->sequence_to_parent = features->sequence_to_parent ; /* n.b. struct copy. */

      region_top_str = g_strdup_printf("%d", navigator->parent_span.x1) ;
      region_bot_str = g_strdup_printf("%d", navigator->parent_span.x2) ;

      GTK_ADJUSTMENT(region_adjuster)->value = (((gdouble)(navigator->sequence_to_parent.p1
						    + navigator->sequence_to_parent.p2)) / 2.0) ;
      GTK_ADJUSTMENT(region_adjuster)->lower = (gdouble)navigator->parent_span.x1 ;
      GTK_ADJUSTMENT(region_adjuster)->upper = (gdouble)navigator->parent_span.x2 ;
      GTK_ADJUSTMENT(region_adjuster)->step_increment = 10.0 ;	    /* step incr, wild guess... */
      GTK_ADJUSTMENT(region_adjuster)->page_increment = 1000 ;	    /* page incr, wild guess... */
      GTK_ADJUSTMENT(region_adjuster)->page_size = (gdouble)(fabs(navigator->sequence_to_parent.p2
								  - navigator->sequence_to_parent.p1) + 1) ;

      window_top_str = g_strdup_printf("%d", navigator->sequence_to_parent.c1) ;
      window_bot_str = g_strdup_printf("%d", navigator->sequence_to_parent.c2) ;

#ifdef RDS_USE_SCROLL
      /* I assume here that we start at the top. */
      GTK_ADJUSTMENT(window_adjuster)->value = (0.0) ;
      GTK_ADJUSTMENT(window_adjuster)->lower = (gdouble)navigator->sequence_to_parent.c1 ;
      GTK_ADJUSTMENT(window_adjuster)->upper = (gdouble)navigator->sequence_to_parent.c2 ;
      GTK_ADJUSTMENT(window_adjuster)->step_increment = 0.0 ;
      GTK_ADJUSTMENT(window_adjuster)->page_increment = 0.0 ;
      GTK_ADJUSTMENT(window_adjuster)->page_size = (gdouble)(fabs(navigator->sequence_to_parent.c2
								  - navigator->sequence_to_parent.c1) + 1) ;
#endif /* RDS_USE_SCROLL */
      zMapNavigatorSetWindowPos(navigator, top, bottom) ;
    }
  else
    {
      navigator->parent_span.x1 = navigator->parent_span.x2 = 0 ;

      navigator->sequence_to_parent.p1 = navigator->sequence_to_parent.p2
	= navigator->sequence_to_parent.c1 = navigator->sequence_to_parent.c2 = 0 ;

      region_top_str = g_strdup(TOPTEXT_NO_SCALE) ;
      region_bot_str = g_strdup(BOTTEXT_NO_SCALE) ;

      GTK_ADJUSTMENT(region_adjuster)->value = 0.0 ;
      GTK_ADJUSTMENT(region_adjuster)->lower = 0.0 ;
      GTK_ADJUSTMENT(region_adjuster)->upper = 0.0 ;
      GTK_ADJUSTMENT(region_adjuster)->step_increment = 0.0 ;
      GTK_ADJUSTMENT(region_adjuster)->page_increment = 0.0 ;
      GTK_ADJUSTMENT(region_adjuster)->page_size = 0.0 ;

      window_top_str = g_strdup(TOPTEXT_NO_SCALE) ;
      window_bot_str = g_strdup(BOTTEXT_NO_SCALE) ;
#ifdef RDS_USE_SCROLL
      GTK_ADJUSTMENT(window_adjuster)->value = 0.0 ;
      GTK_ADJUSTMENT(window_adjuster)->lower = 0.0 ;
      GTK_ADJUSTMENT(window_adjuster)->upper = 0.0 ;
      GTK_ADJUSTMENT(window_adjuster)->step_increment = 0.0 ;
      GTK_ADJUSTMENT(window_adjuster)->page_increment = 0.0 ;
      GTK_ADJUSTMENT(window_adjuster)->page_size = 0.0 ;
#endif /* RDS_USE_SCROLL */
    }

  gtk_adjustment_changed(GTK_ADJUSTMENT(region_adjuster)) ;
#ifdef RDS_USE_SCROLL
  gtk_adjustment_changed(GTK_ADJUSTMENT(window_adjuster)) ;
#endif /* RDS_USE_SCROLL */
  gtk_label_set_text(GTK_LABEL(navigator->topLabel), region_top_str) ;
  gtk_label_set_text(GTK_LABEL(navigator->botLabel), region_bot_str) ;

  g_free(region_top_str) ;
  g_free(region_bot_str) ;
#ifdef RDS_USE_SCROLL
  gtk_label_set_text(GTK_LABEL(navigator->wind_top_label), window_top_str) ;
  gtk_label_set_text(GTK_LABEL(navigator->wind_bot_label), window_bot_str) ;

  g_free(window_top_str) ;
  g_free(window_bot_str) ;
#endif /* RDS_USE_SCROLL */

  return ;
}


/* Destroys a navigator instance, note there is not much to do here because we
 * assume that our caller will destroy our parent widget will in turn destroy
 * all our widgets. */
void zMapNavigatorDestroy(ZMapNavigator navigator)
{
  g_free(navigator) ;

  return ;
}



/* 
 *              Internal functions 
 */



static void valueCB(GtkAdjustment *adjustment, gpointer user_data)
{
  ZMapNavigator navigator = (ZMapNavigator)user_data ;


  //#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  printf("top: %f, bottom: %f\n", adjustment->value, adjustment->value + adjustment->page_size) ;
  //#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  if (navigator->cb_func)
    (*(navigator->cb_func))(navigator->user_data,
			    adjustment->value, adjustment->value + adjustment->page_size) ;

  return ;
}


static void paneNotifyPositionCB(GObject *pane, GParamSpec *scroll, gpointer user_data)
{
  ZMapNavigator navigator = (ZMapNavigator)user_data;
  double width = 0.0;
  int current_position = 0, new_position = 0;

  current_position = gtk_paned_get_position(GTK_PANED( navigator->pane ));

  zMapWindowNavigatorPackDimensions(navigator->locator_widget, &width, NULL);

  new_position = (int)fabs(width);

  if(current_position > new_position)
    gtk_paned_set_position(GTK_PANED( navigator->pane ),
                           new_position);

  return ;
}

