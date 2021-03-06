/*  File: zmapControlPreferences.c
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
 * Description: Implements showing the preferences configuration
 *              window for a zmapControl instance.
 *
 * Exported functions: See zmapControl_P.h
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.hpp>

#include <ZMap/zmapConfigIni.hpp>
#include <ZMap/zmapConfigDir.hpp>
#include <ZMap/zmapConfigStrings.hpp>
#include <ZMap/zmapUtils.hpp>
#include <ZMap/zmapUtilsGUI.hpp>
#include <ZMap/zmapGLibUtils.hpp>
#include <zmapControl_P.hpp>


#define CONTROL_CHAPTER "Display"
#define CONTROL_PAGE "Window"
#define CONTROL_SHRINKABLE "Shrinkable Window"
#define CONTROL_FILTERED "Highlight Filtered Columns"
#define CONTROL_ANNOTATION "Enable Annotation"

/*
 * Holds just the user-configurable preferences
 */
typedef struct PrefsDataStructType
{
  gboolean init ;                            /* TRUE when struct has been initialised. */

  /* Used to detect when user has set data fields. */
  struct
  {
    unsigned int shrinkable : 1 ;
    unsigned int highlight_filtered : 1 ;
    unsigned int enable_annotation : 1 ;
  } is_set ;

  gboolean shrinkable ;
  gboolean highlight_filtered ;
  gboolean enable_annotation ;
} PrefsDataStruct, *PrefsData ;


static void cleanUpCB(ZMapGuiNotebookAny any_section, void *user_data) ;
static ZMapGuiNotebookChapter addControlPrefsChapter(ZMapGuiNotebook note_book_parent, ZMap zmap, ZMapView view) ;
static void applyCB(ZMapGuiNotebookAny any_section, void *user_data) ;
static void saveCB(ZMapGuiNotebookAny any_section, void *user_data) ;
static void cancelCB(ZMapGuiNotebookAny any_section, void *user_data_unused) ;
static void readChapter(ZMapGuiNotebookChapter chapter, ZMap zmap) ;
static void saveChapter(ZMapGuiNotebookChapter chapter, ZMap zmap) ;
static gboolean getUserPrefs(PrefsData curr_prefs, const char *config_file) ;
static void saveUserPrefs(PrefsData prefs, const char *config_file) ;
void userPrefsUpdateContext(ZMapConfigIniContext context, const ZMapConfigIniFileType file_type) ;

/* 
 *                  Globals
 */

static const char *help_title_G = "ZMap Configuration" ;
static const char *help_text_G =
  "The ZMap Configuration Window allows you to configure the appearance\n"
  " and operation of certains parts of ZMap.\n\n"
  "The Configuration Window has the following sections:\n\n"
  "\tThe menubar with general operations such as showing this help.\n"
  "\tThe section chooser where you can click on the section that you want to configure\n"
  "\tThe section resources notebook which displays the elements you can configure for\n"
  "\ta particular section. As you select different sections the resources notebook changes\n"
  "\t to allow you to configure that section.\n\n"
  "After you have made your changes you can click:\n\n"
  "\t\"Cancel\" to discard them and quit the resources dialog\n"
  "\t\"Ok\" to apply them and quit the resources dialog\n\n" 
  "To save settings for future sessions:\n\n"
  "\tClick \"File->Save\" to save settings for the current section only\n" 
  "\tClick \"File->Save All\" to save settigns for all sections\n\n"
  "\t**IMPORTANT NOTE:** You probably SHOULDN'T SAVE CHANGES TO THE ADVANCED settings unless you are\n"
  "\tsure what you are doing. Saved settings will override settings from any peer program (e.g. Otter)\n"
  "\tfor ALL future sessions and the Advanced settings may be key for your setup to work correctly.\n\n"
  "To reset a value:\n" 
  "\tClear the contents of a text box to unset the value. If you had previously saved changes, then\n" 
  "\tsave again to override the previously-saved value. The default values will then be used in your\n"
  "\tnext session.\n" 
  ;

/* WHAT IS THIS FOR ????? 
 * (sm23) I reomved this as it is not used.  
 */
/* static ZMapGuiNotebook note_book_G = NULL ; */

