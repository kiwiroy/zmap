/*  File: zmapWindow.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) Sanger Institute, 2003
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
 * and was written by
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk,
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk and
 *	Simon Kelley (Sanger Institute, UK) srk@sanger.ac.uk
 *
 * Description: Implement the buttons on the zmap.
 *              
 * Exported functions: See zmapControl_P.h
 * HISTORY:
 * Last edited: Dec 20 10:56 2004 (edgrif)
 * Created: Thu Jul 24 14:36:27 2003 (edgrif)
 * CVS info:   $Id: zmapControlWindowButtons.c,v 1.21 2004-12-20 10:57:41 edgrif Exp $
 *-------------------------------------------------------------------
 */

#include <string.h>
#include <ZMap/zmapWindow.h>
#include <zmapControl_P.h>



static void newCB(GtkWidget *widget, gpointer cb_data) ;
static void loadCB(GtkWidget *widget, gpointer cb_data) ;
static void stopCB(GtkWidget *widget, gpointer cb_data) ;
static void zoomInCB(GtkWindow *widget, gpointer cb_data) ;
static void zoomOutCB(GtkWindow *widget, gpointer cb_data) ;
static void splitPaneCB(GtkWidget *widget, gpointer data) ;
static void splitHPaneCB(GtkWidget *widget, gpointer data) ;
static void quitCB(GtkWidget *widget, gpointer cb_data) ;

/* This lot may need to go into a separate file sometime to give more general purpose dialog code. */
static char *getSequenceName(void) ;
static void okCB(GtkWidget *widget, gpointer cb_data) ;
static void cancelCB(GtkWidget *widget, gpointer cb_data) ;
typedef struct
{
  GtkWidget *dialog ;
  GtkWidget *entry ;
  char *new_sequence ;
} ZmapSequenceCallbackStruct, *ZmapSequenceCallback ;





GtkWidget *zmapControlWindowMakeButtons(ZMap zmap)
{
  GtkWidget *hbox, *new_button,
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
    *load_button,
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
    *stop_button, *quit_button,
    *hsplit_button, *vsplit_button, *zoomin_button, *zoomout_button,
    *close_button ;

  hbox = gtk_hbox_new(FALSE, 0) ;
  gtk_container_border_width(GTK_CONTAINER(hbox), 5);

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* Disabled for now, need to do work on view... */

  load_button = gtk_button_new_with_label("Load") ;
  gtk_signal_connect(GTK_OBJECT(load_button), "clicked",
		     GTK_SIGNAL_FUNC(loadCB), (gpointer)zmap) ;
  gtk_box_pack_start(GTK_BOX(hbox), load_button, FALSE, FALSE, 0) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  stop_button = gtk_button_new_with_label("Stop") ;
  gtk_signal_connect(GTK_OBJECT(stop_button), "clicked",
		     GTK_SIGNAL_FUNC(stopCB), (gpointer)zmap) ;
  gtk_box_pack_start(GTK_BOX(hbox), stop_button, FALSE, FALSE, 0) ;
  /*  commented out until we make it work properly.
   *  new_button = gtk_button_new_with_label("New") ;
   *  gtk_signal_connect(GTK_OBJECT(new_button), "clicked",
   *		     GTK_SIGNAL_FUNC(newCB), (gpointer)zmap) ;
   *  gtk_box_pack_start(GTK_BOX(hbox), new_button, FALSE, FALSE, 0) ;
  */
  hsplit_button = gtk_button_new_with_label("H-Split");
  gtk_signal_connect(GTK_OBJECT(hsplit_button), "clicked",
		     GTK_SIGNAL_FUNC(splitPaneCB), (gpointer)zmap) ;
  gtk_box_pack_start(GTK_BOX(hbox), hsplit_button, FALSE, FALSE, 0) ;

  vsplit_button = gtk_button_new_with_label("V-Split");
  gtk_signal_connect(GTK_OBJECT(vsplit_button), "clicked",
		     GTK_SIGNAL_FUNC(splitHPaneCB), (gpointer)zmap) ;
  gtk_box_pack_start(GTK_BOX(hbox), vsplit_button, FALSE, FALSE, 0) ;
                                                                                           
  zmap->zoomin_but = zoomin_button = gtk_button_new_with_label("Zoom In");
  gtk_signal_connect(GTK_OBJECT(zoomin_button), "clicked",
		     GTK_SIGNAL_FUNC(zoomInCB), (gpointer)zmap);
  gtk_box_pack_start(GTK_BOX(hbox), zoomin_button, FALSE, FALSE, 0) ;
                                                                                           
  zmap->zoomout_but = zoomout_button = gtk_button_new_with_label("Zoom Out");
  gtk_signal_connect(GTK_OBJECT(zoomout_button), "clicked",
		     GTK_SIGNAL_FUNC(zoomOutCB), (gpointer)zmap);
  gtk_box_pack_start(GTK_BOX(hbox), zoomout_button, FALSE, FALSE, 0) ;

  zmap->close_but = close_button = gtk_button_new_with_label("Close") ;
  gtk_signal_connect(GTK_OBJECT(close_button), "clicked",
		     GTK_SIGNAL_FUNC(closePane), (gpointer)zmap) ;
  gtk_box_pack_start(GTK_BOX(hbox), close_button, FALSE, FALSE, 0) ;
  gtk_widget_set_sensitive(close_button, FALSE);

  quit_button = gtk_button_new_with_label("Quit") ;
  gtk_signal_connect(GTK_OBJECT(quit_button), "clicked",
		     GTK_SIGNAL_FUNC(quitCB), (gpointer)zmap) ;
  gtk_box_pack_start(GTK_BOX(hbox), quit_button, FALSE, FALSE, 0) ;

  /* Make Quit button the default, its probably what user wants to do mostly. */
  GTK_WIDGET_SET_FLAGS(quit_button, GTK_CAN_DEFAULT) ;
  gtk_window_set_default(GTK_WINDOW(zmap->toplevel), quit_button) ;

  return hbox ;
}


