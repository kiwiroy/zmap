/*  File: zmapCmdLineArgs_P.h
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2006-2017: Genome Research Ltd.
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
 *      Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *        Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk,
 *   Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *
 * Description: Internals for command line parsing.
 *
 *-------------------------------------------------------------------
 */
#ifndef ZMAP_CMDLINEARGS_P_H
#define ZMAP_CMDLINEARGS_P_H

#include <gtk/gtk.h>
#include <ZMap/zmapCmdLineArgs.hpp>


#define ZMAPARG_VERSION_DESC        "Program version."
#define ZMAPARG_RAW_VERSION_DESC    "Code version."
#define ZMAPARG_SLEEP_DESC          "Makes ZMap sleep for given number of secs at start-up to give time to attach a debugger."
#define ZMAPARG_SEQUENCE_START_DESC "Start coord in sequence, must be in range 1 -> seq_length."
#define ZMAPARG_SEQUENCE_END_DESC   "End coord in sequence, must be in range start -> seq_length, but end == 0 means show to end of sequence."
#define ZMAPARG_CONFIG_FILE_DESC    "Relative or full path to configuration file."
#define ZMAPARG_CONFIG_DIR_DESC     "Relative or full path to configuration directory."
#define ZMAPARG_STYLES_FILE_DESC    "Relative or full path to styles file."
#define ZMAPARG_SINGLE_SCREEN_DESC  "Force zmap to open tools like blixem on the same screen that it is running on."
#define ZMAPARG_WINDOW_ID_DESC      "Window ID of the controlling application."
#define ZMAPARG_REMOTE_DEBUG_DESC   "Set RemoteControl debug level."

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
#define ZMAPARG_PEER_NAME_DESC      "Peer Remote Control app name."
#define ZMAPARG_PEER_CLIPBOARD_DESC "Peer Remote Control clipboard name."
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
#define ZMAPARG_PEER_SOCKET_DESC "Peer Remote Control socket name."

#define ZMAPARG_SEQUENCE_DESC       "Sequence name."
#define ZMAPARG_SERIAL_DESC         "Operate pipe servers in serial on startup"
#define ZMAPARG_TIMING_DESC         "switch on timing functions"
#define ZMAPARG_SHRINK_DESC         "allow shrinkable ZMap window"
#define ZMAPARG_FILES_DESC         "allow shrinkable ZMap window"

#define ZMAPARG_NO_ARG              "<none>"
#define ZMAPARG_COORD_ARG           "coord"
#define ZMAPARG_FILE_ARG            "file path"
#define ZMAPARG_DIR_ARG             "directory"
#define ZMAPARG_STYLES_FILE_ARG     "styles file path"
#define ZMAPARG_WINID_ARG           "0x0000000"
#define ZMAPARG_SEQUENCE_ARG        "<sequence name>"
#define ZMAPARG_REMOTE_DEBUG_ARG    "debug level: off | normal | verbose"

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
#define ZMAPARG_PEER_NAME_ARG       "peer app name"
#define ZMAPARG_PEER_CLIPBOARD_ARG  "peer clipboard unique id"
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
#define ZMAPARG_PEER_SOCKET_ARG  "peer socket unique id"

#define ZMAPARG_SERIAL_ARG          "<none>"
#define ZMAPARG_FILES_ARG      "<file(s)>"

#define ZMAPARG_INVALID_INT -1
#define ZMAPARG_INVALID_BOOL FALSE
#define ZMAPARG_INVALID_STR NULL
#define ZMAPARG_INVALID_FLOAT 0.0



typedef struct _ZMapCmdLineArgsStruct
{
  /* The original argc/argv passed in. */
  int argc{0} ;
  char **argv{NULL} ;

  GOptionContext *opt_context{NULL} ;

  char **sequence_arg{NULL} ;

  char **files_arg{NULL} ;	/* non options/ remainder args */

  /* All option values are stored here for later reference. */
  gboolean raw_version{false} ;
  gboolean version{false} ;
  gboolean serial{false} ;
  gboolean timing{false} ;
  gboolean single_screen{false} ;

  int sleep{0} ;

  int start{0} ;
  int end{0} ;

  char *config_dir{NULL} ;
  char *config_file_path{NULL} ;
  char *window{NULL} ;
  char *styles_file{NULL} ;

  char *remote_debug{NULL} ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  char *peer_name{NULL} ;
  char *peer_clipboard{NULL} ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
  char *peer_socket{NULL} ;

  gboolean shrink{false} ;

} ZMapCmdLineArgsStruct, *ZMapCmdLineArgs ;





#endif /* !ZMAP_CMDLINEARGS_P_H */

