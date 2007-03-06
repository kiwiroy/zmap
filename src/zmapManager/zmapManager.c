/*  File: zmapManager.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2006: Genome Research Ltd.
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
 * Description: 
 * Exported functions: See zmapManager.h
 * HISTORY:
 * Last edited: Mar  6 10:23 2007 (edgrif)
 * Created: Thu Jul 24 16:06:44 2003 (edgrif)
 * CVS info:   $Id: zmapManager.c,v 1.20 2007-03-06 10:24:21 edgrif Exp $
 *-------------------------------------------------------------------
 */

#include <ZMap/zmapUtils.h>
#include <zmapManager_P.h>



static void destroyedCB(ZMap zmap, void *cb_data) ;
static void exitCB(ZMap zmap, void *cb_data) ;
static void removeZmapEntry(ZMapManager zmaps, ZMap zmap) ;


/* Holds callbacks the level above us has asked to be called back on. */
static ZMapManagerCallbacks manager_cbs_G = NULL ;



/* Holds callbacks we set in the level below us (ZMap window) to be called back on. */
ZMapCallbacksStruct zmap_cbs_G = {destroyedCB, exitCB} ;



/* This routine must be called just once before any other zmaps routine, it is a fatal error
 * if the caller calls this routine more than once. The caller must supply all of the callback
 * routines.
 * 
 * Note that since this routine is called once per application we do not bother freeing it
 * via some kind of zmaps terminate routine. */
void zMapManagerInit(ZMapManagerCallbacks callbacks)
{
  zMapAssert(!manager_cbs_G) ;

  zMapAssert(callbacks && callbacks->zmap_deleted_func && callbacks->zmap_set_info_func
	     && callbacks->exit_func) ;

  manager_cbs_G = g_new0(ZMapManagerCallbacksStruct, 1) ;

  manager_cbs_G->zmap_deleted_func  = callbacks->zmap_deleted_func ; /* called when zmaps close */
  manager_cbs_G->zmap_set_info_func = callbacks->zmap_set_info_func ;
							    /* called when zmap does something that gui needs to know about (remote calls) */
  manager_cbs_G->exit_func = callbacks->exit_func ;		    /* called when zmap app must exit. */


  /* Init control callbacks.... */
  zMapInit(&zmap_cbs_G) ;

  return ;
}


ZMapManager zMapManagerCreate(void *gui_data)
{
  ZMapManager manager ;

  manager = g_new0(ZMapManagerStruct, 1) ;

  manager->zmap_list = NULL ;

  manager->gui_data = gui_data ;

  return manager ;
}


/* Add a new zmap window with associated thread and all the gubbins.
 * Return indicates what happened on trying to add zmap. */
ZMapManagerAddResult zMapManagerAdd(ZMapManager zmaps, char *sequence, int start, int end, ZMap *zmap_out)
{
  ZMapManagerAddResult result = ZMAPMANAGER_ADD_FAIL ;
  ZMap zmap = NULL ;
  ZMapView view = NULL ;

  zMapAssert(zmaps && sequence) ;

  if ((zmap = zMapCreate((void *)zmaps)))
    {
      if (!(view = zMapAddView(zmap, sequence, start, end)))
	{
	  /* Remove zmap we just created, if this fails then return disaster.... */
	  if (!zMapDestroy(zmap))
	    result = ZMAPMANAGER_ADD_DISASTER ;
	}
      else
	{
	  zmaps->zmap_list = g_list_append(zmaps->zmap_list, zmap) ;
	  *zmap_out = zmap ;

	  (*(manager_cbs_G->zmap_set_info_func))(zmaps->gui_data, zmap) ;      

	  if (!(zMapConnectView(zmap, view)))
	    result = ZMAPMANAGER_ADD_NOTCONNECTED ;
	  else
	    result = ZMAPMANAGER_ADD_OK ;
	}
    }

  return result ;
}

guint zMapManagerCount(ZMapManager zmaps)
{
  guint zmaps_count = 0;
  zmaps_count = g_list_length(zmaps->zmap_list);
  return zmaps_count;
}


/* Reset an existing ZMap, this call will:
 * 
 *    - leave the ZMap window displayed and hold onto user information such as machine/port
 *      sequence etc so user does not have to add the data again.
 *    - Free all ZMap window data that was specific to the view being loaded.
 *    - Kill the existing server thread and start a new one from scratch.
 * 
 * After this call the ZMap will be ready for the user to try again with the existing
 * machine/port/sequence or to enter data for a new sequence etc.
 * 
 *  */
gboolean zMapManagerReset(ZMap zmap)
{
  gboolean result = TRUE ;

  result = zMapReset(zmap) ;

  return result ;
}


gboolean zMapManagerRaise(ZMap zmap)
{
  gboolean result = TRUE ;

  result = zMapRaise(zmap) ;

  return result ;
}



gboolean zMapManagerKill(ZMapManager zmaps, ZMap zmap)
{
  gboolean result = TRUE ;

  result = zMapDestroy(zmap) ;

  removeZmapEntry(zmaps, zmap) ;

  return result ;
}

gboolean zMapManagerKillAllZMaps(ZMapManager zmaps)
{
  if (zmaps->zmap_list)
    {
      GList *next_zmap ;

      /* The "while" seems counterintuitive but the kill removes zmaps from the list
       * so we keep fetching the new head of the list. */
      next_zmap = g_list_first(zmaps->zmap_list) ;
      do
	{
	  zMapManagerKill(zmaps, (ZMap)(next_zmap->data)) ;
	}
      while ((next_zmap = g_list_first(zmaps->zmap_list))) ;
    }

  return TRUE;
}

/* Frees all resources held by a zmapmanager and then frees the manager itself. */
gboolean zMapManagerDestroy(ZMapManager zmaps)
{
  gboolean result = TRUE ;

  /* Free all the existing zmaps. */
  zMapManagerKillAllZMaps(zmaps);

  g_free(zmaps) ;

  return result ;
}




/*
 *  ------------------- Internal functions -------------------
 */


/* Gets called when a single ZMap window closes from "under our feet" as a result of user interaction,
 * we then make sure we clean up. */
static void destroyedCB(ZMap zmap, void *cb_data)
{
  ZMapManager zmaps = (ZMapManager)cb_data ;

  removeZmapEntry(zmaps, zmap) ;

  return ;
}



/* Gets called when a ZMap requests that the application "quits" as a result of user interaction,
 * we then make sure we clean up everything including the zmap that requested the quit. */
static void exitCB(ZMap zmap, void *cb_data)
{
  ZMapManager zmaps = (ZMapManager)cb_data ;

  (*(manager_cbs_G->exit_func))(zmaps->gui_data, zmap) ;

  return ;
}



static void removeZmapEntry(ZMapManager zmaps, ZMap zmap)
{
  zmaps->zmap_list = g_list_remove(zmaps->zmap_list, zmap) ;

  (*(manager_cbs_G->zmap_deleted_func))(zmaps->gui_data, zmap) ;

  return ;
}


