/*  File: zmapViewRemoteRequests.c
 *  Author: Roy Storey (rds@sanger.ac.uk)
 *  Copyright (c) 2006-2012: Genome Research Ltd.
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
 * Description: Receives and interprets xremote messages delivered from
 *              a peer application set up to interact with zmap.
 *
 * Exported functions: See zmapView_P.h
 *
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.h>

#include <string.h>

#include <ZMap/zmapGFF.h>
#include <ZMap/zmapUtils.h>
#include <ZMap/zmapUtilsXRemote.h>
#include <ZMap/zmapGLibUtils.h>
#include <ZMap/zmapXML.h>
#include <zmapView_P.h>
#include <ZMap/zmapConfigStanzaStructs.h>

#define VIEW_POST_EXECUTE_DATA "xremote_post_execute_data"

typedef enum
  {
    ZMAPVIEW_REMOTE_INVALID,
    /* Add below here... */

    ZMAPVIEW_REMOTE_FIND_FEATURE,
    ZMAPVIEW_REMOTE_CREATE_FEATURE,
    ZMAPVIEW_REMOTE_DELETE_FEATURE,

    ZMAPVIEW_REMOTE_HIGHLIGHT_FEATURE,
    ZMAPVIEW_REMOTE_HIGHLIGHT2_FEATURE,
    ZMAPVIEW_REMOTE_UNHIGHLIGHT_FEATURE,

    ZMAPVIEW_REMOTE_ZOOM_TO,

    ZMAPVIEW_REMOTE_GET_MARK,
    ZMAPVIEW_REMOTE_GET_FEATURE_NAMES,
    ZMAPVIEW_REMOTE_LOAD_FEATURES,

    ZMAPVIEW_REMOTE_REGISTER_CLIENT,
    ZMAPVIEW_REMOTE_LIST_WINDOWS,
    ZMAPVIEW_REMOTE_NEW_WINDOW,

    ZMAPVIEW_REMOTE_DUMP_CONTEXT,

    /* ...but above here */
    ZMAPVIEW_REMOTE_UNKNOWN
  } ZMapViewValidXRemoteActions ;



/* Action descriptor. */
typedef struct
{

  ZMapViewValidXRemoteActions action ;

  gboolean is_edit_action ;				    /* Does this action edit the feature
							       context ? */


  gboolean is_feature_action ;				    /* Does action work on a feature ? */
  gboolean create_feature_action ;			    /* Does this action create an edit feature ? */

  /* THIS DOESN'T EVEN LOOK SUPPORTED...SIGH.... */
  gboolean must_exist ;					    /* Does the target of the action need
							       to exist ? e.g. columns may be
							       dynamically loaded so may not exist yet. */

} ActionDescriptorStruct, *ActionDescriptor ;



/* Control block for all the remote actions. */
typedef struct RequestDataStructName
{
  int code ;
  gboolean handled ;
  GString *messages ;

  ZMapView           view;

  /* some operations are "read only" and hence can use the orig_context, others require edits and
   * must create an edit_context to do these. */

  /* Note that only orig_context stays the same throughout processing of a request, other elements
   * will change if more than one align/block or whatever is specified in the request. */
  ZMapFeatureContext orig_context ;
  ZMapFeatureAlignment orig_align;
  ZMapFeatureBlock orig_block;
  ZMapFeatureSet orig_feature_set;
  ZMapFeature orig_feature;

  /* edit_context is constructed to carry out context changes. */
  gboolean is_edit_action ;
  ZMapFeatureContext edit_context ;
  ZMapFeatureAlignment edit_align;
  ZMapFeatureBlock edit_block;
  ZMapFeatureSet edit_feature_set;
  ZMapFeature edit_feature;

  GQuark source_id ;
  GQuark style_id ;
  ZMapFeatureTypeStyle style ;

  GList *feature_list;

  GList *locations ;

  char *filename;
  char *format;

  gboolean use_mark ;
  GList *feature_sets ;

  gboolean zoomed ;

  int start, end ;

} RequestDataStruct, *RequestData ;



typedef struct
{
  ZMapViewValidXRemoteActions action;
  ZMapFeatureContext edit_context;

  ZMapFeature edit_feature ;
  GList *feature_list ;

} PostExecuteDataStruct, *PostExecuteData;


static char *view_execute_command(char *command_text, gpointer user_data,
				  ZMapXRemoteStatus *statusCode, ZMapXRemoteObj owner);
static char *view_post_execute(char *command_text, gpointer user_data,
			       ZMapXRemoteStatus *statusCode, ZMapXRemoteObj owner);
static void delete_failed_make_message(gpointer list_data, gpointer user_data);
static gboolean drawNewFeatures(ZMapView view, RequestData input_data);
static void getChildWindowXID(ZMapView view, RequestData input_data);
static void viewDumpContextToFile(ZMapView view, RequestData input_data);
static gboolean sanityCheckContext(ZMapView view, RequestData input_data);
static void draw_failed_make_message(gpointer list_data, gpointer user_data);
static gint matching_unique_id(gconstpointer list_data, gconstpointer user_data);
static ZMapFeatureContextExecuteStatus delete_from_list(GQuark key, gpointer data, gpointer user_data, char **error_out);
static ZMapFeatureContextExecuteStatus mark_matching_invalid(GQuark key, gpointer data, gpointer user_data, char **error_out);
static ZMapFeatureContextExecuteStatus sanity_check_context(GQuark key, gpointer data, gpointer user_data, char **error_out);

static void zoomWindowToFeature(ZMapView view, RequestData input_data) ;

static void reportWindowMark(ZMapView view, RequestData input_data) ;
static void loadFeatures(ZMapView view, RequestData input_data) ;

static void createClient(ZMapView view, ZMapXRemoteParseCommandData input_data);
static void eraseFeatures(ZMapView view, RequestData input_data);

static gboolean xml_zmap_start_cb(gpointer user_data, ZMapXMLElement zmap_element, ZMapXMLParser parser);
static gboolean xml_request_start_cb(gpointer user_data, ZMapXMLElement zmap_element, ZMapXMLParser parser);
static gboolean xml_request_end_cb(gpointer user_data, ZMapXMLElement zmap_element, ZMapXMLParser parser);
static gboolean xml_export_start_cb(gpointer user_data, ZMapXMLElement zmap_element, ZMapXMLParser parser);
static gboolean xml_align_start_cb(gpointer user_data, ZMapXMLElement zmap_element, ZMapXMLParser parser);
static gboolean xml_block_start_cb(gpointer user_data, ZMapXMLElement zmap_element, ZMapXMLParser parser);
static gboolean xml_featureset_start_cb(gpointer user_data, ZMapXMLElement zmap_element, ZMapXMLParser parser);
static gboolean xml_featureset_end_cb(gpointer user_data, ZMapXMLElement zmap_element, ZMapXMLParser parser);
static gboolean xml_feature_start_cb(gpointer user_data, ZMapXMLElement zmap_element, ZMapXMLParser parser);
static gboolean xml_feature_end_cb(gpointer user_data, ZMapXMLElement zmap_element, ZMapXMLParser parser);
static gboolean xml_subfeature_end_cb(gpointer user_data, ZMapXMLElement zmap_element, ZMapXMLParser parser);

static gboolean xml_return_true_cb(gpointer user_data, ZMapXMLElement zmap_element, ZMapXMLParser parser);

static void getFeatureNames(ZMapView view, RequestData input_data) ;
static void findUniqueCB(gpointer data, gpointer user_data) ;
static void makeUniqueListCB(gpointer key, gpointer value, gpointer user_data) ;
static void copyAddFeature(gpointer key, gpointer value, gpointer user_data) ;

static void setWindowXremote(ZMapView view) ;
static void setXremoteCB(gpointer list_data, gpointer user_data) ;

static gboolean executeRequest(ZMapXMLParser parser, ZMapXRemoteParseCommandData input_data) ;



/*
 *               Globals
 */

/* Descriptor table of action attributes */
static ActionDescriptorStruct action_table_G[] =
  {
    {ZMAPVIEW_REMOTE_INVALID,             FALSE, FALSE, FALSE, FALSE},

    {ZMAPVIEW_REMOTE_FIND_FEATURE,        FALSE, TRUE, FALSE, TRUE},

    {ZMAPVIEW_REMOTE_CREATE_FEATURE,      TRUE, TRUE, TRUE, FALSE},
    {ZMAPVIEW_REMOTE_DELETE_FEATURE,      TRUE, TRUE, TRUE, TRUE},

    {ZMAPVIEW_REMOTE_HIGHLIGHT_FEATURE,   FALSE, TRUE, FALSE, TRUE},
    {ZMAPVIEW_REMOTE_HIGHLIGHT2_FEATURE,  FALSE, TRUE, FALSE, TRUE},
    {ZMAPVIEW_REMOTE_UNHIGHLIGHT_FEATURE, FALSE, TRUE, FALSE, TRUE},
    {ZMAPVIEW_REMOTE_ZOOM_TO,             FALSE, TRUE, FALSE, TRUE},

    {ZMAPVIEW_REMOTE_GET_MARK,            FALSE, FALSE, FALSE, FALSE},

    {ZMAPVIEW_REMOTE_GET_FEATURE_NAMES,   FALSE, TRUE, FALSE, TRUE},

    {ZMAPVIEW_REMOTE_LOAD_FEATURES,       TRUE,  FALSE, FALSE, FALSE},
    {ZMAPVIEW_REMOTE_REGISTER_CLIENT,     FALSE, FALSE, FALSE, FALSE},

    {ZMAPVIEW_REMOTE_LIST_WINDOWS,        FALSE, FALSE, FALSE, FALSE},
    {ZMAPVIEW_REMOTE_NEW_WINDOW,          FALSE, FALSE, FALSE, FALSE},
    {ZMAPVIEW_REMOTE_DUMP_CONTEXT,        FALSE, FALSE, FALSE, FALSE},

    {ZMAPVIEW_REMOTE_INVALID,             FALSE, FALSE, FALSE, FALSE}
  } ;



