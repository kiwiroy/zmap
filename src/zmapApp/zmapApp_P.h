/*  File: zmapApp.h
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
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk,
 *
 * Description: 
 * Exported functions: See XXXXXXXXXXXXX.h
 * HISTORY:
 * Last edited: May 17 10:09 2006 (rds)
 * Created: Thu Jul 24 14:35:41 2003 (edgrif)
 * CVS info:   $Id: zmapApp_P.h,v 1.13 2006-05-17 12:40:55 rds Exp $
 *-------------------------------------------------------------------
 */
#ifndef ZMAP_APP_PRIV_H
#define ZMAP_APP_PRIV_H

#include <gtk/gtk.h>

#include <ZMap/zmapUtils.h>
#include <ZMap/zmapManager.h>
#include <ZMap/zmapXRemote.h>


/* Minimum GTK version supported. */
enum {ZMAP_GTK_MAJOR = 2, ZMAP_GTK_MINOR = 2, ZMAP_GTK_MICRO = 1} ;

/* Overall application control struct. */
typedef struct _ZMapAppContextStruct
{
  GtkWidget *app_widg ;

  GtkWidget *sequence_widg ;
  GtkWidget *start_widg ;
  GtkWidget *end_widg ;

  GtkTreeStore *tree_store_widg ;

  GError *info;                 /* This is an object to hold a code
                                 * and a message as info for the
                                 * remote control simple IPC stuff */
  ZMapManager zmap_manager ;
  ZMap selected_zmap ;

  ZMapLog logger ;

  gulong propertyNotifyEventId;
  zMapXRemoteNotifyData propertyNotifyData;
  zMapXRemoteObj xremoteClient; /* May well be NULL */

  gboolean show_mainwindow ;				    /* Should main window be displayed. */

  char *default_sequence ;				    /* Was a default sequence specified in
							       the config. file.*/

} ZMapAppContextStruct, *ZMapAppContext ;


/* cols in connection list. */
enum {ZMAP_NUM_COLS = 4} ;

enum {
  ZMAPID_COLUMN,
  ZMAPSEQUENCE_COLUMN,
  ZMAPSTATE_COLUMN,
  ZMAPLASTREQUEST_COLUMN,
  ZMAPDATA_COLUMN,
  ZMAP_N_COLUMNS
};

int zmapMainMakeAppWindow(int argc, char *argv[]) ;
GtkWidget *zmapMainMakeMenuBar(ZMapAppContext app_context) ;
GtkWidget *zmapMainMakeConnect(ZMapAppContext app_context) ;
GtkWidget *zmapMainMakeManage(ZMapAppContext app_context) ;
void zmapAppCreateZMap(ZMapAppContext app_context, char *sequence, int start, int end) ;
void zmapAppExit(ZMapAppContext app_context) ;

void zmapAppRemoteInstaller(GtkWidget *widget, gpointer app_context_data);

#endif /* !ZMAP_APP_PRIV_H */