/* Configuration global, holds current config for general preferences, gets overloaded with new user selections. */
static PrefsDataStruct prefs_curr_G = {(FALSE)} ;


/* 
 *                   External interface routines
 */

void zmapControlShowPreferences(ZMap zmap)
{
  ZMapGuiNotebook note_book ;
  char *notebook_title ;
  /* ZMapGuiNotebookChapter chapter ; */

  zMapReturnIfFailSafe(!(zmap->preferences_note_book)) ;


  /* Construct the preferences representation */
  notebook_title = g_strdup_printf("Preferences for zmap %s", zMapGetZMapID(zmap)) ;
  note_book = zMapGUINotebookCreateNotebook(notebook_title, TRUE, cleanUpCB, zmap) ;
  g_free(notebook_title) ;


  /* Now add preferences for the current zmapview and current zmapwindow.... */
  if (note_book)
    {
      ZMapView zmap_view = zMapViewGetView(zmap->focus_viewwindow);

      /* Add prefs for control. */
      if (!prefs_curr_G.init)
        getUserPrefs(&prefs_curr_G, zMapConfigDirGetFile()) ;

      addControlPrefsChapter(note_book, zmap, zmap_view) ;
      
      /* Add blixmem view prefs. */
      zMapViewBlixemGetConfigChapter(zmap_view, note_book) ;

      /* Display the preferences. */
      zMapGUINotebookCreateDialog(note_book, help_title_G, help_text_G) ;

      zmap->preferences_note_book = note_book ;
    }
  else
    {
      zMapWarning("%s", "Error creating preferences dialog\n") ;
    }

  return ;
}


/* Main entry point for saving all user-specified preferences into a context. This is provided
 * in addition to the saveCB (which is called from the prefs dialog itsef) so that we can save 
 * preferences from an external interface such as the main menu or columns dialog */
void zMapControlPreferencesUpdateContext(ZMapConfigIniContext context, 
                                         ZMapConfigIniFileType file_type, 
                                         gpointer data)
{
  userPrefsUpdateContext(context, file_type) ;
  zMapViewBlixemUserPrefsUpdateContext(context, file_type) ;
}


/* 
 *                      Internal routines
 */

static void cleanUpCB(ZMapGuiNotebookAny any_section, void *user_data)
{
  ZMapGuiNotebook note_book = (ZMapGuiNotebook)any_section ;
  ZMap zmap = (ZMap)user_data ;

  zMapGUINotebookDestroyNotebook(note_book) ;

  zmap->preferences_note_book = NULL ;

  return ;
}


/* Does the work to create a chapter in the preferences notebook for general zmap settings. */
static ZMapGuiNotebookChapter addControlPrefsChapter(ZMapGuiNotebook note_book_parent, ZMap zmap, ZMapView view)
{
  ZMapGuiNotebookChapter chapter = NULL ;
  ZMapGuiNotebookCBStruct user_CBs = {cancelCB, NULL, applyCB, zmap, NULL, NULL, saveCB, zmap} ;
  ZMapGuiNotebookPage page ;
  ZMapGuiNotebookSubsection subsection ;
  ZMapGuiNotebookParagraph paragraph ;

  chapter = zMapGUINotebookCreateChapter(note_book_parent, CONTROL_CHAPTER, &user_CBs) ;


  page = zMapGUINotebookCreatePage(chapter, CONTROL_PAGE) ;

  subsection = zMapGUINotebookCreateSubsection(page, NULL) ;

  paragraph = zMapGUINotebookCreateParagraph(subsection, NULL,
					     ZMAPGUI_NOTEBOOK_PARAGRAPH_TAGVALUE_TABLE,
					     NULL, NULL) ;

  zMapGUINotebookCreateTagValue(paragraph, CONTROL_SHRINKABLE, "Make the ZMap window shrinkable beyond the default minimum size",
                                ZMAPGUI_NOTEBOOK_TAGVALUE_CHECKBOX,
                                "bool", zmap->shrinkable) ;

  zMapGUINotebookCreateTagValue(paragraph, CONTROL_FILTERED, "Highlight the background of columns that have features that are not visible due to filtering",
                                ZMAPGUI_NOTEBOOK_TAGVALUE_CHECKBOX,
                                "bool", zMapViewGetFlag(view, ZMAPFLAG_HIGHLIGHT_FILTERED_COLUMNS)) ;

  zMapGUINotebookCreateTagValue(paragraph, CONTROL_ANNOTATION, "Enable feature editing within ZMap via the Annotation column. This enables the Annotation sub-menu when you right-click a feature.",
                                ZMAPGUI_NOTEBOOK_TAGVALUE_CHECKBOX,
                                "bool", zMapViewGetFlag(view, ZMAPFLAG_ENABLE_ANNOTATION)) ;

  return chapter ;
}


