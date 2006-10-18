/*  File: zmapControlWindowMenubar.c
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
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk and
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk
 *
 * Description: Code for the menubar in a zmap window.
 *              
 *              NOTE that this code uses the itemfactory which is
 *              deprecated in GTK 2, BUT to replace it means
 *              understanding their UI manager...we need to do this
 *              but its quite a lot of work...
 *              
 * Exported functions: See zmapControl_P.h
 * HISTORY:
 * Last edited: Oct 18 11:32 2006 (edgrif)
 * Created: Thu Jul 24 14:36:59 2003 (edgrif)
 * CVS info:   $Id: zmapControlWindowMenubar.c,v 1.17 2006-10-18 13:36:54 edgrif Exp $
 *-------------------------------------------------------------------
 */

#include <stdlib.h>
#include <stdio.h>

#include <ZMap/zmapUtilsGUI.h>
#include <ZMap/zmapWebPages.h>
#include <zmapControl_P.h>



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






static void newCB(gpointer cb_data, guint callback_action, GtkWidget *w) ;
static void closeCB(gpointer cb_data, guint callback_action, GtkWidget *w) ;
static void quitCB(gpointer cb_data, guint callback_action, GtkWidget *w) ;
static void exportCB(gpointer cb_data, guint callback_action, GtkWidget *w);
static void printCB(gpointer cb_data, guint callback_action, GtkWidget *w);
static void dumpCB(gpointer cb_data, guint callback_action, GtkWidget *w);
static void redrawCB(gpointer cb_data, guint callback_action, GtkWidget *w);
static void aboutCB(gpointer cb_data, guint callback_action, GtkWidget *w);
static void releaseNotesCB(gpointer cb_data, guint callback_action, GtkWidget *w);
static void print_hello( gpointer data, guint callback_action, GtkWidget *w ) ;


GtkItemFactory *item_factory;


static GtkItemFactoryEntry menu_items[] = {
 { "/_File",         NULL,         NULL, 0, "<Branch>" },
 { "/File/_New",     "<control>N", newCB, 2, NULL },

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
 { "/File/sep1",     NULL,         NULL, 0, "<Separator>" },
 { "/File/_Export",  "<control>E", exportCB, 0, NULL },
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

 { "/File/sep1",     NULL,         NULL, 0, "<Separator>" },
 { "/File/_Save screen shot",    "<control>D", dumpCB, 0, NULL },
 { "/File/_Print screen shot",   "<control>P", printCB, 0, NULL },
 { "/File/sep1",     NULL,         NULL, 0, "<Separator>" },
 { "/File/Close",    "<control>W", closeCB, 0, NULL },
 { "/File/Quit",     "<control>Q", quitCB, 0, NULL },
 { "/_Edit",         NULL,         NULL, 0, "<Branch>" },
 { "/Edit/Cu_t",     "<control>X", print_hello, 0, NULL },
 { "/Edit/_Copy",    "<control>C", print_hello, 0, NULL },
 { "/Edit/_Paste",   "<control>V", print_hello, 0, NULL },
 { "/Edit/_Redraw",  NULL,         redrawCB, 0, NULL },
 { "/_Help",         NULL,         NULL, 0, "<LastBranch>" },
 { "/Help/About",    NULL,         aboutCB, 0, NULL },
 { "/Help/Release Notes", NULL,    releaseNotesCB, 0, NULL },
};




GtkWidget *zmapControlWindowMakeMenuBar(ZMap zmap)
{
  GtkWidget *menubar ;
  GtkAccelGroup *accel_group ;
  gint nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]) ;

  accel_group = gtk_accel_group_new() ;

  item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel_group) ;

  gtk_item_factory_create_items(item_factory, nmenu_items, menu_items, (gpointer)zmap) ;

  gtk_window_add_accel_group(GTK_WINDOW(zmap->toplevel), accel_group) ;

  menubar = gtk_item_factory_get_widget (item_factory, "<main>") ;

  return menubar ;
}



