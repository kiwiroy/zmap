/*  File: zmapView.c
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
 * Description: 
 * Exported functions: See ZMap/zmapView.h
 * HISTORY:
 * Last edited: May 17 16:50 2004 (edgrif)
 * Created: Thu May 13 15:28:26 2004 (edgrif)
 * CVS info:   $Id: zmapView.c,v 1.2 2004-05-17 16:38:40 edgrif Exp $
 *-------------------------------------------------------------------
 */

#include <gtk/gtk.h>
#include <ZMap/zmapUtils.h>
#include <ZMap/zmapConfig.h>
#include <ZMap/zmapConn.h>
#include <ZMap/zmapWindow.h>
#include <zmapView_P.h>



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* ZMap debugging output. */
gboolean zmap_debug_G = TRUE ; 
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


static ZMapView createZMapView(GtkWidget *parent_widget, char *sequence,
			       void *app_data, ZMapViewCallbackFunc destroy_cb) ;
static void destroyZMapView(ZMapView zmap) ;

static gint zmapIdleCB(gpointer cb_data) ;
static void zmapWindowCB(void *cb_data, int reason) ;

static void startConnectionChecking(ZMapView zmap_view) ;
static void stopConnectionChecking(ZMapView zmap_view) ;
static gboolean checkConnections(ZMapView zmap_view) ;

static void loadDataConnections(ZMapView zmap_view, char *sequence) ;

static void killZMapView(ZMapView zmap_view) ;
static void killGUI(ZMapView zmap_view) ;
static void killConnections(ZMapView zmap_view) ;
static void destroyConnection(ZMapConnection *connection) ;

static void resetWindows(ZMapView zmap_view) ;
static void displayDataWindows(ZMapView zmap_view, void *data) ;
static void killWindows(ZMapView zmap_view) ;


/*
 *  ------------------- External functions -------------------
 *                     (includes callbacks)
 */


/* Create a new/blank zmap with its window, has no thread connections to databases.
 * Returns NULL on failure. */
ZMapView zMapViewCreate(GtkWidget *parent_widget, char *sequence,
			void *app_data, ZMapViewCallbackFunc destroy_cb)
{
  ZMapView zmap_view = NULL ;
  ZMapWindow window ;

  zmap_view = createZMapView(parent_widget, sequence, app_data, destroy_cb) ;

  /* Create the zmap window itself. */
  if ((window = zMapWindowCreate(zmap_view->parent_widget, zmap_view->sequence,
				 zmapWindowCB, zmap_view)))
    {
      /* add to list of windows.... */
      zmap_view->window_list = g_list_append(zmap_view->window_list, window) ;

      zmap_view->state = ZMAPVIEW_INIT ;
    }
  else
    {
      destroyZMapView(zmap_view) ;
      zmap_view = NULL ;
    }

  return zmap_view ;
}


/* Connect a blank ZMap to its databases via threads,
 * at this point the ZMap is blank and waiting to be told to load some data. */