static ZMapXMLObjTagFunctionsStruct view_starts_G[] =
  {
    { "zmap",       xml_zmap_start_cb                  },
    { "request",    xml_request_start_cb               },
    { "client",     zMapXRemoteXMLGenericClientStartCB },
    { "export",     xml_export_start_cb                },
    { "align",      xml_align_start_cb                 },
    { "block",      xml_block_start_cb                 },
    { "featureset", xml_featureset_start_cb            },
    { "feature",    xml_feature_start_cb               },
    {NULL, NULL}
  };

static ZMapXMLObjTagFunctionsStruct view_ends_G[] =
  {
    { "zmap",       xml_return_true_cb    },
    { "request",    xml_request_end_cb    },
    { "align",      xml_return_true_cb    },
    { "block",      xml_return_true_cb    },
    { "featureset", xml_featureset_end_cb },
    { "feature",    xml_feature_end_cb    },
    { "subfeature", xml_subfeature_end_cb },
    {NULL, NULL}
  } ;


/* Must match ZMapViewValidXRemoteActions */
static char *actions_G[ZMAPVIEW_REMOTE_UNKNOWN + 1] =
  {
  NULL,
  "find_feature", "create_feature", "delete_feature",

  "single_select", "multiple_select", "unselect",

  "zoom_to",

  "get_mark", "get_feature_names", "load_features",

  "register_client", "list_windows", "new_window",

  "export_context",
  NULL
};






/*
 *                         External routines.
 */



/* Where is all starts from. Everything else should be static */
void zmapViewSetupXRemote(ZMapView view, GtkWidget *widget)
{
  zMapXRemoteInitialiseWidgetFull(widget, PACKAGE_NAME,
				  ZMAP_DEFAULT_REQUEST_ATOM_NAME,
				  ZMAP_DEFAULT_RESPONSE_ATOM_NAME,
				  view_execute_command,
				  view_post_execute,
				  view);

  return ;
}


char *zMapViewRemoteReceiveAccepts(ZMapView view)
{
  char *xml = NULL;

  xml = zMapXRemoteClientAcceptsActionsXML(zMapXRemoteWidgetGetXID(view->xremote_widget),
                                           &actions_G[ZMAPVIEW_REMOTE_INVALID + 1],
                                           ZMAPVIEW_REMOTE_UNKNOWN - 1);

  return xml;
}




/*
 *                       Internal functions.
 */



/* The ZMapXRemoteCallback */
static char *view_execute_command(char *command_text, gpointer user_data,
				  ZMapXRemoteStatus *status_code, ZMapXRemoteObj owner)
{
  char *response = NULL ;
  ZMapXMLParser parser;
  ZMapXRemoteParseCommandDataStruct input = { NULL };
  RequestDataStruct request_data = {0};
  ZMapView view = (ZMapView)user_data;
  gboolean ping ;

  ping = zMapXRemoteIsPingCommand(command_text, status_code, &response) ;


  if (!ping)
    zMapLogMessage("New xremote command received: %s", command_text) ;


  zMapDebugPrint(xremote_debug_GG, "ZMap App View Handler: %s",  command_text) ;


  request_data.messages = g_string_sized_new(512) ;

  request_data.view = view ;
  input.user_data = &request_data ;

  parser = zMapXMLParserCreate(&input, FALSE, FALSE) ;

  zMapXMLParserSetMarkupObjectTagHandlers(parser, &view_starts_G[0], &view_ends_G[0]) ;


  /* When the buffer is parsed all the start/end handlers get called so quite a lot of stuff
   * is set up by those functions, the request(s) are then serviced in the xml request end
   * handler. */
  if (!(zMapXMLParserParseBuffer(parser, command_text, strlen(command_text))))

    {
      response = g_strdup(zMapXMLParserLastErrorMsg(parser)) ;
    }

  zMapXMLParserDestroy(parser);


  *status_code = request_data.code ;

  response = g_string_free(request_data.messages, FALSE) ;


  /* UGH, this code is horrible...be sure to fix with new xremote.... */
  if (!zMapXRemoteValidateStatusCode(status_code) && response != NULL)
    {
      zMapLogWarning("%s", response);
      g_free(response);

      response = g_strdup("Broken code. Check zmap.log file");
    }

  if (response == NULL)
    {
      response = g_strdup("Broken code.") ;
    }

  if (!ping)
    zMapLogMessage("New xremote command processed: %s", command_text) ;


  return response;
}


static gboolean executeRequest(ZMapXMLParser parser, ZMapXRemoteParseCommandData input)
{
  gboolean result = FALSE ;
  RequestData request_data = input->user_data ;
  ZMapView view = request_data->view ;
  char *response = NULL ;

  request_data->code = 0 ;


  /* We need to set the window somehow....   */

  switch (input->common.action)
    {
    case ZMAPVIEW_REMOTE_REGISTER_CLIENT:
      createClient(view, input);
      break;

    case ZMAPVIEW_REMOTE_ZOOM_TO:
      zoomWindowToFeature(view, request_data);
      break;

    case ZMAPVIEW_REMOTE_GET_MARK:
      reportWindowMark(view, request_data);
      break;

    case ZMAPVIEW_REMOTE_GET_FEATURE_NAMES:
      getFeatureNames(view, request_data) ;
      break ;

    case ZMAPVIEW_REMOTE_LOAD_FEATURES:
      loadFeatures(view, request_data) ;
      break ;

    case ZMAPVIEW_REMOTE_FIND_FEATURE:
      break ;

    case ZMAPVIEW_REMOTE_DELETE_FEATURE:
      {
	eraseFeatures(view, request_data) ;
	break ;
      }

    case ZMAPVIEW_REMOTE_CREATE_FEATURE:
      {
	/* why is context only sanity checked for this operation and not for others...???? */

	if (sanityCheckContext(view, request_data))
	  {
	    if (drawNewFeatures(view, request_data)
		&& (view->xremote_widget && request_data->edit_context))
	      {
		/* Copy some of request_data into post_data for view_post_execute(). */
		PostExecuteData post_data = g_new0(PostExecuteDataStruct, 1);

		post_data->action = input->common.action;
		post_data->edit_context = request_data->edit_context;
		post_data->edit_feature = request_data->edit_feature ;
		post_data->feature_list = request_data->feature_list ;


		g_object_set_data(G_OBJECT(view->xremote_widget),
				  VIEW_POST_EXECUTE_DATA,
				  post_data);


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
		/* Need to highlight created feature.....in all windows..... */
		list_item = g_list_first(view->window_list) ;
		do
		  {
		    ZMapViewWindow view_window ;
		    ZMapFeature feature ;

		    if (request_data->edit_feature)
		      feature = request_data->edit_feature ;
		    else
		      feature = (ZMapFeature)(request_data->feature_list->data) ;

		    view_window = list_item->data ;

		    zMapWindowFeatureSelect(view_window->window, feature) ;
		  }
		while ((list_item = g_list_next(list_item))) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


	      }

	    request_data->edit_context = NULL;
	  }
	break;
      }
    case ZMAPVIEW_REMOTE_LIST_WINDOWS:
      {
	getChildWindowXID(view, request_data);
	break;
      }
    case ZMAPVIEW_REMOTE_DUMP_CONTEXT:
      {
	viewDumpContextToFile(view, request_data);
	break;
      }

      /* errrrrrr....what's going on here...... */
    case ZMAPVIEW_REMOTE_UNHIGHLIGHT_FEATURE:
    case ZMAPVIEW_REMOTE_HIGHLIGHT_FEATURE:
    case ZMAPVIEW_REMOTE_HIGHLIGHT2_FEATURE:
    case ZMAPVIEW_REMOTE_NEW_WINDOW:
      //newWindowForView(view, input, &output_data);
      //break;
    case ZMAPVIEW_REMOTE_INVALID:
    case ZMAPVIEW_REMOTE_UNKNOWN:
    default:
      g_string_append_printf(request_data->messages, "%s", "Unknown command") ;
      request_data->code = ZMAPXREMOTE_UNKNOWNCMD ;
      break;
    }


  if (request_data->edit_context && !ZMAPVIEW_REMOTE_DELETE_FEATURE)
    zMapFeatureContextDestroy(request_data->edit_context, TRUE) ;


  if (request_data->code == ZMAPXREMOTE_OK)
    {
      result = TRUE ;
    }
  else
    {
      result = FALSE ;

      zMapXMLParserRaiseParsingError(parser, response) ;
    }



  return result ;
}




static char *view_post_execute(char *command_text, gpointer user_data,
			       ZMapXRemoteStatus *statusCode, ZMapXRemoteObj owner)
{
  char *response = NULL ;
  ZMapView view = (ZMapView)user_data;
  PostExecuteData post_data;
  gboolean status;

  if (view->xremote_widget)
    {
      /* N.B. We _steal_ the g_object data here! */
      if ((post_data = g_object_steal_data(G_OBJECT(view->xremote_widget), VIEW_POST_EXECUTE_DATA)))
	{
	  /* I DON'T UNDERSTAND WHY ONLY CREATE IS HANDLED HERE.... */

	  switch(post_data->action)
	    {
	    case ZMAPVIEW_REMOTE_CREATE_FEATURE:
	      {
		GList* list_item ;
		ZMapFeature feature = NULL ;

		if (post_data->edit_feature)
		  feature = post_data->edit_feature ;
		else
		  feature = (ZMapFeature)(post_data->feature_list->data) ;

		status = zmapViewDrawDiffContext(view, &(post_data->edit_context), feature) ;

		if(!status)
		  post_data->edit_context = NULL; /* So the view->features context doesn't get destroyed */

		if (post_data->edit_context)
		  zMapFeatureContextDestroy(post_data->edit_context, TRUE);

		/* Need to highlight created feature.....in all windows..... */
		list_item = g_list_first(view->window_list) ;
		do
		  {
		    ZMapViewWindow view_window ;
		    ZMapFeature feature ;

		    if (post_data->edit_feature)
		      feature = post_data->edit_feature ;
		    else
		      feature = (ZMapFeature)(post_data->feature_list->data) ;

		    view_window = list_item->data ;

		    zMapWindowFeatureSelect(view_window->window, feature) ;
		  }
		while ((list_item = g_list_next(list_item))) ;


		break;
	      }
	    default:
	      {
		zMapAssertNotReached();
		break;
	      }
	    }

	  g_free(post_data);
	}
    }

  return response ;
}