/* Should pop up a dialog box to ask for a file name....e.g. the file chooser. */
static void exportCB(gpointer cb_data, guint callback_action, GtkWidget *window)
{
  ZMap zmap = (ZMap)cb_data ;
  gchar *file = "ZMap.features" ;

  zmapViewFeatureDump(zmap->focus_viewwindow, file) ;

  return ;
}


static void dumpCB(gpointer cb_data, guint callback_action, GtkWidget *widget)
{
  ZMap zmap = (ZMap)cb_data ;
  ZMapWindow window ;

  window = zMapViewGetWindow(zmap->focus_viewwindow) ;

  zMapWindowDump(window) ;

  return ;
}


static void printCB(gpointer cb_data, guint callback_action, GtkWidget *widget)
{
  ZMap zmap = (ZMap)cb_data ;
  ZMapWindow window ;

  window = zMapViewGetWindow(zmap->focus_viewwindow) ;

  zMapWindowPrint(window) ;

  return ;
}


/* Causes currently focussed zmap window to redraw itself. */
static void redrawCB(gpointer cb_data, guint callback_action, GtkWidget *window)
{
  ZMap zmap = (ZMap)cb_data ;

  zMapViewRedraw(zmap->focus_viewwindow) ;

  return ;
}



/* Show the usual tedious "About" dialog. */
static void aboutCB(gpointer cb_data, guint callback_action, GtkWidget *window)
{

  zMapGUIShowAbout() ;

  return ;
}


/* Show the web page of release notes. */
static void releaseNotesCB(gpointer cb_data, guint callback_action, GtkWidget *window)
{
  char *web_page = ZMAPWEB_URL "/" ZMAPWEB_RELEASE_NOTES_DIR "/" ZMAPWEB_RELEASE_NOTES ;
  gboolean result ;
  GError *error = NULL ;

  if (!(result = zMapLaunchWebBrowser(web_page, &error)))
    {
      zMapWarning("Error: %s\n", error->message) ;
      
      g_error_free(error) ;
    }

  return ;
}



/* Close just this zmap... */
static void closeCB(gpointer cb_data, guint callback_action, GtkWidget *w)
{
  ZMap zmap = (ZMap)cb_data ;

  zmapControlTopLevelKillCB(zmap) ;

  return ;
}



/* Kill the whole zmap application. */
static void quitCB(gpointer cb_data, guint callback_action, GtkWidget *w)
{
  ZMap zmap = (ZMap)cb_data ;

  /* Call the application exit callback to get everything killed...including this zmap. */
  (*(zmap->zmap_cbs_G->exit))(zmap, zmap->app_data) ;

  return ;
}



static void print_hello( gpointer data, guint callback_action, GtkWidget *w )
{

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	GtkWidget *myWidget;
	printf( "widget is %x data is %s\n", w, data );
	g_message ("Hello, World!\n");

	myWidget = gtk_item_factory_get_widget (item_factory, "/File/New");
	printf( "File/New is %x\n", myWidget );

	gtk_item_factory_delete_item( item_factory, "/Edit" );
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

}

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
static void handle_option( gpointer data, guint callback_action, GtkWidget *w )
{

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	GtkCheckMenuItem *checkMenuItem = (GtkCheckMenuItem *) w;

	printf( "widget is %x data is %s\n", w, data );
	g_message ("Hello, World!\n");
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */






/* Load a new sequence into a zmap. */
static void newCB(gpointer cb_data, guint callback_action, GtkWidget *w)
{
  ZMap zmap = (ZMap)cb_data ;
  char *new_sequence ;
  ZMapView view ;

  /* these should be passed in ...... */
  int start = 1, end = 0 ;

  /* Get a new sequence to show.... */
  if ((new_sequence = getSequenceName()))
    {
      if ((view = zmapControlAddView(zmap, new_sequence, start, end)))
	zMapViewConnect(view, NULL) ;				    /* return code ???? */
    }

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