gboolean zMapViewConnect(ZMapView zmap_view)
{
  gboolean result = TRUE ;
  ZMapConfigStanzaSet server_list = NULL ;

  if (zmap_view->state != ZMAPVIEW_INIT)
    {
      /* Probably we should indicate to caller what the problem was here....
       * e.g. if we are resetting then say we are resetting etc..... */
      result = FALSE ;
    }
  else
    {
      ZMapConfig config ;

      /* We need to retrieve the connect data here from the config stuff.... */
      if (result && (config = zMapConfigCreate()))
	{
	  ZMapConfigStanza server_stanza ;
	  ZMapConfigStanzaElementStruct server_elements[] = {{"host", ZMAPCONFIG_STRING, {NULL}},
							     {"port", ZMAPCONFIG_INT, {NULL}},
							     {"protocol", ZMAPCONFIG_STRING, {NULL}},
							     {NULL, -1, {NULL}}} ;

	  /* Set defaults...just set port, host and protocol are done by default. */
	  server_elements[1].data.i = -1 ;

	  server_stanza = zMapConfigMakeStanza("server", server_elements) ;

	  if (!zMapConfigFindStanzas(config, server_stanza, &server_list))
	    result = FALSE ;
	}

      /* Set up connections to the named servers. */
      if (result)
	{
	  int connections = 0 ;
	  ZMapConfigStanza next_server ;


	  /* Current error handling policy is to connect to servers that we can and
	   * report errors for those where we fail but to carry on and set up the ZMap
	   * as long as at least one connection succeeds. */
	  next_server = NULL ;
	  while (result
		 && ((next_server = zMapConfigGetNextStanza(server_list, next_server)) != NULL))
	    {
	      char *machine, *protocol ;
	      int port ;
	      ZMapConnection connection ;

	      machine = zMapConfigGetElementString(next_server, "host") ;
	      port = zMapConfigGetElementInt(next_server, "port") ;
	      protocol = zMapConfigGetElementString(next_server, "protocol") ;

	      if ((connection = zMapConnCreate(machine, port, protocol, zmap_view->sequence)))
		{
		  zmap_view->connection_list = g_list_append(zmap_view->connection_list, connection) ;
		  connections++ ;
		}
	      else
		{
		  zMapWarning("Could not connect to %s protocol server "
			      "on %s, port %s", protocol, machine, port) ;
		}
	    }

	  if (!connections)
	    result = FALSE ;
	}

      /* clean up. */
      if (server_list)
	zMapConfigDeleteStanzaSet(server_list) ;


      /* If everything is ok then set up idle routine to check the connections,
       * otherwise signal failure and clean up. */
      if (result)
	{
	  zmap_view->state = ZMAPVIEW_RUNNING ;
	  startConnectionChecking(zmap_view) ;
	}
      else
	{
	  zmap_view->state = ZMAPVIEW_DYING ;
	  killZMapView(zmap_view) ;
	}
    }

  return result ;
}


/* Signal threads that we want data to stick into the ZMap */
gboolean zMapViewLoad(ZMapView zmap_view, char *sequence)
{
  gboolean result = TRUE ;

  if (zmap_view->state == ZMAPVIEW_RESETTING || zmap_view->state == ZMAPVIEW_DYING)
    result = FALSE ;
  else
    {
      if (zmap_view->state == ZMAPVIEW_INIT)
	result = zMapViewConnect(zmap_view) ;

      if (result)
	{
	  if (sequence && *sequence)
	    {
	      if (zmap_view->sequence)
		g_free(zmap_view->sequence) ;
	      zmap_view->sequence = g_strdup(sequence) ;
	    }

	  loadDataConnections(zmap_view, zmap_view->sequence) ;
	}
    }

  return result ;
}


/* Reset an existing ZMap, this call will:
 * 
 *    - leave the ZMap window(s) displayed and hold onto user information such as machine/port
 *      sequence etc so user does not have to add the data again.
 *    - Free all ZMap window data that was specific to the view being loaded.
 *    - Kill the existing server thread(s).
 * 
 * After this call the ZMap will be ready for the user to try again with the existing
 * machine/port/sequence or to enter data for a new sequence etc.
 * 
 *  */
gboolean zMapViewReset(ZMapView zmap_view)
{
  gboolean result = TRUE ;

  if (zmap_view->state != ZMAPVIEW_RUNNING)
    result = FALSE ;
  else
    {
      zmap_view->state = ZMAPVIEW_RESETTING ;

      /* Reset all the windows to blank. */
      resetWindows(zmap_view) ;

      /* We need to destroy the existing thread connection and wait until user loads a new
	 sequence. */
      killConnections(zmap_view) ;
    }

  return TRUE ;
}



/*
 *    A set of accessor functions.
 */

char *zMapViewGetSequence(ZMapView zmap_view)
{
  char *sequence = NULL ;

  if (zmap_view->state != ZMAPVIEW_DYING)
    sequence = zmap_view->sequence ;

  return sequence ;
}

char *zMapViewGetZMapStatus(ZMapView zmap_view)
{
  /* Array must be kept in synch with ZmapState enum in ZMap_P.h */
  static char *zmapStates[] = {"ZMAPVIEW_INIT", "ZMAPVIEW_RUNNING", "ZMAPVIEW_RESETTING",
			       "ZMAPVIEW_DYING"} ;
  char *state ;

  zMapAssert(zmap_view->state >= ZMAPVIEW_INIT && zmap_view->state <= ZMAPVIEW_DYING) ;

  state = zmapStates[zmap_view->state] ;

  return state ;
}



/* Called to kill a zmap window and get the associated threads killed, note that
 * this call just signals everything to die, its the checkConnections() routine
 * that really clears up.
 */