/* We make an assumption in this routine that zoom will only be one of fixed, min, mid or max and
 * that zoom can only go from min to max _via_ the mid state. */
void zmapControlWindowDoTheZoom(ZMap zmap, double zoom)
{
  ZMapPane pane = zmap->focuspane ;

  zMapWindowZoom(zMapViewGetWindow(pane->curr_view_window), zoom) ;

  return ;
}


/* Provide user with visual feedback about status of zooming via the sensitivity of
 * the zoom buttons. */
void zmapControlWindowSetZoomButtons(ZMap zmap, ZMapWindowZoomStatus zoom_status)
{


  if (zoom_status == ZMAP_ZOOM_FIXED)
    {
      if (GTK_WIDGET_IS_SENSITIVE(zmap->zoomin_but))
	gtk_widget_set_sensitive(zmap->zoomin_but, FALSE) ;
      
      if (GTK_WIDGET_IS_SENSITIVE(zmap->zoomout_but))
	gtk_widget_set_sensitive(zmap->zoomout_but, FALSE) ;
    }
  else if (zoom_status == ZMAP_ZOOM_MIN && GTK_WIDGET_IS_SENSITIVE(zmap->zoomout_but))
    gtk_widget_set_sensitive(zmap->zoomout_but, FALSE) ;
  else if (zoom_status == ZMAP_ZOOM_MID)
    {
      if (!GTK_WIDGET_IS_SENSITIVE(zmap->zoomin_but))
	gtk_widget_set_sensitive(zmap->zoomin_but, TRUE) ;

      if (!GTK_WIDGET_IS_SENSITIVE(zmap->zoomout_but))
	gtk_widget_set_sensitive(zmap->zoomout_but, TRUE) ;
    }
  else if (zoom_status == ZMAP_ZOOM_MAX && GTK_WIDGET_IS_SENSITIVE(zmap->zoomin_but))
    gtk_widget_set_sensitive(zmap->zoomin_but, FALSE) ;


  return ;
}


/*
 *  ------------------- Internal functions -------------------
 */


/* These callbacks simply make calls to routines in zmapControl.c, this is because I want all
 * the state handling etc. to be in one file so that its easier to work on. */
static void loadCB(GtkWidget *widget, gpointer cb_data)
{
  ZMap zmap = (ZMap)cb_data ;

  zmapControlLoadCB(zmap) ;

  return ;
}

static void stopCB(GtkWidget *widget, gpointer cb_data)
{
  ZMap zmap = (ZMap)cb_data ;

  zmapControlResetCB(zmap) ;

  return ;
}


/* Load a new sequence into a zmap. */
static void newCB(GtkWidget *widget, gpointer cb_data)
{
  ZMap zmap = (ZMap)cb_data ;
  char *new_sequence ;

  /* Get a new sequence to show.... */
  if ((new_sequence = getSequenceName()))
    zmapControlNewViewCB(zmap, new_sequence) ;

  return ;
}






