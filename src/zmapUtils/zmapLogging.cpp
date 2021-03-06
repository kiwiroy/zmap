/*  File: zmapLogging.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2006-2017: Genome Research Ltd.
 *-------------------------------------------------------------------
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
 * Description: Log functions for zmap app. Allows start/stop, switching
 *              on/off. Currently there is just one global log for the
 *              whole application.
 *
 * Exported functions: See zmapUtilsLog.h
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.hpp>

#include <string.h>
#include <stdio.h>
#include <sys/param.h>    /* MAXHOSTNAMLEN */
#include <unistd.h>    /* for pid stuff. STDIN_FILENO too. */
#include <glib.h>
#include <glib/gstdio.h>

#include <ZMap/zmapConfigDir.hpp>
#include <ZMap/zmapConfigIni.hpp>
#include <ZMap/zmapConfigStrings.hpp>
#include <ZMap/zmapUtils.hpp>
#include <zmapUtils_P.hpp>



// Temporary hack while moving zmap to antiquated Redhat in the EBI.
//
// Defines macros for glib mutex calls using old and new interfaces.
// Once EBI catches up then the old ones can be removed.
//

#if (GLIB_MINOR_VERSION < 32)

#define OUR_MUTEX_PTR GStaticMutex *log_lock
#define OUR_MUTEX_DECL GStaticMutex log_lock_G = G_STATIC_MUTEX_INIT
#define OUR_MUTEX_LOCK g_static_mutex_lock(log->log_lock)
#define OUR_MUTEX_UNLOCK g_static_mutex_unlock(log->log_lock)
#define OUR_MUTEX_CLEAR g_static_mutex_init(log->log_lock)

#else

#define OUR_MUTEX_PTR GMutex  *log_lock
#define OUR_MUTEX_DECL static GMutex log_lock_G
#define OUR_MUTEX_LOCK g_mutex_lock(log->log_lock)
#define OUR_MUTEX_UNLOCK g_mutex_unlock(log->log_lock)
#define OUR_MUTEX_CLEAR g_mutex_clear(log->log_lock)

#endif




/* I don't know what all this foo log stuff is about....seems dubious and wrong to me...
 * a hack by Malcolm ?....needs rationalising or removing..... */

/* Must be zero for xremote compile. */
#define FOO_LOG 0






/* Common struct for all log handlers, we use these to turn logging on/off. */
typedef struct
{
  guint cb_id ;    /* needed by glib to install/uninstall
                      handler routines. */
  GLogFunc log_cb ;    /* handler routine must change to turn
                          logging on/off. */

  /* log file name/file etc. */
  char *log_path ;
  GIOChannel* logfile ;

} zmapLogHandlerStruct, *zmapLogHandler ;



/* State for the logger as a whole. */
typedef struct  _ZMapLogStruct
{
  OUR_MUTEX_PTR ;                                      /* Ensure only single threading in log handler routine. */

  /* Logging action. */
  gboolean logging ;                                        /* logging on or off ? */
  zmapLogHandlerStruct active_handler ;                     /* Used when logging is on. */
  zmapLogHandlerStruct inactive_handler ;                   /* Used when logging is off. */
  gboolean log_to_file ;                                    /* log to file or leave glib to log to
                                                               stdout/err ? */
  /* Log record content. */
  gchar *userid ;
  gchar *nodeid ;
  int pid ;

  /* Controls whether to display process, code and time details in the log.
   * There are all off by default. */
  gboolean show_process ;
  gboolean show_code ;
  gboolean show_time;

  gboolean catch_glib;
  gboolean echo_glib;   /* to stdout if caught */

} ZMapLogStruct ;




static ZMapLog createLog(void) ;
static void destroyLog(ZMapLog log) ;

static gboolean configureLog(ZMapLog log, GError **error) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
static gboolean getLogConf(ZMapLog log) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


static gboolean startLogging(ZMapLog log, GError **error) ;
static gboolean stopLogging(ZMapLog log, gboolean remove_all_handlers) ;

static gboolean openLogFile(ZMapLog log, GError **error) ;
static gboolean closeLogFile(ZMapLog log) ;

static void writeStartOrStopMessage(gboolean start) ;

static void nullLogger(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message,
                       gpointer user_data) ;
static void fileLogger(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message,
                       gpointer user_data) ;
static void glibLogger(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message,
                       gpointer user_data);