gboolean zMapViewDestroy(ZMapView zmap_view)
{

  if (zmap_view->state != ZMAPVIEW_DYING)
    {
      killGUI(zmap_view) ;

      /* If we are in init state or resetting then the connections have already being killed. */
      if (zmap_view->state != ZMAPVIEW_INIT && zmap_view->state != ZMAPVIEW_RESETTING)
	killConnections(zmap_view) ;

      /* Must set this as this will prevent any further interaction with the ZMap as
       * a result of both the ZMap window and the threads dying asynchronously.  */
      zmap_view->state = ZMAPVIEW_DYING ;
    }

  return TRUE ;
}



/* We could provide an "executive" kill call which just left the threads dangling, a kind
 * of "kill -9" style thing, it would actually kill/delete stuff without waiting for threads
 * to die....  */



/* This is really the guts of the code to check what a connection thread is up
 * to. Every time the GUI thread has stopped doing things GTK calls this routine
 * which then checks our connections for responses from the threads...... */
static gint zmapIdleCB(gpointer cb_data)
{
  gint call_again = 0 ;
  ZMapView zmap_view = (ZMapView)cb_data ;

  /* Returning a value > 0 tells gtk to call zmapIdleCB again, so if checkConnections() returns
   * TRUE we ask to be called again. */
  if (checkConnections(zmap_view))
    call_again = 1 ;
  else
    call_again = 0 ;

  return call_again ;
}




/* This routine needs some work, with the reorganisation of the code it is no longer correct,
 * it depends on what the zmapWindow can return to us. */
/* Gets called when zmap Window code needs to signal us that the user has done something
 * such as click "quit". */
static void zmapWindowCB(void *cb_data, int reason)
{
  ZMapView zmap_view = (ZMapView)cb_data ;
  ZmapWindowCmd window_cmd = (ZmapWindowCmd)reason ;
  char *debug ;

  if (window_cmd == ZMAP_WINDOW_QUIT)
    {
      debug = "ZMAP_WINDOW_QUIT" ;

      zMapViewDestroy(zmap_view) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      (*(zmap_view->app_destroy_cb))(zmap_view, zmap_view->app_data) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

    }
  else if (window_cmd == ZMAP_WINDOW_LOAD)
    {
      debug = "ZMAP_WINDOW_LOAD" ;

      zMapViewLoad(zmap_view, "") ;
    }
  else if (window_cmd == ZMAP_WINDOW_STOP)
    {
      debug = "ZMAP_WINDOW_STOP" ;

      zMapViewReset(zmap_view) ;
    }


  zMapDebug("GUI: received %s from zmap window\n", debug) ;

  return ;
}



/*
 *  ------------------- Internal functions -------------------
 */


static ZMapView createZMapView(GtkWidget *parent_widget, char *sequence,
			       void *app_data, ZMapViewCallbackFunc destroy_cb)
{
  ZMapView zmap_view = NULL ;

  zmap_view = g_new(ZMapViewStruct, sizeof(ZMapViewStruct)) ;

  zmap_view->state = ZMAPVIEW_INIT ;

  zmap_view->sequence = g_strdup(sequence) ;

  zmap_view->parent_widget = parent_widget ;

  zmap_view->window_list = zmap_view->connection_list = NULL ;

  zmap_view->app_data = app_data ;
  zmap_view->app_destroy_cb = destroy_cb ;

  return zmap_view ;
}


/* Should only do this after the window and all threads have gone as this is our only handle
 * to these resources. The lists of windows and thread connections are dealt with somewhat
 * asynchronously by killWindows() & checkConnections() */
static void destroyZMapView(ZMapView zmap_view)
{
  g_free(zmap_view->sequence) ;

  g_free(zmap_view) ;

  return ;
}



/* Start the ZMapView GTK idle function (gets run when the GUI is doing nothing).
 */
static void startConnectionChecking(ZMapView zmap_view)
{
  zmap_view->idle_handle = gtk_idle_add(zmapIdleCB, (gpointer)zmap_view) ;

  return ;
}


/* I think that probably I won't need this most of the time, it could be used to remove the idle
 * function in a kind of unilateral way as a last resort, otherwise the idle function needs
 * to cancel itself.... */
static void stopConnectionChecking(ZMapView zmap_view)
{
  gtk_idle_remove(zmap_view->idle_handle) ;

  return ;
}




