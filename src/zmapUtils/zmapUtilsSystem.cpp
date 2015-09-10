/*  File: zmapUtilsSystem.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2006-2015: Genome Research Ltd.
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
 * originally written by:
 *
 *      Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *        Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk,
 *   Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *
 * Description: Contains routines for various system functions.
 *
 * Exported functions: See zmapUtils.h
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.hpp>

#include <sys/utsname.h>

#include <zmapUtils_P.hpp>




/*
 *               External Interface functions
 */



/* Check the system type, e.g. Linux, Darwin etc, the string returned is as per
 * the uname command so you should test it against the string you expect.
 * The function returns NULL if the uname command failed, otherwise it returns
 * a string that should be g_free'd by the caller. */
char *zMapUtilsSysGetSysName(void)
{
  char *sys_name = NULL ;
  struct utsname unamebuf ;

  if (uname(&unamebuf) == 0)
    {
      sys_name = g_strdup(unamebuf.sysname) ;
    }

  return sys_name ;
}






/*
 *               Internal functions
 */