static void createClient(ZMapView view, ZMapXRemoteParseCommandData input)
{
  ZMapXRemoteObj client;
  ClientParameters client_params = &(input->common.client_params);
  RequestData request_data = input->user_data ;
  char *format_response = "<client xwid=\"0x%lx\" created=\"%d\" exists=\"%d\" />";
  int created, exists;

  if (!(view->xremote_client) && (client = zMapXRemoteNew(GDK_DISPLAY())) != NULL)
    {
      zMapXRemoteInitClient(client, client_params->xid) ;
      zMapXRemoteSetRequestAtomName(client, (char *)g_quark_to_string(client_params->request)) ;
      zMapXRemoteSetResponseAtomName(client, (char *)g_quark_to_string(client_params->response)) ;

      view->xremote_client = client ;

      /* We need to tell any windows we have that there is now an external client. */
      setWindowXremote(view) ;

      created = 1 ;
      exists = 0 ;
    }
  else if (view->xremote_client)
    {
      created = 0;
      exists  = 1;
    }
  else
    {
      created = exists = 0;
    }

  g_string_append_printf(request_data->messages, format_response,
			 zMapXRemoteWidgetGetXID(view->xremote_widget), created, exists);

  request_data->code = ZMAPXREMOTE_OK;

  return;
}

static void getChildWindowXID(ZMapView view, RequestData request_data)
{

  if(view->state < ZMAPVIEW_LOADED)
    {
      request_data->code = ZMAPXREMOTE_PRECOND;
      g_string_append_printf(request_data->messages, "%s",
                             "view isn't loaded yet");
    }
  else
    {
      GList *list_item;

      list_item = g_list_first(view->window_list);

      do
        {
          ZMapViewWindow view_window;
          ZMapWindow window;
          char *client_xml = NULL;

          view_window = (ZMapViewWindow)list_item->data;
          window      = zMapViewGetWindow(view_window);
          client_xml  = zMapWindowRemoteReceiveAccepts(window);

          g_string_append_printf(request_data->messages,
                                 "%s", client_xml);
          if(client_xml)
            g_free(client_xml);
        }
      while((list_item = g_list_next(list_item))) ;

      request_data->code = ZMAPXREMOTE_OK;
    }

  return ;
}

static void viewDumpContextToFile(ZMapView view, RequestData request_data)
{
  GIOChannel *file = NULL;
  GError *error = NULL;
  char *filepath = NULL;

  filepath = request_data->filename;

  if(!(file = g_io_channel_new_file(filepath, "w", &error)))
    {
      request_data->code = ZMAPXREMOTE_UNAVAILABLE;
      request_data->handled = FALSE;
      if(error)
	g_string_append(request_data->messages, error->message);
    }
  else
    {
      char *format = NULL;
      gboolean result = FALSE;

      format = request_data->format;

      if(format && g_ascii_strcasecmp(format, "gff") == 0)
	result = zMapGFFDump((ZMapFeatureAny)view->features, view->context_map.styles, file, &error);
#ifdef STYLES_PRINT_TO_FILE
      else if(format && g_ascii_strcasecmp(format, "test-style") == 0)
	zMapFeatureTypePrintAll(view->features->styles, __FILE__);
#endif /* STYLES_PRINT_TO_FILE */
      else
	result = zMapFeatureContextDump(view->features, view->context_map.styles, file, &error);

      if(!result)
	{
	  request_data->code    = ZMAPXREMOTE_INTERNAL;
	  request_data->handled = FALSE;
	  if(error)
	    g_string_append(request_data->messages, error->message);
	}
      else
	{
	  request_data->code    = ZMAPXREMOTE_OK;
	  request_data->handled = TRUE;
	}
    }

  return ;
}

static gboolean sanityCheckContext(ZMapView view, RequestData request_data)
{
  gboolean features_are_sane = TRUE ;
  ZMapXRemoteStatus save_code ;

  save_code = request_data->code ;
  request_data->code = 0 ;

  zMapFeatureContextExecute((ZMapFeatureAny)(request_data->edit_context),
                            ZMAPFEATURE_STRUCT_FEATURE,
                            sanity_check_context,
                            request_data) ;

  if (request_data->code != 0)
    features_are_sane = FALSE ;
  else
    request_data->code = save_code ;

  return features_are_sane ;
}


/* client has asked us to erase features in a feature set, this could be anything from
 * a single feature to all features in the feature set. Note that we have to handle
 * there being no features in the feature set.
 */
static void eraseFeatures(ZMapView view, RequestData request_data)
{
  if (!(request_data->edit_feature) && !(request_data->feature_list))
    {
      /* What should we return here ? current xremote return codes not great for this.... */

      request_data->code = ZMAPXREMOTE_OK;

      request_data->handled = TRUE;
    }
  else
    {
      GList* list_item ;

      /* If the feature(s) are highlighted we need to unhighlight it in all windows first.... */
      list_item = g_list_first(view->window_list) ;
      do
	{
	  ZMapViewWindow view_window ;
	  ZMapFeature feature ;



	  /* We need to just have a list of features...not either an edit feature or a list.... */

	  /* I'm going to try a hack here...given all features should be unhighlighted perhaps
	   * we only need do the first if it's a list of features.... */
	  /* Also....sometimes the feature is in a list and sometimes on it's own....CHRIST....
	   * NEED TO SORT THIS OUT...SHOULD ALWAYS JUST HAVE A LIST....
	   * 
	   *  */
	  if (request_data->edit_feature)
	    feature = request_data->edit_feature ;
	  else
	    feature = (ZMapFeature)(request_data->feature_list->data) ;


	  view_window = list_item->data ;

	  /* If feature to be erased is highlighted then unhighlight it and tell our
	   * parent that only column is now selected. */
	  if (zMapWindowUnhighlightFeature(view_window->window, feature))
	    {
	      ZMapViewSelectStruct view_select = {0} ;
	      ZMapViewCallbacks view_cbs ;
	      ZMapFeatureDescStruct feature_desc = {ZMAPFEATURE_STRUCT_INVALID} ;

	      feature_desc.struct_type = ZMAPFEATURE_STRUCT_FEATURESET ;
	      feature_desc.feature_set = zMapWindowGetHotColumnName(view_window->window) ;
	      view_select.feature_desc = feature_desc ;

	      view_cbs = zmapViewGetCallbacks() ;

	      (*(view_cbs->select))(view_window, view_window->parent_view->app_data, &view_select) ;
	    }

	}
      while ((list_item = g_list_next(list_item))) ;

      /* OK, now get rid of feature from context. */
      zmapViewEraseFromContext(view, request_data->edit_context);

      zMapFeatureContextExecute((ZMapFeatureAny)(request_data->edit_context),
				ZMAPFEATURE_STRUCT_FEATURE,
				mark_matching_invalid,
				&(request_data->feature_list));

      request_data->code = 0;

      if (g_list_length(request_data->feature_list))
	g_list_foreach(request_data->feature_list, delete_failed_make_message, request_data);

      /* if delete_failed_make_message didn't change the code then all is ok */
      if (request_data->code == 0)
	request_data->code = ZMAPXREMOTE_OK;

      request_data->handled = TRUE;
    }


  return ;
}


static gboolean drawNewFeatures(ZMapView view, RequestData request_data)
{
  gboolean result = FALSE ;

  if (!(request_data->edit_context = zmapViewMergeInContext(view, request_data->edit_context)))
    {
      g_string_append(request_data->messages, "Merge of new feature into View feature context failed") ;
      request_data->code = ZMAPXREMOTE_FAILED ;
    }
  else
    {
      zMapFeatureContextExecute((ZMapFeatureAny)(request_data->edit_context),
				ZMAPFEATURE_STRUCT_FEATURE,
				delete_from_list,
				&(request_data->feature_list));

      zMapFeatureContextExecute((ZMapFeatureAny)(view->features),
				ZMAPFEATURE_STRUCT_FEATURE,
				mark_matching_invalid,
				&(request_data->feature_list));

      request_data->code = 0;

      if (g_list_length(request_data->feature_list))
	g_list_foreach(request_data->feature_list, draw_failed_make_message, request_data);

      if (request_data->code == 0)
	request_data->code = ZMAPXREMOTE_OK ;

      request_data->handled = TRUE ;

      result = TRUE ;
    }

  return result ;
}


static void draw_failed_make_message(gpointer list_data, gpointer user_data)
{
  ZMapFeatureAny feature_any = (ZMapFeatureAny)list_data;
  RequestData request_data = (RequestData)user_data;



  if (feature_any->struct_type == ZMAPFEATURE_STRUCT_INVALID)
    {
      request_data->code = ZMAPXREMOTE_CONFLICT; /* possibly the wrong code */

      g_string_append_printf(request_data->messages,
			     "Failed to draw feature '%s' [%s]. Feature already exists.\n",
			     (char *)g_quark_to_string(feature_any->original_id),
			     (char *)g_quark_to_string(feature_any->unique_id));
    }


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* I DON'T GET THIS, IF THE STRUCT IS VALID SURELY THE FEATURE GOT DRAWN ?? */

  else
    {
      g_string_append_printf(request_data->messages,
			     "Failed to draw feature '%s' [%s]. Unknown reason.\n",
			     (char *)g_quark_to_string(feature_any->original_id),
			     (char *)g_quark_to_string(feature_any->unique_id));
    }
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  return ;
}

static void delete_failed_make_message(gpointer list_data, gpointer user_data)
{
  ZMapFeatureAny feature_any = (ZMapFeatureAny)list_data;
  RequestData request_data = (RequestData)user_data;

  if (feature_any->struct_type == ZMAPFEATURE_STRUCT_INVALID)
    {
      request_data->code = ZMAPXREMOTE_CONFLICT; /* possibly the wrong code */

      g_string_append_printf(request_data->messages,
                             "Failed to delete feature '%s' [%s].\n",
                             (char *)g_quark_to_string(feature_any->original_id),
                             (char *)g_quark_to_string(feature_any->unique_id));
    }

  return ;
}

