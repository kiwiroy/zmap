/*  File: zmapManager.h
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
 * Description: 
 * HISTORY:
 * Last edited: May 27 10:21 2005 (rds)
 * Created: Thu Jul 24 14:39:06 2003 (edgrif)
 * CVS info:   $Id: zmapManager.h,v 1.6 2005-06-01 13:09:31 rds Exp $
 *-------------------------------------------------------------------
 */
#ifndef ZMAP_MANAGER_H
#define ZMAP_MANAGER_H

#include <ZMap/zmapSys.h>
#include <ZMap/zmapControl.h>

/* Opaque type, contains list of current ZMaps. */
typedef struct _ZMapManagerStruct *ZMapManager ;

ZMapManager zMapManagerCreate(ZMapAppCallbacks zmap_gui_funcs, void *gui_data) ;
gboolean zMapManagerAdd(ZMapManager zmaps, char *sequence, int start, int end, ZMap *zmap_out) ;
gboolean zMapManagerReset(ZMap zmap) ;
gboolean zMapManagerRaise(ZMap zmap) ;
gboolean zMapManagerKill(ZMapManager zmaps, ZMap zmap) ;
gboolean zMapManagerDestroy(ZMapManager zmaps) ;

#endif /* !ZMAP_MANAGER_H */