/* This function checks the status of the connection and checks for any reply and
 * then acts on it, it gets called from the ZMap idle function.
 * If all threads are ok and zmap has not been killed then routine returns TRUE
 * meaning it wants to be called again, otherwise FALSE.
 *
 * NOTE that you cannot use a condvar here, if the connection thread signals us using a
 * condvar we will probably miss it, that just doesn't work, we have to pole for changes
 * and this is possible because this routine is called from the idle function of the GUI.
 *  */
static gboolean checkConnections(ZMapView zmap_view)
{
  gboolean call_again = TRUE ;				    /* Normally we want to called continuously. */
  GList* list_item ;

  list_item = g_list_first(zmap_view->connection_list) ;
  /* should assert this as never null.... */

  do
    {
      ZMapConnection connection ;
      ZMapThreadReply reply ;
      void *data = NULL ;
      char *err_msg = NULL ;
      
      connection = list_item->data ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      ZMAP_DEBUG("GUI: checking connection for thread %x\n",
		  zMapConnGetThreadid(connection)) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


      data = NULL ;
      err_msg = NULL ;
      if (!(zMapConnGetReplyWithData(connection, &reply, &data, &err_msg)))
	{
	  zMapDebug("GUI: thread %x, cannot access reply from thread - %s\n",
		     zMapConnGetThreadid(connection), err_msg) ;
	}
      else
	{

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	  zMapDebug("GUI: thread %x, thread reply = %s\n",
		     zMapConnGetThreadid(connection),
		     zMapVarGetReplyString(reply)) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	      
	  if (reply == ZMAP_REPLY_WAIT)
	    {
	      ;					    /* nothing to do. */
	    }
	  else if (reply == ZMAP_REPLY_GOTDATA)
	    {
	      if (zmap_view->state == ZMAPVIEW_RUNNING)
		{
		  zMapDebug("GUI: thread %x, got data\n",
			     zMapConnGetThreadid(connection)) ;

		  /* Is this right....????? check my logic here....  */
		  zMapConnSetReply(connection, ZMAP_REPLY_WAIT) ;
		  
		  /* Signal the ZMap that there is work to be done. */
		  displayDataWindows(zmap_view, data) ;
		}
	      else
		zMapDebug("GUI: thread %x, got data but ZMap state is - %s\n",
			   zMapConnGetThreadid(connection), zMapViewGetZMapStatus(zmap_view)) ;

	    }
	  else if (reply == ZMAP_REPLY_DIED)
	    {
	      if (err_msg)
		zMapWarning("%s", err_msg) ;


	      /* This means the thread has failed for some reason and we should clean up. */
	      zMapDebug("GUI: thread %x has died so cleaning up....\n",
			 zMapConnGetThreadid(connection)) ;
		  
	      /* We are going to remove an item from the list so better move on from
	       * this item. */
	      list_item = g_list_next(list_item) ;
	      zmap_view->connection_list = g_list_remove(zmap_view->connection_list, connection) ;

	      destroyConnection(&connection) ;
	    }
	  else if (reply == ZMAP_REPLY_CANCELLED)
	    {
	      /* I'm not sure we need to do anything here as now this loop is "inside" a
	       * zmap, we should already chopping the zmap threads outside of this routine,
	       * so I think logically this cannot happen...???? */

	      /* This means the thread was cancelled so we should clean up..... */
	      zMapDebug("GUI: thread %x has been cancelled so cleaning up....\n",
			 zMapConnGetThreadid(connection)) ;

	      /* We are going to remove an item from the list so better move on from
	       * this item. */
	      list_item = g_list_next(list_item) ;
	      zmap_view->connection_list = g_list_remove(zmap_view->connection_list, connection) ;

	      destroyConnection(&connection) ;
	    }
	}
    }
  while ((list_item = g_list_next(list_item))) ;


  /* At this point if there are no threads left we need to examine our state and take action
   * depending on whether we are dying or threads have died or whatever.... */
  if (!zmap_view->connection_list)
    {
      if (zmap_view->state == ZMAPVIEW_DYING)
	{
	  /* this is probably the place to callback to zmapcontrol.... */
	  (*(zmap_view->app_destroy_cb))(zmap_view, zmap_view->app_data) ;


	  /* zmap was waiting for threads to die, now they have we can free everything and stop. */
	  destroyZMapView(zmap_view) ;

	  call_again = FALSE ;
	}
      else if (zmap_view->state == ZMAPVIEW_RESETTING)
	{
	  /* zmap is ok but user has reset it and all threads have died so we need to stop
	   * checking.... */
	  zmap_view->state = ZMAPVIEW_INIT ;

	  call_again = FALSE ;
	}
      else
	{
	  /* Threads have died because of their own errors....so what should we do ?
	   * for now we kill the window and then the rest of ZMap..... */
	  zMapWarning("%s", "Cannot show ZMap because server connections have all died") ;

	  killGUI(zmap_view) ;
	  destroyZMapView(zmap_view) ;

	  call_again = FALSE ;
	}
    }

  return call_again ;
}