static gint matching_unique_id(gconstpointer list_data, gconstpointer user_data)
{
  gint match = -1;
  ZMapFeatureAny a = (ZMapFeatureAny)list_data, b = (ZMapFeatureAny)user_data ;

  match = !(a->unique_id == b->unique_id);

  return match;
}

static ZMapFeatureContextExecuteStatus delete_from_list(GQuark key,
                                                        gpointer data,
                                                        gpointer user_data,
                                                        char **error_out)
{
  ZMapFeatureAny any = (ZMapFeatureAny)data;
  GList **list = (GList **)user_data, *match;

  if (any->struct_type == ZMAPFEATURE_STRUCT_FEATURE)
    {
      if ((match = g_list_find_custom(*list, any, matching_unique_id)))
        {
          *list = g_list_remove(*list, match->data);
        }
    }

  return ZMAP_CONTEXT_EXEC_STATUS_OK;
}

static ZMapFeatureContextExecuteStatus mark_matching_invalid(GQuark key,
                                                             gpointer data,
                                                             gpointer user_data,
                                                             char **error_out)
{
  ZMapFeatureAny any = (ZMapFeatureAny)data;
  GList **list = (GList **)user_data, *match;

  if (any->struct_type == ZMAPFEATURE_STRUCT_FEATURE)
    {
      if ((match = g_list_find_custom(*list, any, matching_unique_id)))
        {
          any = (ZMapFeatureAny)(match->data);
          any->struct_type = ZMAPFEATURE_STRUCT_INVALID;
        }
    }

  return ZMAP_CONTEXT_EXEC_STATUS_OK;
}

static ZMapFeatureContextExecuteStatus sanity_check_context(GQuark key,
                                                            gpointer data,
                                                            gpointer user_data,
                                                            char **error_out)
{
  ZMapFeatureAny any = (ZMapFeatureAny)data;
  RequestData request_data = (RequestData)user_data;
  char *reason = NULL;

  if (!zMapFeatureAnyIsSane(any, &reason))
    {
      request_data->code = ZMAPXREMOTE_BADREQUEST;
      request_data->messages = g_string_append(request_data->messages, reason) ;

      if (reason)
	g_free(reason);
    }

  return ZMAP_CONTEXT_EXEC_STATUS_OK ;
}





/* ======================================================
 *                 XML_TAG_HANDLERS
 * ======================================================
 *
 * Decode messages like these:
 *
 * <zmap action="create_feature">
 *   <featureset name="Coding">
 *     <feature name="RP6-206I17.6-001" start="3114" end="99885" strand="-">
 *       <subfeature ontology="exon"   start="3114"  end="3505" />
 *       <subfeature ontology="intron" start="3505"  end="9437" />
 *       <subfeature ontology="exon"   start="9437"  end="9545" />
 *       <subfeature ontology="intron" start="9545"  end="18173" />
 *       <subfeature ontology="exon"   start="18173" end="19671" />
 *       <subfeature ontology="intron" start="19671" end="99676" />
 *       <subfeature ontology="exon"   start="99676" end="99885" />
 *       <subfeature ontology="cds"    start="1"     end="2210" />
 *     </feature>
 *   </featureset>
 * </zmap>
 *
 * <zmap action="shutdown" />
 *
 * <zmap action="new">
 *   <segment sequence="U.2969591-3331416" start="1" end="" />
 * </zmap>
 *
 *  <zmap action="delete_feature">
 *    <featureset name="polyA_site">
 *      <feature name="U.2969591-3331416" start="87607" end="87608" strand="+"score="0.000"></feature>
 *    </featureset>
 *  </zmap>
 *
 */


static gboolean xml_zmap_start_cb(gpointer user_data, ZMapXMLElement zmap_element, ZMapXMLParser parser)
{
  /* actions all moved into the response tag, zmap tag will probably take on other meanings. */


  return TRUE ;
}



static gboolean xml_request_start_cb(gpointer user_data, ZMapXMLElement set_element, ZMapXMLParser parser)
{
  gboolean result = FALSE ;
  ZMapXRemoteParseCommandData xml_data = (ZMapXRemoteParseCommandData)user_data;
  ZMapXMLAttribute attr = NULL;
  GQuark action = 0;

  if ((attr = zMapXMLElementGetAttributeByName(set_element, "action")) == NULL)
    {
      zMapXMLParserRaiseParsingError(parser, "action is a required attribute for zmap.");
      xml_data->common.action = ZMAPVIEW_REMOTE_INVALID;
    }
  else
    {
      int i;
      action = zMapXMLAttributeGetValue(attr);

      xml_data->common.action = ZMAPVIEW_REMOTE_INVALID;

      for (i = ZMAPVIEW_REMOTE_INVALID + 1; i < ZMAPVIEW_REMOTE_UNKNOWN; i++)
        {
          if (action == g_quark_from_string(actions_G[i]))
            xml_data->common.action = i ;
        }

      /* unless((action > INVALID) and (action < UNKNOWN)) */
      if (!(xml_data->common.action > ZMAPVIEW_REMOTE_INVALID && xml_data->common.action < ZMAPVIEW_REMOTE_UNKNOWN))
        {
          zMapLogWarning("action='%s' is unknown", g_quark_to_string(action));
          xml_data->common.action = ZMAPVIEW_REMOTE_UNKNOWN ;
        }
      else
	{
	  switch(xml_data->common.action)
	    {
	    case ZMAPVIEW_REMOTE_GET_FEATURE_NAMES:
	    case ZMAPVIEW_REMOTE_LOAD_FEATURES:
	    case ZMAPVIEW_REMOTE_ZOOM_TO:
	    case ZMAPVIEW_REMOTE_FIND_FEATURE:
	    case ZMAPVIEW_REMOTE_CREATE_FEATURE:
	    case ZMAPVIEW_REMOTE_DELETE_FEATURE:
	    case ZMAPVIEW_REMOTE_HIGHLIGHT_FEATURE:
	    case ZMAPVIEW_REMOTE_HIGHLIGHT2_FEATURE:
	    case ZMAPVIEW_REMOTE_UNHIGHLIGHT_FEATURE:
	      {
		RequestData request_data = (RequestData)(xml_data->user_data);

		request_data->orig_context = zMapViewGetFeatures(request_data->view) ;


		/* Record whether action changes the feature context. */
		request_data->is_edit_action = action_table_G[xml_data->common.action].is_edit_action ;


		/* For actions that change the feature context create an "edit" context. */
		if (request_data->is_edit_action)
		  request_data->edit_context
		    = (ZMapFeatureContext)zMapFeatureAnyCopy((ZMapFeatureAny)(request_data->orig_context)) ;


		if (xml_data->common.action == ZMAPVIEW_REMOTE_LOAD_FEATURES)
		  {
		    if ((attr = zMapXMLElementGetAttributeByName(set_element, "load")))
		      {
			GQuark load_id ;
			RequestData request_data = (RequestData)(xml_data->user_data);

			load_id = zMapXMLAttributeGetValue(attr) ;

			if (zMapLogQuarkIsStr(load_id, "mark"))
			  request_data->use_mark = TRUE ;
			else if (zMapLogQuarkIsStr(load_id, "full"))
			  request_data->use_mark = FALSE ;
			else
			  {
			    zMapLogWarning("Value \"%s\" for \"load\" attr is unknown.", g_quark_to_string(load_id));
			    xml_data->common.action = ZMAPVIEW_REMOTE_UNKNOWN;

			    result = FALSE ;
			  }
		      }
		  }

		result = TRUE ;

		break;
	      }

	    case ZMAPVIEW_REMOTE_GET_MARK:
	    case ZMAPVIEW_REMOTE_LIST_WINDOWS:
	    case ZMAPVIEW_REMOTE_REGISTER_CLIENT:
	    case ZMAPVIEW_REMOTE_NEW_WINDOW:
	    case ZMAPVIEW_REMOTE_DUMP_CONTEXT:
	      {
		result = TRUE ;
		break;
	      }

	    default:
	      {
		zMapLogWarning("Invalid XRemote action: %d, reset to ZMAPVIEW_REMOTE_INVALID",
			       xml_data->common.action) ;

		xml_data->common.action = ZMAPVIEW_REMOTE_INVALID;

		break;
	      }
	    }
	}
    }

  return result ;
}


static gboolean xml_request_end_cb(gpointer user_data, ZMapXMLElement set_element, ZMapXMLParser parser)
{
  gboolean result = FALSE ;
  ZMapXRemoteParseCommandData xml_data = (ZMapXRemoteParseCommandData)user_data ;
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  RequestData request_data = (RequestData)(xml_data->user_data) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  if (xml_data->common.action != ZMAPVIEW_REMOTE_INVALID)
    {
      result = executeRequest(parser, xml_data) ;
    }

  return result ;
}





static gboolean xml_export_start_cb(gpointer user_data, ZMapXMLElement export_element, ZMapXMLParser parser)
{
  ZMapXRemoteParseCommandData xml_data = (ZMapXRemoteParseCommandData)user_data;
  RequestData request_data = (RequestData)(xml_data->user_data);
  ZMapXMLAttribute attr = NULL;
  /* <export filename="" format="" /> */

  if((attr = zMapXMLElementGetAttributeByName(export_element, "filename")))
    request_data->filename = (char *)g_quark_to_string(zMapXMLAttributeGetValue(attr));
  else
    zMapXMLParserRaiseParsingError(parser, "filename is a required attribute for export.");

  if((attr = zMapXMLElementGetAttributeByName(export_element, "format")))
    request_data->format = (char *)g_quark_to_string(zMapXMLAttributeGetValue(attr));
  else
    request_data->format = "gff";

  return FALSE;
}