#if FOO_LOG
static void logTime(int what, int how) ;
#endif

static bool getFuncNamePosition(const char *function_prototype, char **start_out, unsigned int *len_out) ;




/* We only ever have one log so its kept internally here. */
static ZMapLog log_G = NULL ;

// glib says that we should not intialise this, it is done for us.
OUR_MUTEX_DECL ;




/* WHAT IS THIS....WHY IS THIS EVEN HERE.......STREUTH..... */
void foo_logger(char *x)
{
  /* can't call this from foo as it's a macro */
  zMapLogWarning(x,"");
  printf("%s\n",x);

  return ;
}


/* This function is NOT thread safe, you should not call this from individual threads. The log
 * package expects you to call this outside of threads then you should be safe to use the log
 * routines inside threads.
 * Note that the package expects the caller to have called  g_thread_init() before calling this
 * function. */
gboolean zMapLogCreate(char *logname)
{
  gboolean result = FALSE ;
  ZMapLog log = log_G ;

  /* zMapAssert(!log) ; */
  if (log)
    return result ;

#if FOO_LOG// log timing stats from foo
  // have to take this out to get xremote to compile for perl
  // should be ok when we get the new xremote

  extern void (*foo_log)(char *x);


  if (zmap_timing_G)
    {
      extern void (*foo_timer)(int,int);

      foo_timer = logTime;
    }

  if (zmap_debug_G)
    {
      extern void (*foo_log_stack)(void);

      foo_log_stack = zMapPrintStack;
    }

  foo_log = foo_logger;

#endif


  log_G = log = createLog() ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  if (!configureLog(log, g_error))
    {
      destroyLog(log) ;
      log_G = log = NULL ;
    }
  else
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
    {

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      zmap_log_timer_G = g_timer_new();
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

      result = TRUE ;
    }

  return result ;
}


void zMapWriteStartMsg(void)
{
  ZMapLog log = log_G ;

  /* zMapAssert(log) ; */
  if (!log)
    return ;

  writeStartOrStopMessage(TRUE) ;

  return ;
}

void zMapWriteStopMsg(void)
{
  ZMapLog log = log_G ;

  /* zMapAssert(log) ; */
  if (!log)
    return ;

  writeStartOrStopMessage(FALSE) ;

  return ;
}


/* Configure the log. */
gboolean zMapLogConfigure(gboolean logging, gboolean log_to_file,
                          gboolean show_process, gboolean show_code, gboolean show_time,
                          gboolean catch_glib, gboolean echo_glib,
                          char *logfile_path, GError **error)
{
  gboolean result = FALSE ;
  GError *g_error = NULL ;
  ZMapLog log = log_G ;

  /* Log at all ? */
  log->logging = logging ;

  /* logging to file ? */
  log->log_to_file = log_to_file ;

  /* How much detail to show in log records ? */
  log->show_process = show_process ;

  log->show_code = show_code ;

  log->show_time = show_time ;

  /* glib error message handling, catch GLib errors, else they stay on stdout */
  log->catch_glib = catch_glib ;
  log->echo_glib = echo_glib ;

  /* user specified dir, default to config dir */
  log->active_handler.log_path = logfile_path ;

  result = configureLog(log, &g_error) ;

  if (g_error)
    g_propagate_error(error, g_error) ;

  return result ;
}


/* The log and Start and Stop routines write out a record to the log to show start and stop
 * of the log but there is a window where a thread could get in and write to the log
 * before/after they do. We'll just have to live with this... */
gboolean zMapLogStart(GError **error)
{
  gboolean result = FALSE ;
  GError *g_error = NULL ;
  ZMapLog log = log_G ;

  /* zMapAssert(log) ; */
  if (!log)
    return result  ;

  OUR_MUTEX_LOCK ;

  result = startLogging(log, &g_error) ;

  OUR_MUTEX_UNLOCK ;

  if (g_error)
    g_propagate_error(error, g_error) ;

  return result ;
}