static void quitCB(GtkWidget *widget, gpointer cb_data)
{
  ZMap zmap = (ZMap)cb_data ;

  zmapControlTopLevelKillCB(zmap) ;

  return ;
}


static void zoomInCB(GtkWindow *widget, gpointer cb_data)
{
  ZMap zmap = (ZMap)cb_data ;

  zmapControlWindowDoTheZoom(zmap, 2.0) ;

  return ;
}


static void zoomOutCB(GtkWindow *widget, gpointer cb_data)
{
  ZMap zmap = (ZMap)cb_data ;

  zmapControlWindowDoTheZoom(zmap, 0.5) ;

  return ;
}



static void splitPaneCB(GtkWidget *widget, gpointer data)
{
  ZMap zmap = (ZMap)data ;
  ZMapPane pane ;
  GtkWidget *parent_widget = NULL ;
  ZMapViewWindow view_window ;

  /* this all needs to do view stuff here and return a parent widget............ */
  parent_widget = splitPane(zmap) ;

  pane = zmap->focuspane ;

  view_window = zMapViewAddWindow(zMapViewGetView(pane->curr_view_window), parent_widget,
				  zMapViewGetWindow(zmap->focuspane->curr_view_window)) ;

  pane->curr_view_window = view_window ;		    /* new focus window ?? */

  /* We'll need to update the display..... */
  gtk_widget_show_all(zmap->toplevel) ;

  return ;
}


static void splitHPaneCB(GtkWidget *widget, gpointer data)
{
  ZMap zmap = (ZMap)data ;
  ZMapPane pane ;
  GtkWidget *parent_widget = NULL ;
  ZMapViewWindow view_window ;

  /* this all needs to do view stuff here and return a parent widget............ */
  parent_widget = splitHPane(zmap) ;

  pane = zmap->focuspane ;

  view_window = zMapViewAddWindow(zMapViewGetView(pane->curr_view_window), parent_widget,
				  zMapViewGetWindow(zmap->focuspane->curr_view_window)) ;

  pane->curr_view_window = view_window ;		    /* new focus window ?? */

  /* We'll need to update the display..... */
  gtk_widget_show_all(zmap->toplevel) ;

  return ;
}



static char *getSequenceName(void)
{
  char *sequence_name = NULL ;
  GtkWidget *dialog, *label, *text, *ok_button, *cancel_button ;
  ZmapSequenceCallbackStruct cb_data = {NULL, NULL} ;

  cb_data.dialog = dialog = gtk_dialog_new() ;
 
  gtk_window_set_title(GTK_WINDOW(dialog), "ZMap - Enter New Sequence") ;

  label = gtk_label_new("Sequence Name:") ;
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, TRUE, TRUE, 0) ;

  cb_data.entry = text = gtk_entry_new() ;
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), text, TRUE, TRUE, 0) ;

  ok_button = gtk_button_new_with_label("OK") ;
  GTK_WIDGET_SET_FLAGS(ok_button, GTK_CAN_DEFAULT) ;
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area), ok_button, FALSE, FALSE, 0) ;
  gtk_widget_grab_default(ok_button) ;
  gtk_signal_connect(GTK_OBJECT(ok_button), "clicked", GTK_SIGNAL_FUNC(okCB), &cb_data) ;

  cancel_button = gtk_button_new_with_label("Cancel") ;
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area), cancel_button, FALSE, FALSE, 0) ;
  gtk_signal_connect(GTK_OBJECT(cancel_button), "clicked", GTK_SIGNAL_FUNC(cancelCB), &cb_data) ;

  gtk_widget_show_all(dialog) ;

  gtk_main() ;

  sequence_name = cb_data.new_sequence ;

  return sequence_name ;
}


static void okCB(GtkWidget *widget, gpointer cb_data)
{
  ZmapSequenceCallback mydata = (ZmapSequenceCallback)cb_data ;
  char *text ;

  if ((text = (char *)gtk_entry_get_text(GTK_ENTRY(mydata->entry)))
      && *text)						    /* entry returns "" for no string. */
    mydata->new_sequence = g_strdup(text) ;

  gtk_widget_destroy(mydata->dialog) ;

  gtk_main_quit() ;

  return ;
}

static void cancelCB(GtkWidget *widget, gpointer cb_data)
{
  ZmapSequenceCallback mydata = (ZmapSequenceCallback)cb_data ;

  gtk_widget_destroy(mydata->dialog) ;

  gtk_main_quit() ;

  return ;
}
