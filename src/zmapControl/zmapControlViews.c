/*  File: zmapControlSplit.c
 *  Author: Rob Clack (rnc@sanger.ac.uk)
 *  Copyright (c) Sanger Institute, 2005
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
 * Description: Splits the zmap window to show either the same view twice
 *              or two different views.
 *              
 *              This is a complete rewrite of the original.
 *              
 * Exported functions: See zmapControl.h
 * HISTORY:
 * Last edited: Feb  1 09:30 2005 (edgrif)
 * Created: Mon Jan 10 10:38:43 2005 (edgrif)
 * CVS info:   $Id: zmapControlViews.c,v 1.3 2005-02-02 11:03:49 edgrif Exp $
 *-------------------------------------------------------------------
 */
 
#include <zmapControl_P.h>
#include <ZMap/zmapUtilsDebug.h>


/* Used to record which child we are of a pane widget. */
typedef enum {ZMAP_PANE_NONE, ZMAP_PANE_CHILD_1, ZMAP_PANE_CHILD_2} ZMapPaneChild ;


/* Used in doing a reverse lookup for the ZMapViewWindow -> container widget hash. */
typedef struct
{
  GtkWidget *widget ;
  ZMapViewWindow view_window ;
} ZMapControlWidget2ViewwindowStruct ;



/* GTK didn't introduce a function call to get at pane children until at least release 2.6,
 * I hope this is correct, no easy way to check exactly when function calls were introduced. */