void zMapLogTime(int what, int how, long data, const char *string_arg)
{
  static double times[N_TIMES];
  static double when[N_TIMES];
  double x,e;

  /* THERE IS A WHOLE ENUMS MECHANISM TO AVOID HAVING TO DO THIS....SEE zmapEnum.h */
  /* these mirror the #defines in zmapUtilsDebug.h */
  const char *which[] = { "none", "foo-expose", "foo-update",
                          "foo-draw", "draw_context", "revcomp", "zoom", "bump", "setvis", "load", 0 } ;

  if (zmap_timing_G)
    {
      e = zMap_elapsed();

      switch(how)
        {
        case TIMER_CLEAR:
          times[what] = 0;
          zMapLogMessage("Timed: %s %s (%ld) Clear",which[what],string_arg,data);
          break;
        case TIMER_START:
          when[what] = e;
          zMapLogMessage("Timed: %s %s (l%d) Start  %.3f",which[what],string_arg,data,e);
          break;
        case TIMER_STOP:
          x = e - when[what];
          times[what] += x;
          zMapLogMessage("Timed: %s %s (%ld) Stop %.3f = %.3f",which[what],string_arg,data,x,times[what]);
          break;
        case TIMER_ELAPSED:
          zMapLogMessage("Timed: %s %s (%ld) Elasped %.3f",which[what],string_arg,data,e);
          break;
        }
    }

  return ;
}

const char *zMapLogGetLogFilePath(void)
{
  const char* log_file_path = NULL ;
  ZMapLog log = log_G ;

  if (log)
    log_file_path = log->active_handler.log_path ;

  return log_file_path ;
}


int zMapLogFileSize(void)
{
  int size = -1 ;
  ZMapLog log = log_G ;

  if (log)
    {
      struct stat file_stats ;

      if (log->log_to_file)
        {
          if (g_stat(log->active_handler.log_path, &file_stats) == 0)
            {
              size = file_stats.st_size ;
            }
        }
    }

  return size ;
}


void zMapLogMsg(const char *domain, GLogLevelFlags log_level,
                const char *file, const char *function, int line,
                const char *format, ...)
{
  ZMapLog log = log_G ;
  va_list args ;
  GString *format_str ;
  const char *msg_level = NULL ;


  /* zMapAssert(log) ;*/
  if (!log)
    return ;

  /* zMapAssert(domain && *domain && file && *file && format && *format) ; */
  if (!domain || !*domain || !file || !*file || !format || !*format)
    return ;

  format_str = g_string_sized_new(2000) ;    /* Not too many records longer than this. */


  /* include nodeid/pid ? */
  if (log->show_process)
    {
      g_string_append_printf(format_str, "%s[%s:%s:%d]\t",
                             ZMAPLOG_PROCESS_TUPLE, log->userid, log->nodeid, log->pid) ;
    }

  /* If code details are wanted then output them in the log. */
  if (log->show_code)
    {
      char *file_basename ;

      file_basename = g_path_get_basename(file) ;

      g_string_append_printf(format_str, "%s[%s ",
                             ZMAPLOG_CODE_TUPLE,
                             file_basename) ;

      if (function)
        {
          char *func_name = NULL ;
          unsigned int func_name_len = 0 ;

          // C++ functions are often reported verbosely so cut down to just function name if possible.
          if (getFuncNamePosition(function, &func_name, &func_name_len))
            format_str = g_string_append_len(format_str, func_name, func_name_len) ;
          else
            g_string_append_printf(format_str, "%s", function) ;

          format_str = g_string_append(format_str, "() ") ;
        }
      else
        {
          format_str = g_string_append(format_str, "<NO FUNC NAME> ") ;
        }
      
      g_string_append_printf(format_str, "%d]\t", line) ;


      g_free(file_basename) ;
    }

  /* include a timestamp? */
  if (log->show_time)
    {
      char *time_str ;

      time_str = zMapGetTimeString(ZMAPTIME_LOG, NULL) ;

      g_string_append_printf(format_str, "%s[%s]\t", ZMAPLOG_TIME_TUPLE, time_str) ;
    }

  /* Now show the actual message. */
  switch(log_level)
    {
    case G_LOG_LEVEL_MESSAGE:
      msg_level = "Information" ;
      break ;
    case G_LOG_LEVEL_WARNING:
      msg_level = "Warning" ;
      break ;
    case G_LOG_LEVEL_CRITICAL:
      msg_level = "Critical" ;
      break ;
    case G_LOG_LEVEL_ERROR:
      msg_level = "Fatal" ;
      break ;
    default:
      zMapWarnIfReached() ;
      break ;
    }

  if (msg_level)
    {
      g_string_append_printf(format_str, "%s[%s:%s]\n",
                             ZMAPLOG_MESSAGE_TUPLE, msg_level, format) ;

      va_start(args, format) ;
      g_logv(domain, log_level, format_str->str, args) ;
      va_end(args) ;

      g_string_free(format_str, TRUE) ;
    }

  return ;
}



