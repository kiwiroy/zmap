/*  File: zmapStackTrace.c
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
 * originally written by:
 * 
 *      Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk
 *        Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk
 *   Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *       Gemma Guest (Sanger Institute, UK) gb10@sanger.ac.uk
 *      Steve Miller (Sanger Institute, UK) sm23@sanger.ac.uk
 *  
 * Description: Interface to GNU-based functions for reporting
 *              stack trace.
 *
 * Exported functions: See ZMap/zmapUtils.h
 *
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.hpp>

#include <unistd.h>
#include <glib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>/* config generated by configure */
#endif

#ifdef HAVE_GNUC_BACKTRACE
#include <execinfo.h>/* backtrace() & backtrace_symbols_fd() */
#endif /* HAVE_GNUC_BACKTRACE */

#include <ZMap/zmapUtils.hpp>
#include <ZMap/zmapUtilsDebug.hpp>



#define ZMAPLOG_MAX_TRACE_SIZE 50



/* 
 *                   External interface routines.
 */


/* Given access to gnu glibc with backtrace and backtrace_symbols_fd
 * this function will log the current stack to the given file descriptor.
 * 
 * An example of the output is:
 * 
 *  ./zmap(zMapLogStack+0x10d)[0x808226e]
 *  ./zmap[0x807600a]
 *  ./zmap(zMapFeatureContextExecuteComplete+0xa2)[0x8075752]
 *  ./zmap[0x80a282e]
 *  ./zmap(zmapWindowDrawFeatures+0x536)[0x80a1bb2]
 *  ./zmap[0x8090db2]
 *  /software/acedb/lib/libgtk-x11-2.0.so.0[0x40173ff0]
 *  /software/acedb/lib/libgobject-2.0.so.0(g_closure_invoke+0xd5)[0x406de6f5]
 *  /software/acedb/lib/libgobject-2.0.so.0[0x406f099f]
 *  /software/acedb/lib/libgobject-2.0.so.0(g_signal_emit_valist+0x52c)[0x406ef85c]
 *  /software/acedb/lib/libgobject-2.0.so.0(g_signal_emit+0x26)[0x406efcf6]
 *  /software/acedb/lib/libgtk-x11-2.0.so.0[0x402958a7]
 *  /software/acedb/lib/libgtk-x11-2.0.so.0(gtk_main_do_event+0x2ba)[0x401719ba]
 *  
 * zmap must be compiled/linked with -rdynamic to get function names. There appears
 * to be no way to get static function names displayed, see the entries of the
 * form "./zmap[0x80a282e]" in the trace above.
 * 
 * Note that backtrace() and backtrace_symbols_fd() do not use malloc() allowing
 * a stacktrace to be retrieved even if the malloc system has become corrupted.
 * 
 * This is why we don't just use zMapLog calls from here but instead the
 * user can call zMapLogStack() which passes the zmap log file fd direct to
 * this function which passes it direct to backtrace_symbols_fd().
 * 
 * If remove is greater zero it specifies the number of elements at the start
 * of the stack trace to omit.
 * 
 * fd must be a writeable file descriptor, the results are undefined if it is not.
 * 
 */
gboolean zMapStack2fd(unsigned int remove, int fd)
{
  gboolean traced = FALSE ;

#ifdef HAVE_GNUC_BACKTRACE

  void *stack[ZMAPLOG_MAX_TRACE_SIZE] ;
  size_t size, first = 0 ;

  zMapReturnValIfFail((remove >= 0), FALSE) ;
  zMapReturnValIfFail((fd >= 0), FALSE) ;

  if ((size = backtrace(stack, ZMAPLOG_MAX_TRACE_SIZE)))
    {
      /* remove requested number of members of stack */
      if (size > remove)
        {
          size  -= remove ;
          first += remove ;
        }

      /* remove this function from the stack. */
      if (size > 1)
        {
          size-- ;
          first++ ;
        }

      backtrace_symbols_fd(&stack[first], size, fd) ;

      traced = TRUE ;
    }

#endif /* HAVE_GNUC_BACKTRACE */

  return traced ;
}