#if ((GTK_MAJOR_VERSION == 2) && (GTK_MINOR_VERSION == 6))
#define myGetChild(WIDGET, CHILD_NUMBER)         \
  (gtk_paned_get_child##CHILD_NUMBER(GTK_PANED(WIDGET)))
#else
#define myGetChild(WIDGET, CHILD_NUMBER)         \
  ((GTK_PANED(WIDGET))->child##CHILD_NUMBER)
#endif


static ZMapPaneChild whichChildOfPane(GtkWidget *child) ;
static void splitPane(GtkWidget *curr_frame, GtkWidget *new_frame, GtkOrientation orientation) ;
static GtkWidget *closePane(GtkWidget *close_frame) ;

static ZMapViewWindow widget2ViewWindow(GHashTable* hash_table, GtkWidget *widget) ;
static void findViewWindow(gpointer key, gpointer value, gpointer user_data) ;




/* Not a great name as it may not split and orientation may be ignored..... */
void zmapControlSplitInsertWindow(ZMap zmap, ZMapView new_view, GtkOrientation orientation)
{
  GtkWidget *curr_container, *view_container ;
  ZMapViewWindow view_window ;
  ZMapView zmap_view ;
  ZMapWindow zmap_window ;

  /* If there is a focus window then that will be the one we split and we need to find out
   * the container parent of that canvas. */
  if (zmap->focus_viewwindow)
    curr_container = g_hash_table_lookup(zmap->viewwindow_2_parent, zmap->focus_viewwindow) ;
  else
    curr_container = NULL ;

  /* If we are adding a new view then there won't yet be a window for it, if we are splitting
   * an existing view then if it has a window we need to split that, otherwise we need to add
   * the first window to that view. */
  zmap_window = NULL ;
  if (new_view)
    {
      zmap_view = new_view ;
    }
  else
    {
      if (zmap->focus_viewwindow)
	{
	  zmap_view = zMapViewGetView(zmap->focus_viewwindow) ;
	  zmap_window = zMapViewGetWindow(zmap->focus_viewwindow) ;
	}
      else
	{
	  /* UGH, I don't like this, seems a bit addhoc to just grab the only view.... */
	  zMapAssert((g_list_length(zmap->view_list) == 1)) ;
	  zmap_view = (ZMapView)(g_list_first(zmap->view_list)->data) ;
	}
    }

  /* Add a new container that will hold the new view window. */
  view_container = zmapControlAddWindow(zmap, curr_container, orientation) ;

  /* Either add a completely new view window to the container or copy an existing view
   * window into it. */
  if (!zmap_window)
    view_window = zMapViewMakeWindow(zmap_view, view_container) ;
  else
    view_window = zMapViewCopyWindow(zmap_view, view_container, zmap_window) ;


  /* Add to hash of viewwindows to frames */
  g_hash_table_insert(zmap->viewwindow_2_parent, view_window, view_container) ;


  /* If there is no current focus window we need to make this new one the focus,
   * if we don't of code will fail because it relies on having a focus window and
   * the user will not get visual feedback that a window is the focus window. */
  if (!zmap->focus_viewwindow)
    zmapControlSetWindowFocus(zmap, view_window) ;

  /* We'll need to update the display..... */
  gtk_widget_show_all(zmap->toplevel) ;

  return ;
}



void zmapControlRemoveWindow(ZMap zmap)
{
  GtkWidget *close_container ;
  ZMapViewWindow view_window ;
  gboolean remove ;

  /* We shouldn't get called if there are no windows. */
  zMapAssert((g_list_length(zmap->view_list) >= 1) && zmap->focus_viewwindow) ;

  view_window = zmap->focus_viewwindow ;		    /* focus_viewwindow gets reset so hang
							       on view_window.*/

  /* It may be that the window to be removed was also due to be unfocussed so it needs to be
   * removed from the unfocus slot. */
  if (view_window == zmap->unfocus_viewwindow)
    zmap->unfocus_viewwindow = NULL ;

  close_container = g_hash_table_lookup(zmap->viewwindow_2_parent, view_window) ;

  zMapViewRemoveWindow(view_window) ;

  /* this needs to remove the pane.....AND  set a new focuspane.... */
  zmapControlCloseWindow(zmap, close_container) ;

  /* Remove from hash of viewwindows to frames */
  remove = g_hash_table_remove(zmap->viewwindow_2_parent, view_window) ;
  zMapAssert(remove) ;

  /* Having removed one window we need to refocus on another, if there is one....... */
  if (zmap->focus_viewwindow)
    zmapControlSetWindowFocus(zmap, zmap->focus_viewwindow) ;

  /* We'll need to update the display..... */
  gtk_widget_show_all(zmap->toplevel) ;

  return ;
}




/* 
 * view_window is the view to add, this must be supplied.
 * orientation is GTK_ORIENTATION_HORIZONTAL or GTK_ORIENTATION_VERTICAL
 * 
 *  */
GtkWidget *zmapControlAddWindow(ZMap zmap, GtkWidget *curr_frame, GtkOrientation orientation)
{
  GtkWidget *new_frame ;

  /* We could put the sequence name as the label ??? Maybe good to do...think about it.... */
  new_frame = gtk_frame_new(NULL) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* Making the border huge does _not_ make the red bit huge when we highlight so that's not
   * much good..... */

  gtk_container_set_border_width(GTK_CONTAINER(new_frame), 30) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  /* If there is a parent then add this pane as child two of parent, otherwise it means
   * this is the first pane and it it just gets added to the zmap vbox. */
  if (curr_frame)
    {
      /* Here we want to split the existing pane etc....... */
      splitPane(curr_frame, new_frame, orientation) ;
    }
  else
    gtk_box_pack_start(GTK_BOX(zmap->pane_vbox), new_frame, TRUE, TRUE, 0) ;

  return new_frame ;
}



void zmapControlCloseWindow(ZMap zmap, GtkWidget *close_container)
{
  GtkWidget *pane_parent ;

  /* If parent is a pane then we need to remove that pane, otherwise we simply destroy the
   * container. */
  pane_parent = gtk_widget_get_parent(close_container) ;
  if (GTK_IS_PANED(pane_parent))
    {
      GtkWidget *keep_container ;

      keep_container = closePane(close_container) ;

      /* Set the focus to the window left over. */
      zmap->focus_viewwindow = widget2ViewWindow(zmap->viewwindow_2_parent, keep_container) ;
    }
  else
    {
      gtk_widget_destroy(close_container) ;
      zmap->focus_viewwindow = NULL ;
    }

  return ;
}

static ZMapViewWindow widget2ViewWindow(GHashTable* hash_table, GtkWidget *widget)
{
  ZMapControlWidget2ViewwindowStruct widg2view ;

  widg2view.widget = widget ;
  widg2view.view_window = NULL ;

  g_hash_table_foreach(hash_table, findViewWindow, &widg2view) ;

  return widg2view.view_window ;
}


/* Test for value == user_data, i.e. have we found the widget given by user_data ? */
static void findViewWindow(gpointer key, gpointer value, gpointer user_data)
{
  ZMapControlWidget2ViewwindowStruct *widg2View = (ZMapControlWidget2ViewwindowStruct *)user_data ;

  if (value == widg2View->widget)
    {
      widg2View->view_window = key ;
    }

  return ;
}



/* Split a pane, actually we add a new pane into the selected window of the parent pane. */
static void splitPane(GtkWidget *curr_frame, GtkWidget *new_frame, GtkOrientation orientation)
{
  GtkWidget *pane_parent, *new_pane ;
  ZMapPaneChild curr_child = ZMAP_PANE_NONE ;


  /* Get current frames parent, if window is unsplit it will be a vbox, otherwise its a pane
   * and we need to know which child we are of the pane. */
  pane_parent = gtk_widget_get_parent(curr_frame) ;
  if (GTK_IS_PANED(pane_parent))
    {
      /* Which child are we of the parent pane ? */
      curr_child = whichChildOfPane(curr_frame) ;
    }


  /* Remove the current frame from its container so we can insert a new container as the child
   * of that container, we have to increase its reference counter to stop it being.
   * destroyed once its removed from its container. */
  curr_frame = gtk_widget_ref(curr_frame) ;
  gtk_container_remove(GTK_CONTAINER(pane_parent), curr_frame) ;

  /* Create the new pane, note that horizontal split => splitting the pane across the middle,
   * vertical split => splitting the pane down the middle. */
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    new_pane = gtk_vpaned_new() ;
  else
    new_pane = gtk_hpaned_new() ;

  /* Now insert the new pane into the vbox or pane parent. */
  if (!GTK_IS_PANED(pane_parent))
    {
      gtk_box_pack_start(GTK_BOX(pane_parent), new_pane, TRUE, TRUE, 0) ;
    }
  else
    {
      if (curr_child == ZMAP_PANE_CHILD_1)
	gtk_paned_pack1(GTK_PANED(pane_parent), new_pane, TRUE, TRUE) ;
      else
	gtk_paned_pack2(GTK_PANED(pane_parent), new_pane, TRUE, TRUE) ;
    }


  /* Add the frame views to the new pane. */
  gtk_paned_pack1(GTK_PANED(new_pane), curr_frame, TRUE, TRUE);
  gtk_paned_pack2(GTK_PANED(new_pane), new_frame, TRUE, TRUE);

  /* Now dereference the original frame as its now back in a container. */
  gtk_widget_unref(curr_frame) ;

  return ;
}



/* Close the container in one half of a pane, reparent the container in the other half into
 * the that panes parent, get rid of the original pane.
 * Returns a new/current direct container of a view. */
static GtkWidget *closePane(GtkWidget *close_frame)
{
  GtkWidget *keep_container, *keep_frame = NULL ;
  GtkWidget *parent_pane, *parents_parent ;
  ZMapPaneChild close_child, parent_parent_child ;

  parent_pane = gtk_widget_get_parent(close_frame) ;
  zMapAssert(GTK_IS_PANED(parent_pane)) ;

  /* Find out which child of the pane the close_frame is and hence record the container
   * (n.b. might be another frame or might be a pane containing more panes/frames)
   * frame we want to keep. */
  close_child = whichChildOfPane(close_frame) ;
  if (close_child == ZMAP_PANE_CHILD_1)
    keep_container = myGetChild(parent_pane, 2) ;
  else
    keep_container = myGetChild(parent_pane, 1) ;


  /* Remove the keep_container from its container, we will insert it into the place where
   * its parent was originally. */
  keep_container = gtk_widget_ref(keep_container) ;
  zMapAssert(GTK_IS_PANED(keep_container) || GTK_IS_FRAME(keep_container)) ;
  gtk_container_remove(GTK_CONTAINER(parent_pane), keep_container) ;


  /* Find out what the parent of the parent_pane is, if its not a pane then we simply insert
   * keep_container into it, if it is a pane, we need to know which child of it our parent_pane
   * is so we insert the keep_container in the correct place. */
  parents_parent = gtk_widget_get_parent(parent_pane) ;
  if (GTK_IS_PANED(parents_parent))
    {
      parent_parent_child = whichChildOfPane(parent_pane) ;
    }


  /* Destroy the parent_pane, this will also destroy the close_frame as it is still a child. */
  gtk_widget_destroy(parent_pane) ;

  /* Put the keep_container into the parent_parent. */
  if (!GTK_IS_PANED(parents_parent))
    {
      gtk_box_pack_start(GTK_BOX(parents_parent), keep_container, TRUE, TRUE, 0) ;
    }
  else
    {
      if (parent_parent_child == ZMAP_PANE_CHILD_1)
	gtk_paned_pack1(GTK_PANED(parents_parent), keep_container, TRUE, TRUE) ;
      else
	gtk_paned_pack2(GTK_PANED(parents_parent), keep_container, TRUE, TRUE) ;
    }

  /* Now dereference the keep_container as its now back in a container. */
  gtk_widget_unref(keep_container) ;

  /* The keep_container may be a frame _but_ it may also be a pane and its children may be
   * panes so we have to go down until we find a child that is a frame to return as the
   * new current frame. (Note that we arbitrarily go down the child1 children until we find
   * a frame.) */
  while (GTK_IS_PANED(keep_container))
    {
      keep_container = myGetChild(keep_container, 1) ;
    }
  keep_frame = keep_container ;

  return keep_frame ;
}



/* The enter and leave stuff is slightly convoluted in that we don't want to unfocus
 * a window just because the pointer leaves it. We only want to unfocus a previous
 * window when we enter a different view window. Hence in the leave callback we just
 * record a window to be unfocussed if we subsequently enter a different view window. */
void zmapControlSetWindowFocus(ZMap zmap, ZMapViewWindow new_viewwindow)
{
  GtkWidget *viewwindow_frame ;
  ZMapView view ;
  ZMapWindow window ;
  ZMapWindowZoomStatus zoom_status ;
  GdkColor color ;
  double top, bottom ;

  view = zMapViewGetView(new_viewwindow) ;
  window = zMapViewGetWindow(new_viewwindow) ;

  if (zmap->unfocus_viewwindow)
    {
      GtkWidget *unfocus_frame ;

      unfocus_frame = g_hash_table_lookup(zmap->viewwindow_2_parent, zmap->unfocus_viewwindow) ;

      gtk_frame_set_shadow_type(GTK_FRAME(unfocus_frame), GTK_SHADOW_OUT) ;

      gtk_widget_modify_bg(GTK_WIDGET(unfocus_frame), GTK_STATE_NORMAL, NULL) ;
      gtk_widget_modify_fg(GTK_WIDGET(unfocus_frame), GTK_STATE_NORMAL, NULL) ;

      zmap->unfocus_viewwindow = NULL ;
    }

  zmap->focus_viewwindow = new_viewwindow ;

  viewwindow_frame = g_hash_table_lookup(zmap->viewwindow_2_parent, zmap->focus_viewwindow) ;

  gtk_frame_set_shadow_type(GTK_FRAME(viewwindow_frame), GTK_SHADOW_IN);

  gdk_color_parse("red", &color);
  gtk_widget_modify_bg(GTK_WIDGET(viewwindow_frame), GTK_STATE_NORMAL, &color);
  gtk_widget_modify_fg(GTK_WIDGET(viewwindow_frame), GTK_STATE_NORMAL, &color);


  /* make sure the zoom buttons are appropriately sensitised for this window. */
  zoom_status = zMapWindowGetZoomStatus(window) ;
  zmapControlWindowSetZoomButtons(zmap, zoom_status) ;
      


  /* NOTE HOW NONE OF THE NAVIGATOR STUFF IS SET HERE....BUT NEEDS TO BE.... */
  zMapWindowGetVisible(window, &top, &bottom) ;
  zMapNavigatorSetView(zmap->navigator, zMapViewGetFeatures(view), top, bottom) ;

  return ;
}


/* When we get notified that the pointer has left a window, mark that window to
 * be unfocussed if we subsequently enter another view window. */
void zmapControlUnSetWindowFocus(ZMap zmap, ZMapViewWindow new_viewwindow)
{
  zmap->unfocus_viewwindow = new_viewwindow ;

  return ;
}



/* Returns which child of a pane the given widget is, the child widget _must_ be in the pane
 * otherwise the function will abort. */
static ZMapPaneChild whichChildOfPane(GtkWidget *child)
{
  ZMapPaneChild pane_child = ZMAP_PANE_NONE ;
  GtkWidget *pane_parent ;

  pane_parent = gtk_widget_get_parent(child) ;
  zMapAssert(GTK_IS_PANED(pane_parent)) ;

  if (myGetChild(pane_parent, 1) == child)
    pane_child = ZMAP_PANE_CHILD_1 ;
  else if (myGetChild(pane_parent, 2) == child)
    pane_child = ZMAP_PANE_CHILD_2 ;
  zMapAssert(pane_child != ZMAP_PANE_NONE) ;		    /* Should never happen. */

  return pane_child ;
}