/* Logs current stack to logfile but note this only happens if compiled
 * with gnu glibc with backtrace facilities.
 */
void zMapLogStack(void)
{
  ZMapLog log = log_G;
  int log_fd = 0;

  /* zMapAssert(log); */
  /* zMapAssert(log->logging);*/

  if (!log || !log->logging)
    return ;

  OUR_MUTEX_LOCK ;

  if (log->active_handler.logfile &&
      (log_fd = g_io_channel_unix_get_fd(log->active_handler.logfile)))
    {
      zMapStack2fd(1, log_fd) ;
    }

  OUR_MUTEX_UNLOCK ;

  /*  if (!logged)
      zMapLogWarning("Failed to log stack trace to fd %d. "
      "Check availability of function backtrace()",
      log_fd);
  */
  return ;
}


gboolean zMapLogStop(void)
{
  gboolean result = FALSE ;
  ZMapLog log = log_G ;

  /* zMapAssert(log) ; */
  if (!log)
    return result ;

  OUR_MUTEX_LOCK ;

  result = stopLogging(log, FALSE) ;

  OUR_MUTEX_UNLOCK ;

  return result ;
}


/* This routine should be thread safe, it locks the mutex and then removes our handler so none
 * of our logging handlers can be called any more, it then unlocks the mutex and destroys it.
 * This should be ok because logging calls in the code go via g_log and know nothing about
 * the ZMapLog log struct.
 */
void zMapLogDestroy(void)
{
  ZMapLog log = log_G ;

  /* zMapAssert(log) ; */
  if (!log)
    return ;

  OUR_MUTEX_LOCK  ;

  stopLogging(log, TRUE) ;

  OUR_MUTEX_UNLOCK ;

  destroyLog(log) ;

  log_G = NULL ;

  return ;
}




/*
 *                Internal routines.
 */




static ZMapLog createLog(void)
{
  ZMapLog log ;

  log = g_new0(ZMapLogStruct, 1) ;

  /* Init the lock for ensuring single thread access to logger. */
  log->log_lock = &log_lock_G ;

  /* Default will be logging active. */
  log->logging = TRUE ;
  log->log_to_file = TRUE ;

  /* By default we log to a file, but if we don't specify a callback, glib logging will default
   * to stdout/err so logging can still be active without a file. Note that most of this gets
   * filled in later. */
  log->active_handler.cb_id = 0 ;
  log->active_handler.log_cb = fileLogger ;
  log->active_handler.log_path = NULL ;
  log->active_handler.logfile = NULL ;

  /* inactive handler will be our nullLogger routine. */
  log->inactive_handler.cb_id = 0 ;
  log->inactive_handler.log_cb = nullLogger ;
  log->inactive_handler.log_path = NULL ;
  log->inactive_handler.logfile = NULL ;

  log->userid = g_strdup(g_get_user_name()) ;

  log->nodeid = (char *)g_malloc0(MAXHOSTNAMELEN + 2) ;    /* + 2 for safety, interface not clear. */
  if (gethostname(log->nodeid, (MAXHOSTNAMELEN + 1)) == -1)
    {
      zMapLogCritical("%s", "Cannot retrieve hostname for this computer.") ;
      sprintf(log->nodeid, "%s", "**NO-NODEID**") ;
    }

  log->pid = getpid() ;

  return log ;
}


static void destroyLog(ZMapLog log)
{
  if (log->active_handler.log_path)
    g_free(log->active_handler.log_path) ;

  g_free(log->userid) ;

  g_free(log->nodeid) ;

  OUR_MUTEX_CLEAR ;

  g_free(log) ;

  return ;
}



/* We start and stop logging by swapping from a logging routine that writes to file with one
 * that does nothing. */