static gboolean xml_align_start_cb(gpointer user_data, ZMapXMLElement set_element, ZMapXMLParser parser)
{
  gboolean result = FALSE ;
  ZMapXMLAttribute attr = NULL;
  ZMapXRemoteParseCommandData xml_data = (ZMapXRemoteParseCommandData)user_data;
  RequestData request_data = (RequestData)(xml_data->user_data);
  GQuark align_id ;
  char *align_name = NULL ;

  if (xml_data->common.action != ZMAPVIEW_REMOTE_INVALID)
    {
      /* Look for an align name...doesn't have to be one. */
      if ((attr = zMapXMLElementGetAttributeByName(set_element, "name")))
	{
	  if ((align_id = zMapXMLAttributeGetValue(attr)))
	    align_name = (char *)g_quark_to_string(align_id) ;
	}

      if (align_name)
	{
	  gboolean master_align ;

	  request_data->orig_align = NULL ;

	  if ((request_data->orig_align
	       = zMapFeatureContextGetAlignmentByID(request_data->orig_context,
						    zMapFeatureAlignmentCreateID(align_name, TRUE))))
	    master_align = TRUE ;
	  else if ((request_data->orig_align
		    = zMapFeatureContextGetAlignmentByID(request_data->orig_context,
							 zMapFeatureAlignmentCreateID(align_name, FALSE))))
	    master_align = FALSE ;

	  if ((request_data->orig_align))
	    {
	      result = TRUE ;
	    }
	  else
	    {
	      /* If we can't find the align it's a serious error and we can't carry on. */
	      char *err_msg ;

	      err_msg = g_strdup_printf("Unknown Align \"%s\":  not found in original_context", align_name) ;
	      zMapXMLParserRaiseParsingError(parser, err_msg) ;
	      g_free(err_msg) ;

	      result = FALSE ;
	    }
	}
      else
	{
	  /* default to master align. */
	  request_data->orig_align = request_data->orig_context->master_align ;

	  result = TRUE ;
	}

      if (result && request_data->is_edit_action)
	{
	  request_data->edit_align
	    = (ZMapFeatureAlignment)zMapFeatureAnyCopy((ZMapFeatureAny)(request_data->orig_align)) ;

	  zMapFeatureContextAddAlignment(request_data->edit_context, request_data->edit_align, FALSE) ;
	}
    }

  return result ;
}



static gboolean xml_block_start_cb(gpointer user_data, ZMapXMLElement set_element, ZMapXMLParser parser)
{
  gboolean result = FALSE ;
  ZMapXMLAttribute attr = NULL;
  ZMapXRemoteParseCommandData xml_data = (ZMapXRemoteParseCommandData)user_data;
  RequestData request_data = (RequestData)(xml_data->user_data);
  GQuark block_id ;
  char *block_name = NULL ;

  if (xml_data->common.action != ZMAPVIEW_REMOTE_INVALID)
    {
      if ((attr = zMapXMLElementGetAttributeByName(set_element, "name")))
	{
	  if ((block_id = zMapXMLAttributeGetValue(attr)))
	    block_name = (char *)g_quark_to_string(block_id) ;
	}

      if (block_name)
	{
	  int ref_start, ref_end, non_start, non_end;
	  ZMapStrand ref_strand, non_strand;

	  if (!zMapFeatureBlockDecodeID(block_id, &ref_start, &ref_end, &ref_strand,
					&non_start, &non_end, &non_strand))
	    {
	      /* Bad format block name. */
	      char *err_msg ;

	      err_msg = g_strdup_printf("Bad Format Block name: \"%s\"", block_name) ;
	      zMapXMLParserRaiseParsingError(parser, err_msg) ;
	      g_free(err_msg) ;

	      result = FALSE ;
	    }
	  else if (!(request_data->orig_block
		     = zMapFeatureAlignmentGetBlockByID(request_data->orig_align, block_id)))
	    {
	      /* If we can't find the block it's a serious error and we can't carry on. */
	      char *err_msg ;

	      err_msg = g_strdup_printf("Unknown Block \"%s\":  not found in original_context", block_name) ;
	      zMapXMLParserRaiseParsingError(parser, err_msg) ;
	      g_free(err_msg) ;

	      result = FALSE ;
	    }
	  else
	    {
	      result = TRUE ;
	    }
	}
      else
	{
	  /* Get the first one! */
	  request_data->orig_block = zMap_g_hash_table_nth(request_data->orig_context->master_align->blocks, 0) ;

	  result = TRUE ;
	}

      if (result && request_data->is_edit_action)
	{
	  request_data->edit_block = (ZMapFeatureBlock)zMapFeatureAnyCopy((ZMapFeatureAny)(request_data->orig_block)) ;

	  zMapFeatureAlignmentAddBlock(request_data->edit_align, request_data->edit_block) ;

	  result = TRUE ;
	}
    }

  return result ;
}




static gboolean xml_featureset_start_cb(gpointer user_data, ZMapXMLElement set_element,
                                        ZMapXMLParser parser)
{
  gboolean result = FALSE ;
  ZMapXRemoteParseCommandData xml_data = (ZMapXRemoteParseCommandData)user_data ;

  if (xml_data->common.action != ZMAPVIEW_REMOTE_INVALID)
    {
      RequestData request_data = (RequestData)(xml_data->user_data) ;
      ZMapXMLAttribute attr = NULL ;
      ZMapFeatureSet feature_set ;
      GQuark featureset_id ;
      char *featureset_name ;
      GQuark unique_set_id ;
      char *unique_set_name ;

      request_data->source_id = 0 ;			    /* reset needed for remove features. */

      if ((attr = zMapXMLElementGetAttributeByName(set_element, "name")))
	{
	  /* Record original name and unique names for feature set, all look-ups are via the latter. */
	  featureset_id = zMapXMLAttributeGetValue(attr) ;
	  featureset_name = (char *)g_quark_to_string(featureset_id) ;

	  request_data->source_id = unique_set_id = zMapFeatureSetCreateID(featureset_name) ;
	  unique_set_name = (char *)g_quark_to_string(unique_set_id);

	  /* Look for the feature set in the current context, it's an error if it's supposed to exist. */
	  request_data->orig_feature_set = zMapFeatureBlockGetSetByID(request_data->orig_block, unique_set_id) ;
	  if (action_table_G[xml_data->common.action].must_exist && !(request_data->orig_feature_set))
	    {
	      /* If we can't find the featureset but it's meant to exist then it's a serious error
	       * and we can't carry on. */
	      char *err_msg ;

	      err_msg = g_strdup_printf("Unknown Featureset \"%s\":  not found in original_block", featureset_name) ;
	      zMapXMLParserRaiseParsingError(parser, err_msg) ;
	      g_free(err_msg) ;

	      result = FALSE ;
	    }
	  else
	    {
	      if (!(request_data->is_edit_action))
		{
		   result = TRUE ;
		}
	      else
		{
		  if (request_data->orig_feature_set)
		    {
		      feature_set
			= (ZMapFeatureSet)zMapFeatureAnyCopy((ZMapFeatureAny)(request_data->orig_feature_set)) ;
		    }
		  else
		    {
		      /* Make sure this feature set is a child of the block........ */
		      if (!(feature_set = zMapFeatureBlockGetSetByID(request_data->edit_block, unique_set_id)))
			{
			  feature_set = zMapFeatureSetCreate(featureset_name, NULL) ;


			  /* At this point we need the style.....(from column ????) */

			  

			}

		    }

		  zMapFeatureBlockAddFeatureSet(request_data->edit_block, feature_set) ;

		  request_data->edit_feature_set = feature_set ;

		  result = TRUE ;
		}
	    }


	}

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      /* WE NEED TO STOP SUPPORTING THIS...IT'S RUBBISH... */

      else
	{
	  /* No name, then get the first one! No one in their right mind should leave this to chance.... */
	  request_data->orig_feature_set = zMap_g_hash_table_nth(request_data->orig_block->feature_sets, 0) ;

	  if (request_data->is_edit_action)
	    request_data->edit_feature_set
	      = (ZMapFeatureSet)zMapFeatureAnyCopy((ZMapFeatureAny)(request_data->orig_feature_set)) ;

	  result = TRUE ;
	}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


      /* We must be able to look up the column and other data for this featureset otherwise we
       * won't know which column/style etc to use. */
      if (result)
	{
	  ZMapFeatureSource source_data ;

	  if (!(source_data = g_hash_table_lookup(request_data->view->context_map.source_2_sourcedata,
						  GINT_TO_POINTER(unique_set_id))))
	    {
	      char *err_msg ;

	      err_msg = g_strdup_printf("Source %s not found in view->context_map.source_2_sourcedata",
					g_quark_to_string(unique_set_id)) ;
	      zMapXMLParserRaiseParsingError(parser, err_msg) ;
	      g_free(err_msg) ;

	      result = FALSE ;
	    }
	  else
	    {
	      request_data->style_id = source_data->style_id ;
	    }

	  if (result && (xml_data->common.action != ZMAPVIEW_REMOTE_LOAD_FEATURES))
	    {
              // check style for all command except load_features
              // as load features processes a complete step list that inludes requesting the style
              // then if the style is not there then we'll drop the features

	      if (!(request_data->style = zMapFindStyle(request_data->view->context_map.styles, request_data->style_id)))
	        {
		  char *err_msg ;

		  err_msg = g_strdup_printf("Style %s not found in view->context_map.styles",
					    g_quark_to_string(request_data->style_id)) ;
		  zMapXMLParserRaiseParsingError(parser, err_msg) ;
		  g_free(err_msg) ;

		  result = FALSE ;
		}
	    }
    	}

      if (result)
	{
	  if (request_data->is_edit_action)
	    request_data->edit_context->req_feature_set_names
	      = g_list_append(request_data->edit_context->req_feature_set_names, GINT_TO_POINTER(featureset_id)) ;

	  switch (xml_data->common.action)
	    {
	    case ZMAPVIEW_REMOTE_LOAD_FEATURES:
	      {
		request_data->feature_sets = g_list_append(request_data->feature_sets, GINT_TO_POINTER(featureset_id)) ;

		break;
	      }

	    case ZMAPVIEW_REMOTE_GET_FEATURE_NAMES:
	      {
		/* NOTE SURE WHY THIS IS HERE.....should be in the feature handler ?? */

		if (result && (attr = zMapXMLElementGetAttributeByName(set_element, "start")))
		  {
		    request_data->start = strtol((char *)g_quark_to_string(zMapXMLAttributeGetValue(attr)),
						 (char **)NULL, 10);
		  }
		else
		  {
		    zMapXMLParserRaiseParsingError(parser, "start is a required attribute for feature.");
		    result = FALSE ;
		  }

		if (result && (attr = zMapXMLElementGetAttributeByName(set_element, "end")))
		  {
		    request_data->end = strtol((char *)g_quark_to_string(zMapXMLAttributeGetValue(attr)),
					       (char **)NULL, 10);
		  }
		else
		  {
		    zMapXMLParserRaiseParsingError(parser, "end is a required attribute for feature.");
		    result = FALSE ;
		  }

		break;
	      }

	    default:
	      {
		/* Mostly nothing to do.... */
		break ;
	      }
	    }
	}
    }


  return result ;
}


