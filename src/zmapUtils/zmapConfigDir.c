/*  File: zmapConfigDir.c
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
 * originated by
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *      Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk,
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk
 *
 * Description: 
 *
 * Exported functions: See ZMap/zmapConfigDir.h
 * HISTORY:
 * Last edited: Oct 10 10:11 2006 (edgrif)
 * Created: Thu Feb 10 10:05:36 2005 (edgrif)
 * CVS info:   $Id: zmapConfigDir.c,v 1.5 2006-11-08 09:24:40 edgrif Exp $
 *-------------------------------------------------------------------
 */

#include <glib.h>
#include <ZMap/zmapUtils.h>
#include <zmapConfigDir_P.h>





static ZMapConfigDir dir_context_G = NULL ;



/* The context is only created the first time this function is called, after that
 * it just returns a reference to the existing context.
 * 
 * Note that as this routine should be called once per application it is not
 * thread safe, it could be made so but is overkill as the GUI/master thread is
 * not thread safe anyway.
 * 
 * If config_dir is NULL, the default ~/.ZMap directory is used.
 * If config_file is NULL, the default ZMap file is used. 
 * 
 * returns FALSE if the configuration directory does not exist/read/writeable.
 * 
 *  */
gboolean zMapConfigDirCreate(char *config_dir, char *config_file)
{
  gboolean result = FALSE ;
  ZMapConfigDir dir_context = NULL ;
  gboolean home_relative = FALSE, make_dir = FALSE ;

  zMapAssert(!dir_context_G) ;

  dir_context_G = dir_context = g_new0(ZMapConfigDirStruct, 1) ;

  if (!config_dir)
    {
      config_dir = ZMAP_USER_CONFIG_DIR ;
      home_relative = TRUE ;
    }

  if (!config_file)
    config_file = ZMAP_USER_CONFIG_FILE ;
     
  if ((dir_context->config_dir = zMapGetDir(config_dir, home_relative, make_dir))
      && (dir_context->config_file = zMapGetFile(dir_context->config_dir, config_file, FALSE)))
    result = TRUE ;


  return result ;
}



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* Is this needed ?? */

char *zMapConfigDirSetConfigFile(char *config_file)
{
  char *config_path ;
  ZMapConfigDir dir_context = dir_context_G ;

  zMapAssert(dir_context) ;



  config_path = dir_context->config__dir ;

  return config_dir ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */




/* Not sure if this is really necessary....maybe.... */
/* DO NOT FREE THE RESULTING STRING...COPY IF NEED BE.... */
const char *zMapConfigDirDefaultName(void)
{
  char *dir_name = ZMAP_USER_CONFIG_DIR ;

  return dir_name ;
}



char *zMapConfigDirGetDir(void)
{
  char *config_dir ;
  ZMapConfigDir dir_context = dir_context_G ;

  zMapAssert(dir_context) ;

  config_dir = dir_context->config_dir ;

  return config_dir ;
}


char *zMapConfigDirGetFile(void)
{
  char *config_file ;
  ZMapConfigDir dir_context = dir_context_G ;

  zMapAssert(dir_context) ;

  config_file = dir_context->config_file ;

  return config_file ;
}


char *zMapConfigDirFindFile(char *filename)
{
  char *file_path = NULL ;
  ZMapConfigDir dir_context = dir_context_G ;

  zMapAssert(dir_context) ;

  file_path = zMapGetFile(dir_context->config_dir, filename, FALSE) ;

  return file_path ;
}



char *zMapConfigDirFindDir(char *directory_in)
{
  char *control_dir ;

  control_dir = zMapGetDir(directory_in, TRUE, FALSE) ;

  return control_dir ;
}



void zMapConfigDirDestroy(void)
{
  ZMapConfigDir dir_context = dir_context_G ;

  zMapAssert(dir_context) ;

  g_free(dir_context->config_dir) ;

  g_free(dir_context) ;

  return ;
}

/* Needs top ZMap window id.... and name for it. 
 * Get a window id with GDK_DRAWABLE_XID(top_zmap_window);
 * It'll need to be a unique name */
void zMapConfigDirWriteWindowIdFile(unsigned long id, char *window_name)
{
  GError *g_error = NULL ;
  GIOChannel *winid_file = NULL;
  gsize bytes = 0;
  char *config_dir = NULL;
  char *path, *id_line;

  if((config_dir = zMapConfigDirGetDir()))
    {
      path = g_strdup_printf("%s/%s.%s", config_dir, window_name, WINDOWID_SUFFIX);
      if((winid_file = g_io_channel_new_file(path, "w+", &g_error)))
        {
          g_error = NULL;
          id_line = g_strdup_printf("WindowID: 0x%lx\n", id);
          /* zMapLogMessage("Recorded %s to file %s", id_line, path); */
          g_io_channel_write_chars(winid_file, id_line, -1, &bytes, &g_error); /* should catch not writing here */
          g_io_channel_flush(winid_file, &g_error);                            /* should catch not writing here */
          g_free(id_line);
        }
      else{
        zMapLogWarning("Error doing something to path '%s', %s", path, g_error->message);
      }
      if(path)
        g_free(path);
    }
  else
    {
      zMapLogWarning("%s","Unable to find a configuration directory.") ;
    }
}