static gboolean startLogging(ZMapLog log, GError **error)
{
  gboolean result = FALSE ;
  GError *g_error = NULL ;

  if (!(log->logging))
    {
      /* If logging inactive then remove our inactive handler. */
      if (log->inactive_handler.cb_id)
        {
          g_log_remove_handler(ZMAPLOG_DOMAIN, log->inactive_handler.cb_id) ;
          log->inactive_handler.cb_id = 0 ;
        }

      /* Only need to do something if we are logging to a file. */
      if (log->log_to_file)
        {
          if (openLogFile(log, &g_error))
            {
              log->active_handler.cb_id = g_log_set_handler(ZMAPLOG_DOMAIN,
                                                            (GLogLevelFlags)(G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
                                                                             | G_LOG_FLAG_RECURSION),
                                                            log->active_handler.log_cb,
                                                            (gpointer)log) ;

              /* try to get the glib critical errors handled */
              if (log->catch_glib)
                g_log_set_default_handler(glibLogger, (gpointer) log);
              result = TRUE ;
            }
        }
      else
        result = TRUE ;

      if (result)
        log->logging = TRUE ;
    }

  if (g_error)
    g_propagate_error(error, g_error) ;

  return result ;
}

/* Error handling is crap here.... */
static gboolean stopLogging(ZMapLog log, gboolean remove_all_handlers)
{
  gboolean result = FALSE ;

  if (log->logging)
    {
      /* If we are logging to a file then remove our handler routine and close the file. */
      if (log->log_to_file)
        {
          g_log_remove_handler(ZMAPLOG_DOMAIN, log->active_handler.cb_id) ;
          log->active_handler.cb_id = 0 ;

          /* try to get the glib critical errors handled */
          if (log->catch_glib)
            g_log_set_default_handler(g_log_default_handler, NULL);

          /* Need to close the log file here. */
          if (!closeLogFile(log))
            result = FALSE ;
        }


      /* If we just want to suspend logging then we install our own null handler which does
       * nothing, hence logging stops. If we want to stop all our logging then we do not install
       * our null handler. */
      if (!remove_all_handlers)
        {
          log->inactive_handler.cb_id = g_log_set_handler(ZMAPLOG_DOMAIN,
                                                          (GLogLevelFlags)(G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
                                                                           | G_LOG_FLAG_RECURSION),
                                                          log->inactive_handler.log_cb,
                                                          (gpointer)log) ;

          /* try to get the glib critical errors handled */
          if (log->catch_glib)
            g_log_set_default_handler(g_log_default_handler, NULL);
        }

      if (result)
        log->logging = FALSE ;
    }
  else
    {
      if (remove_all_handlers && log->inactive_handler.cb_id)
        {
          g_log_remove_handler(ZMAPLOG_DOMAIN, log->inactive_handler.cb_id) ;
          log->inactive_handler.cb_id = 0 ;
          /* try to get the glib critical errors handled */

          if (log->catch_glib)
            g_log_set_default_handler(g_log_default_handler, NULL);

        }
    }

  return result ;
}



/* Read the configuration information logging and set up the log. */
static gboolean configureLog(ZMapLog log, GError **error)
{
  gboolean result = FALSE ;
  GError *g_error = NULL ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  if ((result = getLogConf(log)))
    {
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

      /* This seems stupid, but the start/stopLogging routines need logging to be in the
       * opposite state so we negate the current logging state before calling them,
       * only pertains to initialisation. */
      log->logging = !(log->logging) ;

      if (log->logging)
        {

          result = stopLogging(log, FALSE) ;
        }
      else
        {
          /* If we are logging to a file then we will log via our own routine,
           * otherwise by default glib will log to stdout/err. */
          if (log->log_to_file)
            {
              log->active_handler.log_cb = fileLogger ;
            }

          result = startLogging(log, &g_error) ;
        }

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
    }
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  if (g_error)
    g_propagate_error(error, g_error) ;

  return result ;
}


static gboolean openLogFile(ZMapLog log, GError **error)
{
  gboolean result = FALSE ;
  GError *g_error = NULL ;

  /* Must be opened for appending..... */
  if ((log->active_handler.logfile = g_io_channel_new_file(log->active_handler.log_path,
                                                           "a", &g_error)))
    {
      result = TRUE ;
    }
  else
    {
      result = FALSE ;
      g_set_error(&g_error, ZMAP_UTILS_ERROR, ZMAPUTILS_ERROR_GET_LOG,
                  "Cannot access log file '%s'",
                  log->active_handler.log_path) ;
    }

  if (g_error)
    g_propagate_error(error, g_error) ;

  return result ;
}



static gboolean closeLogFile(ZMapLog log)
{
  gboolean result = FALSE ;
  GIOStatus g_status ;
  GError *g_error = NULL ;

  if ((g_status = g_io_channel_shutdown(log->active_handler.logfile, TRUE, &g_error))
      != G_IO_STATUS_NORMAL)
    {
      /* disaster....need to report the error..... */


    }
  else
    {
      g_io_channel_unref(log->active_handler.logfile) ;
      result = TRUE ;
    }

  return result ;
}