static gboolean xml_featureset_end_cb(gpointer user_data, ZMapXMLElement set_element, ZMapXMLParser parser)
{
  gboolean result = FALSE ;
  ZMapXRemoteParseCommandData xml_data = (ZMapXRemoteParseCommandData)user_data;
  RequestData request_data = (RequestData)(xml_data->user_data);

  if (xml_data->common.action != ZMAPVIEW_REMOTE_INVALID)
    {
      /* Only do stuff if a feature set was found and has features. */
      if (request_data->edit_feature_set && request_data->edit_feature_set->features)
	{
	  /* If deleting and no feature was specified then delete them all. NOTE that if
	   * a source name was given then only features from _that_ source are deleted, not
	   * all features in the column. */
	  if (xml_data->common.action == ZMAPVIEW_REMOTE_DELETE_FEATURE && !(request_data->feature_list))
	    {
	      g_hash_table_foreach(request_data->orig_feature_set->features,
				   copyAddFeature, request_data) ;
	    }
	}
    }

  return result ;
}



static gboolean xml_feature_start_cb(gpointer user_data, ZMapXMLElement feature_element, ZMapXMLParser parser)
{
  gboolean result = FALSE ;
  ZMapXMLAttribute attr = NULL;
  ZMapXRemoteParseCommandData xml_data = (ZMapXRemoteParseCommandData)user_data;
  RequestData request_data = (RequestData)(xml_data->user_data);
  ZMapFeatureAny feature_any;
  ZMapStrand strand = ZMAPSTRAND_NONE;
  ZMapPhase start_phase = ZMAPPHASE_NONE ;
  gboolean has_score = FALSE, start_not_found = FALSE, end_not_found = FALSE;
  char *feature_name ;
  GQuark feature_name_id, feature_unique_id ;
  int start = 0, end = 0 ;
  double score = 0.0 ;


  if (xml_data->common.action != ZMAPVIEW_REMOTE_INVALID)
    {
      ZMapFeature feature ;
      ZMapStyleMode mode ;

      result = TRUE ;

      /* Must have following attributes for all feature level operations. */
      if (result && (attr = zMapXMLElementGetAttributeByName(feature_element, "name")))
	{
	  feature_name_id = zMapXMLAttributeGetValue(attr) ;
	  feature_name = (char *)g_quark_to_string(feature_name_id) ;
	}
      else
	{
	  zMapXMLParserRaiseParsingError(parser, "\"name\" is a required attribute for feature.") ;
	  result = FALSE ;
	}


      if (result && (attr = zMapXMLElementGetAttributeByName(feature_element, "start")))
	{
	  start = strtol((char *)g_quark_to_string(zMapXMLAttributeGetValue(attr)),
			 (char **)NULL, 10);
	}
      else
	{
	  zMapXMLParserRaiseParsingError(parser, "\"start\" is a required attribute for feature.");
	  result = FALSE ;
	}

      if (result && (attr = zMapXMLElementGetAttributeByName(feature_element, "end")))
	{
	  end = strtol((char *)g_quark_to_string(zMapXMLAttributeGetValue(attr)),
		       (char **)NULL, 10);
	}
      else
	{
	  zMapXMLParserRaiseParsingError(parser, "\"end\" is a required attribute for feature.");
	  result = FALSE ;
	}

      if (result && (attr = zMapXMLElementGetAttributeByName(feature_element, "strand")))
	{
	  zMapFeatureFormatStrand((char *)g_quark_to_string(zMapXMLAttributeGetValue(attr)),
				  &(strand));
	}
      else
	{
	  zMapXMLParserRaiseParsingError(parser, "\"strand\" is a required attribute for feature.");
	  result = FALSE ;
	}

      /* Need the style to get hold of the feature mode, better would be for
       * xml to contain SO term ?? Maybe not....not sure. */
      if (result && ((mode = zMapStyleGetMode(request_data->style)) == ZMAPSTYLE_MODE_INVALID))
	{
	  zMapXMLParserRaiseParsingError(parser, "\"style\" must have a valid feature mode.");
	  result = FALSE ;
	}


      /* Check if feature exists, for some commands it must do, for others it must not. */
      if (result)
	{

	  feature_unique_id = zMapFeatureCreateID(mode, feature_name, strand, start, end, 0, 0) ;


	  switch(xml_data->common.action)
	    {
	    case ZMAPVIEW_REMOTE_ZOOM_TO:
	    case ZMAPVIEW_REMOTE_DELETE_FEATURE:
	    case ZMAPVIEW_REMOTE_FIND_FEATURE:
	    case ZMAPVIEW_REMOTE_HIGHLIGHT_FEATURE:
	    case ZMAPVIEW_REMOTE_HIGHLIGHT2_FEATURE:
	    case ZMAPVIEW_REMOTE_UNHIGHLIGHT_FEATURE:
	      {
		feature = zMapFeatureSetGetFeatureByID(request_data->orig_feature_set, feature_unique_id) ;

		if (!feature)
		  {
		    /* If we _don't_ find the feature then it's a serious error for these commands. */
		    char *err_msg ;

		    err_msg = g_strdup_printf("Feature \"%s\" with id \"%s\" could not be found in featureset \"%s\",",
					      feature_name, g_quark_to_string(feature_unique_id),
					      zMapFeatureName((ZMapFeatureAny)(request_data->orig_feature_set))) ;
		    zMapXMLParserRaiseParsingError(parser, err_msg) ;
		    g_free(err_msg) ;

		    result = FALSE ;
		  }
		break ;
	      }
	    case ZMAPVIEW_REMOTE_CREATE_FEATURE:
	      {
		feature = zMapFeatureSetGetFeatureByID(request_data->edit_feature_set, feature_unique_id) ;

		if (feature)
		  {
		    /* If we _do_ find the feature then it's a serious error. */
		    char *err_msg ;

		    err_msg = g_strdup_printf("Feature \"%s\" with id \"%s\" already exists in featureset \"%s\",",
					      feature_name, g_quark_to_string(feature_unique_id),
					      zMapFeatureName((ZMapFeatureAny)(request_data->orig_feature_set))) ;
		    zMapXMLParserRaiseParsingError(parser, err_msg) ;
		    g_free(err_msg) ;

		    result = FALSE ;
		  }
		break ;
	      }
	    default:
	      {
		zMapAssertNotReached() ;
		break ;
	      }
	    }
	}


      /* Now do something... */
      if (result)
	{
	  switch(xml_data->common.action)
	    {
	    case ZMAPVIEW_REMOTE_ZOOM_TO:
	    case ZMAPVIEW_REMOTE_CREATE_FEATURE:
	    case ZMAPVIEW_REMOTE_DELETE_FEATURE:
	    case ZMAPVIEW_REMOTE_FIND_FEATURE:
	    case ZMAPVIEW_REMOTE_HIGHLIGHT_FEATURE:
	    case ZMAPVIEW_REMOTE_HIGHLIGHT2_FEATURE:
	    case ZMAPVIEW_REMOTE_UNHIGHLIGHT_FEATURE:
	      {
		result = TRUE ;

		if (result)
		  {
		    if((attr = zMapXMLElementGetAttributeByName(feature_element, "score")))
		      {
			score = zMapXMLAttributeValueToDouble(attr);
			has_score = TRUE;
		      }

		    if ((attr = zMapXMLElementGetAttributeByName(feature_element, "start_not_found")))
		      {
			start_phase = strtol((char *)g_quark_to_string(zMapXMLAttributeGetValue(attr)),
					     (char **)NULL, 10);
			start_not_found = TRUE;

			switch(start_phase)
			  {
			  case 1:
			  case -1:
			    start_phase = ZMAPPHASE_0;
			    break;
			  case 2:
			  case -2:
			    start_phase = ZMAPPHASE_1;
			    break;
			  case 3:
			  case -3:
			    start_phase = ZMAPPHASE_2;
			    break;
			  default:
			    start_not_found = FALSE;
			    start_phase = ZMAPPHASE_NONE;
			    break;
			  }
		      }

		    if ((attr = zMapXMLElementGetAttributeByName(feature_element, "end_not_found")))
		      {
			end_not_found = zMapXMLAttributeValueToBool(attr);
		      }

		    if ((attr = zMapXMLElementGetAttributeByName(feature_element, "suid")))
		      {
			/* Nothing done here yet. */
			zMapXMLAttributeGetValue(attr);
		      }
		  }

		if (result && !zMapXMLParserLastErrorMsg(parser))
		  {
		    switch(xml_data->common.action)
		      {
		      case ZMAPVIEW_REMOTE_ZOOM_TO:
			{
			  ZMapFeature feature ;

			  /* should be removed.... */
			  feature_unique_id = zMapFeatureCreateID(mode,
								  feature_name,
								  strand,
								  start, end, 0, 0) ;

			  if ((feature = zMapFeatureSetGetFeatureByID(request_data->orig_feature_set,
								      feature_unique_id)))
			    {
			      request_data->feature_list = g_list_prepend(request_data->feature_list, feature) ;
			    }
			  else
			    {
			      zMapXMLParserRaiseParsingError(parser, "Cannot find feature in zmap.");
			      result = FALSE ;
			    }

			  break ;
			}

		      case ZMAPVIEW_REMOTE_CREATE_FEATURE:
		      case ZMAPVIEW_REMOTE_DELETE_FEATURE:
		      case ZMAPVIEW_REMOTE_FIND_FEATURE:
		      case ZMAPVIEW_REMOTE_HIGHLIGHT_FEATURE:
		      case ZMAPVIEW_REMOTE_HIGHLIGHT2_FEATURE:
		      case ZMAPVIEW_REMOTE_UNHIGHLIGHT_FEATURE:
			{
			  zMapXMLParserCheckIfTrueErrorReturn(request_data->edit_block == NULL,
							      parser,
							      "feature tag not contained within featureset tag");

			   /* NOTE we assume that OTF features are neatly arranged in featuresets
			    * and the styles match any that have previously been defined
			    */
			  request_data->edit_feature_set->style = request_data->style;

			  if (result
			      && (request_data->edit_feature = zMapFeatureCreateFromStandardData(feature_name, NULL, "",
											    mode,
											    &request_data->edit_feature_set->style,
											    start, end, has_score,
											    score, strand)))
			    {
//			      request_data->feature->style_id = request_data->style_id ;

			      zMapFeatureSetAddFeature(request_data->edit_feature_set, request_data->edit_feature);

			      /* We need to get all the source info. in here somehow.... */
			      zMapFeatureAddText(request_data->edit_feature, request_data->source_id, NULL, NULL) ;

			      if (start_not_found || end_not_found)
				{
				  zMapFeatureAddTranscriptStartEnd(request_data->edit_feature, start_not_found,
								   start_phase, end_not_found);
				}

			      if ((attr = zMapXMLElementGetAttributeByName(feature_element, "locus")))
				{
				  ZMapFeatureSet locus_feature_set = NULL;
				  ZMapFeature old_feature;
				  ZMapFeature locus_feature = NULL;
				  GQuark new_locus_id  = zMapXMLAttributeGetValue(attr);
				  GQuark locus_set_id  = zMapStyleCreateID(ZMAP_FIXED_STYLE_LOCUS_NAME);
				  char *new_locus_name = (char *)g_quark_to_string(new_locus_id);
				  ZMapFeatureTypeStyle locus_style ;

				  if (!(locus_style
					= zMapFindStyle(request_data->view->context_map.styles, zMapStyleCreateID(ZMAP_FIXED_STYLE_LOCUS_NAME))))
				    {
				      zMapLogWarning("Feature %s created but Locus %s could not be added"
						     " because Locus style could not be found.",
						     feature_name, new_locus_name) ;
				    }
				  else
				    {
				      /* Find locus feature set first ... */
				      if (!(locus_feature_set = zMapFeatureBlockGetSetByID(request_data->edit_block, locus_set_id)))
					{
					  /* Can't find one, create one and add it. */
					  locus_feature_set = zMapFeatureSetCreate(ZMAP_FIXED_STYLE_LOCUS_NAME, NULL);
					  zMapFeatureBlockAddFeatureSet(request_data->edit_block, locus_feature_set);
					}

					locus_feature_set->style = locus_style;

				      /* For some reason lace passes over odd xml here...
					 <zmap action="delete_feature">
					 <featureset>
					 <feature style="Known_CDS" name="RP23-383F20.1-004" locus="Oxsm" ... />
					 </featureset>
					 </zmap>
					 <zmap action="create_feature">
					 <featureset>
					 <feature style="Known_CDS" name="RP23-383F20.1-004" locus="Oxsm" ... />
					 ...
					 i.e. deleting the locus name it's creating!
				      */

				      old_feature =
					(ZMapFeature)zMapFeatureContextFindFeatureFromFeature(request_data->orig_context,
											      (ZMapFeatureAny)request_data->edit_feature);

				      if ((old_feature) && (old_feature->type == ZMAPSTYLE_MODE_TRANSCRIPT)
					  && (old_feature->feature.transcript.locus_id != 0)
					  && (old_feature->feature.transcript.locus_id != new_locus_id))
					{
					  /* ^^^ check the old one was a transcript and had a locus that doesn't match this one */
					  ZMapFeature tmp_locus_feature;
					  ZMapFeatureAny old_locus_feature;
					  char *old_locus_name = (char *)g_quark_to_string(old_feature->feature.transcript.locus_id);

					  /* I DON'T REALLY UNDERSTAND THIS BIT CURRENTLY...WHY DO WE ALWAYS LOG THIS... */
					  /* If we're here, assumptions have been made
					   * 1 - We are in an action=delete_feature request
					   * 2 - We are modifying an existing feature.
					   * 3 - Lace has passed a locus="name" which does not = existing feature locus name.
					   * 4 - Locus start and end are based on feature start end.
					   *     If they are the extent of the locus as they should be...
					   *     The unique_id will be different and therefore the next
					   *     zMapFeatureContextFindFeatureFromFeature will fail.
					   *     This means the locus won't be deleted as it should be.
					   */
					  zMapLogMessage("loci '%s' & '%s' don't match will delete '%s'",
							 old_locus_name, new_locus_name, old_locus_name);

					  tmp_locus_feature = zMapFeatureCreateFromStandardData(old_locus_name,
												NULL, "",
												ZMAPSTYLE_MODE_BASIC, &locus_feature_set->style,
												start, end, FALSE, 0.0,
												ZMAPSTRAND_NONE) ;

					  zMapFeatureSetAddFeature(locus_feature_set, tmp_locus_feature);

					  if ((old_locus_feature
					       = zMapFeatureContextFindFeatureFromFeature(request_data->orig_context,
											  (ZMapFeatureAny)tmp_locus_feature)))
					    {
					      zMapLogMessage("Found old locus '%s', deleting.", old_locus_name);
					      locus_feature = (ZMapFeature)zMapFeatureAnyCopy(old_locus_feature);
					    }
					  else
					    {
					      zMapLogMessage("Failed to find old locus '%s' during delete.", old_locus_name);
					      /* make the locus feature itself. */
					      locus_feature = zMapFeatureCreateFromStandardData(old_locus_name,
												NULL, "",
												ZMAPSTYLE_MODE_BASIC, &locus_feature_set->style,
												start, end, FALSE, 0.0,
												ZMAPSTRAND_NONE) ;
					    }

					  zMapFeatureDestroy(tmp_locus_feature);
					}
				      else
					{
					  /* make the locus feature itself. */
					  locus_feature = zMapFeatureCreateFromStandardData(new_locus_name,
											    NULL, "",
											    ZMAPSTYLE_MODE_BASIC, &locus_feature_set->style,
											    start, end, FALSE, 0.0,
											    ZMAPSTRAND_NONE) ;

					}

				      /* The feature set and feature need to have their styles set... */

				      /* managed to get the styles set up. Add the feature to
				       * feature set and finish up the locus. */
				      zMapFeatureSetAddFeature(locus_feature_set,
							       locus_feature);
				      zMapFeatureAddLocus(locus_feature, new_locus_id);

				      /* We'll still add the locus to the transcript
				       * feature so at least this information is
				       * preserved whatever went on with the styles */
				      zMapFeatureAddLocus(request_data->edit_feature, new_locus_id);
				    }
				}

			      feature_any = zMapFeatureAnyCopy((ZMapFeatureAny)request_data->edit_feature) ;

			      request_data->feature_list = g_list_prepend(request_data->feature_list, feature_any);
			    }

			  break ;
			}

		      default:
			{
			  zMapXMLParserRaiseParsingError(parser, "Unexpected element for action");
			  result = FALSE ;

			  break ;
			}
		      }

		    break;
		  }
	      }

	    default:
	      {
		zMapXMLParserRaiseParsingError(parser, "Unexpected element for action");
		result = FALSE ;

		break;
	      }
	    }
	}
    }


  return result ;
}


