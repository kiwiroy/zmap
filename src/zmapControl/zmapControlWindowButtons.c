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
 * Description: 
 * Exported functions: See XXXXXXXXXXXXX.h
 * HISTORY:
 * Last edited: May 20 15:40 2004 (edgrif)
 * Created: Thu Jul 24 14:36:27 2003 (edgrif)
 * CVS info:   $Id: zmapControlWindowButtons.c,v 1.3 2004-05-27 13:41:29 edgrif Exp $
 *-------------------------------------------------------------------
 */

#include <string.h>
#include <ZMap_P.h>

static void newCB(GtkWidget *widget, gpointer cb_data) ;
static void loadCB(GtkWidget *widget, gpointer cb_data) ;
static void stopCB(GtkWidget *widget, gpointer cb_data) ;
static void quitCB(GtkWidget *widget, gpointer cb_data) ;


GtkWidget *zmapControlWindowMakeButtons(ZMap zmap)
{
  GtkWidget *frame ;
  GtkWidget *hbox, *new_button, *load_button, *stop_button, *quit_button ;

  frame = gtk_frame_new(NULL);
  gtk_container_border_width(GTK_CONTAINER(frame), 5);

  hbox = gtk_hbox_new(FALSE, 0) ;
  gtk_container_border_width(GTK_CONTAINER(hbox), 5);
  gtk_container_add(GTK_CONTAINER(frame), hbox);

  load_button = gtk_button_new_with_label("Load") ;
  gtk_signal_connect(GTK_OBJECT(load_button), "clicked",
		     GTK_SIGNAL_FUNC(loadCB), (gpointer)zmap) ;
  gtk_box_pack_start(GTK_BOX(hbox), load_button, FALSE, FALSE, 0) ;

  stop_button = gtk_button_new_with_label("Stop") ;
  gtk_signal_connect(GTK_OBJECT(stop_button), "clicked",
		     GTK_SIGNAL_FUNC(stopCB), (gpointer)zmap) ;
  gtk_box_pack_start(GTK_BOX(hbox), stop_button, FALSE, FALSE, 0) ;

  new_button = gtk_button_new_with_label("New") ;
  gtk_signal_connect(GTK_OBJECT(new_button), "clicked",
		     GTK_SIGNAL_FUNC(newCB), (gpointer)zmap) ;
  gtk_box_pack_start(GTK_BOX(hbox), new_button, FALSE, FALSE, 0) ;

  quit_button = gtk_button_new_with_label("Quit") ;
  gtk_signal_connect(GTK_OBJECT(quit_button), "clicked",
		     GTK_SIGNAL_FUNC(quitCB), (gpointer)zmap) ;
  gtk_box_pack_start(GTK_BOX(hbox), quit_button, FALSE, FALSE, 0) ;

  /* Make Load button the default, its probably what user wants to do mostly. */
  GTK_WIDGET_SET_FLAGS(load_button, GTK_CAN_DEFAULT) ;
  gtk_window_set_default(GTK_WINDOW(zmap->toplevel), load_button) ;


  return frame ;
}




/*
 *  ------------------- Internal functions -------------------
 */

/* These callbacks simple make calls to routines in zmapControl.c, this is because I want all
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

static void newCB(GtkWidget *widget, gpointer cb_data)
{
  ZMap zmap = (ZMap)cb_data ;

  /* The string is for testing only, in the full code there should be a dialog box that enables
   * the user to provide a whole load of info. */
  zmapControlNewCB(zmap, "eds_sequence") ;

  return ;
}

static void quitCB(GtkWidget *widget, gpointer cb_data)
{
  ZMap zmap = (ZMap)cb_data ;

  zmapControlTopLevelKillCB(zmap) ;

  return ;
}