/* Write out a start or stop record. */
static void writeStartOrStopMessage(int start)
{
  char *time_str ;

  time_str = zMapGetTimeString(ZMAPTIME_STANDARD, NULL) ;

  zMapLogMessage("****  %s  Session %s %s  ****", zMapGetAppTitle(), (start ? "started" : "stopped"), time_str) ;

  g_free(time_str) ;

  return ;
}



/* BIG QUESTION HERE...ARE WE SUPPOSED TO FREE THE MESSAGE FROM WITHIN THIS ROUTINE OR IS THAT
 * DONE BY THE glog calling code ?? Docs say nothing, perhaps need to look in code..... */


/* We use this routine to "stop" logging because it doesn't do anything so no log records are
 * output. When we want to restart logging we simply restore the fileLogger() routine as the
 * logging routine. */
static void nullLogger(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message,
                       gpointer user_data)
{


  return ;
}


/* This routine is called by the g_log package as a callback to do the actual logging. Note that
 * we have to lock and unlock a mutex here because g_log, although it mutex locks its own stuff
 * does not lock this routine...STUPID...they should have an option to do this if they don't
 * like having it on all the time. */
static void fileLogger(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message_in,
                       gpointer user_data)
{
  ZMapLog log = (ZMapLog)user_data ;
  GError *g_error = NULL ;
  gsize bytes_written = 0 ;
  gchar *message = (gchar *)message_in ;
  const gchar *bad_char_out = NULL ;


  zMapReturnIfFail((log && log->logging && message_in && *message_in)) ;


  /* Must make sure it's UTF8 or g_io_channel_write_chars will crash,
   * for now just replace the bad character with a question mark */
  while (!g_utf8_validate(message_in, -1, &bad_char_out))
    {
      char *bad_char = (char *)bad_char_out ;

      *bad_char='?';
    }

  /* glib logging routines are not thread safe so must lock here. */
  OUR_MUTEX_LOCK ;

  if ((g_io_channel_write_chars(log->active_handler.logfile, message, -1, &bytes_written, &g_error)
       != G_IO_STATUS_NORMAL)
      || (g_io_channel_flush(log->active_handler.logfile, &g_error) != G_IO_STATUS_NORMAL))
    {
      g_log_remove_handler(ZMAPLOG_DOMAIN, log->active_handler.cb_id) ;
      log->active_handler.cb_id = 0 ;

      zMapLogCritical("Unable to log to file %s, logging to this file has been turned off.",
                      log->active_handler.log_path) ;
    }


  /* now unlock. */
  OUR_MUTEX_UNLOCK ;

  return ;
}


static void glibLogger(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message,
                       gpointer user_data)
{
  log_G->active_handler.log_cb(log_domain,log_level,message,user_data);
  if (log_G->echo_glib)
    printf("**** %s ****\n",message);

  if (log_level <= G_LOG_LEVEL_WARNING)
    zMapLogStack();   /* we really do want to know where these come from */

  return ;
}





#if FOO_LOG
static void logTime(int what, int how)
{
  int stuff[] = { TIMER_EXPOSE, TIMER_UPDATE, TIMER_DRAW };

  /* indexed by what */
  /* foo canvas does not have our defines */
  if (what >= 0 && what < 3)
    {
      what = stuff[what];
      zMapLogTime(what,how,0,"");
    }

  return ;
}

#endif



// Find out where function name is in the function prototype, note that the prototype we are
// expecting is a string in g++ format:
//
// "gboolean zmapMainMakeAppWindow(int, char**, ZMapAppContext, GList*)"
//
// Add support for other compilers as needed.
//
static bool getFuncNamePosition(const char *function_prototype, char **start_out, unsigned int *len_out)
{
  bool result = false ;

#ifdef __GNUC__
  const char *start ;
  const char *end ;

  // Jump to function name
  start = strchr(function_prototype, ' ') ;
  start++ ;

  // Jump to args opening bracket
  end = strchr(function_prototype, '(') ;
  end-- ;


  *start_out = (char *)start ;
  *len_out = (end - start) + 1 ;

  result = true ;
#endif /* __GNUC__ */

  return result ;
}