static gboolean xml_feature_end_cb(gpointer user_data, ZMapXMLElement sub_element, ZMapXMLParser parser)
{
  gboolean result = FALSE ;
  ZMapXRemoteParseCommandData xml_data = (ZMapXRemoteParseCommandData)user_data;
  RequestData request_data = (RequestData)(xml_data->user_data);

  if (xml_data->common.action != ZMAPVIEW_REMOTE_INVALID)
    {
      if (action_table_G[xml_data->common.action].is_feature_action)
	{
	  if (action_table_G[xml_data->common.action].create_feature_action)
	    {
	      if (!(request_data->edit_feature))
		{
		  zMapXMLParserCheckIfTrueErrorReturn(request_data->edit_feature == NULL,
						      parser,
						      "a feature end tag without a created feature.") ;
		}
	      else
		{
		  /* If we are revcomp'd then any feature passed to us by an external client will
		   * be on the opposite strand from what we expect so we need to revcomp. */
		  if (request_data->view->revcomped_features)
		    {
		      zMapFeatureReverseComplement(request_data->orig_context, request_data->edit_feature) ;
		    }
		}
	    }
	}
      result = TRUE ;
    }

  return result ;
}



static gboolean xml_subfeature_end_cb(gpointer user_data, ZMapXMLElement sub_element, ZMapXMLParser parser)
{
  ZMapXMLAttribute attr = NULL;
  ZMapXRemoteParseCommandData xml_data = (ZMapXRemoteParseCommandData)user_data;
  RequestData request_data = (RequestData)(xml_data->user_data);
  ZMapFeature feature = NULL;

  if(xml_data->common.action == ZMAPVIEW_REMOTE_INVALID)
    return FALSE;

  zMapXMLParserCheckIfTrueErrorReturn(request_data->edit_feature == NULL,
                                      parser,
                                      "a feature end tag without a created feature.");
  feature = request_data->edit_feature;

  if((attr = zMapXMLElementGetAttributeByName(sub_element, "ontology")))
    {
      GQuark ontology = zMapXMLAttributeGetValue(attr);
      ZMapSpanStruct span = {0,0};
      ZMapSpan exon_ptr = NULL, intron_ptr = NULL;

      feature->type = ZMAPSTYLE_MODE_TRANSCRIPT;

      if((attr = zMapXMLElementGetAttributeByName(sub_element, "start")))
        {
          span.x1 = strtol((char *)g_quark_to_string(zMapXMLAttributeGetValue(attr)),
                           (char **)NULL, 10);
        }
      else
        zMapXMLParserRaiseParsingError(parser, "start is a required attribute for subfeature.");

      if((attr = zMapXMLElementGetAttributeByName(sub_element, "end")))
        {
          span.x2 = strtol((char *)g_quark_to_string(zMapXMLAttributeGetValue(attr)),
                           (char **)NULL, 10);
        }
      else
        zMapXMLParserRaiseParsingError(parser, "end is a required attribute for subfeature.");

      /* Don't like this lower case stuff :( */
      if(ontology == g_quark_from_string("exon"))
        exon_ptr   = &span;
      if(ontology == g_quark_from_string("intron"))
        intron_ptr = &span;
      if(ontology == g_quark_from_string("cds"))
        zMapFeatureAddTranscriptCDS(feature, TRUE, span.x1, span.x2) ;

      if(exon_ptr != NULL || intron_ptr != NULL) /* Do we need this check isn't it done internally? */
        zMapFeatureAddTranscriptExonIntron(feature, exon_ptr, intron_ptr);

    }

  return TRUE;                  /* tell caller to clean us up. */
}