/* User applies the preferences. */
static void applyCB(ZMapGuiNotebookAny any_section, void *user_data)
{
  ZMap zmap = (ZMap)user_data;

  readChapter((ZMapGuiNotebookChapter)any_section, zmap) ;

  return ;
}

/* Save the preferences to the config file */
static void saveCB(ZMapGuiNotebookAny any_section, void *user_data)
{
  ZMapGuiNotebookChapter chapter = (ZMapGuiNotebookChapter)any_section ;
  ZMap zmap = (ZMap)user_data;

  readChapter(chapter, zmap);

  saveChapter(chapter, zmap) ;

  return ;
}

/* User cancels preferences. */
static void cancelCB(ZMapGuiNotebookAny any_section, void *user_data_unused)
{
  return ;
}


/* Get any user preferences specified in config file. */
static gboolean getUserPrefs(PrefsData curr_prefs, const char *config_file)
{
  gboolean status = FALSE ;
  ZMapConfigIniContext context = NULL ;
  PrefsDataStruct file_prefs = {FALSE} ;

  if (!curr_prefs)
    return status ;

  if ((context = zMapConfigIniContextProvide(config_file, ZMAPCONFIG_FILE_PREFS)))
    {
      gboolean tmp_bool ;

      if (zMapConfigIniContextGetBoolean(context, ZMAPSTANZA_APP_CONFIG, ZMAPSTANZA_APP_CONFIG,
                                         ZMAPSTANZA_APP_SHRINKABLE, &tmp_bool))
        {
          file_prefs.shrinkable = tmp_bool;
        }

      if (zMapConfigIniContextGetBoolean(context, ZMAPSTANZA_APP_CONFIG, ZMAPSTANZA_APP_CONFIG,
                                         ZMAPSTANZA_APP_HIGHLIGHT_FILTERED, &tmp_bool))
        {
          file_prefs.highlight_filtered = tmp_bool;
        }

      if (zMapConfigIniContextGetBoolean(context, ZMAPSTANZA_APP_CONFIG, ZMAPSTANZA_APP_CONFIG,
                                         ZMAPSTANZA_APP_ENABLE_ANNOTATION, &tmp_bool))
        {
          file_prefs.enable_annotation = tmp_bool;
        }

      zMapConfigIniContextDestroy(context);
    }

  /* If curr_prefs is initialised then copy any curr prefs set by user into file prefs,
   * thus overriding file prefs with user prefs. Then copy file prefs into curr_prefs
   * so that curr prefs now represents latest config file and user prefs combined. */
  if (curr_prefs->init)
    {
      file_prefs.is_set = curr_prefs->is_set ;                    /* struct copy. */

      if (curr_prefs->is_set.shrinkable)
        file_prefs.shrinkable = curr_prefs->shrinkable ;

      if (curr_prefs->is_set.highlight_filtered)
        file_prefs.highlight_filtered = curr_prefs->highlight_filtered ;

      if (curr_prefs->is_set.enable_annotation)
        file_prefs.enable_annotation = curr_prefs->enable_annotation ;
    }


  *curr_prefs = file_prefs ;                                    /* Struct copy. */

  return status ;
}


/* Save user preferences back to the given config file (don't lose anything already in that config
 * file). */
static void saveUserPrefs(PrefsData prefs, const char *config_file)
{
  zMapReturnIfFail(prefs) ;

  ZMapConfigIniContext context = NULL;

  /* Create the context from the existing config file (if any - otherwise create an empty
   * context). */
  if ((context = zMapConfigIniContextProvide(config_file, ZMAPCONFIG_FILE_PREFS)))
    {
      /* Update the settings in the context. Note that the file type of 'user' means the
       * user-specified config file, i.e. the one we passed in to ContextProvide */
      ZMapConfigIniFileType file_type = ZMAPCONFIG_FILE_PREFS ;

      userPrefsUpdateContext(context, file_type) ;

      zMapConfigIniContextSave(context, file_type);

      zMapConfigIniContextDestroy(context) ;
    }
}