static void loadDataConnections(ZMapView zmap_view, char *sequence)
{

  if (zmap_view->connection_list)
    {
      GList* list_item ;

      list_item = g_list_first(zmap_view->connection_list) ;

      do
	{
	  ZMapConnection connection ;

 	  connection = list_item->data ;

	  /* ERROR HANDLING..... */
	  zMapConnLoadData(connection, sequence) ;

	} while ((list_item = g_list_next(list_item))) ;
    }

  return ;
}


/* SOME ORDER ISSUES NEED TO BE ATTENDED TO HERE...WHEN SHOULD THE IDLE ROUTINE BE REMOVED ? */

/* destroys the window if this has not happened yet and then destroys the slave threads
 * if there are any.
 */
static void killZMapView(ZMapView zmap_view)
{
  /* precise order is not straight forward...bound to be some race conditions here but...
   * 
   * - should inactivate GUI as this should be cheap and prevents further interactions, needs
   *   to inactivate ZMap _and_ controlling GUI.
   * - then should kill the threads so we can then wait until they have all died _before_
   *   freeing up data etc. etc.
   * - then we should stop checking the threads once they have died.
   * - then GUI should disappear so user cannot interact with it anymore.
   * Then we should kill the threads */


  if (zmap_view->window_list != NULL)
    {
      killGUI(zmap_view) ;
    }

  /* NOTE, we do a _kill_ here, not a destroy. This just signals the thread to die, it
   * will actually die sometime later...check clean up sequence..... */
  if (zmap_view->connection_list != NULL)
    {
      killConnections(zmap_view) ;
    }
		  
  return ;
}


/* Calls the control window callback to remove any reference to the zmap and then destroys
 * the actual zmap itself.
 *  */
static void killGUI(ZMapView zmap_view)
{
  killWindows(zmap_view) ;

  return ;
}


static void killConnections(ZMapView zmap_view)
{
  GList* list_item ;

  list_item = g_list_first(zmap_view->connection_list) ;

  do
    {
      ZMapConnection connection ;

      connection = list_item->data ;

      /* NOTE, we do a _kill_ here, not a destroy. This just signals the thread to die, it
       * will actually die sometime later. */
      zMapConnKill(connection) ;
    }
  while ((list_item = g_list_next(list_item))) ;

  return ;
}


static void destroyConnection(ZMapConnection *connection)
{
  zMapConnDestroy(*connection) ;

  *connection = NULL ;

  return ;
}


/* set all the windows attached to this view to blank. */
static void resetWindows(ZMapView zmap_view)
{
  GList* list_item ;

  list_item = g_list_first(zmap_view->window_list) ;

  do
    {
      ZMapWindow window ;

      window = list_item->data ;

      zMapWindowReset(window) ;
    }
  while ((list_item = g_list_next(list_item))) ;

  return ;
}



/* Signal all windows there is data to draw. */
static void displayDataWindows(ZMapView zmap_view, void *data)
{
  GList* list_item ;

  list_item = g_list_first(zmap_view->window_list) ;

  do
    {
      ZMapWindow window ;

      window = list_item->data ;

      zMapWindowDisplayData(window, data) ;
    }
  while ((list_item = g_list_next(list_item))) ;

  return ;
}


/* Kill all the windows... */
static void killWindows(ZMapView zmap_view)
{

  while (zmap_view->window_list)
    {
      ZMapWindow window ;

      window = zmap_view->window_list->data ;

      zMapWindowDestroy(window) ;

      zmap_view->window_list = g_list_remove(zmap_view->window_list, window) ;
    }


  return ;
}