static gboolean xml_return_true_cb(gpointer user_data,
                                   ZMapXMLElement zmap_element,
                                   ZMapXMLParser parser)
{
  return TRUE;
}



static void zoomWindowToFeature(ZMapView view, RequestData request_data)
{
  GList *list;
  ZMapViewWindow view_window ;

  request_data->code = ZMAPXREMOTE_OK;

  /* Hack, just grab first window...work out what to do about this.... */
  view_window = (ZMapViewWindow)(request_data->view->window_list->data) ;



  if ((list = g_list_first(request_data->feature_list)))
    {
      ZMapFeature feature = (ZMapFeature)(list->data);

      if ((zMapWindowZoomToFeature(view_window->window, feature)))
	{
	  if (zMapWindowFeatureSelect(view_window->window, feature))
	    {
	      g_string_append_printf(request_data->messages,
				     "Zoom/Select feature %s executed",
				     (char *)g_quark_to_string(feature->original_id));
	    }
	  else
	    {
	      request_data->code = ZMAPXREMOTE_CONFLICT;

	      g_string_append_printf(request_data->messages,
				     "Select feature %s failed",
				     (char *)g_quark_to_string(feature->original_id));
	    }
	}
      else
	{
	  request_data->code = ZMAPXREMOTE_CONFLICT;

	  g_string_append_printf(request_data->messages,
				 "Zoom feature %s failed",
				 (char *)g_quark_to_string(feature->original_id));
	}
    }
  else if ((list = g_list_first(request_data->locations)))
    {
      /* Hack, just grab first window...work out what to do about this.... */
      ZMapViewWindow view_window = (ZMapViewWindow)(request_data->view->window_list->data) ;

      /* LOOKING IN THE WINDOW CODE I DON'T THINK THIS EVER WORKED.... */

      ZMapSpan span;

      span = (ZMapSpan)(list->data);

      zMapWindowZoomToWorldPosition(view_window->window, FALSE,
                                    0.0, span->x1,
                                    100.0, span->x2);

      g_string_append_printf(request_data->messages,
                             "Zoom to location %d-%d executed",
                             span->x1, span->x2);
    }
  else
    {
      g_string_append_printf(request_data->messages,
                             "No data for %s action",
                             actions_G[ZMAPVIEW_REMOTE_ZOOM_TO]);

      request_data->code = ZMAPXREMOTE_BADREQUEST;
    }

  return ;
}


static void reportWindowMark(ZMapView view, RequestData request_data)
{
  ZMapViewWindow view_window ;
  ZMapWindow window ;
  int start = 0, end = 0 ;

  /* Hack, just grab first window...work out what to do about this.... */
  view_window = (ZMapViewWindow)(request_data->view->window_list->data) ;
  window = view_window->window ;

  if (zMapWindowGetMark(window, &start, &end))
    {
      g_string_append_printf(request_data->messages,
			     "<mark exists=\"true\" start=\"%d\" end=\"%d\" />", start, end) ;

      request_data->code = ZMAPXREMOTE_OK ;
    }
  else
    {
      g_string_append(request_data->messages, "<mark exists=\"false\" />") ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      /* i'd like to return an error code but this would need sorting out... */

      request_data->code = ZMAPXREMOTE_FAILED;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


      request_data->code = ZMAPXREMOTE_OK ;
    }



  return ;
}


/* Load features on request from a client. */
static void loadFeatures(ZMapView view, RequestData request_data)
{
  gboolean use_mark = FALSE ;
  int start = 0, end = 0 ;

  request_data->code = ZMAPXREMOTE_OK ;

  /* If mark then get mark, otherwise get big start/end. */
  if (request_data->use_mark)
    {
      ZMapViewWindow view_window ;
      ZMapWindow window ;

      /* mh17: did this used to cause a problem by being outside this if? */
        /* Hack, just grab first window...work out what to do about this.... */
      view_window = (ZMapViewWindow)(request_data->view->window_list->data) ;
      window = view_window->window ;

      if ((zMapWindowMarkGetSequenceSpan(window, &start, &end)))
	{
	  use_mark = TRUE ;
	}
      else
	{
	  g_string_append(request_data->messages, "Load features to marked region failed: no mark set.") ;
	  request_data->code = ZMAPXREMOTE_BADREQUEST;
	}
    }
  else
    {
      start = request_data->edit_block->block_to_sequence.parent.x1 ;
      end = request_data->edit_block->block_to_sequence.parent.x2 ;
    }

  if (request_data->code == ZMAPXREMOTE_OK)
    zmapViewLoadFeatures(view, request_data->edit_block, request_data->feature_sets, NULL, start, end,
			 SOURCE_GROUP_DELAYED, TRUE, FALSE) ;	/* will terminate if is pipe otherwase keep alive */


  return ;
}



/* Get the names of all features within a given range. */
static void getFeatureNames(ZMapView view, RequestData request_data)
{
  ZMapViewWindow view_window ;
  ZMapWindow window ;

  request_data->code = ZMAPXREMOTE_OK ;

  /* Hack, just grab first window...work out what to do about this.... */
  view_window = (ZMapViewWindow)(request_data->view->window_list->data) ;
  window = view_window->window ;


  /* IS THIS RIGHT ??? AREN'T WE USING PARENT COORDS...??? */
  if (request_data->start < request_data->edit_block->block_to_sequence.block.x2
      || request_data->end > request_data->edit_block->block_to_sequence.block.x2)
    {
      g_string_append_printf(request_data->messages,
			     "Requested coords (%d, %d) are outside of block coords (%d, %d).",
			     request_data->start, request_data->end,
			     request_data->edit_block->block_to_sequence.block.x1,
			     request_data->edit_block->block_to_sequence.block.x2) ;

      request_data->code = ZMAPXREMOTE_BADREQUEST;
    }
  else
    {
      GList *feature_list ;

      if (!(feature_list = zMapFeatureSetGetRangeFeatures(request_data->edit_feature_set,
							  request_data->start, request_data->end)))
	{
	  g_string_append_printf(request_data->messages,
				 "No features found for feature set \"%s\" in range (%d, %d).",
				 zMapFeatureSetGetName(request_data->edit_feature_set),
				 request_data->start, request_data->end) ;

	  request_data->code = ZMAPXREMOTE_NOCONTENT ;
	}
      else
	{
	  GHashTable *uniq_features = g_hash_table_new(NULL,NULL) ;

	  g_list_foreach(feature_list, findUniqueCB, &uniq_features) ;

	  g_hash_table_foreach(request_data->edit_feature_set->features, makeUniqueListCB, request_data->messages) ;
	}
    }

  return ;
}

/* A GFunc() to add feature names to a hash list. */
static void findUniqueCB(gpointer data, gpointer user_data)
{
  GHashTable **uniq_features_ptr = (GHashTable **)user_data ;
  GHashTable *uniq_features = *uniq_features_ptr ;
  ZMapFeature feature = (ZMapFeature)user_data ;

  g_hash_table_insert(uniq_features, GINT_TO_POINTER(feature->original_id), NULL) ;

  *uniq_features_ptr = uniq_features ;

  return ;
}

/* A GHFunc() to add a feature to a list if it within a given range */
static void makeUniqueListCB(gpointer key, gpointer value, gpointer user_data)
{
  GQuark feature_id = GPOINTER_TO_INT(key) ;
  GString *list_str = (GString *)user_data ;

  g_string_append_printf(list_str, " %s ;", g_quark_to_string(feature_id)) ;

  return ;
}



/* A GHFunc() to copy a feature and add it to the supplied list. */
static void copyAddFeature(gpointer key, gpointer value, gpointer user_data)
{
  ZMapFeature feature = (ZMapFeature)value ;
  RequestData request_data = (RequestData)user_data ;
  ZMapFeatureAny feature_any ;

  /* Only add to this list if there is no source name or the feature and source
   * names match, caller will have given a source name. */
  if (!(request_data->source_id) || feature->source_id == request_data->source_id)
    {
      if ((feature_any = zMapFeatureAnyCopy((ZMapFeatureAny)feature)))
	{
	  ZMapFeature feature_copy ;

	  zMapFeatureSetAddFeature(request_data->edit_feature_set, (ZMapFeature)feature_any) ;

	  feature_copy = (ZMapFeature)zMapFeatureAnyCopy(feature_any) ;
	  feature_copy->parent = (ZMapFeatureAny)request_data->edit_feature_set ;
	  request_data->feature_list = g_list_append(request_data->feature_list, feature_copy) ;
	}
    }

  return ;
}



/* run through windows registering that there is a remote client with each. */
static void setWindowXremote(ZMapView view)
{
  g_list_foreach(view->window_list, setXremoteCB, NULL) ;

  return ;
}

static void setXremoteCB(gpointer list_data, gpointer user_data)
{
  ZMapViewWindow view_window = (ZMapViewWindow)list_data ;

  if (!zMapWindowXRemoteRegister(view_window->window))
    zMapLogWarning("Remote Client register failed for window %p", view_window->window) ;

  return ;
}