/* Update the given context with any preferences that have been changed by the user */
void userPrefsUpdateContext(ZMapConfigIniContext context, const ZMapConfigIniFileType file_type)
{ 
  PrefsData prefs = &prefs_curr_G ;
  gboolean changed = FALSE ;

  if (prefs->is_set.shrinkable)
    {
      changed = TRUE ;

      zMapConfigIniContextSetBoolean(context, file_type,
                                     ZMAPSTANZA_APP_CONFIG, ZMAPSTANZA_APP_CONFIG,
                                     ZMAPSTANZA_APP_SHRINKABLE, prefs->shrinkable);
    }

  if (prefs->is_set.highlight_filtered)
    {
      changed = TRUE ;

      zMapConfigIniContextSetBoolean(context, file_type,
                                     ZMAPSTANZA_APP_CONFIG, ZMAPSTANZA_APP_CONFIG,
                                     ZMAPSTANZA_APP_HIGHLIGHT_FILTERED, prefs->highlight_filtered);
    }

  if (prefs->is_set.enable_annotation)
    {
      changed = TRUE ;

      zMapConfigIniContextSetBoolean(context, file_type,
                                     ZMAPSTANZA_APP_CONFIG, ZMAPSTANZA_APP_CONFIG,
                                     ZMAPSTANZA_APP_ENABLE_ANNOTATION, prefs->enable_annotation);
    }
  
  /* Set the unsaved flag in the context if there were any changes */
  if (changed)
    {
      zMapConfigIniContextSetUnsavedChanges(context, file_type, TRUE) ;
    }
}


/* Set the preferences. */
static void readChapter(ZMapGuiNotebookChapter chapter, ZMap zmap)
{
  ZMapGuiNotebookPage page ;
  gboolean bool_value = FALSE ;
  ZMapView view = zMapViewGetView(zmap->focus_viewwindow);

  if ((page = zMapGUINotebookFindPage(chapter, CONTROL_PAGE)))
    {
      if (zMapGUINotebookGetTagValue(page, CONTROL_SHRINKABLE, "bool", &bool_value))
	{
	  if (zmap->shrinkable != bool_value)
	    {
              zmap->shrinkable = bool_value ;

              prefs_curr_G.shrinkable = bool_value ;
              prefs_curr_G.is_set.shrinkable = TRUE ;

              gtk_window_set_policy(GTK_WINDOW(zmap->toplevel), zmap->shrinkable, TRUE, FALSE ) ;
	    }
	}

      if (zMapGUINotebookGetTagValue(page, CONTROL_FILTERED, "bool", &bool_value))
        {
          if (zMapViewGetFlag(view, ZMAPFLAG_HIGHLIGHT_FILTERED_COLUMNS) != bool_value)
            {
              zMapViewSetFlag(view, ZMAPFLAG_HIGHLIGHT_FILTERED_COLUMNS, bool_value) ;

              prefs_curr_G.highlight_filtered = bool_value ;
              prefs_curr_G.is_set.highlight_filtered = TRUE ;
            }
        }

      if (zMapGUINotebookGetTagValue(page, CONTROL_ANNOTATION, "bool", &bool_value))
        {
          if (zMapViewGetFlag(view, ZMAPFLAG_ENABLE_ANNOTATION) != bool_value)
            {
              zMapViewSetFlag(view, ZMAPFLAG_ENABLE_ANNOTATION, bool_value) ;

              prefs_curr_G.enable_annotation = bool_value ;
              prefs_curr_G.is_set.enable_annotation = TRUE ;
            }
        }

    }
  
  return ;
}


void saveChapter(ZMapGuiNotebookChapter chapter, ZMap zmap)
{
  /* By default, save to the input zmap config file, if there was one */
  saveUserPrefs(&prefs_curr_G, zMapConfigDirGetFile());

  return ;
}

