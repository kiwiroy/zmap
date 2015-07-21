/*  File: zmapWindowMenus.c
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
 * originated by
 *      Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *        Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk,
 *   Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *
 * Description: Code implementing the menus for sequence display.
 *
 * Exported functions: ZMap/zmapWindows.h
 *
 *-------------------------------------------------------------------
 */


/* PLEASE READ:
 *
 * This file is a unification of code that was scattered and replicated
 * in a number of files. The unification is not complete, itemMenuCB()
 * needs merging with other callbacks to remove all duplication and
 * there is further simplification to be done as there used to be
 * completely separate code to service the feature menu as opposed
 * to the column menu.
 *
 *  */

#include <ZMap/zmap.hpp>

#include <string.h>

#include <ZMap/zmapUtils.hpp>
#include <ZMap/zmapGLibUtils.hpp> /* zMap_g_hash_table_nth */
#include <ZMap/zmapFASTA.hpp>
#include <ZMap/zmapGFF.hpp>
#include <ZMap/zmapPeptide.hpp>
#include <zmapWindow_P.hpp>
#include <zmapWindowCanvasItem.hpp>
#include <zmapWindowContainerUtils.hpp>
#include <zmapWindowContainerFeatureSet_I.hpp>




/*
 * some common menu strings, needed because cascading menus need the same string as their parent
 * menu item.
 */

/* Show Feature, DNA, Peptide */
#define FEATURE_SHOW_STR           "View or Export Feature, DNA, Peptide"

#define FEATURE_VIEW_STR           "View"
#define FEATURE_EXPORT_STR         "Export"

#define FEATURE_STR                "Feature"

#define FEATURE_DNA_STR            "Feature DNA"
#define FEATURE_DNA_EXPORT_STR     FEATURE_DNA_STR
#define FEATURE_DNA_SHOW_STR       FEATURE_DNA_STR

#define FEATURE_PEPTIDE_STR        "Feature Peptide"
#define FEATURE_PEPTIDE_EXPORT_STR FEATURE_PEPTIDE_STR
#define FEATURE_PEPTIDE_SHOW_STR   FEATURE_PEPTIDE_STR

#define CONTEXT_STR                "All Features"
#define MARK_STR                   "Marked Features"
#define DEVELOPER_STR              "Developer"


/* Column filtering options. */
#define COL_FILTERING_STR          "Column Filtering"
#define COL_FILT_THIS_STR          COL_FILTERING_STR"/Filter This Col"
#define COL_FILT_ALL_STR           COL_FILTERING_STR"/Filter All Cols"
#define COL_FILT_SHOW_STR          "/To Show... "


/* These flags get OR'd together to set a menu value, hence the shifting.
 * Seems messy but the alternative is a load of code to store per menu item
 * data which would be messy too.....
 * See ZMapWindowContainerFilterType, ZMapWindowContainerActionType &
 * ZMapWindowContainerTargetType in zmapWindowContainerFeatureSet.h
 * to see how these values are derived. */
typedef enum
  {
    /* Part of selected feature(s) to use for filtering. */
    SELECTED_NONE              = 1 << 0,
    SELECTED_GAPS              = 1 << 1,
    SELECTED_PARTS             = 1 << 2,

    /* Part of feature to use for filtering. */
    FILTER_GAPS                = 1 << 4,
    FILTER_PARTS               = 1 << 5,
    FILTER_CDS                 = 1 << 6,
    FILTER_NONE                = 1 << 7,

    /* Action to take on filtering. */
    ACTION_HIGHLIGHT_SPLICE    = 1 << 8,
    ACTION_HIDE                = 1 << 9,
    ACTION_SHOW                = 1 << 10,
    ACTION_CREATE_NEW          = 1 << 11,


    /* When filtering, what should be filtered in the selected col. */
    TARGET_NOT_SOURCE_FEATURES = 1 << 12,
    TARGET_NOT_SOURCE_COLUMN   = 1 << 13,
    TARGET_ALL                 = 1 << 14,


    FILTER_ALL                 = 1 << 25                    /* Filter all cols. */
  } ColFilteringType ;


typedef enum {
  EXPORT_DNA,
  EXPORT_FEATURES_ALL,
  EXPORT_FEATURES_MARKED,
  EXPORT_FEATURES_CLICKED
} ExportType ;


/* Strings/enums for invoking blixem. */
#define BLIXEM_MENU_STR            "Blixem"
#define BLIXEM_OPS_STR             BLIXEM_MENU_STR " - more options"
#define BLIXEM_READS_STR           BLIXEM_MENU_STR " paired reads data"

#define BLIXEM_DNA_STR             "DNA"
#define BLIXEM_DNAS_STR            BLIXEM_DNA_STR "s"
#define BLIXEM_AA_STR              "Protein"
#define BLIXEM_AAS_STR             BLIXEM_AA_STR "s"


/* Column bumping/state. */

/* Column configuration */
#define COLUMN_CONFIG_STR          "Column Configuration"
#define COLUMN_THIS_ONE            "Configure This Column"
#define COLUMN_ALL                 "Configure All Columns"
#define COLUMN_BUMP_OPTS           "Column Bump More Opts"

#define SCRATCH_CONFIG_STR         "Annotation"

#define SCRATCH_DELETE_SUBFEATURE  SCRATCH_CONFIG_STR "/Delete subfeature"
#define SCRATCH_CREATE             SCRATCH_CONFIG_STR "/Save to featureset"
#define SCRATCH_EXPORT             SCRATCH_CONFIG_STR "/Export temp feature"
#define SCRATCH_ATTRIBUTES         SCRATCH_CONFIG_STR "/Set attributes"
#define SCRATCH_UNDO               SCRATCH_CONFIG_STR "/Undo"
#define SCRATCH_REDO               SCRATCH_CONFIG_STR "/Redo"
#define SCRATCH_CLEAR              SCRATCH_CONFIG_STR "/Clear"
#define SCRATCH_COPY               SCRATCH_CONFIG_STR "/Copy"
#define SCRATCH_CDS                SCRATCH_CONFIG_STR "/Set CDS"

#define SELECTED_FEATURES          "Selected feature(s)"
#define SELECTED_ALIGNS            "Selected match(es)"
#define SELECTED_TRANSCRIPTS       "Selected transcript(s)"
#define THIS_EXON                  "This exon"
#define THIS_INTRON                "This intron"
#define THIS_MATCH                 "This match"
#define THIS_FEATURE               "This feature"
#define THIS_COORD                 "This coord"
#define RANGE                      "Range"
#define START                      "Start"
#define END                        "End"


#define PAIRED_READS_RELATED       "Request %s paired reads"
#define PAIRED_READS_ALL           "Request all paired reads"
#define PAIRED_READS_DATA          "Request paired reads data"

#define COLUMN_COLOUR                        "Edit Style"
#define COLUMN_STYLE_OPTS                "Choose Style"

/* Search/Listing menus. */
#define SEARCH_STR                 "Search or List Features and Sequence"


enum
  {
    BLIX_INVALID,
    BLIX_NONE,                              /* Blixem on column with no aligns. */
    BLIX_SELECTED,                          /* Blixem all matches for selected features in this column. */
    BLIX_EXPANDED,                          /* selected features expanded into hidden underlying data */
    BLIX_SET,                               /* Blixem all matches for all features in this column. */
    BLIX_MULTI_SETS,                        /* Blixem all matches for all features in the list of columns in the blixem config file. */
    BLIX_SEQ_COVERAGE,                      /* Blixem a coverage column: find the real data column */
    /*BLIX_SEQ_SET */                         /* Blixem a paired read featureset */
  } ;

#define BLIX_SEQ                10000       /* Blixem short reads data base menu index */
#define REQUEST_SELECTED 20000        /* data related to selected featureset in column */
#define REQUEST_ALL_SEQ 20001                /* all data related to coverage featuresets in column */
#define REQUEST_SEQ        20002                /* request SR data from mark from menu */


/* Choose which way a transcripts dna is exported... */
enum
  {
    ZMAPCDS,
    ZMAPTRANSCRIPT,
    ZMAPUNSPLICED,
    ZMAPCHOOSERANGE,
    ZMAPCDS_FILE,
    ZMAPTRANSCRIPT_FILE,
    ZMAPUNSPLICED_FILE,
    ZMAPCHOOSERANGE_FILE
} ;


enum {
  ZMAPNAV_SPLIT_FIRST_LAST_EXON,
  ZMAPNAV_SPLIT_FLE_WITH_OVERVIEW,
  ZMAPNAV_FIRST,
  ZMAPNAV_PREV,
  ZMAPNAV_NEXT,
  ZMAPNAV_LAST
};


typedef enum
  {
    ITEM_MENU_INVALID,

    ITEM_MENU_LIST_NAMED_FEATURES,
    ITEM_MENU_LIST_ALL_FEATURES,
    ITEM_MENU_MARK_ITEM,
    ITEM_MENU_COPY_TO_SCRATCH,
    ITEM_MENU_COPY_SUBPART_TO_SCRATCH,
    ITEM_MENU_SCRATCH_CDS_START,
    ITEM_MENU_SCRATCH_CDS_END,
    ITEM_MENU_SCRATCH_CDS_RANGE,
    ITEM_MENU_SCRATCH_CDS_START_SUBPART,
    ITEM_MENU_SCRATCH_CDS_END_SUBPART,
    ITEM_MENU_SCRATCH_CDS_RANGE_SUBPART,
    ITEM_MENU_DELETE_FROM_SCRATCH,
    ITEM_MENU_SCRATCH_ATTRIBUTES,
    ITEM_MENU_SCRATCH_CREATE,
    ITEM_MENU_SCRATCH_EXPORT,
    ITEM_MENU_CLEAR_SCRATCH,
    ITEM_MENU_UNDO_SCRATCH,
    ITEM_MENU_REDO_SCRATCH,
    ITEM_MENU_SEARCH,
    ITEM_MENU_FEATURE_DETAILS,
    ITEM_MENU_PFETCH,
    ITEM_MENU_SEQUENCE_SEARCH_DNA,
    ITEM_MENU_SEQUENCE_SEARCH_PEPTIDE,
    ITEM_MENU_SHOW_URL_IN_BROWSER,
    ITEM_MENU_SHOW_TRANSLATION,

    ITEM_MENU_SHOW_EVIDENCE,
    ITEM_MENU_HIDE_EVIDENCE,
    ITEM_MENU_ADD_EVIDENCE,
    ITEM_MENU_SHOW_TRANSCRIPT,
    ITEM_MENU_ADD_TRANSCRIPT,

    ITEM_MENU_EXPAND,
    ITEM_MENU_CONTRACT,

    ITEM_MENU_ITEMS
  } ZMapWindowMenuItemId;


/* Developer menu ops. */
enum
  {
    DEVELOPER_FEATURE_ONLY = 1,
    DEVELOPER_FEATUREITEM_FEATURE,
    DEVELOPER_FEATURESETITEM_FEATUREITEM_FEATURE,
    DEVELOPER_PRINT_STYLE,
    DEVELOPER_PRINT_CANVAS,
    DEVELOPER_STATS
  } ;



typedef struct AlignBlockMenuStructType
{
  char *stem;
  ZMapGUIMenuItem each_align_items;
  ZMapGUIMenuItem each_block_items;
  GArray **array;
} AlignBlockMenuStruct, *AlignBlockMenu;


typedef struct MakeTextAttrStructType
{
  ZMapFeature feature ;

  gboolean cds ;
  gboolean spliced ;

  ZMapFeatureTypeStyle sequence_style ;

  GdkColor *non_coding ;
  GdkColor *coding ;
  GdkColor *split_5 ;
  GdkColor *split_3 ;

  GList *text_attrs ;
} MakeTextAttrStruct, *MakeTextAttr ;


typedef struct FilterColsDataStructType
{
  ItemMenuCBData menu_data ;                                /* need this for which item was clicked etc. */

  int all_menu_start_val ;


  ZMapWindowContainerActionType filter_action ;

} FilterColsDataStruct, *FilterColsData ;




static void maskToggleMenuCB(int menu_item_id, gpointer callback_data);

static void searchListMenuCB(int menu_item_id, gpointer callback_data) ;

static void hideEvidenceMenuCB(int menu_item_id, gpointer callback_data);
static void compressMenuCB(int menu_item_id, gpointer callback_data);
static void copyPasteMenuCB(int menu_item_id, gpointer callback_data) ;
static void configureMenuCB(int menu_item_id, gpointer callback_data) ;

static void colourMenuCB(int menu_item_id, gpointer callback_data);
static void setStyleCB(int menu_item_id, gpointer callback_data);


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
static void bumpToInitialCB(int menu_item_id, gpointer callback_data);
static void unbumpAllCB(int menu_item_id, gpointer callback_data);
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

static void bumpMenuCB(int menu_item_id, gpointer callback_data) ;
static void bumpToggleMenuCB(int menu_item_id, gpointer callback_data) ;
static void markMenuCB(int menu_item_id, gpointer callback_data) ;

static void featureMenuCB(int menu_item_id, gpointer callback_data) ;
static void dnaMenuCB(int menu_item_id, gpointer callback_data) ;
static void peptideMenuCB(int menu_item_id, gpointer callback_data) ;
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
static void transcriptNavMenuCB(int menu_item_id, gpointer callback_data) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
static void exportMenuCB(int menu_item_id, gpointer callback_data) ;
static void developerMenuCB(int menu_item_id, gpointer callback_data) ;
static void colFilterMenuCB(int menu_item_id, gpointer callback_data) ;
static void requestShortReadsCB(int menu_item_id, gpointer callback_data);
static void blixemMenuCB(int menu_item_id, gpointer callback_data) ;
static char *get_menu_string(GQuark set_quark,char disguise) ;
GQuark related_column(ZMapFeatureContextMap map,GQuark fset_id) ;


static void addMenuItem(ZMapGUIMenuItem menu,
                        int *i, const int max_elements,
                        ZMapGUIMenuType item_type, const char *item_text, int item_id,
                        ZMapGUIMenuItemCallbackFunc callback_func, const gpointer callback_data) ;


/* from zmapWindowFeature.c */


static ZMapGUIMenuItem makeMenuURL(int *start_index_inout,
                                   ZMapGUIMenuItemCallbackFunc callback_func,
                                   gpointer callback_data) ;
static ZMapGUIMenuItem makeMenuShowTranslation(int *start_index_inout,
                                               ZMapGUIMenuItemCallbackFunc callback_func,
                                               gpointer callback_data);
static ZMapGUIMenuItem makeMenuPfetchOps(int *start_index_inout,
                                         ZMapGUIMenuItemCallbackFunc callback_func,
                                         gpointer callback_data) ;
static void itemMenuCB(int menu_item_id, gpointer callback_data) ;

static ZMapGUIMenuItem zmapWindowMakeMenuStyle(int *start_index_inout,
                                       ZMapGUIMenuItemCallbackFunc callback_func,
                                       gpointer callback_data, ZMapFeatureTypeStyle cur_style, ZMapStyleMode f_type);



static FooCanvasGroup *menuDataItemToColumn(FooCanvasItem *item) ;

static gboolean exportFASTA(ZMapWindow window,
                            ZMapFASTASeqType seq_type, const char *seq, const char *seq_name, int seq_len,
                            const char *molecule_name, const char *gene_name, GError **error) ;
static gboolean exportFeatures(ZMapWindow window, gboolean all_features, ZMapSpan region_span, ZMapFeatureAny feature_in, char **filepath_inout, GError **error) ;

static void insertSubMenus(GString *branch_point_string,
                           ZMapGUIMenuItem sub_menus,
                           ZMapGUIMenuItem item,
                           GArray **items_array);

static ZMapFeatureContextExecuteStatus alignBlockMenusDataListForeach(GQuark key,
                                                                      gpointer data,
                                                                      gpointer user_data,
                                                                      char **error_out) ;

static gboolean getSeqColours(ZMapFeatureTypeStyle style,
                              GdkColor **non_coding_out, GdkColor **coding_out,
                              GdkColor **split_5_out, GdkColor **split_3_out) ;
static GList *getTranscriptTextAttrs(ZMapFeature feature, gboolean spliced, gboolean cds,
                                     GdkColor *non_coding_out, GdkColor *coding_out,
                                     GdkColor *split_5_out, GdkColor *split_3_out) ;
static void createExonTextTag(gpointer data, gpointer user_data) ;
static void offsetTextAttr(gpointer data, gpointer user_data) ;







/* Set of makeMenuXXXX functions to create common subsections of menus. If you add to this
 * you should make sure you understand how to specify menu paths in the item factory style.
 * If you get it wrong then the menus will be scr*wed up.....
 *
 * The functions are defined in pairs: one to define the menu, one to handle the callback
 * actions, this is to emphasise that their indexes must be kept in step !
 *
 * NOTE HOW THE MENUS ARE DECLARED STATIC IN THE VARIOUS ROUTINES TO MAKE SURE THEY STAY
 * AROUND...OTHERWISE WE WILL HAVE TO KEEP ALLOCATING/DEALLOCATING THEM.....
 *
 * and this is also the weakness of the system, there is duplication in the code because
 * the menus in each function must persist after the function has exitted. This isn't
 * worth fixing now as we will be moving on to the better gtk UI scheme later.
 *
 */



/* Probably it would be wise to pass in the callback function, the start index for the item
 * identifier and perhaps the callback data......
 *
 * There is some mucky stuff for setting buttons etc but it's a bit unavoidable....
 *
 */

/* mh17 somehow we need this number to be unique but many of the id's
 * are enums and will overlap
 * the callbacks are specified per item so that cures one problem (one func per enum)
 * but for menu init here's a bodge up:
 */
#define ZMAPWINDOWCOLUMN_MASK 5678
#define ZMAPWINDOW_HIDE_EVIDENCE 5679
#define ZMAPWINDOW_SHOW_EVIDENCE 5680






/*
 *                          External interface
 *
 * NOTE: restructuring this lot so all menu function is in this file, this will mean
 *       moving functions around and making many static that were external.
 */



/*!
 * \brief Get the ZMapWindowContainerFeatureSet for the scratch column
 */
static ZMapWindowContainerFeatureSet getScratchContainerFeatureset(ZMapWindow window)
{
  ZMapWindowContainerFeatureSet scratch_container = NULL ;
  zMapReturnValIfFail(window, scratch_container) ;

  ZMapFeatureSet scratch_featureset = zmapWindowScratchGetFeatureset(window) ;
  zMapReturnValIfFail(scratch_featureset->parent && scratch_featureset->parent->parent,
                      scratch_container) ;

  if (scratch_featureset)
    {
      FooCanvasItem *scratch_column_item = zmapWindowFToIFindItemFull(window, window->context_to_item,
                                                                      scratch_featureset->parent->parent->unique_id,
                                                                      scratch_featureset->parent->unique_id,
                                                                      scratch_featureset->unique_id,
                                                                      ZMAPSTRAND_NONE, ZMAPFRAME_NONE, 0) ;

      ZMapWindowContainerGroup scratch_group  = zmapWindowContainerCanvasItemGetContainer(scratch_column_item) ;

      if (scratch_group && ZMAP_IS_CONTAINER_FEATURESET(scratch_group))
        {
          scratch_container = ZMAP_CONTAINER_FEATURESET(scratch_group) ;
        }
    }

  return scratch_container ;
}

/*
 * Build the menu for a feature item.
 *
 * Select/highlight a feature and then right click on it.
 */
void zmapMakeItemMenu(GdkEventButton *button_event, ZMapWindow window, FooCanvasItem *item)
{
  static const int max_name_length = 20 ;
  static const char* filler =  "[...]" ;
  int name_length = 0, fill_length = 0 ;
  ZMapWindowContainerGroup column_group =  NULL ;
  static ZMapGUIMenuItemStruct separator[] =
    {
      {ZMAPGUI_MENU_SEPARATOR, NULL, 0, NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL, 0, NULL, NULL}
    } ;
  char *menu_title = NULL,
    *the_name = NULL,
    *temp_name1 = NULL,
    *temp_name2 = NULL ;
  GList *menu_sets = NULL ;
  ItemMenuCBData menu_data = NULL ;
  ZMapFeature feature = NULL ;
  ZMapFeatureTypeStyle style = NULL ;
  ZMapFeatureSet feature_set = NULL ;
  ZMapWindowContainerFeatureSet container_set = NULL ;
  ZMapGUIMenuItem seq_menus = NULL ;


  /* Some parts of the menu are feature type specific so retrieve the feature item info
   * from the canvas item. */
  feature = zMapWindowCanvasItemGetFeature(item);
  zMapReturnIfFail(window && feature && button_event) ;
  feature_set = (ZMapFeatureSet)(feature->parent);

  style = *feature->style;

  /*
   * Make sure that the menu title cannot exceed a certain length due to a long
   * feature name or featureset name.
   */
  fill_length = strlen(filler) ;
  temp_name1 = g_new0(char, max_name_length+1+fill_length) ;
  temp_name2 = g_new0(char, max_name_length+1+fill_length) ;

  the_name = zMapFeatureName((ZMapFeatureAny) feature) ;
  name_length = strlen(the_name) ;
  if (name_length >= max_name_length)
    {
      strncpy(temp_name1, the_name, max_name_length) ;
      strncat(temp_name1, filler, fill_length) ;
    }
  else
    {
      strncpy(temp_name1, the_name, max_name_length) ;
    }

  the_name = zMapFeatureSetGetName(feature_set) ;
  name_length = strlen(the_name) ;
  if (name_length >= max_name_length)
    {
      strncpy(temp_name2, the_name, max_name_length) ;
      strncat(temp_name2, filler, fill_length) ;
    }
  else
    {
      strncpy(temp_name2, the_name, max_name_length) ;
    }

  menu_title = g_strdup_printf("%s (%s)", temp_name1, temp_name2 ) ;

  if (temp_name1)
    g_free(temp_name1) ;
  if (temp_name2)
    g_free(temp_name2) ;


  column_group  = zmapWindowContainerCanvasItemGetContainer(item) ;
  container_set = (ZMapWindowContainerFeatureSet) column_group;

  /* Call back stuff.... */
  menu_data = g_new0(ItemMenuCBDataStruct, 1) ;
  menu_data->x = button_event->x ;
  menu_data->y = button_event->y ;
  menu_data->item_cb = TRUE ;
  menu_data->window = window ;
  menu_data->item = item ;
  menu_data->feature = feature;
  menu_data->context_map = window->context_map;                /* window has it but when we make the menu it's out of scope */

  /* get the featureset and container for the feature, needed for Show Masked Features */
  menu_data->feature_set = feature_set;
  menu_data->container_set = container_set;


  /* Make up the menu. */

  /* optional developer-only functions. */
  if (zMapUtilsUserIsDeveloper())
    {
      menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuDeveloperOps(NULL, NULL, menu_data)) ;

      menu_sets = g_list_append(menu_sets, separator) ;
    }

  /* Only add this item if the container set is filterable (via styles setting). */
  if (zMapWindowContainerFeatureSetIsFilterable(container_set))
    {
      menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuColFilterOps(NULL, NULL, menu_data)) ;

      menu_sets = g_list_append(menu_sets, separator) ;
    }

  if (feature->mode != ZMAPSTYLE_MODE_ALIGNMENT)
    {
      menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuNonHomolFeature(NULL, NULL, menu_data)) ;
    }
  else if (zMapStyleIsPfetchable(style))
    {
      menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuBlixCommon(NULL, NULL, menu_data)) ;

      menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuBlixTop(NULL, NULL, menu_data)) ;

      if (feature->feature.homol.type == ZMAPHOMOL_X_HOMOL)
        {
          menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuProteinHomol(NULL, NULL, menu_data)) ;
        }
      else
        {
          menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuDNAHomol(NULL, NULL, menu_data)) ;
          menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuDNAHomolFeature(NULL, NULL, menu_data)) ;
        }
    }
  else if (zMapStyleBlixemType(style) != ZMAPSTYLE_BLIXEM_INVALID)
    {
      menu_sets = g_list_append(menu_sets,  zmapWindowMakeMenuDNAHomolFeature(NULL, NULL, menu_data)) ;
    }

  /* list all short reads data, temp access till we get wiggle plots running */
  if ((seq_menus = zmapWindowMakeMenuBlixemBAM(NULL, NULL, menu_data)))
      menu_sets = g_list_append(menu_sets, seq_menus) ;

  if (zMapStyleIsPfetchable(style))
    menu_sets = g_list_append(menu_sets, makeMenuPfetchOps(NULL, NULL, menu_data)) ;

  /* Feature ops. */
  menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuFeatureOps(NULL, NULL, menu_data)) ;

  /* Show translation on forward strand features only */
  if (feature->mode == ZMAPSTYLE_MODE_TRANSCRIPT && feature->strand == ZMAPSTRAND_FORWARD)
    menu_sets = g_list_append(menu_sets, makeMenuShowTranslation(NULL, NULL, menu_data));

  if (feature->url)
    menu_sets = g_list_append(menu_sets, makeMenuURL(NULL, NULL, menu_data)) ;

  menu_sets = g_list_append(menu_sets, separator) ;

  /* Scratch column ops */
  ZMapWindowContainerFeatureSet scratch_container = getScratchContainerFeatureset(window) ;

  if (scratch_container &&
      (scratch_container->display_state == ZMAPSTYLE_COLDISPLAY_SHOW ||
       scratch_container->display_state == ZMAPSTYLE_COLDISPLAY_SHOW))
    {
      menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuScratchOps(NULL, NULL, menu_data)) ;
      menu_sets = g_list_append(menu_sets, separator) ;
    }


  /* Big bump menu.... */
  menu_sets = g_list_append(menu_sets,
                            zmapWindowMakeMenuBump(NULL, NULL, menu_data,
                                                   zmapWindowContainerFeatureSetGetBumpMode(container_set))) ;

  {
    GQuark parent_id = 0;
    ZMapFeatureTypeStyle parent = NULL;

    if(!(style->is_default) && (parent_id = style->parent_id))
      {
        parent = (ZMapFeatureTypeStyle)g_hash_table_lookup(window->context_map->styles, GUINT_TO_POINTER(parent_id));
      }

    /*
     * hacky: otterlace styles will not be defaulted
     * any styles defined in a style cannot be edited live
     * NOTE is we define EST_ALIGN as a default style then EST_Human etc can be edited
     */
    if(window->edit_styles || style->is_default || (parent && parent->is_default))
      menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuStyle(NULL, NULL, menu_data, style, feature->mode));
  }

  /* list all short reads data, temp access till we get wiggle plots running */
  if ((seq_menus = zmapWindowMakeMenuRequestBAM(NULL, NULL, menu_data)))
    menu_sets = g_list_append(menu_sets, seq_menus) ;

  menu_sets = g_list_append(menu_sets, separator) ;

  menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuSearchListOps(NULL, NULL, menu_data)) ;

  /* Feature/DNA/Peptide view/export ops. */
  menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuShowTop(NULL, NULL, menu_data)) ;
  menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuShowFeature(NULL, NULL, menu_data)) ;

  if (feature->mode == ZMAPSTYLE_MODE_TRANSCRIPT)
    {
      menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuDNATranscript(NULL, NULL, menu_data)) ;
      menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuDNATranscriptFile(NULL, NULL, menu_data)) ;
      menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuPeptide(NULL, NULL, menu_data)) ;
      menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuPeptideFile(NULL, NULL, menu_data)) ;
    }
  else
    {
      menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuDNAFeatureAny(NULL, NULL, menu_data)) ;
      menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuDNAFeatureAnyFile(NULL, NULL, menu_data)) ;
    }

  menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuItemExportOps(NULL, NULL, menu_data)) ;

  zMapGUIMakeMenu(menu_title, menu_sets, button_event) ;

  g_free(menu_title) ;

  return ;
}


/*
 * Build the background menu for a column.
 *
 * Select a column (but not a feature) and then right-click.
 */
void zmapMakeColumnMenu(GdkEventButton *button_event, ZMapWindow window,
                        FooCanvasItem *item,
                        ZMapWindowContainerFeatureSet container_set,
                        ZMapFeatureTypeStyle style_unused)
{
  static ZMapGUIMenuItemStruct separator[] =
    {
      {ZMAPGUI_MENU_SEPARATOR, NULL, 0, NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL, 0, NULL, NULL}
    } ;
  char *menu_title = NULL ;
  GList *menu_sets = NULL ;
  ItemMenuCBData cbdata = NULL ;
  ZMapFeature feature = NULL ;
  ZMapFeatureSet feature_set = NULL ;
  ZMapGUIMenuItem seq_menus = NULL ;

  zMapReturnIfFail(window && button_event && container_set) ;

  menu_title = (char *) g_quark_to_string(container_set->original_id);

  feature_set = zmapWindowContainerFeatureSetRecoverFeatureSet(container_set) ;

  if (feature_set)
    {

      cbdata = g_new0(ItemMenuCBDataStruct, 1) ;
      cbdata->x = button_event->x ;
      cbdata->y = button_event->y ;
      cbdata->item_cb = FALSE ;
      cbdata->window = window ;
      cbdata->item = item ;
      cbdata->feature_set = feature_set ;
      cbdata->container_set = container_set;
      cbdata->context_map = window->context_map ;

      /* Make up the menu. */
      if (zMapUtilsUserIsDeveloper())
        {
          menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuDeveloperOps(NULL, NULL, cbdata)) ;

          menu_sets = g_list_append(menu_sets, separator) ;
        }


      /* Only add this item if the container set is filterable (via styles setting). */
      if (zMapWindowContainerFeatureSetIsFilterable(container_set))
        {
          menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuColFilterOps(NULL, NULL, cbdata)) ;

          menu_sets = g_list_append(menu_sets, separator) ;
        }

      /* Quite a big hack actually, we judge feature type and protein/dna on the first feature
       * we find in the column..... */
      if ((feature = (ZMapFeature)zMap_g_hash_table_nth(feature_set->features, 0)))
        {
          if (feature->mode != ZMAPSTYLE_MODE_ALIGNMENT)
            {
              menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuNonHomolFeature(NULL, NULL, cbdata)) ;
            }
          else if (zMapStyleIsPfetchable(*feature->style))
            {
              menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuBlixColCommon(NULL, NULL, cbdata)) ;

              if (feature->feature.homol.type == ZMAPHOMOL_X_HOMOL)
                {
                  menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuProteinHomol(NULL, NULL, cbdata)) ;
                }
              else
                {
                  menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuDNAHomolFeature(NULL, NULL, cbdata)) ;
                  menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuDNAHomol(NULL, NULL, cbdata)) ;
                }
            }
          else if (zMapStyleBlixemType(*feature->style) != ZMAPSTYLE_BLIXEM_INVALID)
            {
              menu_sets = g_list_append(menu_sets,  zmapWindowMakeMenuDNAHomolFeature(NULL, NULL, cbdata)) ;
            }

          if ((seq_menus = zmapWindowMakeMenuBlixemBAM(NULL, NULL, cbdata)))
            menu_sets = g_list_append(menu_sets, seq_menus) ;
        }

      menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuFeatureOps(NULL, NULL, cbdata)) ;
      menu_sets = g_list_append(menu_sets, separator) ;

      /* Scratch column ops */
      ZMapWindowContainerFeatureSet scratch_container = getScratchContainerFeatureset(window) ;

      if (scratch_container &&
          (scratch_container->display_state == ZMAPSTYLE_COLDISPLAY_SHOW ||
           scratch_container->display_state == ZMAPSTYLE_COLDISPLAY_SHOW))
        {
          menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuScratchOps(NULL, NULL, cbdata)) ;
          menu_sets = g_list_append(menu_sets, separator) ;
        }

      menu_sets
        = g_list_append(menu_sets,
                        zmapWindowMakeMenuBump(NULL, NULL, cbdata,
                                               zmapWindowContainerFeatureSetGetBumpMode((ZMapWindowContainerFeatureSet)item))) ;

      if ((seq_menus = zmapWindowMakeMenuRequestBAM(NULL, NULL, cbdata)))
        menu_sets = g_list_append(menu_sets, seq_menus);

      menu_sets = g_list_append(menu_sets, separator) ;

      menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuSearchListOps(NULL, NULL, cbdata)) ;

      menu_sets = g_list_append(menu_sets, zmapWindowMakeMenuColumnExportOps(NULL, NULL, cbdata)) ;

      zMapGUIMakeMenu(menu_title, menu_sets, button_event) ;

    }

  return ;
}


/* Utility to add an option to the given menu */
static void addMenuItem(ZMapGUIMenuItem menu,
                        int *i,
                        const int max_elements,
                        ZMapGUIMenuType item_type,
                        const char *item_text,
                        int item_id,
                        ZMapGUIMenuItemCallbackFunc callback_func,
                        const gpointer callback_data)
{
  if (!i)
    return ;

  /* Check we're not trying to index out of range based on the maximum number
   * of elements we expect in this menu */
  if (*i < max_elements)
    {
      menu[*i].type = item_type;
      menu[*i].name = g_strdup(item_text);
      menu[*i].id = item_id;
      menu[*i].callback_func = callback_func;
      menu[*i].callback_data = callback_data;

      *i = *i + 1;
    }
  else
    {
      zMapWarning("Program error: tried to add menu item to out-of-range menu position %d (max=%d)", *i, max_elements) ;
    }
}


/* This is in the general menu and needs to be handled separately perhaps as the index is a global
 * one shared amongst all general menu functions...
 * NOTE HOW THE MENUS ARE DECLARED STATIC IN THE VARIOUS ROUTINES TO MAKE SURE THEY STAY
 * AROUND...OTHERWISE WE WILL HAVE TO KEEP ALLOCATING/DEALLOCATING THEM.....
 *
 * Called upon right-click on a selected feature.
 */
ZMapGUIMenuItem zmapWindowMakeMenuFeatureOps(int *start_index_inout,
                                             ZMapGUIMenuItemCallbackFunc callback_func,
                                             gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL, NULL},

      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL,       NULL}
    } ;
  int i = 0 ;
  int max_elements = 5 ;
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;

  zMapReturnValIfFail(menu_data && menu_data->window, menu );

  menu[i].type = ZMAPGUI_MENU_NONE;

  if (menu_data->feature)
    {
      /* add in feature options */
      addMenuItem(menu, &i, max_elements, ZMAPGUI_MENU_NORMAL, "Use Feature for Mark", ITEM_MENU_MARK_ITEM, itemMenuCB, NULL);
    }

  /* add in evidence/ transcript items option to remove existing is in column menu */
  if (menu_data->feature)
    {
      ZMapFeatureTypeStyle style = *menu_data->feature->style;

      if (!style)
        {
          /* style should be attached to the feature, but if not don't fall over
             new features should also have styles attached */
          zMapLogWarning("Feature menu item does not have style","");
        }
      else
        {
          if(style->mode == ZMAPSTYLE_MODE_TRANSCRIPT)
            {
              addMenuItem(menu, &i, max_elements, ZMAPGUI_MENU_NORMAL, "Highlight Evidence", ITEM_MENU_SHOW_EVIDENCE, itemMenuCB, NULL);
              addMenuItem(menu, &i, max_elements, ZMAPGUI_MENU_NORMAL, "Highlight Evidence (add more)", ITEM_MENU_ADD_EVIDENCE, itemMenuCB, NULL);
            }
          else if (style->mode == ZMAPSTYLE_MODE_ALIGNMENT)
            {
              addMenuItem(menu, &i, max_elements, ZMAPGUI_MENU_NORMAL, "Highlight Transcript", ITEM_MENU_SHOW_TRANSCRIPT, itemMenuCB, NULL);
              addMenuItem(menu, &i, max_elements, ZMAPGUI_MENU_NORMAL, "Highlight Transcript (add more)", ITEM_MENU_ADD_TRANSCRIPT, itemMenuCB, NULL);

              if(menu_data->feature->children)
                {
                  addMenuItem(menu, &i, max_elements, ZMAPGUI_MENU_NORMAL, "Expand Feature", ITEM_MENU_EXPAND, itemMenuCB, NULL);
                }
              else if(menu_data->feature->composite)
                {
                  addMenuItem(menu, &i, max_elements, ZMAPGUI_MENU_NORMAL, "Contract Features", ITEM_MENU_CONTRACT, itemMenuCB, NULL);
                }
            }
        }
    }

  if (zmapWindowFocusHasType(menu_data->window->focus, WINDOW_FOCUS_GROUP_EVIDENCE))
    {
      addMenuItem(menu, &i, max_elements, ZMAPGUI_MENU_TOGGLEACTIVE, "Hide Evidence/ Transcript", ITEM_MENU_HIDE_EVIDENCE, hideEvidenceMenuCB, NULL);
    }
  else
    {
      menu[i].type = ZMAPGUI_MENU_HIDE ;
      i++ ;
    }

  menu[i].type = ZMAPGUI_MENU_NONE;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


/* Small utility to get the subpart description for a particular feature */
static const char* getFeatureSubpartStr(ZMapFeature feature)
{
  const char *subpart = THIS_FEATURE ;

  if (feature->mode == ZMAPSTYLE_MODE_TRANSCRIPT)
    subpart = THIS_EXON ;
  else if (feature->mode == ZMAPSTYLE_MODE_ALIGNMENT)
    subpart = THIS_MATCH ;
  else if (feature->mode == ZMAPSTYLE_MODE_SEQUENCE)
    subpart = THIS_COORD ;

  return subpart ;
}

/* Small utility to get the feature type description for a particular feature */
static const char* getFeatureModeStr(ZMapFeature feature)
{
  const char *result = SELECTED_FEATURES ;

  if (feature->mode == ZMAPSTYLE_MODE_TRANSCRIPT)
    result = SELECTED_TRANSCRIPTS ;
  else if (feature->mode == ZMAPSTYLE_MODE_ALIGNMENT)
    result = SELECTED_ALIGNS ;

  return result ;
}


/* Small utility to return the config string for copying a particular subpart type.
 * Result should be free'd with g_free. */
static char* getScratchCopyStr(ZMapFeature feature, const gboolean subpart)
{
  const char *desc = subpart ? getFeatureSubpartStr(feature) : getFeatureModeStr(feature) ;
  return g_strdup_printf("%s/%s", SCRATCH_COPY, desc) ;
}

/* Small utility to return the cds-start string for a particular subpart type */
static char* getScratchCdsStrStart(ZMapFeature feature, const gboolean subpart)
{
  const char *desc = subpart ? getFeatureSubpartStr(feature) : getFeatureModeStr(feature) ;
  return g_strdup_printf("%s/%s/%s", SCRATCH_CDS, START, desc) ;
}

/* Small utility to return the cds-end string for a particular subpart type */
static char* getScratchCdsStrEnd(ZMapFeature feature, const gboolean subpart)
{
  const char *desc = subpart ? getFeatureSubpartStr(feature) : getFeatureModeStr(feature) ;
  return g_strdup_printf("%s/%s/%s", SCRATCH_CDS, END, desc) ;
}

/* Small utility to return the cds-range string for a particular subpart type */
static char* getScratchCdsStrRange(ZMapFeature feature, const gboolean subpart)
{
  const char *desc = subpart ? getFeatureSubpartStr(feature) : getFeatureModeStr(feature) ;
  return g_strdup_printf("%s/%s/%s", SCRATCH_CDS, RANGE, desc) ;
}


/* Add Annotation menu options that are applicable to the clicked feature (unless it's the
 * Annotation feature) */
void  makeMenuScratchOpsClickedFeature(ZMapGUIMenuItem menu,
                                       ZMapFeature feature,
                                       const int max_elements, 
                                       int *i)
{
  zMapReturnIfFail(feature) ;
  
  char *item_text = getScratchCopyStr(feature, TRUE) ;
  addMenuItem(menu, i, max_elements, ZMAPGUI_MENU_NORMAL, item_text, ITEM_MENU_COPY_SUBPART_TO_SCRATCH, itemMenuCB, NULL);
  g_free(item_text) ;
  
  item_text = getScratchCdsStrStart(feature, TRUE) ;
  addMenuItem(menu, i, max_elements, ZMAPGUI_MENU_NORMAL, item_text, ITEM_MENU_SCRATCH_CDS_START_SUBPART, itemMenuCB, NULL) ;
  g_free(item_text) ;

  item_text = getScratchCdsStrEnd(feature, TRUE) ;
  addMenuItem(menu, i, max_elements, ZMAPGUI_MENU_NORMAL, item_text, ITEM_MENU_SCRATCH_CDS_END_SUBPART, itemMenuCB, NULL) ;
  g_free(item_text) ;

  item_text = getScratchCdsStrRange(feature, TRUE) ;
  addMenuItem(menu, i, max_elements, ZMAPGUI_MENU_NORMAL, item_text, ITEM_MENU_SCRATCH_CDS_RANGE_SUBPART, itemMenuCB, NULL) ;
  g_free(item_text) ;
}


/* Add Annotation menu options that are applicable to selected features (except the 
 * Annotation feature) */
void makeMenuScratchOpsSelectedFeature(ZMapGUIMenuItem menu,
                                       GList *selected_features,
                                       const int max_elements,
                                       int *i)
{
  ZMapFeature feature = (ZMapFeature)(selected_features->data) ;

  char *item_text = getScratchCopyStr(feature, FALSE) ;
  addMenuItem(menu, i, max_elements, ZMAPGUI_MENU_NORMAL, item_text, ITEM_MENU_COPY_TO_SCRATCH, itemMenuCB, (gpointer)"<Ctrl>K");
  g_free(item_text) ;

  item_text = getScratchCdsStrStart(feature, FALSE) ;
  addMenuItem(menu, i, max_elements, ZMAPGUI_MENU_NORMAL, item_text, ITEM_MENU_SCRATCH_CDS_START, itemMenuCB, (gpointer)"<Ctrl>K");
  g_free(item_text) ;

  item_text = getScratchCdsStrEnd(feature, FALSE) ;
  addMenuItem(menu, i, max_elements, ZMAPGUI_MENU_NORMAL, item_text, ITEM_MENU_SCRATCH_CDS_END, itemMenuCB, (gpointer)"<Ctrl>K");
  g_free(item_text) ;

  item_text = getScratchCdsStrRange(feature, FALSE) ;
  addMenuItem(menu, i, max_elements, ZMAPGUI_MENU_NORMAL, item_text, ITEM_MENU_SCRATCH_CDS_RANGE, itemMenuCB, (gpointer)"<Ctrl>K");
  g_free(item_text) ;
}


/* Add Annotation menu options that are applicable to the clicked column (for all columns) */
void makeMenuScratchOpsClickedColumn(ZMapGUIMenuItem menu,
                                     const int max_elements, 
                                     int *i)
{
  addMenuItem(menu, i, max_elements, ZMAPGUI_MENU_NORMAL, SCRATCH_UNDO, ITEM_MENU_UNDO_SCRATCH, itemMenuCB, (gpointer)"<Ctrl>Z");
  addMenuItem(menu, i, max_elements, ZMAPGUI_MENU_NORMAL, SCRATCH_REDO, ITEM_MENU_REDO_SCRATCH, itemMenuCB, (gpointer)"<Ctrl>Y");
  addMenuItem(menu, i, max_elements, ZMAPGUI_MENU_NORMAL, SCRATCH_CLEAR, ITEM_MENU_CLEAR_SCRATCH, itemMenuCB, NULL);
}


/* Add Annotation menu options that are applicable when the Annotation feature was clicked */
void makeMenuScratchOpsAnnotationFeature(ZMapGUIMenuItem menu,
                                         const int max_elements, 
                                         int *i)
{
  addMenuItem(menu, i, max_elements, ZMAPGUI_MENU_NORMAL, SCRATCH_DELETE_SUBFEATURE, ITEM_MENU_DELETE_FROM_SCRATCH, itemMenuCB, NULL);
  addMenuItem(menu, i, max_elements, ZMAPGUI_MENU_NORMAL, SCRATCH_ATTRIBUTES, ITEM_MENU_SCRATCH_ATTRIBUTES, itemMenuCB, NULL);
  addMenuItem(menu, i, max_elements, ZMAPGUI_MENU_NORMAL, SCRATCH_CREATE, ITEM_MENU_SCRATCH_CREATE, itemMenuCB, NULL);

  addMenuItem(menu, i, max_elements, ZMAPGUI_MENU_NORMAL, SCRATCH_EXPORT, EXPORT_FEATURES_CLICKED, exportMenuCB, NULL);
}


/*
 * Options for the scratch column
 *
 * Called upon right-click within the editing/annotation/scratch column.
 */
ZMapGUIMenuItem zmapWindowMakeMenuScratchOps(int *start_index_inout,
                                             ZMapGUIMenuItemCallbackFunc callback_func,
                                             gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_BRANCH, SCRATCH_CONFIG_STR, 0, NULL, NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL, NULL},

      {ZMAPGUI_MENU_NONE, NULL,                     ITEM_MENU_INVALID,         NULL,       NULL}
    } ;
  int i ;
  int max_elements = 14 ;
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;

  zMapReturnValIfFail(menu_data && menu_data->window, menu) ;

  i = 1;
  menu[i].type = ZMAPGUI_MENU_NONE;

  ZMapFeature clicked_feature = menu_data->feature ;
  ZMapFeatureSet clicked_feature_set = menu_data->feature_set ;
  GList *selected_features = zmapWindowFocusGetFeatureList(menu_data->window->focus) ;

  /* Is the selected feature/column in the annotation column? */
  const gboolean selected_annotation = (menu_data->feature_set && 
                                        menu_data->feature_set->unique_id == zMapStyleCreateID(ZMAP_FIXED_STYLE_SCRATCH_NAME)) ;


  if (!selected_annotation && clicked_feature && clicked_feature_set)
    makeMenuScratchOpsClickedFeature(menu, clicked_feature, max_elements, &i) ;

  if (!selected_annotation && g_list_length(selected_features) > 0)
    makeMenuScratchOpsSelectedFeature(menu, selected_features, max_elements, &i) ;

  makeMenuScratchOpsClickedColumn(menu, max_elements, &i) ;

  if (selected_annotation && clicked_feature && clicked_feature_set)
    makeMenuScratchOpsAnnotationFeature(menu, max_elements, &i) ;


  menu[i].type = ZMAPGUI_MENU_NONE;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


static void itemMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapFeature feature ;

  zMapReturnIfFail(menu_data && menu_data->window) ;

  /* Retrieve the feature item info from the canvas item. */
  feature = zmapWindowItemGetFeature(menu_data->item);

  switch (menu_item_id)
    {
    case ITEM_MENU_MARK_ITEM:
      zmapWindowMarkSetItem(menu_data->window->mark, menu_data->item) ;
      break;

    case ITEM_MENU_COPY_TO_SCRATCH:
      zmapWindowScratchCopyFeature(menu_data->window, feature, menu_data->item, menu_data->x, menu_data->y, FALSE);
      break ;

    case ITEM_MENU_COPY_SUBPART_TO_SCRATCH:
      zmapWindowScratchCopyFeature(menu_data->window, feature, menu_data->item, menu_data->x, menu_data->y, TRUE);
      break ;

    case ITEM_MENU_SCRATCH_CDS_START:
      zmapWindowScratchSetCDS(menu_data->window, feature, menu_data->item, menu_data->x, menu_data->y, FALSE, TRUE, FALSE);
      break ;
    case ITEM_MENU_SCRATCH_CDS_END:
      zmapWindowScratchSetCDS(menu_data->window, feature, menu_data->item, menu_data->x, menu_data->y, FALSE, FALSE, TRUE);
      break ;
    case ITEM_MENU_SCRATCH_CDS_RANGE:
      zmapWindowScratchSetCDS(menu_data->window, feature, menu_data->item, menu_data->x, menu_data->y, FALSE, TRUE, TRUE);
      break ;
    case ITEM_MENU_SCRATCH_CDS_START_SUBPART:
      zmapWindowScratchSetCDS(menu_data->window, feature, menu_data->item, menu_data->x, menu_data->y, TRUE, TRUE, FALSE);
      break ;
    case ITEM_MENU_SCRATCH_CDS_END_SUBPART:
      zmapWindowScratchSetCDS(menu_data->window, feature, menu_data->item, menu_data->x, menu_data->y, TRUE, FALSE, TRUE);
      break ;
    case ITEM_MENU_SCRATCH_CDS_RANGE_SUBPART:
      zmapWindowScratchSetCDS(menu_data->window, feature, menu_data->item, menu_data->x, menu_data->y, TRUE, TRUE, TRUE);
      break ;

    case ITEM_MENU_DELETE_FROM_SCRATCH:
      zmapWindowScratchDeleteFeature(menu_data->window, feature, menu_data->item, menu_data->x, menu_data->y, TRUE);
      break ;

    case ITEM_MENU_SCRATCH_ATTRIBUTES:
      zmapWindowFeatureShow(menu_data->window, menu_data->item, TRUE) ;
      break ;

    case ITEM_MENU_SCRATCH_CREATE:
      /* If an xremote peer is connected, send create-feature message; otherwise show
       * create-feature dialog */
      if (menu_data->window->xremote_client)
        zmapWindowFeatureCallXRemote(menu_data->window, (ZMapFeatureAny)menu_data->feature, ZACP_CREATE_FEATURE, NULL) ;
      else
        zmapWindowFeatureShow(menu_data->window, menu_data->item, TRUE) ;
      break ;

    case ITEM_MENU_SCRATCH_EXPORT:
      

    case ITEM_MENU_UNDO_SCRATCH:
      zmapWindowScratchUndo(menu_data->window);
      break ;

    case ITEM_MENU_REDO_SCRATCH:
      zmapWindowScratchRedo(menu_data->window);
      break ;

    case ITEM_MENU_CLEAR_SCRATCH:
      zmapWindowScratchClear(menu_data->window);
      break ;

    case ITEM_MENU_SEARCH:
      zmapWindowCreateSearchWindow(menu_data->window,
                                   NULL, NULL,
                                   menu_data->window->context_map,
                                   menu_data->item) ;

      break ;
    case ITEM_MENU_PFETCH:
      zmapWindowPfetchEntry(menu_data->window, (char *)g_quark_to_string(feature->original_id)) ;
      break ;

    case ITEM_MENU_SEQUENCE_SEARCH_DNA:
      zmapWindowCreateSequenceSearchWindow(menu_data->window, menu_data->item, ZMAPSEQUENCE_DNA) ;

      break ;

    case ITEM_MENU_SEQUENCE_SEARCH_PEPTIDE:
      zmapWindowCreateSequenceSearchWindow(menu_data->window, menu_data->item, ZMAPSEQUENCE_PEPTIDE) ;

      break ;

    case ITEM_MENU_SHOW_URL_IN_BROWSER:
      {
        gboolean result ;
        GError *error = NULL ;

        if (!(result = zMapLaunchWebBrowser(feature->url, &error)))
          {
            zMapWarning("Error: %s", error->message) ;

            g_error_free(error) ;
          }
        break ;
      }
    case ITEM_MENU_SHOW_TRANSLATION:
      {
        zmapWindowItemShowTranslation(menu_data->window, menu_data->item) ;

        break;
      }

    case ITEM_MENU_SHOW_EVIDENCE:
    case ITEM_MENU_ADD_EVIDENCE:
    case ITEM_MENU_SHOW_TRANSCRIPT:       /* XML formats are different for evidence and transcripts */
    case ITEM_MENU_ADD_TRANSCRIPT:        /* but we handle that in zmapWindowFeatureGetEvidence() */
      {
        // show evidence for a transcript
        ZMapWindowFocus focus = menu_data->window->focus ;

        if (focus)
          {
            ZMapWindowHighlightData highlight_data ;

            ZMapFeatureAny any = (ZMapFeatureAny)menu_data->feature ; /* our transcript */

            /* clear any existing highlight */
            if (menu_item_id != ITEM_MENU_ADD_EVIDENCE)
              zmapWindowFocusResetType(focus,WINDOW_FOCUS_GROUP_EVIDENCE) ;

            /* add the transcript to the evidence group */
            zmapWindowFocusAddItemType(menu_data->window->focus,
                                       menu_data->item, menu_data->feature, NULL, WINDOW_FOCUS_GROUP_EVIDENCE) ;

            /* Now call xremote to request the list of evidence features from otterlace. */
            highlight_data = g_new0(ZMapWindowHighlightDataStruct, 1) ;
            highlight_data->window = menu_data->window ;
            highlight_data->feature = any ;

            zmapWindowFeatureGetEvidence(menu_data->window, menu_data->feature,
                                         zmapWindowHighlightEvidenceCB, highlight_data) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
            /* search for the features named in the list and find thier canvas items */

            for ( ; evidence ; evidence = evidence->next)
              {
                GList *items,*items_free;
                GQuark wildcard = g_quark_from_string("*");
                char *feature_name;
                GQuark feature_search_id;

                /* need to add a * to the end to match strand and frame name mangling */
                feature_name = g_strdup_printf("%s*",g_quark_to_string(GPOINTER_TO_UINT(evidence->data)));
                feature_name = zMapFeatureCanonName(feature_name);    /* done in situ */
                feature_search_id = g_quark_from_string(feature_name);

                /* catch NULL names due to ' getting escaped int &apos; */
                if(feature_search_id == wildcard)
                  continue;

                items_free = zmapWindowFToIFindItemSetFull(menu_data->window,
                                                           menu_data->window->context_to_item,
                                                           any->parent->parent->parent->unique_id,
                                                           any->parent->parent->unique_id,
                                                           wildcard,0,"*","*",
                                                           feature_search_id,
                                                           NULL,NULL);


                /* zMapLogWarning("evidence %s returns %d features", feature_name, g_list_length(items_free));*/
                g_free(feature_name);

                for(items = items_free; items; items = items->next)
                  {
                    /* NOTE: need to filter by transcript start and end coords in case of repeat alignments */
                    /* Not so - annotators want to see duplicated features' data */

                    evidence_items = g_list_prepend(evidence_items,items->data);
                  }

                g_list_free(items_free);
              }

            /* menu_data->item is the transcript and would be the
             * focus hot item if this was a focus highlight
             * in this call it's irrelevant so don't set the hot item, pass NULL instead
             */

            zmapWindowFocusAddItemsType(menu_data->window->focus, evidence_items,
                  NULL /* menu_data->item */, WINDOW_FOCUS_GROUP_EVIDENCE);


            g_list_free(evidence);
            g_list_free(evidence_items);
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

          }

        break;
      }

    case ITEM_MENU_EXPAND:
      {
        zmapWindowFeatureExpand(menu_data->window, menu_data->item, menu_data->feature, menu_data->container_set);

        break;
      }

    case ITEM_MENU_CONTRACT:
      {
        zmapWindowFeatureContract(menu_data->window, menu_data->item, menu_data->feature, menu_data->container_set);

        break;
      }

    default:
      {
        zMapWarning("%s", "Unexpected menu callback action\n") ;
        zMapWarnIfReached() ;
        break ;
      }
    }

  g_free(menu_data) ;

  return ;
}




static ZMapGUIMenuItem makeMenuShowTranslation(int *start_index_inout,
                                               ZMapGUIMenuItemCallbackFunc callback_func,
                                               gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_NORMAL, "Show Translation in ZMap", ITEM_MENU_SHOW_TRANSLATION, itemMenuCB, NULL, "T"},
      {ZMAPGUI_MENU_NONE, NULL,                         ITEM_MENU_INVALID,          NULL,       NULL}
    } ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}



static ZMapGUIMenuItem makeMenuPfetchOps(int *start_index_inout,
                                         ZMapGUIMenuItemCallbackFunc callback_func,
                                         gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_NORMAL, "Pfetch this feature", ITEM_MENU_PFETCH,  itemMenuCB, NULL},
      {ZMAPGUI_MENU_NONE, NULL,                    ITEM_MENU_INVALID, NULL,       NULL}
    } ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}









static ZMapGUIMenuItem makeMenuURL(int *start_index_inout,
                                   ZMapGUIMenuItemCallbackFunc callback_func,
                                   gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_NORMAL, "URL", ITEM_MENU_SHOW_URL_IN_BROWSER, itemMenuCB, NULL},
      {ZMAPGUI_MENU_NONE,   NULL,  ITEM_MENU_INVALID,             NULL,       NULL}
    } ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


ZMapGUIMenuItem zmapWindowMakeMenuSearchListOps(int *start_index_inout,
                                                ZMapGUIMenuItemCallbackFunc callback_func,
                                                gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_BRANCH, SEARCH_STR, 0, NULL, NULL},
      {ZMAPGUI_MENU_NORMAL, SEARCH_STR"/""DNA Search Window",              ITEM_MENU_SEQUENCE_SEARCH_DNA, searchListMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, SEARCH_STR"/""Peptide Search Window",          ITEM_MENU_SEQUENCE_SEARCH_PEPTIDE, searchListMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, SEARCH_STR"/""Feature Search Window",          ITEM_MENU_SEARCH,              searchListMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, SEARCH_STR"/""List All Column Features",       ITEM_MENU_LIST_ALL_FEATURES,   searchListMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, SEARCH_STR"/""List Column Features With This Name", ITEM_MENU_LIST_NAMED_FEATURES, searchListMenuCB, NULL},
      {ZMAPGUI_MENU_NONE, NULL, ITEM_MENU_INVALID, NULL, NULL}
    } ;
  enum {NAMED_FEATURE_INDEX = 5} ;
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapFeatureAny feature_any = NULL ;

  zMapReturnValIfFail(menu_data, menu) ;

  /* If user clicked on a feature set then "List with name" option is not relevant. */
  feature_any = zmapWindowItemGetFeatureAny(menu_data->item);
  if (feature_any->struct_type == ZMAPFEATURE_STRUCT_FEATURESET)
    menu[NAMED_FEATURE_INDEX].type = ZMAPGUI_MENU_NONE ;
  else
    menu[NAMED_FEATURE_INDEX].type = ZMAPGUI_MENU_NORMAL ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}




/* Make the Bump, Hide, Masked, Mark and Compress subpart of the menu. */
ZMapGUIMenuItem zmapWindowMakeMenuBump(int *start_index_inout,
                                       ZMapGUIMenuItemCallbackFunc callback_func,
                                       gpointer callback_data, ZMapStyleBumpMode curr_bump)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_NORMAL, "Copy Chr Coords", ZMAPWINDOW_COPY, copyPasteMenuCB,  NULL},
      {ZMAPGUI_MENU_NORMAL, "Paste Chr Coords", ZMAPWINDOW_PASTE, copyPasteMenuCB,  NULL},


      {ZMAPGUI_MENU_TOGGLE, "Column Bump", ZMAPBUMP_UNBUMP, bumpToggleMenuCB, NULL, "B"},
      {ZMAPGUI_MENU_NORMAL, "Column Hide", ZMAPWINDOWCOLUMN_HIDE, configureMenuCB,  NULL},
      {ZMAPGUI_MENU_TOGGLE, "Show Masked Features", ZMAPWINDOWCOLUMN_MASK,  maskToggleMenuCB, NULL, NULL},
      {ZMAPGUI_MENU_TOGGLE, "Toggle Mark", 0, markMenuCB, NULL, "M"},
      {ZMAPGUI_MENU_TOGGLE, "Compress Columns", ZMAPWINDOW_COMPRESS_MARK, compressMenuCB, NULL, "c"},

      {ZMAPGUI_MENU_SEPARATOR, NULL, 0, NULL, NULL, NULL},

      {ZMAPGUI_MENU_BRANCH, COLUMN_CONFIG_STR, 0, NULL, NULL, NULL},
      {ZMAPGUI_MENU_NORMAL, COLUMN_CONFIG_STR "/" COLUMN_THIS_ONE, ZMAPWINDOWCOLUMN_CONFIGURE, configureMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, COLUMN_CONFIG_STR "/" COLUMN_ALL, ZMAPWINDOWCOLUMN_CONFIGURE_ALL, configureMenuCB, NULL},
      {ZMAPGUI_MENU_BRANCH, COLUMN_CONFIG_STR "/" COLUMN_BUMP_OPTS, 0, NULL, NULL},
      {ZMAPGUI_MENU_RADIO, NULL, ZMAPBUMP_OVERLAP, bumpMenuCB, NULL},
      {ZMAPGUI_MENU_RADIO, NULL, ZMAPBUMP_ALTERNATING, bumpMenuCB, NULL},
      {ZMAPGUI_MENU_RADIO, NULL, ZMAPBUMP_ALL, bumpMenuCB, NULL},
      {ZMAPGUI_MENU_RADIO, NULL, ZMAPBUMP_UNBUMP, bumpMenuCB, NULL},

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      /* Remove when we are sure no one wants it... */
      {ZMAPGUI_MENU_NORMAL, "Unbump All Columns", 0, unbumpAllCB, NULL},
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

      {ZMAPGUI_MENU_NONE, NULL, 0, NULL, NULL}  /* menu terminates on id = 0 in one loop below */
    } ;
  enum {MENU_INDEX_BUMP = 2, MENU_INDEX_MARK = 5,  MENU_INDEX_COMPRESS = 6} ;
                                                            /* Keep in step with menu[] positions. */
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data;
  static gboolean menu_set = FALSE ;
#if NOT_USED
  static int ind_hide_evidence = -1;
#endif
  static int ind_mask = -1;
  ZMapGUIMenuItem menu_item = NULL ;
  static gboolean compress = FALSE ;
  ZMapWindowContainerGroup column_container, block_container;
  FooCanvasGroup *column_group =  NULL;

  zMapReturnValIfFail(menu_data, menu) ;


  /* Set the initial toggle button correctly....make sure this stays in step with the array above.
   * NOTE logic, this button is either "no bump" or "Name + No Bump", the latter should be
   * selectable whatever.... */

  /* Dynamically set various options in the menu text. */
  if (!menu_set)
    {
      ZMapGUIMenuItem tmp = menu ;

      while ((tmp->type))
        {
          if (tmp->type == ZMAPGUI_MENU_RADIO)
            tmp->name = g_strdup_printf("%s/%s/%s",
                                        COLUMN_CONFIG_STR, COLUMN_BUMP_OPTS, zmapStyleBumpMode2ShortText((ZMapStyleBumpMode)(tmp->id))) ;

          if (tmp->id == ZMAPWINDOWCOLUMN_MASK)
            ind_mask = tmp - menu;

          tmp++ ;
        }

      menu_set = TRUE ;
    }

  /* Set main bump toggle menu item. */
  menu_item = &(menu[MENU_INDEX_BUMP]) ;
  if (curr_bump != ZMAPBUMP_UNBUMP)
    {
      menu_item->type = ZMAPGUI_MENU_TOGGLEACTIVE ;
      menu_item->id = curr_bump ;
    }
  else
    {
      menu_item->type = ZMAPGUI_MENU_TOGGLE ;
      menu_item->id = ZMAPBUMP_UNBUMP ;
    }

  /* Set "Mark" toggle item. */
  menu_item = &(menu[MENU_INDEX_MARK]) ;
  if (zmapWindowMarkIsSet(menu_data->window->mark))
    {
      menu_item->type = ZMAPGUI_MENU_TOGGLEACTIVE ;
    }
  else
    {
      menu_item->type = ZMAPGUI_MENU_TOGGLE ;
    }

  /* Set "Compress" toggle item. */
  menu_item = &(menu[MENU_INDEX_COMPRESS]) ;
  column_container = zmapWindowContainerCanvasItemGetContainer(menu_data->item) ;
  column_group = (FooCanvasGroup *)column_container;
  block_container = zmapWindowContainerUtilsGetParentLevel(column_container, ZMAPCONTAINER_LEVEL_BLOCK) ;
  if (zmapWindowContainerBlockIsCompressedColumn((ZMapWindowContainerBlock)block_container))
    {
      compress = TRUE ;
      menu_item->type = ZMAPGUI_MENU_TOGGLEACTIVE ;
    }
  else
    {
      compress = FALSE ;
      menu_item->type = ZMAPGUI_MENU_TOGGLE ;
    }


  /* set toggle state of masked features for this column */
  if (ind_mask >= 0)
    {
      menu_item = &(menu[ind_mask]) ;

      /* set toggle state of masked features for this column
         hide the menu item if it's not maskable */
      menu_item->type = ZMAPGUI_MENU_TOGGLE ;

      if (!menu_data->container_set->maskable
          || !menu_data->window->highlights_set.masked
          || !zMapWindowFocusCacheGetSelectedColours(WINDOW_FOCUS_GROUP_MASKED, NULL, NULL))
        menu_item->type = ZMAPGUI_MENU_HIDE;
      else if (!menu_data->container_set->masked)
        menu_item->type = ZMAPGUI_MENU_TOGGLEACTIVE ;
    }


  /* Unset any previously active bump sub-menu radio button.... */
  menu_item = &(menu[0]) ;
  while (menu_item->type != ZMAPGUI_MENU_NONE)
    {
      if (menu_item->type == ZMAPGUI_MENU_RADIOACTIVE)
        {
          menu_item->type = ZMAPGUI_MENU_RADIO ;
          break ;
        }

      menu_item++ ;
    }

  /* Set any new active bump sub-menu radio button... */
  menu_item = &(menu[0]) ;
  while (menu_item->type != ZMAPGUI_MENU_NONE)
    {
      if (menu_item->type == ZMAPGUI_MENU_RADIO && menu_item->id == zMapStylePatchBumpMode(curr_bump))
        {
          menu_item->type = ZMAPGUI_MENU_RADIOACTIVE ;
          break ;
        }

      menu_item++ ;
    }

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


/* sort into order of style mode then alpha by name but put current style at the front */
gint style_menu_sort(gconstpointer a, gconstpointer b, gpointer data)
{
  ZMapFeatureTypeStyle style = (ZMapFeatureTypeStyle) data;
  ZMapFeatureTypeStyle sa = (ZMapFeatureTypeStyle) a, sb = (ZMapFeatureTypeStyle) b;
  const char *na  = NULL , *nb = NULL ;

  if (!sa || !sb)
    return 0 ;

  if(sa->mode != sb->mode)
    {
      if(sa->mode == style->mode)
        return -1;
      if(sb->mode == style->mode)
        return  1;
      if(sa->mode < sb->mode)
        return -1;
      return 1;
    }

  na = g_quark_to_string(sa->unique_id);
  nb = g_quark_to_string(sb->unique_id);

  return strcmp(na,nb);
}


/* is a style comapatble with a feature type: need to avoid crashes due to wrong data */
gboolean style_is_compatable(ZMapFeatureTypeStyle style, ZMapStyleMode f_type)
{
  if (!style)
    return FALSE ;
  if(f_type == style->mode)
    return TRUE;

  switch(f_type)
    {
    case ZMAPSTYLE_MODE_BASIC:
      if(style->mode == ZMAPSTYLE_MODE_GRAPH)
        return TRUE;
      return FALSE;

    case ZMAPSTYLE_MODE_GRAPH:
    case ZMAPSTYLE_MODE_ASSEMBLY_PATH:
    case ZMAPSTYLE_MODE_GLYPH:
      if(style->mode == ZMAPSTYLE_MODE_BASIC)
        return TRUE;
      return FALSE;

    case ZMAPSTYLE_MODE_ALIGNMENT:
    case ZMAPSTYLE_MODE_TRANSCRIPT:
    case ZMAPSTYLE_MODE_SEQUENCE:
    case ZMAPSTYLE_MODE_TEXT:
    default:
      return FALSE;
    }
}


static ZMapGUIMenuItem zmapWindowMakeMenuStyle(int *start_index_inout,
                                               ZMapGUIMenuItemCallbackFunc callback_func, gpointer callback_data,
                                               ZMapFeatureTypeStyle cur_style, ZMapStyleMode f_type)
{
  /* get a list of featuresets from the window's context_map */
  static ZMapGUIMenuItem menu = NULL;
  static int n_menu = 0;
  ItemMenuCBData cbdata  = (ItemMenuCBData)callback_data;
  ZMapGUIMenuItem m;
  GHashTable *styles = NULL ;
  GList *style_list, *sl;
  int n_styles;
  int i = 0;

  zMapReturnValIfFail(cbdata && cbdata->window && cbdata->window->context_map, NULL ) ;

  styles = cbdata->window->context_map->styles;

  zMap_g_hash_table_get_data(&style_list,styles);
  style_list = g_list_sort_with_data(style_list, style_menu_sort, (gpointer) cur_style);
  n_styles = g_list_length(style_list);                /* max possible, will never be reached */

  if (!n_styles)
    return NULL;


  if(n_menu < n_styles + N_STYLE_MODE + 1)
    {
      /* as this derives from config data read on creating the view
       * it will not change unless we reconfig and open another view
       * alloc enough for all views
       */
      if(menu)
        {
          for(m = menu; m->type != ZMAPGUI_MENU_NONE ;m++)
            g_free((void *)(m->name));

          g_free(menu);
        }

        n_menu = n_styles + N_STYLE_MODE + 10;
      menu = g_new0(ZMapGUIMenuItemStruct, n_menu);

    }

  m = menu;

  m->type = ZMAPGUI_MENU_NORMAL;
  m->name = g_strdup(COLUMN_CONFIG_STR "/" COLUMN_COLOUR);
  m->id = 0;
  m->callback_func = colourMenuCB;
  m++;

  /* add sub menu */
  m->type = ZMAPGUI_MENU_BRANCH;
  m->name = g_strdup(COLUMN_CONFIG_STR "/" COLUMN_STYLE_OPTS);
  m->id = 0;
  m->callback_func = NULL;
  m++;

  for( i = 0, sl = style_list;i < n_styles; i++, sl = sl->next)
    {
      char *name;
      const char *mode = "";
      ZMapFeatureTypeStyle s = (ZMapFeatureTypeStyle) sl->data;

      if (!style_is_compatable(s,f_type))
        continue;

      m->type = ZMAPGUI_MENU_NORMAL;

      name = get_menu_string(s->original_id, '-');

      if(s->mode != cur_style->mode)
        mode = (char *) zmapStyleMode2ShortText(s->mode);

      m->name = g_strdup_printf(COLUMN_CONFIG_STR "/" COLUMN_STYLE_OPTS"/%s%s%s", mode, *mode ? "/" : "", name);
      g_free(name);

      m->id = s->unique_id;
      m->callback_func = setStyleCB;
      m++;
    }

  if (m <= menu + 3)        /* empty sub_menu or choice of current */
    m = menu + 1;
  m->type = ZMAPGUI_MENU_NONE;
  m->name = NULL;


  /* this overrides data in the menus as given in the args, but index and func are always NULL */
  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu;
}


/*
 *     Following routines set up the feature, dna, peptide and context view or export functions.
 */


/* Make the top level Show entry. */
ZMapGUIMenuItem zmapWindowMakeMenuShowTop(int *start_index_inout,
                                          ZMapGUIMenuItemCallbackFunc callback_func,
                                          gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_BRANCH, FEATURE_SHOW_STR, 0, NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL, 0, NULL, NULL}
    } ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


/* Make top entry of View submenu. */
ZMapGUIMenuItem zmapWindowMakeMenuViewTop(int *start_index_inout,
                                          ZMapGUIMenuItemCallbackFunc callback_func,
                                          gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_BRANCH, FEATURE_SHOW_STR "/" FEATURE_VIEW_STR, 0, NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL, 0, NULL, NULL}
    } ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


/* View feature. */
ZMapGUIMenuItem zmapWindowMakeMenuShowFeature(int *start_index_inout,
                                              ZMapGUIMenuItemCallbackFunc callback_func,
                                              gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_VIEW_STR "/" FEATURE_STR, 0, featureMenuCB, NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL, 0, NULL, NULL}
    } ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}

/* View dna for non-transcripts. */
ZMapGUIMenuItem zmapWindowMakeMenuDNAFeatureAny(int *start_index_inout,
                                                ZMapGUIMenuItemCallbackFunc callback_func,
                                                gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_VIEW_STR "/" FEATURE_DNA_SHOW_STR, ZMAPUNSPLICED, dnaMenuCB, NULL},
      {ZMAPGUI_MENU_NONE, NULL, 0, NULL, NULL}
    } ;
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


ZMapGUIMenuItem zmapWindowMakeMenuDNATranscript(int *start_index_inout,
                                                ZMapGUIMenuItemCallbackFunc callback_func,
                                                gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_BRANCH, FEATURE_SHOW_STR "/" FEATURE_VIEW_STR "/" FEATURE_DNA_SHOW_STR, 0, NULL, NULL},
      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_VIEW_STR "/" FEATURE_DNA_SHOW_STR "/CDS",                    ZMAPCDS,           dnaMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_VIEW_STR "/" FEATURE_DNA_SHOW_STR "/transcript",             ZMAPTRANSCRIPT,    dnaMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_VIEW_STR "/" FEATURE_DNA_SHOW_STR "/unspliced",              ZMAPUNSPLICED,     dnaMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_VIEW_STR "/" FEATURE_DNA_SHOW_STR "/with flanking sequence", ZMAPCHOOSERANGE,   dnaMenuCB, NULL},
      {ZMAPGUI_MENU_NONE, NULL, 0, NULL, NULL}
    } ;
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


ZMapGUIMenuItem zmapWindowMakeMenuPeptide(int *start_index_inout,
                                          ZMapGUIMenuItemCallbackFunc callback_func,
                                          gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
#ifdef RDS_REMOVED_TICKET_50781
      {ZMAPGUI_MENU_BRANCH, "_" FEATURE_PEPTIDE_SHOW_STR, 0, NULL, NULL},
      {ZMAPGUI_MENU_NORMAL, FEATURE_PEPTIDE_SHOW_STR"/CDS",                    ZMAPCDS,           peptideMenuCB, NULL},
#endif /* RDS_REMOVED_TICKET_50781 */

      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_VIEW_STR "/" FEATURE_PEPTIDE_SHOW_STR,                    ZMAPCDS,           peptideMenuCB, NULL},

#ifdef RDS_REMOVED_TICKET_50781
      {ZMAPGUI_MENU_NORMAL, FEATURE_PEPTIDE_SHOW_STR"/transcript",             ZMAPTRANSCRIPT,    peptideMenuCB, NULL},
#endif /* RDS_REMOVED_TICKET_50781 */
      {ZMAPGUI_MENU_NONE, NULL, 0, NULL, NULL}
    } ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}




/* Make top entry of Export submenu. */
ZMapGUIMenuItem zmapWindowMakeMenuExportTop(int *start_index_inout,
                                            ZMapGUIMenuItemCallbackFunc callback_func,
                                            gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_BRANCH, FEATURE_SHOW_STR "/" FEATURE_EXPORT_STR, 0, NULL, NULL},
      {ZMAPGUI_MENU_NONE, NULL, 0, NULL, NULL}
    } ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


ZMapGUIMenuItem zmapWindowMakeMenuDNAFeatureAnyFile(int *start_index_inout,
                                                    ZMapGUIMenuItemCallbackFunc callback_func,
                                                    gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_EXPORT_STR "/" FEATURE_DNA_EXPORT_STR,               ZMAPUNSPLICED_FILE,     dnaMenuCB, NULL},
      {ZMAPGUI_MENU_NONE, NULL, 0, NULL, NULL}
    } ;
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


ZMapGUIMenuItem zmapWindowMakeMenuDNATranscriptFile(int *start_index_inout,
                                                    ZMapGUIMenuItemCallbackFunc callback_func,
                                                    gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_BRANCH, FEATURE_SHOW_STR "/" FEATURE_EXPORT_STR "/" FEATURE_DNA_EXPORT_STR, 0, NULL, NULL},
      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_EXPORT_STR "/" FEATURE_DNA_EXPORT_STR "/CDS",                    ZMAPCDS_FILE,           dnaMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_EXPORT_STR "/" FEATURE_DNA_EXPORT_STR "/transcript",             ZMAPTRANSCRIPT_FILE,    dnaMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_EXPORT_STR "/" FEATURE_DNA_EXPORT_STR "/unspliced",              ZMAPUNSPLICED_FILE,     dnaMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_EXPORT_STR "/" FEATURE_DNA_EXPORT_STR "/with flanking sequence", ZMAPCHOOSERANGE_FILE,   dnaMenuCB, NULL},
      {ZMAPGUI_MENU_NONE, NULL, 0, NULL, NULL}
    } ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


ZMapGUIMenuItem zmapWindowMakeMenuPeptideFile(int *start_index_inout,
                                              ZMapGUIMenuItemCallbackFunc callback_func,
                                              gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_BRANCH, FEATURE_SHOW_STR "/" FEATURE_EXPORT_STR "/" FEATURE_PEPTIDE_EXPORT_STR, 0, NULL, NULL},
      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_EXPORT_STR "/" FEATURE_PEPTIDE_EXPORT_STR "/CDS",             ZMAPCDS_FILE,           peptideMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_EXPORT_STR "/" FEATURE_PEPTIDE_EXPORT_STR "/transcript",      ZMAPTRANSCRIPT_FILE,    peptideMenuCB, NULL},
      {ZMAPGUI_MENU_NONE, NULL, 0, NULL, NULL}
    } ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


/*
 * This is the right-click menu in a view (no feature selected)
 *
 * (sm23) I have altered this to output features only from the column in which
 * the right-click is made (irrespective of whether a column is hot). Output is
 * now all features in all featuresets in the column or all that overlap with
 * the marked region if it is set.
 */
ZMapGUIMenuItem zmapWindowMakeMenuColumnExportOps(int *start_index_inout,
                                                  ZMapGUIMenuItemCallbackFunc callback_func,
                                                  gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_EXPORT_STR "/Column features" , EXPORT_FEATURES_ALL, exportMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_EXPORT_STR "/Column features (marked)" , EXPORT_FEATURES_MARKED, exportMenuCB, NULL},
      {ZMAPGUI_MENU_NONE, NULL, 0, NULL, NULL}
    } ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


ZMapGUIMenuItem zmapWindowMakeMenuItemExportOps(int *start_index_inout,
                                                ZMapGUIMenuItemCallbackFunc callback_func,
                                                gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_EXPORT_STR "/Clicked feature" , EXPORT_FEATURES_CLICKED, exportMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_EXPORT_STR "/Column features" , EXPORT_FEATURES_ALL, exportMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, FEATURE_SHOW_STR "/" FEATURE_EXPORT_STR "/Column features (marked)" , EXPORT_FEATURES_MARKED, exportMenuCB, NULL},
      {ZMAPGUI_MENU_NONE, NULL, 0, NULL, NULL}
    } ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}



static void featureMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;

  zmapWindowFeatureShow(menu_data->window, menu_data->item, FALSE) ;

  return ;
}

static void dnaMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapFeature feature = NULL ;
  gboolean spliced = FALSE, cds = FALSE ;
  char *dna = NULL ;
  ZMapFeatureContext context = NULL ;
  const char *seq_name = NULL , *molecule_type = NULL, *gene_name = NULL ;
  int seq_len = 0, user_start = 0, user_end = 0 ;
  GError *error = NULL ;

  if (!menu_data || !menu_data->window )
    return ;

  /* Retrieve feature selected by user. */
  feature = zMapWindowCanvasItemGetFeature(menu_data->item) ;
  if (!feature)
    return ;

  user_start = feature->x1 ;
  user_end = feature->x2 ;

  context = menu_data->window->feature_context ;

  seq_name = (char *)g_quark_to_string(context->original_id) ;

  spliced = cds = FALSE ;
  switch (menu_item_id)
    {
    case ZMAPCDS:
    case ZMAPCDS_FILE:
      {
        spliced = cds = TRUE ;

        break ;
      }
    case ZMAPTRANSCRIPT:
    case ZMAPTRANSCRIPT_FILE:
      {
        spliced = TRUE ;

        break ;
      }
    case ZMAPUNSPLICED:
    case ZMAPUNSPLICED_FILE:
      {
        break ;
      }
    case ZMAPCHOOSERANGE:
    case ZMAPCHOOSERANGE_FILE:
      {
        break ;
      }
    default:
      {
        zMapWarnIfReached() ;
        break ;
      }
    }

  if (feature->mode == ZMAPSTYLE_MODE_TRANSCRIPT)
    {
      molecule_type = "DNA" ;
      gene_name = (char *)g_quark_to_string(feature->original_id) ;
    }

  if (menu_item_id == ZMAPCHOOSERANGE || menu_item_id == ZMAPCHOOSERANGE_FILE)
    {
      ZMapWindowDialogType dialog_type ;

      if (menu_item_id == ZMAPCHOOSERANGE)
        dialog_type = ZMAP_DIALOG_SHOW ;
      else
        dialog_type = ZMAP_DIALOG_EXPORT ;

      dna = zmapWindowDNAChoose(menu_data->window, menu_data->item, dialog_type, &user_start, &user_end) ;
    }
  else if (feature->mode == ZMAPSTYLE_MODE_TRANSCRIPT)
    {
      dna = zMapFeatureGetTranscriptDNA(feature, spliced, cds) ;
    }
  else
    {
      dna = zMapFeatureGetFeatureDNA(feature) ;
    }


  if (!dna)
    {
      zMapWarning("%s", "No DNA available") ;
    }
  else
    {
      seq_len = strlen(dna) ;

      if (menu_item_id == ZMAPCDS || menu_item_id == ZMAPTRANSCRIPT || menu_item_id == ZMAPUNSPLICED
          || menu_item_id == ZMAPCHOOSERANGE)
        {
          GList *text_attrs = NULL ;
          char *dna_fasta ;
          char *window_title ;
          ZMapFeatureTypeStyle style ;
          GdkColor *non_coding, *coding, *split_5, *split_3 ;

          window_title = g_strdup_printf("%s%s%s",
                                         seq_name,
                                         gene_name ? ":" : "",
                                         gene_name ? gene_name : "") ;

          dna_fasta = zMapFASTAString(ZMAPFASTA_SEQTYPE_DNA, seq_name, molecule_type, gene_name, seq_len, dna) ;

          /* Transcript annotation only supported if whole transcript shown, add annotation of
           * partial transcript as an enhancement if/when users ask for it. */
          if (feature->mode == ZMAPSTYLE_MODE_TRANSCRIPT
              && (user_start <= feature->x1 && user_end >= feature->x2)
              && ((style = zMapFindStyle(menu_data->window->context_map->styles,
                                         zMapStyleCreateID(ZMAP_FIXED_STYLE_DNA_NAME)))
                  && getSeqColours(style, &non_coding, &coding, &split_5, &split_3)))
            {
              char *dna_fasta_title ;
              int title_len ;

              text_attrs = getTranscriptTextAttrs(feature, spliced, cds,
                                                  non_coding, coding, split_5, split_3) ;

              /* Find out length of FASTA title so we can offset for it. */
              dna_fasta_title = zMapFASTATitle(ZMAPFASTA_SEQTYPE_DNA, seq_name, molecule_type, gene_name, seq_len) ;
              title_len = strlen(dna_fasta_title) - 1 ;            /* Get rid of newline in length. */

              /* If user chose a DNA range then offset for that too. */
              if (user_start)
                user_start = feature->x1 - user_start ;
              title_len += user_start ;

              /* Now offset all the exons to take account of title length. */
              g_list_foreach(text_attrs, offsetTextAttr, GINT_TO_POINTER(title_len)) ;
            }

          zMapGUIShowTextFull(window_title, dna_fasta, FALSE, text_attrs, NULL) ;

          g_free(window_title) ;
          g_free(dna_fasta) ;
        }
      else
        {
          exportFASTA(menu_data->window, ZMAPFASTA_SEQTYPE_DNA, dna, seq_name, seq_len, molecule_type, gene_name, &error) ;
        }

      g_free(dna) ;
    }

  g_free(menu_data) ;

  if (error)
    {
      zMapShowMsg(ZMAP_MSG_WARNING, "%s", error->message) ;
      g_error_free(error) ;
    }

  return ;
}

/* If we had a more generalised "sequence" object that could be with dna or peptide we
 * could merge this routine with the dnamenucb...much more elegant.... */
static void peptideMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapFeature feature ;
  gboolean spliced, cds ;
  char *dna ;
  ZMapPeptide peptide ;
  ZMapFeatureContext context ;
  const char *seq_name = NULL , *molecule_type = NULL, *gene_name = NULL ;
  int pep_length, start_incr = 0 ;
  GError *error = NULL ;

  if (!menu_data || !menu_data->item || !menu_data->window)
    return ;

  feature = zMapWindowCanvasItemGetFeature(menu_data->item) ;
  if (feature->mode != ZMAPSTYLE_MODE_TRANSCRIPT)
    return ;

  context = menu_data->window->feature_context ;

  seq_name = (char *)g_quark_to_string(context->original_id) ;

  spliced = cds = FALSE ;
  switch (menu_item_id)
    {
    case ZMAPCDS:
    case ZMAPCDS_FILE:
      {
        spliced = cds = TRUE ;

        break ;
      }
    case ZMAPTRANSCRIPT:
    case ZMAPTRANSCRIPT_FILE:
      {
        spliced = TRUE ;

        break ;
      }
    default:
      zMapWarnIfReached() ;
      break ;
    }

  molecule_type = "Protein" ;
  gene_name = (char *)g_quark_to_string(feature->original_id) ;

  if ((dna = zMapFeatureGetTranscriptDNA(feature, spliced, cds)))
    {
      /* Adjust for when its known that the start exon is incomplete.... */
      if (feature->feature.transcript.flags.start_not_found)
        start_incr = feature->feature.transcript.start_not_found - 1 ; /* values are 1 <= start_not_found <= 3 */

      peptide = zMapPeptideCreate(seq_name, gene_name, (dna + start_incr), NULL, TRUE) ;

      /* Note that we do not include the "Stop" in the peptide length, is this the norm ? */
      pep_length = zMapPeptideLength(peptide) ;
      if (zMapPeptideHasStopCodon(peptide))
        pep_length-- ;

      if (menu_item_id == ZMAPCDS || menu_item_id == ZMAPTRANSCRIPT || menu_item_id == ZMAPUNSPLICED)
        {
          char *title ;
          char *peptide_fasta ;

          peptide_fasta = zMapFASTAString(ZMAPFASTA_SEQTYPE_AA, seq_name, molecule_type, gene_name,
                                          pep_length, zMapPeptideSequence(peptide)) ;

          title = g_strdup_printf("%s%s%s",
                                  seq_name,
                                  gene_name ? ":" : "",
                                  gene_name ? gene_name : "") ;
          zMapGUIShowText(title, peptide_fasta, FALSE) ;
          g_free(title) ;

          g_free(peptide_fasta) ;
        }
      else
        {
          exportFASTA(menu_data->window, ZMAPFASTA_SEQTYPE_AA,
                    zMapPeptideSequence(peptide), seq_name,
                      pep_length, molecule_type, gene_name, &error) ;
        }

      zMapPeptideDestroy(peptide) ;

      g_free(dna) ;
    }

  g_free(menu_data) ;

  if (error)
    {
      zMapShowMsg(ZMAP_MSG_WARNING, "%s", error->message) ;
      g_error_free(error) ;
    }

  return ;
}


gboolean zMapWindowExportFASTA(ZMapWindow window, ZMapFeatureAny feature_in, GError **error)
{
  gboolean result = FALSE ;

  char *sequence = NULL ;
  char *seq_name = NULL ;
  int seq_len = 0 ;
  ZMapFeatureAny feature = feature_in ;
  ZMapFeatureBlock block = NULL ;

  if (!feature && window )
    {
      /* If no feature is given, use the selected feature (if there is one and only one) */
      GList *features = zmapWindowFocusGetFeatureList(window->focus) ;

      if (features && g_list_length(features) == 1)
        feature = (ZMapFeatureAny)(features->data) ;
    }


  if (feature)
    {
      block = (ZMapFeatureBlock)zMapFeatureGetParentGroup(feature, ZMAPFEATURE_STRUCT_BLOCK);

      if (zMapFeatureBlockDNA(block, &seq_name, &seq_len, &sequence))
        result = exportFASTA(window, ZMAPFASTA_SEQTYPE_DNA, sequence, seq_name, seq_len, "DNA", NULL, error) ;
      else
        g_set_error(error, g_quark_from_string("ZMap"), 99, "Context contains no DNA") ;
    }
  else
    {
      g_set_error(error, g_quark_from_string("ZMap"), 99, "Please select a single feature") ;
    }

  return result ;
}


/*
 * This is the callback for export options when the user right-clicks in the
 * view. It now only deals with exporting features from a specific column. It
 * uses the column in which the right-click happens, not necessarily the same
 * as the highlighted column.
 *
 * Options are:
 *
 * case EXPORT_DNA:                 DNA (sm23) I've not altered this behaviour
 * case EXPORT_FEATURES_ALL:        All features from all featuresets in the column
 * case EXPORT_FEATURES_MARKED:     All features from all featuresets in the column that
 *                                  overlap the marked region. Error if mark not set.
 * case EXPORT_FEATURES_CLICKED:   The clicked feature.
 */
static void exportMenuCB(int menu_item_id, gpointer callback_data)
{
  gboolean result = FALSE ;
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapFeatureAny feature = NULL ;
  GError *error = NULL ;
  FooCanvasGroup *featureset_group = NULL ;
  ZMapWindowContainerFeatureSet container = NULL ;

  if (!menu_data)
    return ;

  feature = zmapWindowItemGetFeatureAny(menu_data->item);
   if (feature->struct_type == ZMAPFEATURE_STRUCT_FEATURESET)
    {
      featureset_group = FOO_CANVAS_GROUP(menu_data->item) ;
    }
  else
    {
      featureset_group = (FooCanvasGroup *)zmapWindowContainerCanvasItemGetContainer(menu_data->item) ;
    }

  container = (ZMapWindowContainerFeatureSet)(featureset_group);

  switch (menu_item_id)
    {
    case EXPORT_DNA: /* export dna */
      {
        result = zMapWindowExportFASTA(menu_data->window, feature, &error) ;
        break ;
      }
    case EXPORT_FEATURES_ALL: /* export all features */
      {
        result = zMapWindowExportFeatureSets(menu_data->window, container->featuresets, FALSE, NULL, &error) ;
        break ;
      }
    case EXPORT_FEATURES_MARKED: /* features in marked region */
      {
        result = zMapWindowExportFeatureSets(menu_data->window, container->featuresets, TRUE, NULL, &error) ;
        break;
      }
    case EXPORT_FEATURES_CLICKED: /* clicked feature */
      {
        if (feature)
          result = zMapWindowExportFeatures(menu_data->window, FALSE, FALSE, feature, NULL, &error) ;
        break;
      }
    default:
      break ;
    }

  g_free(menu_data) ;

  if (error)
    {
      zMapShowMsg(ZMAP_MSG_WARNING, "%s", error->message) ;
      g_error_free(error) ;
    }

  return ;
}





#if MH17_NOT_CALLED

ZMapGUIMenuItem zmapWindowMakeMenuTranscriptTools(int *start_index_inout,
                                                  ZMapGUIMenuItemCallbackFunc callback_func,
                                                  gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_BRANCH, "Transcript Tools",       0,             NULL,               NULL},
      {ZMAPGUI_MENU_NORMAL, "Transcript Tools/Split",   ZMAPNAV_SPLIT_FIRST_LAST_EXON,   transcriptNavMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, "Transcript Tools/Split 2", ZMAPNAV_SPLIT_FLE_WITH_OVERVIEW, transcriptNavMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, "Transcript Tools/First", ZMAPNAV_FIRST, transcriptNavMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, "Transcript Tools/Prev",  ZMAPNAV_PREV,  transcriptNavMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, "Transcript Tools/Next",  ZMAPNAV_NEXT,  transcriptNavMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, "Transcript Tools/Last",  ZMAPNAV_LAST,  transcriptNavMenuCB, NULL},
      {ZMAPGUI_MENU_NONE, NULL, 0, NULL, NULL}
    } ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}

static void transcriptNavMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data;
  ZMapWindow window = NULL;
  GArray  *patterns = NULL;
  ZMapSplitPatternStruct split_style = {0};

  window   = menu_data->window;
  patterns = g_array_new(FALSE, FALSE, sizeof(ZMapSplitPatternStruct));

  switch(menu_item_id)
    {
    case ZMAPNAV_SPLIT_FLE_WITH_OVERVIEW:
      {
        ZMapWindowSplittingStruct split = {0};
        split.original_window   = window;

        split_style.subject     = ZMAPSPLIT_ORIGINAL;
        split_style.orientation = GTK_ORIENTATION_HORIZONTAL;
        g_array_append_val(patterns, split_style);

        split_style.subject     = ZMAPSPLIT_ORIGINAL;
        split_style.orientation = GTK_ORIENTATION_VERTICAL;
        g_array_append_val(patterns, split_style);

        split_style.subject     = ZMAPSPLIT_NEW;
        split_style.orientation = GTK_ORIENTATION_HORIZONTAL;
        g_array_append_val(patterns, split_style);
        /*
        */
        split.split_patterns    = patterns;

        (*(window->caller_cbs->splitToPattern))(window, window->app_data, &split);
      }
      break;
    case ZMAPNAV_SPLIT_FIRST_LAST_EXON:
      {
        ZMapWindowSplittingStruct split = {0};
        split.original_window   = window;

        split_style.subject     = ZMAPSPLIT_ORIGINAL;
        split_style.orientation = GTK_ORIENTATION_VERTICAL;
        g_array_append_val(patterns, split_style);

        split_style.subject     = ZMAPSPLIT_NEW;
        split_style.orientation = GTK_ORIENTATION_HORIZONTAL;
        g_array_append_val(patterns, split_style);

        split.split_patterns    = patterns;

        (*(window->caller_cbs->splitToPattern))(window, window->app_data, &split);
      }
      break;
    case ZMAPNAV_FIRST:
      break;
    case ZMAPNAV_LAST:
      break;
    case ZMAPNAV_NEXT:
      break;
    case ZMAPNAV_PREV:
      break;
    default:
      break;
    }

  if(patterns)
    g_array_free(patterns, TRUE);

  return ;
}

#endif

static void markMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  if (!menu_data)
    return ;

  zmapWindowToggleMark(menu_data->window, 0);

  return ;
}

static void compressMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapWindowCompressMode compress_mode = (ZMapWindowCompressMode)menu_item_id  ;
  FooCanvasGroup *column_group = NULL ;

  if (!menu_data)
    return ;

  if (menu_data->item)
    column_group = FOO_CANVAS_GROUP(menu_data->item) ;

  if (compress_mode != ZMAPWINDOW_COMPRESS_VISIBLE)
    {
      if (zmapWindowMarkIsSet(menu_data->window->mark))
        compress_mode = ZMAPWINDOW_COMPRESS_MARK ;
      else
        compress_mode = ZMAPWINDOW_COMPRESS_ALL ;
    }

  zmapWindowColumnsCompress(FOO_CANVAS_ITEM(column_group), menu_data->window, compress_mode) ;

  g_free(menu_data) ;

  return ;

}


/* Copy or paste chromosome coords. */
static void copyPasteMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapWindowCopyPasteType mode = (ZMapWindowCopyPasteType)menu_item_id  ;

  if (!menu_data)
    return ;

  if (mode == ZMAPWINDOW_PASTE)
    {
      gboolean result ;

      result = zmapWindowZoomFromClipboard(menu_data->window, menu_data->x, menu_data->y) ;
    }
  else
    {
      char *clipboard_txt = NULL ;
      ZMapWindowDisplayStyleStruct display_style = {ZMAPWINDOW_COORD_NATURAL, ZMAPWINDOW_PASTE_FORMAT_BROWSER,
                                                    ZMAPWINDOW_PASTE_TYPE_EXTENT} ;

      if (menu_data->feature)
        {
          clipboard_txt = zmapWindowMakeFeatureSelectionTextFromFeature(menu_data->window,
                                                                        &display_style, menu_data->feature) ;
        }
      else
        {
          clipboard_txt = zmapWindowMakeColumnSelectionText(menu_data->window, menu_data->x, menu_data->y,
                                                            &display_style, NULL) ;
        }

      zMapGUISetClipboard(menu_data->window->toplevel, GDK_SELECTION_PRIMARY, clipboard_txt) ;
      zMapGUISetClipboard(menu_data->window->toplevel, GDK_SELECTION_CLIPBOARD, clipboard_txt) ;

      g_free(clipboard_txt) ;
    }


  g_free(menu_data) ;

  return ;
}



/* Configure a column, may mean repositioning the other columns. */
static void configureMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapWindowColConfigureMode configure_mode = (ZMapWindowColConfigureMode)menu_item_id  ;
  FooCanvasGroup *column_group = NULL ;

  if (!menu_data)
    return ;

  /* did user click on an item or on the column background ? */
  if(configure_mode != ZMAPWINDOWCOLUMN_CONFIGURE_ALL)
      column_group = menuDataItemToColumn(menu_data->item);

  zmapWindowColumnConfigure(menu_data->window, column_group, configure_mode) ;

  g_free(menu_data) ;

  return ;
}






static void colourMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;

  zmapWindowShowStyleDialog(menu_data);
  /* don't free the callback data, it's in use */

  return ;
}

static void setStyleCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;

  zmapWindowMenuSetStyleCB(menu_item_id, menu_data);

  g_free(menu_data) ;

  return ;
}




#if ED_G_NEVER_INCLUDE_THIS_CODE

static void unbumpAllCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  FooCanvasGroup *column_group ;

  column_group = menuDataItemToColumn(menu_data->item);

  zmapWindowColumnUnbumpAll(FOO_CANVAS_ITEM(column_group));

  zmapWindowFullReposition(menu_data->window->feature_root_group,TRUE, "unbump all");

  g_free(menu_data) ;

  return ;
}

#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


/* Bump a column and reposition the other columns.
 *
 * NOTE that this function may be called for an individual feature OR a column and
 * needs to deal with both.
 */
static void bumpMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapStyleBumpMode bump_type = (ZMapStyleBumpMode)menu_item_id  ;
  ZMapWindowCompressMode compress_mode ;
  FooCanvasItem *style_item = NULL ;

  if (!menu_data || !menu_data->window)
    return ;

  style_item = menu_data->item ;

  if (zmapWindowMarkIsSet(menu_data->window->mark))
    compress_mode = ZMAPWINDOW_COMPRESS_MARK ;
  else
    compress_mode = ZMAPWINDOW_COMPRESS_ALL ;

  zmapWindowColumnBumpRange(style_item, bump_type, compress_mode) ;

  zmapWindowFullReposition(menu_data->window->feature_root_group,TRUE, "bump menu") ;

  g_free(menu_data) ;

  return ;
}

/* Bump a column and reposition the other columns.
 *
 * NOTE that this function may be called for an individual feature OR a column and
 * needs to deal with both.
 */
static void bumpToggleMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  FooCanvasGroup *column_group = NULL;

  if (!menu_data)
    return ;

  column_group = menuDataItemToColumn(menu_data->item);

  if (column_group)
    {
      ZMapWindowContainerFeatureSet container;
      ZMapStyleBumpMode curr_bump_mode, bump_mode ;
      ZMapWindowCompressMode compress_mode ;

      container = (ZMapWindowContainerFeatureSet)column_group;

      curr_bump_mode = zmapWindowContainerFeatureSetGetBumpMode(container);

      if (curr_bump_mode != ZMAPBUMP_UNBUMP)
              bump_mode = ZMAPBUMP_UNBUMP ;
      else
        bump_mode = zmapWindowContainerFeatureSetGetDefaultBumpMode(container);

      if (zmapWindowMarkIsSet(menu_data->window->mark))
        compress_mode = ZMAPWINDOW_COMPRESS_MARK ;
      else
        compress_mode = ZMAPWINDOW_COMPRESS_ALL ;

      zmapWindowColumnBumpRange(FOO_CANVAS_ITEM(column_group), bump_mode, compress_mode) ;

      zmapWindowFullReposition(menu_data->window->feature_root_group,TRUE, "bump toggle menu") ;
    }

  g_free(menu_data) ;

  return ;
}



static void maskToggleMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapWindowContainerFeatureSet container = NULL ;

  if (!menu_data || !menu_data->window || !menu_data->container_set)
    return ;

  container = menu_data->container_set;

  zMapWindowContainerFeatureSetShowHideMaskedFeatures(container, container->masked, FALSE);

      /* un/bumped features might be wider */
  zmapWindowFullReposition(menu_data->window->feature_root_group,TRUE, "mask toggle menu") ;

  g_free(menu_data) ;

  return ;
}




/* un-highlight the evidence features */
static void hideEvidenceMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapWindowFocus focus = NULL ;

  if (!menu_data || !menu_data->window)
    return ;

  focus = menu_data->window->focus;

  if (focus)
    {
      menu_data->window->highlight_evidence_featureset_id = 0 ;
      zmapWindowFocusResetType(focus,WINDOW_FOCUS_GROUP_EVIDENCE);
    }

  g_free(menu_data) ;

  return ;
}


ZMapGUIMenuItem zmapWindowMakeMenuDeveloperOps(int *start_index_inout,
                                               ZMapGUIMenuItemCallbackFunc callback_func,
                                               gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_BRANCH, "_" DEVELOPER_STR,                   0, NULL,       NULL},
      {ZMAPGUI_MENU_NORMAL, DEVELOPER_STR "/Show Feature Only",  DEVELOPER_FEATURE_ONLY, developerMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, DEVELOPER_STR "/Show FeatureItem & Feature",
       DEVELOPER_FEATUREITEM_FEATURE, developerMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, DEVELOPER_STR "/Show FeaturesetItem, FeatureItem and Feature",
       DEVELOPER_FEATURESETITEM_FEATUREITEM_FEATURE, developerMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, DEVELOPER_STR "/Print Style", DEVELOPER_PRINT_STYLE, developerMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, DEVELOPER_STR "/Print Canvas", DEVELOPER_PRINT_CANVAS, developerMenuCB, NULL},
      {ZMAPGUI_MENU_NORMAL, DEVELOPER_STR "/Show Window Stats", DEVELOPER_STATS, developerMenuCB, NULL},
      {ZMAPGUI_MENU_NONE, NULL               , 0, NULL, NULL}
    } ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}

static void developerMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapFeatureAny feature_any ;

  if (!menu_data)
    return ;

  feature_any = zmapWindowItemGetFeatureAny(menu_data->item) ;

  switch (menu_item_id)
    {
    case DEVELOPER_FEATURE_ONLY:
      {
        if (feature_any->struct_type != ZMAPFEATURE_STRUCT_FEATURE)
          {
            zMapWarning("%s", "Not on a feature.") ;
          }
        else
          {
            ZMapFeature feature ;
            char *feature_text ;
            char *msg_text ;

            feature = (ZMapFeature)feature_any ;

            feature_text = zMapFeatureAsString(feature) ;

            msg_text = g_strdup_printf("\n%s\n", feature_text) ;

            zMapGUIShowText((char *)g_quark_to_string(feature->original_id), feature_text, FALSE) ;

            g_free(msg_text) ;
            g_free(feature_text) ;
          }

        break ;
      }
    case DEVELOPER_FEATUREITEM_FEATURE:
      {
        if (feature_any->struct_type != ZMAPFEATURE_STRUCT_FEATURE)
          {
            zMapWarning("%s", "Not on a feature.") ;
          }
        else
          {
            ZMapWindowFeaturesetItem featureset_item = ZMAP_WINDOW_FEATURESET_ITEM(menu_data->item) ;
            ZMapWindowCanvasFeature feature_item ;
            char *feature_text ;
            ZMapFeature feature ;
            GString *canvas_feature_text ;
            char *msg_text ;

            feature_item = zMapWindowCanvasFeaturesetGetPointFeatureItem(featureset_item) ;

            canvas_feature_text = zMapWindowCanvasFeature2Txt(feature_item) ;

            feature = (ZMapFeature)feature_any ;

            feature_text = zMapFeatureAsString(feature) ;

            msg_text = g_strdup_printf("\n%s\n%s\n", canvas_feature_text->str, feature_text) ;

            zMapGUIShowText((char *)g_quark_to_string(feature->original_id), msg_text, FALSE) ;

            g_free(msg_text) ;
            g_string_free(canvas_feature_text, TRUE) ;
            g_free(feature_text) ;
          }

        break ;
      }
    case DEVELOPER_FEATURESETITEM_FEATUREITEM_FEATURE:
      {
        if (feature_any->struct_type != ZMAPFEATURE_STRUCT_FEATURE)
          {
            zMapWarning("%s", "Not on a feature.") ;
          }
        else
          {

            /* TRY ASSIGNING TO A WindowCanvasItem AND SEE IF THAT WORKS..... */
            ZMapWindowFeaturesetItem featureset_item = ZMAP_WINDOW_FEATURESET_ITEM(menu_data->item) ;


            ZMapWindowCanvasFeature feature_item ;
            GString *canvas_featureset_text ;
            GString *canvas_feature_text ;
            ZMapFeature feature ;
            char *feature_text ;
            char *msg_text ;

            feature_item = zMapWindowCanvasFeaturesetGetPointFeatureItem(featureset_item) ;

            canvas_featureset_text = zMapWindowCanvasFeatureset2Txt(featureset_item) ;

            canvas_feature_text = zMapWindowCanvasFeature2Txt(feature_item) ;

            feature = (ZMapFeature)feature_any ;

            feature_text = zMapFeatureAsString(feature) ;

            msg_text = g_strdup_printf("\n%s\n%s\n%s\n", canvas_featureset_text->str, canvas_feature_text->str, feature_text) ;

            zMapGUIShowText((char *)g_quark_to_string(feature->original_id), msg_text, FALSE) ;

            g_free(msg_text) ;
            g_free(feature_text) ;
            g_string_free(canvas_feature_text, TRUE) ;
            g_string_free(canvas_featureset_text, TRUE) ;
          }

        break ;
      }
    case DEVELOPER_PRINT_STYLE:
      {
        ZMapWindowContainerFeatureSet container = NULL;

        /* Christ...what's all this internal data structs stuff..... */

        if (feature_any->struct_type == ZMAPFEATURE_STRUCT_FEATURESET)
          {
            ZMapFeatureTypeStyle style ;

            container = (ZMapWindowContainerFeatureSet)(menu_data->item) ;

            /*zmapWindowStyleTableForEach(container->style_table, show_all_styles_cb, NULL);*/
            style = container->style ;
            zmapWindowShowStyle(style) ;
          }
        else if (feature_any->struct_type == ZMAPFEATURE_STRUCT_FEATURE)
          {
            container = (ZMapWindowContainerFeatureSet)(menu_data->item->parent) ;
            if (container)
              {
                ZMapFeatureTypeStyle style ;
                ZMapFeature feature ;

                feature = (ZMapFeature)feature_any ;
                style = *feature->style ;

                zmapWindowShowStyle(style) ;
              }
          }

        break ;
      }

    case DEVELOPER_PRINT_CANVAS:
      {
        zmapWindowPrintCanvas(menu_data->window->canvas) ;

        break ;
      }

    case DEVELOPER_STATS:
      {
        GString *session_text ;
        char *title ;

        session_text = g_string_new(NULL) ;

        g_string_append(session_text, "General\n") ;
        g_string_append_printf(session_text, "\tProgram: %s\n\n", zMapGetAppTitle()) ;
        g_string_append_printf(session_text, "\tUser: %s (%s)\n\n", g_get_user_name(), g_get_real_name()) ;
        g_string_append_printf(session_text, "\tMachine: %s\n\n", g_get_host_name()) ;
        g_string_append_printf(session_text, "\tSequence: %s\n\n", menu_data->window->sequence->sequence) ;

        g_string_append(session_text, "Window Statistics\n") ;
        zMapWindowStats(menu_data->window, session_text) ;

        title = zMapGUIMakeTitleString(NULL, "Session Statistics") ;
        zMapGUIShowText(title, session_text->str, FALSE) ;
        g_free(title) ;
        g_string_free(session_text, TRUE) ;

        break ;
      }

    default:
      {
        break ;
      }
    }

  g_free(menu_data) ;

  return ;
}




/* Set up column filtering options.
 *
 * Note that we have radio buttons for column-specific action as it makes sense that
 * each column shows just one type of filtering whereas for "all" actions there is no
 * sensible way we can record a single option that applies to all so they are normal
 * manu buttons. (Our radio button setting code below only sets radio buttons.)
 *
 *  */
ZMapGUIMenuItem zmapWindowMakeMenuColFilterOps(int *start_index_inout,
                                               ZMapGUIMenuItemCallbackFunc callback_func,
                                               gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_BRANCH, "_" COL_FILTERING_STR, FILTER_NONE, NULL, NULL},

      {ZMAPGUI_MENU_BRANCH, "_" COL_FILT_THIS_STR, FILTER_NONE, NULL, NULL},

      {ZMAPGUI_MENU_TITLE, "_" COL_FILT_THIS_STR COL_FILT_SHOW_STR, FILTER_NONE, NULL, NULL},

      {ZMAPGUI_MENU_RADIO, COL_FILT_THIS_STR "/Common Splices",
       (SELECTED_PARTS | FILTER_PARTS | ACTION_HIGHLIGHT_SPLICE), colFilterMenuCB, NULL},
      {ZMAPGUI_MENU_RADIO, COL_FILT_THIS_STR "/Non-matching Introns",
       (SELECTED_GAPS | FILTER_GAPS | ACTION_HIDE | TARGET_NOT_SOURCE_FEATURES), colFilterMenuCB, NULL},
      {ZMAPGUI_MENU_RADIO, COL_FILT_THIS_STR "/Non-matching Exons",
       (SELECTED_PARTS | FILTER_PARTS | ACTION_HIDE | TARGET_NOT_SOURCE_FEATURES), colFilterMenuCB, NULL},
      {ZMAPGUI_MENU_RADIO, COL_FILT_THIS_STR "/Non-matching Confirmed Introns",
       (SELECTED_GAPS | FILTER_PARTS | ACTION_HIDE | TARGET_NOT_SOURCE_FEATURES), colFilterMenuCB, NULL},
      {ZMAPGUI_MENU_RADIO, COL_FILT_THIS_STR "/Matching Exons",
       (SELECTED_PARTS | FILTER_PARTS | ACTION_SHOW | TARGET_NOT_SOURCE_FEATURES), colFilterMenuCB, NULL},
      {ZMAPGUI_MENU_RADIO, COL_FILT_THIS_STR "/Matching CDS",
       (SELECTED_PARTS | FILTER_CDS | ACTION_SHOW | TARGET_NOT_SOURCE_FEATURES), colFilterMenuCB, NULL},
      {ZMAPGUI_MENU_RADIO, COL_FILT_THIS_STR "/Unfilter",
       (SELECTED_NONE | FILTER_NONE | ACTION_SHOW), colFilterMenuCB, NULL},

      /* selected needs adding here..... */
      {ZMAPGUI_MENU_BRANCH, "_" COL_FILT_ALL_STR, FILTER_NONE, NULL, NULL},
      {ZMAPGUI_MENU_TITLE, "_" COL_FILT_ALL_STR COL_FILT_SHOW_STR, FILTER_NONE, NULL, NULL},

      {ZMAPGUI_MENU_RADIO, COL_FILT_ALL_STR "/Common Splices",
       (SELECTED_PARTS | FILTER_PARTS | ACTION_HIGHLIGHT_SPLICE | FILTER_ALL), colFilterMenuCB, NULL},
      {ZMAPGUI_MENU_RADIO, COL_FILT_ALL_STR "/Non-matching Introns",
       (SELECTED_GAPS | FILTER_GAPS | ACTION_HIDE | TARGET_NOT_SOURCE_COLUMN | FILTER_ALL), colFilterMenuCB, NULL},
      {ZMAPGUI_MENU_RADIO, COL_FILT_ALL_STR "/Non-matching Exons",
       (SELECTED_PARTS | FILTER_PARTS | ACTION_HIDE | TARGET_NOT_SOURCE_COLUMN | FILTER_ALL), colFilterMenuCB, NULL},
      {ZMAPGUI_MENU_RADIO, COL_FILT_ALL_STR "/Non-matching Confirmed Introns",
       (SELECTED_GAPS | FILTER_PARTS | ACTION_HIDE | TARGET_NOT_SOURCE_COLUMN | FILTER_ALL), colFilterMenuCB, NULL},
      {ZMAPGUI_MENU_RADIO, COL_FILT_ALL_STR "/Matching Exons",
       (SELECTED_PARTS | FILTER_PARTS | ACTION_SHOW | TARGET_NOT_SOURCE_COLUMN | FILTER_ALL), colFilterMenuCB, NULL},
      {ZMAPGUI_MENU_RADIO, COL_FILT_ALL_STR "/Matching CDS",
       (SELECTED_PARTS | FILTER_CDS | ACTION_SHOW | TARGET_NOT_SOURCE_COLUMN | FILTER_ALL), colFilterMenuCB, NULL},
      {ZMAPGUI_MENU_RADIO, COL_FILT_ALL_STR "/Unfilter",
       (SELECTED_NONE | FILTER_NONE | ACTION_SHOW | FILTER_ALL), colFilterMenuCB, NULL},

      {ZMAPGUI_MENU_NONE, NULL, 0, NULL, NULL}
    } ;
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapWindowContainerGroup column_container ;
  ZMapWindowContainerFilterType curr_selected_filter_type, curr_filter_type ;
  ZMapWindowContainerActionType curr_action_type ;
  ColFilteringType curr_selected_type, curr_menu_type, curr_menu_action ;
  gboolean cds_match ;
  ZMapGUIMenuItem menu_item ;

  column_container = zmapWindowContainerCanvasItemGetContainer(menu_data->item) ;

  curr_selected_filter_type
    = zMapWindowContainerFeatureSetGetSelectedType((ZMapWindowContainerFeatureSet)column_container) ;
  curr_filter_type = zMapWindowContainerFeatureSetGetFilterType((ZMapWindowContainerFeatureSet)column_container) ;
  curr_action_type = zMapWindowContainerFeatureSetGetActionType((ZMapWindowContainerFeatureSet)column_container) ;
  cds_match = zMapWindowContainerFeatureSetIsCDSMatch((ZMapWindowContainerFeatureSet)column_container) ;

  switch (curr_selected_filter_type)
    {
    case ZMAP_CANVAS_FILTER_NONE:
      curr_selected_type = SELECTED_NONE ;
      break ;
    case ZMAP_CANVAS_FILTER_PARTS:
      curr_selected_type = SELECTED_PARTS ;
      break ;
    case ZMAP_CANVAS_FILTER_GAPS:
      curr_selected_type = SELECTED_GAPS ;
      break ;

    default:
      zMapWarnIfReached() ;
      break ;
    }

  switch (curr_filter_type)
    {
    case ZMAP_CANVAS_FILTER_NONE:
      curr_menu_type = FILTER_NONE ;
      break ;
    case ZMAP_CANVAS_FILTER_PARTS:
      if (cds_match)
        curr_menu_type = FILTER_CDS ;
      else
        curr_menu_type = FILTER_PARTS ;
      break ;
    case ZMAP_CANVAS_FILTER_GAPS:
      curr_menu_type = FILTER_GAPS ;
      break ;

    default:
      zMapWarnIfReached() ;

      break ;
    }

  switch (curr_action_type)
    {
    case ZMAP_CANVAS_ACTION_HIGHLIGHT_SPLICE:
      curr_menu_action = ACTION_HIGHLIGHT_SPLICE ;
      break ;
    case ZMAP_CANVAS_ACTION_HIDE:
      curr_menu_action = ACTION_HIDE ;
      break ;
    case ZMAP_CANVAS_ACTION_SHOW:
      curr_menu_action = ACTION_SHOW ;
      break ;
    case ZMAP_CANVAS_ACTION_CREATE_NEW:
      curr_menu_action = ACTION_CREATE_NEW ;
      break ;
    default:
      curr_menu_action = FILTER_NONE ;
      break ;
    }

  /* Unset any previously active filter sub-menu radio button....note that the item
   * will be set in both the single column and "all" column submenus. */
  menu_item = &(menu[0]) ;
  while (menu_item->type != ZMAPGUI_MENU_NONE)
    {
      if (menu_item->type == ZMAPGUI_MENU_RADIOACTIVE)
        {
          menu_item->type = ZMAPGUI_MENU_RADIO ;
        }

      menu_item++ ;
    }

  /* Set any new active filter sub-menu radio button...n.b. !curr_menu_action => unfilter,
   * note that the item will be set in both the single column and "all" column submenus. */
  menu_item = &(menu[0]) ;
  while (menu_item->type != ZMAPGUI_MENU_NONE)
    {
      if (menu_item->type == ZMAPGUI_MENU_RADIO)
        {
          if (menu_item->id & curr_selected_type && menu_item->id & curr_menu_type)
            {
              if (menu_item->id & curr_menu_action || !curr_menu_action)
                {
                  menu_item->type = ZMAPGUI_MENU_RADIOACTIVE ;
                }
            }
        }

      menu_item++ ;
    }

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


/* Callback to select different ways of filtering the features in a column using
 * the focus feature splice positions for the filtering.
 *
 * Note that if user selects a new filter then any previous filter is removed
 * as a part of applying the new one, i.e. they are not additive.
 */
static void colFilterMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapWindow window = menu_data->window ;
  ZMapWindowCallbackCommandFilterStruct filter_data = {ZMAPWINDOW_CMD_COLFILTER, FALSE} ;
  ZMapWindowCallbacks window_cbs_G ;
  ZMapWindowContainerGroup column_group ;
  gboolean all_cols = FALSE ;
  FooCanvasGroup *focus_column ;
  FooCanvasItem *focus_item ;
  GList *filterfeatures = NULL ;


  window_cbs_G = zmapWindowGetCBs() ;

  /* Column where menu was clicked. */
  column_group = zmapWindowContainerCanvasItemGetContainer(menu_data->item) ;


  if (menu_item_id & FILTER_ALL)
    all_cols = TRUE ;


  /* If we are doing filtering then check for highlighted feature and also for mark... */
  if (!(menu_item_id & SELECTED_NONE && menu_item_id & FILTER_NONE))
    {
      if (!((focus_column = zmapWindowFocusGetHotColumn(window->focus))
            && (focus_item = zmapWindowFocusGetHotItem(window->focus))
            && (filterfeatures = zmapWindowFocusGetFeatureList(window->focus))))
        {
          /* We can't do anything if there is no highlight feature. */

          zMapMessage("%s", "No features selected.") ;
        }
      else
        {
          /* If the mark is set then exclude any features not overlapping it.... */
          if (zmapWindowMarkIsSet(window->mark) && zmapWindowMarkGetSequenceRange(window->mark,
                                                                                  &(filter_data.seq_start),
                                                                                  &(filter_data.seq_end)))
            {
              filterfeatures = zMapFeatureGetOverlapFeatures(filterfeatures,
                                                             filter_data.seq_start, filter_data.seq_end,
                                                             ZMAPFEATURE_OVERLAP_ALL) ;
            }
        }
    }


  /* Note, if filtering then any mark may result in no filter features. */
  if ((menu_item_id & SELECTED_NONE && menu_item_id & FILTER_NONE) || filterfeatures)
    {

      if (menu_item_id & SELECTED_GAPS)
        filter_data.selected = ZMAP_CANVAS_FILTER_GAPS ;
      else if (menu_item_id & SELECTED_PARTS)
        filter_data.selected = ZMAP_CANVAS_FILTER_PARTS ;
      else
        filter_data.selected = ZMAP_CANVAS_FILTER_NONE ;

      if (menu_item_id & FILTER_GAPS)
        filter_data.filter = ZMAP_CANVAS_FILTER_GAPS ;
      else if (menu_item_id & FILTER_PARTS)
        filter_data.filter = ZMAP_CANVAS_FILTER_PARTS ;
      else if (menu_item_id & FILTER_CDS)
        filter_data.filter = ZMAP_CANVAS_FILTER_PARTS ;
      else
        filter_data.filter = ZMAP_CANVAS_FILTER_NONE ;


      if (menu_item_id & FILTER_CDS)
        filter_data.cds_match = TRUE ;

      if (menu_item_id & ACTION_HIGHLIGHT_SPLICE)
        filter_data.action = ZMAP_CANVAS_ACTION_HIGHLIGHT_SPLICE ;
      else if (menu_item_id & ACTION_HIDE)
        filter_data.action = ZMAP_CANVAS_ACTION_HIDE ;
      else if (menu_item_id & ACTION_SHOW)
        filter_data.action = ZMAP_CANVAS_ACTION_SHOW ;
      else
        filter_data.action = ZMAP_CANVAS_ACTION_CREATE_NEW ;

      if (menu_item_id & TARGET_NOT_SOURCE_FEATURES)
        filter_data.target_type = ZMAP_CANVAS_TARGET_NOT_SOURCE_FEATURES ;
      else if (menu_item_id & TARGET_NOT_SOURCE_COLUMN)
        filter_data.target_type = ZMAP_CANVAS_TARGET_NOT_SOURCE_COLUMN ;
      else
        filter_data.target_type = ZMAP_CANVAS_TARGET_ALL ;


      if (menu_item_id & SELECTED_NONE && menu_item_id & FILTER_NONE)
        {
          filter_data.filter_column = (ZMapWindowContainerFeatureSet)column_group ;
        }
      else
        {
          filter_data.do_filter = TRUE ;
          filter_data.filter_column = (ZMapWindowContainerFeatureSet)focus_column ;
          filter_data.filter_features = filterfeatures ;
        }

      if (!all_cols)
        filter_data.target_column = (ZMapWindowContainerFeatureSet)column_group ;
      else
        filter_data.target_column = NULL ;


      /* send command up to view so it can be run on all windows for this view.... */
      (*(window_cbs_G->command))(window, window->app_data, &filter_data) ;


      /* Hacky showing of errors, may not work when there are multiple windows.....
       * needs wider fix to window command interface....errors should be reported in view
       * for each window as it's processed... */
      if (!filter_data.result)
        {
          const char *error, *columns, *err_msg ;

          if (filter_data.filter_result == ZMAP_CANVAS_FILTER_NOT_SENSITIVE)
            error = "is non-filterable (check styles configuration file)." ;
          else
            error = "have no matches" ;

          if (!filter_data.target_column)
            columns = "All columns" ;
          else
            columns = zmapWindowContainerFeaturesetGetColumnName(filter_data.target_column) ;

          err_msg = g_strdup_printf("%s: %s", columns, error) ;

          zMapMessage("%s", err_msg) ;

          g_free((void *)err_msg) ;

        }

      if (filterfeatures)
        g_list_free(filterfeatures) ;
    }


  g_free(menu_data) ;

  return ;
}


/* Clicked on a non-alignment feature... */
ZMapGUIMenuItem zmapWindowMakeMenuNonHomolFeature(int *start_index_inout,
                                                  ZMapGUIMenuItemCallbackFunc callback_func,
                                                  gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_NORMAL, BLIXEM_MENU_STR " " BLIXEM_DNA_STR " - show this column",
       BLIX_NONE, blixemMenuCB, NULL, "<shift>A"},
      {ZMAPGUI_MENU_NONE,   NULL,                                        0, NULL,         NULL}
    } ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}



/* Common blixem ops that must be in top level of menu. */
ZMapGUIMenuItem zmapWindowMakeMenuBlixCommon(int *start_index_inout,
                                             ZMapGUIMenuItemCallbackFunc callback_func,
                                             gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_NORMAL, BLIXEM_MENU_STR " - all matches for this column",
       BLIX_SET, blixemMenuCB, NULL, "A"},
      {ZMAPGUI_MENU_NORMAL, BLIXEM_MENU_STR " - all matches for selected features",
       BLIX_SELECTED, blixemMenuCB, NULL, "<shift>A"},
      {ZMAPGUI_MENU_NONE,   NULL,                                        0, NULL,         NULL}
    } ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


/* Blixem top level menu branch entry.... */
ZMapGUIMenuItem zmapWindowMakeMenuBlixTop(int *start_index_inout,
                                          ZMapGUIMenuItemCallbackFunc callback_func,
                                          gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_BRANCH, BLIXEM_OPS_STR, 0, NULL,       NULL},
      {ZMAPGUI_MENU_NONE,   NULL,               0, NULL,       NULL}
    } ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


/* Common blixem ops that must be at top of column sub-menu for blixem. */
ZMapGUIMenuItem zmapWindowMakeMenuBlixColCommon(int *start_index_inout,
                                                ZMapGUIMenuItemCallbackFunc callback_func,
                                                gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_NORMAL, BLIXEM_OPS_STR "/" BLIXEM_MENU_STR " - all matches for this column",
       BLIX_SET, blixemMenuCB, NULL, "A"},
      {ZMAPGUI_MENU_NORMAL, BLIXEM_OPS_STR "/" BLIXEM_MENU_STR " - all matches for selected features",
       BLIX_SELECTED, blixemMenuCB, NULL, "<shift>A"},
      {ZMAPGUI_MENU_NONE,   NULL,                                        0, NULL,         NULL}
    } ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


/* Clicked on a dna homol feature... */
ZMapGUIMenuItem zmapWindowMakeMenuDNAHomolFeature(int *start_index_inout,
                                                  ZMapGUIMenuItemCallbackFunc callback_func,
                                                  gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_NORMAL, BLIXEM_OPS_STR "/" BLIXEM_DNA_STR " - all matches for selected features, expanded",
       BLIX_EXPANDED, blixemMenuCB, NULL, "<shift>X"},
      {ZMAPGUI_MENU_NONE,   NULL,                                        0, NULL,         NULL}
    } ;


  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}

/* Clicked on a dna homol column... */
ZMapGUIMenuItem zmapWindowMakeMenuDNAHomol(int *start_index_inout,
                                           ZMapGUIMenuItemCallbackFunc callback_func,
                                           gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_NORMAL, BLIXEM_OPS_STR "/" BLIXEM_DNAS_STR " - all matches for associated columns",
       BLIX_MULTI_SETS, blixemMenuCB, NULL, "<Ctrl>A"},
      {ZMAPGUI_MENU_NONE,   NULL,                                             0, NULL,         NULL}
    } ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* no protein specific operations currently.... */
ZMapGUIMenuItem zmapWindowMakeMenuProteinHomolFeature(int *start_index_inout,
                                                      ZMapGUIMenuItemCallbackFunc callback_func,
                                                      gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_NONE,   NULL,                             0, NULL,         NULL}
    } ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


ZMapGUIMenuItem zmapWindowMakeMenuProteinHomol(int *start_index_inout,
                                               ZMapGUIMenuItemCallbackFunc callback_func,
                                               gpointer callback_data)
{
  static ZMapGUIMenuItemStruct menu[] =
    {
      {ZMAPGUI_MENU_NORMAL, BLIXEM_OPS_STR "/" BLIXEM_AAS_STR " - all matches for associated columns",
       BLIX_MULTI_SETS, blixemMenuCB, NULL, "<Ctrl>A"},

      {ZMAPGUI_MENU_NONE,   NULL,                                            0, NULL,         NULL}
    } ;

  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


/* This function creates a Blixem BAM menu. */
ZMapGUIMenuItem zmapWindowMakeMenuBlixemBAM(int *start_index_inout,
                                            ZMapGUIMenuItemCallbackFunc callback_func,
                                            gpointer callback_data)
{
  /* get a list of featuresets from the window's context_map */
  static ZMapGUIMenuItem menu = NULL;
  static int n_menu = 0;
  ItemMenuCBData cbdata  = (ItemMenuCBData)callback_data;
  ZMapGUIMenuItem m = NULL ;
  GList *fs_list = NULL , *fsl = NULL ;
  int n_sets = 0 ;
  int i = 0;
  char * related = NULL;        /* req data from coverage */
  char * blixem_col = NULL;        /* blixem related from coverage or directly from real data */
  GQuark fset_id = 0, req_id = 0;

  if (!cbdata || !cbdata->window || !cbdata->window->context_map )
    return menu ;

  fs_list = cbdata->window->context_map->seq_data_featuresets;
  n_sets = g_list_length(fs_list);

  if (!n_sets)
    return menu ;

  if(n_menu < n_sets)
    {
      /* as this derives from config data read on creating the view
       * it will not change unless we reconfig and open another view
       * alloc enough for all views
       */
      if(menu)
        {
          for(m = menu; m->type != ZMAPGUI_MENU_NONE ;m++)
            g_free((void *)(m->name));

          g_free(menu);
        }

      m = menu = g_new0(ZMapGUIMenuItemStruct, n_sets * 2 + 2 + 2 + 1 + 6);
      /* main menu, sub menu, two extra items plus terminator, plus 6 just to be sure */
    }

  /* add request from coverage items and blixem from coverage and real data */
  if(cbdata->feature)
    {
      fset_id = ((ZMapFeatureSet) (cbdata->feature->parent))->unique_id;
      req_id = related_column(cbdata->window->context_map,fset_id);

      if(req_id)
        related = get_menu_string(req_id,'-');
    }

  if(related)
    {
      blixem_col = related;
      cbdata->req_id = req_id;
      /* if we click on a coverage column we blixem the data column which contains featuresets */
    }
  else if(zMapFeatureIsSeqFeatureSet(cbdata->window->context_map,fset_id))
    {
      /* if we click on a data column we blixem that not the featureset as we may have several featuresets in the column */
      /* can get the column from the menu_data->container_set or from the featureset_2_column... */
      /* can't remember how reliable is the container */
      ZMapFeatureSetDesc f2c ;

      if ((f2c = (ZMapFeatureSetDesc)g_hash_table_lookup(cbdata->window->context_map->featureset_2_column,GUINT_TO_POINTER(fset_id))))
          {
          cbdata->req_id = f2c->column_id;
          blixem_col = get_menu_string(f2c->column_ID,'-');
          }
      else
          {
          zMapLogWarning("cannot find column for featureset %s",g_quark_to_string(fset_id));
          /* rather than asserting */
          }
    }

  if (blixem_col && m)
    {
      m->type = ZMAPGUI_MENU_NORMAL;
      m->name = g_strdup_printf("Blixem %s paired reads", blixem_col);
      m->id = BLIX_SEQ_COVERAGE;
      m->callback_func = blixemMenuCB;
      m++;
    }

  /* add sub menu */
  if (m)
    {
      m->type = ZMAPGUI_MENU_BRANCH;
      m->name = g_strdup(BLIXEM_OPS_STR "/" BLIXEM_READS_STR);
      m->id = 0;
      m->callback_func = NULL;
      m++;

      for(i = 0, fsl = fs_list;i < n_sets; i++, fsl = fsl->next)
        {
          const gchar *fset;
          GQuark req_id;
          ZMapFeatureSetDesc f2c;

          m->type = ZMAPGUI_MENU_NORMAL;

          f2c = (ZMapFeatureSetDesc)g_hash_table_lookup(cbdata->window->context_map->featureset_2_column,fsl->data);
          if(f2c)
            {
              fset = get_menu_string(f2c->feature_src_ID,'/');

              req_id = related_column(cbdata->window->context_map,GPOINTER_TO_UINT(fsl->data));

              if(!req_id)                /* don't include coverage data */
                {
                  m->name = g_strdup_printf(BLIXEM_OPS_STR "/" BLIXEM_READS_STR"/%s", fset);
                  m->id = BLIX_SEQ + i;
                  m->callback_func = blixemMenuCB;
                  m++;
                }
            }
        }

      m->type = ZMAPGUI_MENU_NONE;
      m->name = NULL;
    }

  /* this overrides data in the menus as given in the args, but index and func are always NULL */
  if (menu)
    zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu;
}



ZMapGUIMenuItem zmapWindowMakeMenuRequestBAM(int *start_index_inout,
                                             ZMapGUIMenuItemCallbackFunc callback_func,
                                             gpointer callback_data)
{
  /* get a list of featuresets from the window's context_map */
  static ZMapGUIMenuItem menu = NULL;
  static int n_menu = 0;
  ItemMenuCBData cbdata  = (ItemMenuCBData)callback_data;
  ZMapGUIMenuItem m;
  GList *fs_list = NULL, *fsl = NULL ;
  int n_sets = 0;
  int i = 0;
  char * related = NULL;        /* req data from coverage */
  char * blixem_col = NULL;        /* blixem related from coverage or directly from real data */
  GQuark fset_id = 0,
    req_id = 0;

  if (!cbdata || !cbdata->window || !cbdata->window->context_map )
    return menu ;

  fs_list = cbdata->window->context_map->seq_data_featuresets;
  n_sets = g_list_length(fs_list) ;

  if (!n_sets)
    return menu ;

  if(n_menu < n_sets)
    {
      /* as this derives from config data read on creating the view
       * it will not change unless we reconfig and open another view
       * alloc enough for all views
       */
      if(menu)
        {
          for(m = menu; m->type != ZMAPGUI_MENU_NONE ;m++)
            g_free((void *)(m->name));

          g_free(menu);
        }
      m = menu = g_new0(ZMapGUIMenuItemStruct, n_sets * 2 + 2 + 2 + 1 + 6);
      /* main menu, sub menu, two extra items plus terminator, plus 6 just to be sure */
    }

  /* add request from coverage items and blixem from coverage and real data */
  if(cbdata->feature)
    {
      fset_id = ((ZMapFeatureSet) (cbdata->feature->parent))->unique_id;
      req_id = related_column(cbdata->window->context_map,fset_id);

      if(req_id)
        related = get_menu_string(req_id,'-');
    }

  if(related)
    {
      blixem_col = related;
      cbdata->req_id = req_id;
      /* if we click on a coverage column we blixem the data column which contains featuresets */
    }
  else if(zMapFeatureIsSeqFeatureSet(cbdata->window->context_map,fset_id))
    {
      /* if we click on a data column we blixem that not the featureset as we may have several featuresets in the column */
      /* can get the column from the menu_data->container_set or from the featureset_2_column... */
      /* can't remember how reliable is the container */
      ZMapFeatureSetDesc f2c ;

      if ((f2c = (ZMapFeatureSetDesc)g_hash_table_lookup(cbdata->window->context_map->featureset_2_column,GUINT_TO_POINTER(fset_id))))
          {
          cbdata->req_id = f2c->column_id;
          blixem_col = get_menu_string(f2c->column_ID,'-');
          }
      else
          {
          zMapLogWarning("cannot find column for featureset %s",g_quark_to_string(fset_id));
          /* rather than asserting */
          }
    }

  if (related)
    {
      m->type = ZMAPGUI_MENU_NORMAL;
      m->name = g_strdup_printf(COLUMN_CONFIG_STR "/" PAIRED_READS_RELATED, related);
      m->id = REQUEST_SELECTED;
      m->callback_func = requestShortReadsCB;
      m++;
    }

  /* add menu item for all data in column id any of them have related featuresets */
  /* can't trigger off selected feature as we don't always have one */
  {
    GQuark fset = 0 ;
    GList *l = NULL ;

    l = zmapWindowContainerFeatureSetGetFeatureSets(cbdata->container_set);
    for(;l; l = l->next)
      {
        fset = GPOINTER_TO_UINT(l->data);
        if(related_column(cbdata->window->context_map,fset))
          {
            m->type = ZMAPGUI_MENU_NORMAL;
            m->name = g_strdup(COLUMN_CONFIG_STR "/" PAIRED_READS_ALL);
            m->id = REQUEST_ALL_SEQ;
            m->callback_func = requestShortReadsCB;
            m++;
            /* found one, so we can request all, don't do it twice */
            break;
          }
      }
  }


  /* add sub menu */
  m->type = ZMAPGUI_MENU_BRANCH;
  m->name = g_strdup(COLUMN_CONFIG_STR "/" PAIRED_READS_DATA);
  m->id = 0;
  m->callback_func = NULL;
  m++;

  for(i = 0, fsl = fs_list;i < n_sets; i++, fsl = fsl->next)
    {
      const gchar *fset;
      GQuark req_id;
      ZMapFeatureSetDesc f2c;

      m->type = ZMAPGUI_MENU_NORMAL;

      f2c = (ZMapFeatureSetDesc)g_hash_table_lookup(cbdata->window->context_map->featureset_2_column,fsl->data);
      if(f2c)
        {
          fset = get_menu_string(f2c->feature_src_ID,'/');

          req_id = related_column(cbdata->window->context_map,GPOINTER_TO_UINT(fsl->data));

          if(!req_id)                /* don't include coverage data */
            {
              m->name = g_strdup_printf(COLUMN_CONFIG_STR "/" PAIRED_READS_DATA"/%s", fset);
              m->id = REQUEST_SEQ + i;
              m->callback_func = requestShortReadsCB;
              m++;
            }
        }
    }

  m->type = ZMAPGUI_MENU_NONE;
  m->name = NULL;

  /* this overrides data in the menus as given in the args, but index and func are always NULL */
  zMapGUIPopulateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu;
}



/* return string with / instead of _ */
static char *get_menu_string(GQuark set_quark, char disguise)
{
  char *p = NULL ;
  char *seq_set = NULL ;

  seq_set = g_strdup ((char *) g_quark_to_string(GPOINTER_TO_UINT(set_quark)));

  for(p = seq_set ; *p ;p++)
    {
      /* _ adds an accelerator and gives us a GTK error */
      /* / is what we'd prefer as that what we use on the menus but that would create a menu structure */
      /* the problem is using the menu text for presentation and also for structure */
      /*  Gthanks GLib */

      if(*p == '_')        /* / is there for menus, we want featuresets without escaping names*/
        *p = disguise;
    }

  return(seq_set);
}


/* does a featureset have a related one?
 * FTM this means fset is coverage and the related is the real data (a one way relation)
 * is so don't include in menus
 */
GQuark related_column(ZMapFeatureContextMap map,GQuark fset_id)
{
  GQuark q = 0;
  ZMapFeatureSource src = NULL ;

  src = (ZMapFeatureSource)g_hash_table_lookup(map->source_2_sourcedata,GUINT_TO_POINTER(fset_id));
  if(src)
    q = src->related_column;
  else
    zMapLogWarning("Can't find source data for featureset '%s'.",g_quark_to_string(fset_id));

  return q;
}





GList * add_column_featuresets(ZMapFeatureContextMap map, GList *glist, GQuark column_id, gboolean unique_id)
{
  ZMapFeatureColumn column = NULL ;
  GList *l = NULL;
  if (!map || !column_id)
    return glist ;

  column = (ZMapFeatureColumn)g_hash_table_lookup(map->columns,(GUINT_TO_POINTER(column_id)));

  if(column)
    l = zMapFeatureGetColumnFeatureSets(map, column_id, unique_id);

  if(column && l)
    {
      for(;l ; l = l->next)
        {
          glist = g_list_prepend(glist,l->data);
        }
    }
  else                /* not configured: assume one featureset of the same name */
    {
      glist = g_list_prepend(glist,GUINT_TO_POINTER(column_id));
    }
  return glist;
}

/* make requests either for a single type of paired read of for all in the current column. */
/* NOTE that 'a single type of paired read' is several featuresets combined into a column
 * and we have to request all the featuresets
 */
static void requestShortReadsCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  GList *l = NULL ;
  int i = 0 ;
  GList *req_list = NULL;

  if (!menu_data)
    return ;

#if 0
  if (!zmapWindowMarkIsSet(menu_data->window->mark))
    {
      zMapMessage("You must set the mark first to select this option","");
    }
  else
#endif
  if(menu_item_id == REQUEST_SELECTED)
    {
      /* this is for a column related to a coverage featureset so we get several featuresets */
      req_list = add_column_featuresets(menu_data->context_map,req_list,menu_data->req_id,TRUE);
    }
  else if (menu_item_id == REQUEST_ALL_SEQ)
    {
      GQuark fset_id,req_id;
      ZMapFeatureSource src;

      l = zmapWindowContainerFeatureSetGetFeatureSets(menu_data->container_set);
      for(;l; l = l->next)
        {

          fset_id = GPOINTER_TO_UINT(l->data);
          if ((src = (ZMapFeatureSource)g_hash_table_lookup(menu_data->context_map->source_2_sourcedata,GUINT_TO_POINTER(fset_id))))
            {
              req_id = src->related_column;
            }

          if(req_id)
            req_list = add_column_featuresets(menu_data->context_map,req_list,req_id,TRUE);
        }
    }
  else
    /* legacy menu */
    {
      for(i = menu_item_id - REQUEST_SEQ,
            l = menu_data->window->context_map->seq_data_featuresets;
          i && l; l = l->next, i--)
        {
          continue;
        }

      if(l)
        {        /* this is a featureset not a column ! */
          req_list = g_list_prepend(req_list,l->data);
        }
    }

  if(req_list)
    {
      ZMapFeatureBlock block;
      ZMapWindowContainerGroup container;

      /* may not have a feature but must have clicked on a column
       * we need the containing block to fetch the data
       */
      container = zmapWindowContainerUtilsGetParentLevel((ZMapWindowContainerGroup)(menu_data->container_set),
                                                         ZMAPCONTAINER_LEVEL_BLOCK);
      block = zmapWindowItemGetFeatureBlock((FooCanvasItem *) container);

      /* can't use 'is_column' here as we may be requesting several */
      zmapWindowFetchData(menu_data->window, block, req_list, TRUE,FALSE);
    }
  g_free(menu_data) ;

  return ;
}




/* call blixem either for a single type of homology or for all homologies. */
static void blixemMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapWindowAlignSetType requested_homol_set = ZMAPWINDOW_ALIGNCMD_INVALID ;
  GList *seq_sets = NULL;

  if (!menu_data)
    return ;

  switch(menu_item_id)
    {
    case BLIX_NONE:
      requested_homol_set = ZMAPWINDOW_ALIGNCMD_NONE ;
      break;
    case BLIX_SELECTED:
      requested_homol_set = ZMAPWINDOW_ALIGNCMD_FEATURES ;
      break;
    case BLIX_EXPANDED:
      requested_homol_set = ZMAPWINDOW_ALIGNCMD_EXPANDED ;
      break;
    case BLIX_SET:
      requested_homol_set = ZMAPWINDOW_ALIGNCMD_SET ;
      break;
    case BLIX_MULTI_SETS:
      requested_homol_set = ZMAPWINDOW_ALIGNCMD_MULTISET ;
      break;

    case BLIX_SEQ_COVERAGE:                /* blixem from a selected item in a coverage featureset */
#if RESTRICT_TO_MARK
      if (!zmapWindowMarkIsSet(menu_data->window->mark))
        {
          zMapMessage("You must set the mark first to select this option","");
        }
      else
#endif
        {
          /*! \todo #warning if we ever have paired reads data in a virtual featureset we need to expand that here */
          seq_sets = add_column_featuresets(menu_data->window->context_map,seq_sets,menu_data->req_id,FALSE);
          requested_homol_set = ZMAPWINDOW_ALIGNCMD_SEQ ;
        }
      break;

#if 0
    default:
      break;
#else
    default:
      if(menu_item_id >= BLIX_SEQ)      /* one or more sets starting from BLIX_SEQ */
        {
          GList *l;
          int i;

#if RESTRICT_TO_MARK
          if (!zmapWindowMarkIsSet(menu_data->window->mark))
            {
              zMapMessage("You must set the mark first to select this option","");
            }
          else
#endif
            {
              for (i = menu_item_id - BLIX_SEQ, l = menu_data->window->context_map->seq_data_featuresets ;
                   i && l ;
                   l = l->next, i--)
                {
                  continue ;
                }

              if (l)
                {
                  ZMapFeatureSetDesc src ;

                  src = (ZMapFeatureSetDesc)g_hash_table_lookup(menu_data->window->context_map->featureset_2_column,l->data);
                  if(src)
                    {
                      seq_sets = g_list_prepend (seq_sets,GUINT_TO_POINTER(src->feature_src_ID));
                      requested_homol_set = ZMAPWINDOW_ALIGNCMD_SEQ ;
                    }
                }
            }

          break ;
        }
#endif
    }


  if (requested_homol_set)
    zmapWindowCallBlixem(menu_data->window, menu_data->item, requested_homol_set,
                         menu_data->feature_set, seq_sets, menu_data->x, menu_data->y) ;

  g_free(menu_data) ;

  return ;
}



/* this needs to be a general function... */
static FooCanvasGroup *menuDataItemToColumn(FooCanvasItem *item)
{
  ZMapWindowContainerGroup container;
  FooCanvasGroup *column_group = NULL;

  if((container = zmapWindowContainerCanvasItemGetContainer(item)))
    {
      column_group = (FooCanvasGroup *)container;
    }

  return column_group;
}



static gboolean exportFASTA(ZMapWindow window,
                            ZMapFASTASeqType seq_type, const char *sequence, const char *seq_name, int seq_len,
                            const char *molecule_name, const char *gene_name, GError **error)
{
  static const char *error_prefix = "FASTA DNA export failed:" ;
  gboolean result = FALSE ;
  char *filepath = NULL ;
  GIOChannel *file = NULL ;
  GError *tmp_error = NULL ;
  GtkWidget *toplevel ;

  if (!window)
    return result ;

  toplevel = zMapGUIFindTopLevel(window->toplevel) ;


  if (!(filepath = zmapGUIFileChooser(toplevel, "FASTA filename ?", NULL, NULL))
      || !(file = g_io_channel_new_file(filepath, "w", &tmp_error))
      || !zMapFASTAFile(file, seq_type, seq_name, seq_len, sequence,
                        molecule_name, gene_name, &tmp_error))
    {
      if (tmp_error)
        {
          g_propagate_error(error, tmp_error) ;
        }
    }
  else
    {
      result = TRUE ;
    }


  if (file)
    {
      GIOStatus status ;

      if ((status = g_io_channel_shutdown(file, TRUE, &tmp_error)) != G_IO_STATUS_NORMAL)
        {
          zMapShowMsg(ZMAP_MSG_WARNING, "%s  %s", error_prefix, tmp_error->message) ;

          g_error_free(tmp_error) ;
        }
    }

  if (filepath)
    g_free(filepath) ;


  return result ;
}


/*
 * This function exports a list of featuresets. Arguments are:
 *   window                    ZMapWindow
 *   featuresets               GList containing featureset->unique_id
 *   marked_region             whether we are to use the marked region or all features
 *   filepath_inout            may be set on entry, but does not have to be
 *   error                     GError to propagate (this should be propagated to the
 *                             caller and not deleted locally)
 */
gboolean zMapWindowExportFeatureSets(ZMapWindow window,
                                     GList* featuresets,
                                     gboolean marked_region,
                                     char **filepath_inout,
                                     GError **error)
{
  gboolean result = FALSE ;
  ZMapFeatureAny context = NULL ;
  GHashTable *styles = NULL ;
  ZMapSpanStruct mark_region = {0,0};
  GIOChannel *file = NULL ;
  char *filepath = NULL ;

  zMapReturnValIfFail(     window
                        && window->feature_context
                        && window->context_map
                        && featuresets
                        && g_list_length(featuresets)
                        && error,
                           result) ;

  /*
   * This should be the whole thing.
   */
  context = (ZMapFeatureAny) window->feature_context ;


  zMapReturnValIfFail(    (context->struct_type == ZMAPFEATURE_STRUCT_CONTEXT)
                       && window->context_map->styles,
                          result ) ;

  styles = window->context_map->styles ;

  /*
   * Fish out the marked region if it is available; otherwise leave its start
   * and end zero.
   */
  if (marked_region)
    {
      if (zmapWindowMarkIsSet(window->mark))
        {
          zmapWindowMarkGetSequenceRange(window->mark, &(mark_region.x1), &(mark_region.x2));
          result = TRUE ;
        }
      else
        {
          g_set_error(error, g_quark_from_string("ZMap"), 99, "The mark is not set") ;
        }
    }
  else
    {
      result = TRUE ;
    }

  if (result )
    {

      /*
       * Revcomp if required.
      */
      if (window->flags[ZMAPFLAG_REVCOMPED_FEATURES])
        zMapFeatureContextReverseComplement(window->feature_context, window->context_map->styles) ;

      /*
       * Do the filenames/path business and create the IOChannel.
       * If a filepath is passed in, then we use that, otherwise must
       * prompt for one.
       */
      if (filepath_inout && *filepath_inout)
        filepath = g_strdup(*filepath_inout) ;

      if (!filepath)
        filepath = zmapGUIFileChooser(gtk_widget_get_toplevel(window->toplevel), "Feature Export filename ?", NULL, "gff") ;

      if (    filepath
           && (file = g_io_channel_new_file(filepath, "w", error)))
        {
          result = TRUE ;
        }

      /*
       * Now we finally get around to doing some output.
       */
      if (result && file)
        {
          result = zMapGFFDumpFeatureSets((ZMapFeatureAny)window->feature_context,
                                          styles, featuresets, &mark_region, file, error) ;
        }

      /*
       * Shutdown IO channel. Must be done whether the call to dump data
       * succeeded or not.
       */
      if (file)
        {
          GError *tmp_error = NULL ;
          GIOStatus status ;
          if ((status = g_io_channel_shutdown(file, TRUE, &tmp_error)) != G_IO_STATUS_NORMAL)
            {
              if (tmp_error)
                {
                  zMapShowMsg(ZMAP_MSG_WARNING, "%s", tmp_error->message) ;
                  g_error_free(tmp_error) ;
                }
            }
        }

      /*
       * Revcomp if required. Must be done irrespective of whether anything above
       * succeeded.
       */
      if (window->flags[ZMAPFLAG_REVCOMPED_FEATURES])
        zMapFeatureContextReverseComplement(window->feature_context, window->context_map->styles) ;

    }

  return result ;
}


/*
 * If the feature that is passed is NULL then the whole context is used.
 * If all_features is true then the parent block is used, otherwise just the feature itself.
 * If marked_region is true then the results are limited to features that overlap the mark.
 */
gboolean zMapWindowExportFeatures(ZMapWindow window, gboolean all_features, gboolean marked_region,
                                  ZMapFeatureAny feature_in, char **filepath_inout, GError **error)
{
  gboolean result = FALSE ;
  ZMapFeatureAny feature = feature_in ;

  if (!window)
    return result ;

  if (!feature)
    feature = (ZMapFeatureAny)window->feature_context ;

  if (marked_region)
    {
      if (zmapWindowMarkIsSet(window->mark))
        {
          ZMapSpanStruct mark_region = {0,0};

          zmapWindowMarkGetSequenceRange(window->mark, &(mark_region.x1), &(mark_region.x2));

          result = exportFeatures(window, all_features, &mark_region, feature, filepath_inout, error) ;
        }
      else
        {
          g_set_error(error, g_quark_from_string("ZMap"), 99, "The mark is not set") ;
        }
    }
  else
    {
      result = exportFeatures(window, all_features, NULL, feature, filepath_inout, error) ;
    }

  return result ;
}


static gboolean exportFeatures(ZMapWindow window, gboolean all_features, ZMapSpan region_span, ZMapFeatureAny feature_in,
                               char **filepath_inout, GError **error)
{
  static const char *error_prefix = "Features export failed:" ;
  gboolean result = FALSE ;
  char *filepath = NULL ;
  GIOChannel *file = NULL ;
  GError *tmp_error = NULL ;
  ZMapFeatureAny feature = feature_in ;

  if (!window)
    return result ;

  if (filepath_inout && *filepath_inout)
    filepath = g_strdup(*filepath_inout) ;

  if (all_features &&
      (feature->struct_type == ZMAPFEATURE_STRUCT_FEATURESET
       || feature->struct_type == ZMAPFEATURE_STRUCT_FEATURE)
     )
    {
      /* For features and featuresets, get the parent block */
      feature = (ZMapFeatureAny)zMapFeatureGetParentGroup(feature, ZMAPFEATURE_STRUCT_BLOCK);
    }

  /* Swop to other strand..... */
  if (window->flags[ZMAPFLAG_REVCOMPED_FEATURES])
    zMapFeatureContextReverseComplement(window->feature_context, window->context_map->styles) ;

  if (!filepath)
    filepath = zmapGUIFileChooser(gtk_widget_get_toplevel(window->toplevel), "Feature Export filename ?", NULL, "gff") ;

  /* If saving a single feature and it's in the Annotation column, we need to extract any unsaved
   * attributes. */
  ZMapFeatureAny temp_feature = NULL ;
  ZMapFeatureSet temp_featureset = NULL ;
  ZMapFeatureBlock block = NULL ;
  const GQuark scratch_id = zMapStyleCreateID(ZMAP_FIXED_STYLE_SCRATCH_NAME) ;

  if (!all_features && feature->struct_type == ZMAPFEATURE_STRUCT_FEATURE &&
      feature->parent && feature->parent->unique_id == scratch_id)
    {
      /* save the original feature as temp_feature */
      temp_feature = feature ;

      /* create a copy and update the name to that in the attributes */
      feature = (ZMapFeatureAny)zMapFeatureShallowCopy((ZMapFeature)temp_feature) ;

      feature->original_id = window->int_values[ZMAPINT_SCRATCH_ATTRIBUTE_FEATURE] ;
      feature->unique_id = feature->original_id ;

      /* We need to update the parent to be the featureset from the attributes. Create a temp
       * featureset with this name, because it may not exist */
      const GQuark featureset_id = window->int_values[ZMAPINT_SCRATCH_ATTRIBUTE_FEATURESET] ;
      block = (ZMapFeatureBlock)(zMap_g_hash_table_nth(window->feature_context->master_align->blocks, 0)) ;

      temp_featureset = (ZMapFeatureSet)zMapFeatureParentGetFeatureByID((ZMapFeatureAny)block, featureset_id) ;

      if (temp_featureset)
        {
          feature->parent = (ZMapFeatureAny)temp_featureset ;
          temp_featureset = NULL ; /* reset pointer so we don't free it */
        }
      else
        {
          const GQuark featureset_unique_id = zMapFeatureSetCreateID(g_quark_to_string(featureset_id)) ;
          temp_featureset = zMapFeatureSetIDCreate(featureset_id, featureset_unique_id, NULL, NULL) ;
          zMapFeatureBlockAddFeatureSet(block, temp_featureset) ;
          feature->parent = (ZMapFeatureAny)temp_featureset ;
        }
    }

  if (!filepath
      || !(file = g_io_channel_new_file(filepath, "w", &tmp_error))
      || !zMapGFFDumpRegion(feature, window->context_map->styles, region_span, file, &tmp_error))
    {
      /* N.B. if there is no filepath it means user cancelled so take no action...,
       * otherwise we output the error message. */
      if (tmp_error)
        {
          g_propagate_error(error, tmp_error) ;
        }
    }
  else
    {
      result = TRUE ;
    }

  if (temp_feature)
    {
      /* Free temp feature and set pointer back to original feature */
      zMapFeatureAnyDestroyShallow(feature) ;
      feature = (ZMapFeatureAny)temp_feature ;
      temp_feature = NULL ;
    }

  if (temp_featureset)
    {
      zMapFeatureBlockRemoveFeatureSet(block, temp_featureset) ;
      zMapFeatureAnyDestroyShallow((ZMapFeatureAny)temp_featureset) ;
      temp_featureset = NULL ;
    }

  if (file)
    {
      GIOStatus status ;

      if ((status = g_io_channel_shutdown(file, TRUE, &tmp_error)) != G_IO_STATUS_NORMAL)
        {
          zMapShowMsg(ZMAP_MSG_WARNING, "%s  %s", error_prefix, tmp_error->message) ;

          g_error_free(tmp_error) ;
        }
    }

  /* And swop it back again. */
  if (window->flags[ZMAPFLAG_REVCOMPED_FEATURES])
    zMapFeatureContextReverseComplement(window->feature_context, window->context_map->styles) ;

  if (filepath)
    {
      if (filepath_inout && !*filepath_inout)
        {
          /* We've created the filepath in this function so set the output value from it */
          *filepath_inout = filepath ;
        }
      else
        {
          /* Return value wasn't requested so free this */
          g_free(filepath) ;
        }
    }

  return result ;
}



/* for migration from ACEDB call this via the --dump_config command line arg
 * NB only write the column and featureset config stanzas
 * this really belongs int he view but due to scope issue we cannot access that data here :-(
 * so instead we dump the copy of that data that had to be passed to the window so that we could access it
 *
 * Hmmm.... we have featureset_2_column but not src_2_src
 * so no can do.
 * Will revisit after tidying up the view/window data copying fiasco
 */
#if MH17_DONT_INCLUDE   // can't do this due top scope
void dumpConfig(GHashTable *f2c, GHashTable *f2s)
{
  GList *iter;
  ZMapFeatureSetDesc gff_set;
  ZMapFeatureSource gff_src;
  gpointer key, value;
  GHashTable * cols = zMap_g_hashlist_create();
  GList *l;
  char *filepath = NULL ;
  GIOChannel *file = NULL ;
  GError *error = NULL ;
  char *error_prefix = "Feature config dump failed:" ;

  if (!(filepath = zmapGUIFileChooser(window->toplevel, "Config Dump filename ?", NULL, "zmap"))
      || !(file = g_io_channel_new_file(filepath, "w", &error))
      || !zMapFeatureContextDump(window->feature_context, window->context_map->styles, file, &error))
    {
      /* N.B. if there is no filepath it means user cancelled so take no action...,
       * otherwise we output the error message. */
      if (error)
        {
          zMapShowMsg(ZMAP_MSG_WARNING, "%s  %s", error_prefix, error->message) ;

          g_error_free(error) ;
        }
    }


  if (file)
    {
      GIOStatus status ;

      if ((status = g_io_channel_shutdown(file, TRUE, &error)) != G_IO_STATUS_NORMAL)
        {
          zMapShowMsg(ZMAP_MSG_WARNING, "%s  %s", error_prefix, error->message) ;

          g_error_free(error) ;
        }
    }



  zMap_g_hash_table_iter_init (&iter, f2c);
  while (zMap_g_hash_table_iter_next (&iter, &key, &value))
    {
      gff_set = (ZMapFeatureSetDesc) value;
      zMap_g_hashlist_insert(cols,value,key);
    }

  printf("\n[columns]\n");

  zMap_g_hash_table_iter_init (&iter, cols);
  while (zMap_g_hash_table_iter_next (&iter, &key, &value))
    {
      char *sep = "";
      printf("%s = ");
      for(l = (GList *) value;l;l = l->next)
        {
          printf(" %s %s",sep,g_quark_to_string(GPOINTER_TO_UINT(l->data)));
          sep = ";";
        }
      printf("\n");
    }


  zMap_g_hashlist_destroy(cols);

  printf("\nf[featureset-style]\n");
  zMap_g_hash_table_iter_init (&iter, f2s);
  while (zMap_g_hash_table_iter_next (&iter, &key, &value))
    {
      gff_src = (ZMapFeatureSource) value;
      printf("%s = %s\n",g_quark_to_string(GPOINTER_TO_UINT(key)),
             g_quark_to_string(GPOINTER_TO_UINT(gff_src->style_id)));
    }
  return ;
}

#endif

gboolean zMapWindowExportContext(ZMapWindow window, GError **error)
{
  static const char *error_prefix = "Feature context export failed:" ;
  gboolean result = FALSE ;
  char *filepath = NULL ;
  GIOChannel *file = NULL ;
  GError *tmp_error = NULL ;

  if (!window || !window->context_map)
    return result ;

  if (!(filepath = zmapGUIFileChooser(window->toplevel, "Context Export filename ?", NULL, "zmap"))
      || !(file = g_io_channel_new_file(filepath, "w", &tmp_error))
      || !zMapFeatureContextDump(window->feature_context, window->context_map->styles, file, &tmp_error))
    {
      /* N.B. if there is no filepath it means user cancelled so take no action...,
       * otherwise we output the error message. */
      if (tmp_error)
        {
          g_propagate_error(error, tmp_error) ;
        }
    }
  else
    {
      result = TRUE ;
    }

  if (file)
    {
      GIOStatus status ;

      if ((status = g_io_channel_shutdown(file, TRUE, &tmp_error)) != G_IO_STATUS_NORMAL)
        {
          zMapShowMsg(ZMAP_MSG_WARNING, "%s  %s", error_prefix, tmp_error->message) ;

          g_error_free(tmp_error) ;
        }
    }


  return result ;
}

/*

* This is an attempt to make dynamic menus... Only really worth it
* for the align & block menus. With others a static struct, as above,
* is much more preferable.

*/

static void insertSubMenus(GString *branch_point_string,
                           ZMapGUIMenuItem sub_menus,
                           ZMapGUIMenuItem item,
                           GArray **items_array)
{
  int branch_length = 0;
  if (!branch_point_string)
    return ;
  branch_length = branch_point_string->len;

  while(sub_menus && sub_menus->name != NULL)
    {
      if(*items_array)
        {
          g_string_append_printf(branch_point_string, "/%s", sub_menus->name);

          memcpy(item, sub_menus, sizeof(ZMapGUIMenuItemStruct));

          item->name   = g_strdup(branch_point_string->str); /* memory leak */

          *items_array = g_array_append_val(*items_array, *item);
        }

      branch_point_string = g_string_erase(branch_point_string,
                                           branch_length,
                                           branch_point_string->len - branch_length);
      sub_menus++;        /* move on */
    }

  return ;
}


static ZMapFeatureContextExecuteStatus alignBlockMenusDataListForeach(GQuark key,
                                                                      gpointer data,
                                                                      gpointer user_data,
                                                                      char **error_out)
{
  ZMapFeatureAny feature_any = (ZMapFeatureAny)data;
  ZMapFeatureLevelType feature_type = ZMAPFEATURE_STRUCT_INVALID;
  GString *item_name = NULL;
  ZMapGUIMenuItem item = NULL, align_items = NULL, block_items = NULL;
  GArray **items_array = NULL;
  char *stem = NULL;
  AlignBlockMenu  all_data = (AlignBlockMenu)user_data;

  if (!data || !user_data)
    return ZMAP_CONTEXT_EXEC_STATUS_ERROR ;

  items_array = all_data->array;
  stem        = all_data->stem;

  feature_type = feature_any->struct_type;

  item = g_new0(ZMapGUIMenuItemStruct, 1);

  switch(feature_type)
    {
    case ZMAPFEATURE_STRUCT_ALIGN:
      item_name = g_string_sized_new(100);
      g_string_printf(item_name, "%s%sAlign %s",
                      (stem ? stem : ""),
                      (stem ? "/"  : ""),
                      g_quark_to_string( feature_any->original_id ));

      item->name   = g_strdup(item_name->str); /* memory leak */
      item->type   = ZMAPGUI_MENU_BRANCH;
      *items_array = g_array_append_val(*items_array, *item);

      align_items  = all_data->each_align_items;
      while(align_items && align_items->name != NULL)
        {
          ZMapGUIMenuSubMenuData sub_data = g_new0(ZMapGUIMenuSubMenuDataStruct, 1);
          sub_data->align_unique_id  = feature_any->unique_id;
          sub_data->block_unique_id  = 0;
          sub_data->original_data    = align_items->callback_data;
          align_items->callback_data = sub_data;
          align_items++;
        }
      align_items  = all_data->each_align_items;
      insertSubMenus(item_name, align_items, item, items_array);

      g_string_free(item_name, TRUE);
      break;
    case ZMAPFEATURE_STRUCT_BLOCK:
      item_name = g_string_sized_new(100);
      g_string_printf(item_name, "%s%sAlign %s/Block %s",
                      (stem ? stem : ""),
                      (stem ? "/"  : ""),
                      g_quark_to_string( feature_any->parent->original_id ),
                      g_quark_to_string( feature_any->original_id ));

      item->name   = g_strdup(item_name->str); /* memory leak */
      item->type   = ZMAPGUI_MENU_BRANCH;
      *items_array = g_array_append_val(*items_array, *item);

      block_items  = all_data->each_block_items;
      while(block_items && block_items->name != NULL)
        {
          ZMapGUIMenuSubMenuData sub_data = g_new0(ZMapGUIMenuSubMenuDataStruct, 1);
          sub_data->align_unique_id  = feature_any->parent->unique_id;
          sub_data->block_unique_id  = feature_any->unique_id;
          sub_data->original_data    = block_items->callback_data;
          block_items->callback_data = sub_data;
          block_items++;
        }

      block_items  = all_data->each_block_items;
      insertSubMenus(item_name, block_items, item, items_array);

      g_string_free(item_name, TRUE);
      break;
    case ZMAPFEATURE_STRUCT_CONTEXT:
      break;
    case ZMAPFEATURE_STRUCT_FEATURESET:
    case ZMAPFEATURE_STRUCT_FEATURE:
    case ZMAPFEATURE_STRUCT_INVALID:
    default:
      zMapWarnIfReached();
      break;

    }

  g_free(item);

  return ZMAP_CONTEXT_EXEC_STATUS_OK;
}


 void zMapWindowMenuAlignBlockSubMenus(ZMapWindow window,
                                       ZMapGUIMenuItem each_align,
                                       ZMapGUIMenuItem each_block,
                                       char *root,
                                       GArray **items_array_out)
{
  if (!window)
    return ;
  AlignBlockMenuStruct data = {0};

  data.each_align_items = each_align;
  data.each_block_items = each_block;
  data.stem             = root;
  data.array            = items_array_out;

  zMapFeatureContextExecute((ZMapFeatureAny)(window->feature_context),
                            ZMAPFEATURE_STRUCT_BLOCK,
                            alignBlockMenusDataListForeach,
                            &data);

  return ;
}


/* Get colours for coding, non-coding etc of sequence. */
static gboolean getSeqColours(ZMapFeatureTypeStyle style,
                              GdkColor **non_coding_out, GdkColor **coding_out,
                              GdkColor **split_5_out, GdkColor **split_3_out)
{
  gboolean result =  FALSE ;
  if (!style)
    return result ;

  /* I'm sure this is not the way to access these...find out from Malcolm where we stand on this... */
  if (style->mode_data.sequence.non_coding.normal.fields_set.fill_col
      && style->mode_data.sequence.coding.normal.fields_set.fill_col
      && style->mode_data.sequence.split_codon_5.normal.fields_set.fill_col
      && style->mode_data.sequence.split_codon_3.normal.fields_set.fill_col)
    {
      *non_coding_out = (GdkColor *)(&(style->mode_data.sequence.non_coding.normal.fill_col)) ;
      *coding_out = (GdkColor *)(&(style->mode_data.sequence.coding.normal.fill_col)) ;
      *split_5_out = (GdkColor *)(&(style->mode_data.sequence.split_codon_5.normal.fill_col)) ;
      *split_3_out = (GdkColor *)(&(style->mode_data.sequence.split_codon_3.normal.fill_col)) ;

      result = TRUE ;
    }

  return result ;
}


/* Given a transcript feature get the annotated exons for that feature and create
 * corresponding text attributes for each one, these give position of annotated
 * exon and it's colour. */
static GList *getTranscriptTextAttrs(ZMapFeature feature, gboolean spliced, gboolean cds,
                                     GdkColor *non_coding, GdkColor *coding,
                                     GdkColor *split_5, GdkColor *split_3)
{
  GList *text_attrs = NULL ;
  GList *exon_list = NULL ;

  if (zMapFeatureAnnotatedExonsCreate(feature, FALSE, FALSE, &exon_list))
    {
      MakeTextAttrStruct text_data ;

      text_data.feature = feature ;
      text_data.cds = cds ;
      text_data.spliced = spliced ;
      text_data.non_coding = non_coding ;
      text_data.coding = coding ;
      text_data.split_5 = split_5 ;
      text_data.split_3 = split_3 ;
      text_data.text_attrs = text_attrs ;

      g_list_foreach(exon_list, createExonTextTag, &text_data) ;

      text_attrs = text_data.text_attrs ;

      zMapFeatureAnnotatedExonsDestroy(exon_list) ;
    }

  return text_attrs ;
}

/* Create a text attribute for a given annotated exon. */
static void createExonTextTag(gpointer data, gpointer user_data)
{
  ZMapFullExon exon = (ZMapFullExon)data ;
  MakeTextAttr text_data = (MakeTextAttr)user_data ;
  GList *text_attrs_list = text_data->text_attrs ;
  ZMapGuiTextAttr text_attr = NULL ;

  /* Text buffer positions are zero-based so "- 1" for all positions. */
  if (!(text_data->spliced))
    {
      text_attr = g_new0(ZMapGuiTextAttrStruct, 1) ;

      text_attr->start = exon->unspliced_span.x1 - 1 ;
      text_attr->end = exon->unspliced_span.x2 - 1 ;
    }
  else
    {
      if (!(text_data->cds) || exon->region_type != EXON_NON_CODING)
        {
          text_attr = g_new0(ZMapGuiTextAttrStruct, 1) ;

          if ((text_data->cds))
            {
              text_attr->start = exon->cds_span.x1 - 1 ;
              text_attr->end = exon->cds_span.x2 - 1 ;
            }
          else
            {
              text_attr->start = exon->spliced_span.x1 - 1 ;
              text_attr->end = exon->spliced_span.x2 - 1 ;
            }
        }
    }


  if (text_attr)
    {
      switch (exon->region_type)
        {
        case EXON_NON_CODING:
          text_attr->background = text_data->non_coding ;

          break ;

        case EXON_CODING:
          {
            text_attr->background = text_data->coding ;

            break ;
          }

        case EXON_START_NOT_FOUND:
        case EXON_SPLIT_CODON_5:
          text_attr->background = text_data->split_5 ;

          break ;

        case EXON_SPLIT_CODON_3:
          text_attr->background = text_data->split_3 ;

          break ;

        default:
          zMapWarnIfReached() ;
        }

      text_attrs_list = g_list_append(text_attrs_list, text_attr) ;

      text_data->text_attrs = text_attrs_list ;
    }

  return ;
}


/* Offset text attributes by given amount. */
static void offsetTextAttr(gpointer data, gpointer user_data)
{
  ZMapGuiTextAttr text_attr = (ZMapGuiTextAttr)data ;
  if (!data)
    return ;
  int offset = GPOINTER_TO_INT(user_data) ;

  text_attr->start += offset ;
  text_attr->end += offset ;

  return ;
}


/* Services the "Search or List Features and Sequence" sub-menu.  */
static void searchListMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapFeatureAny feature_any ;
  FooCanvasGroup *featureset_group ;
  ZMapWindowContainerFeatureSet container;
  ZMapFeatureSet featureset ;
  ZMapFeature feature = NULL ;
  gboolean zoom_to_item = TRUE;
  if (!menu_data)
    return ;


  /* Retrieve the feature or featureset info from the canvas item. */
  feature_any = zmapWindowItemGetFeatureAny(menu_data->item);
  if (feature_any->struct_type != ZMAPFEATURE_STRUCT_FEATURESET
             && feature_any->struct_type != ZMAPFEATURE_STRUCT_FEATURE)
    return ;

  if (feature_any->struct_type == ZMAPFEATURE_STRUCT_FEATURESET)
    {
      featureset_group = FOO_CANVAS_GROUP(menu_data->item) ;
      featureset = (ZMapFeatureSet)feature_any ;
    }
  else
    {
      featureset_group = (FooCanvasGroup *)zmapWindowContainerCanvasItemGetContainer(menu_data->item) ;
      feature = zmapWindowItemGetFeature(menu_data->item);
      featureset = (ZMapFeatureSet)feature->parent ;
    }

  container = (ZMapWindowContainerFeatureSet)(featureset_group);


  switch (menu_item_id)
    {
    case ITEM_MENU_LIST_ALL_FEATURES:
    case ITEM_MENU_LIST_NAMED_FEATURES:
      {
        ZMapWindowFToISetSearchData search_data = NULL;
        ZMapStrand set_strand ;
        ZMapFrame set_frame ;
        GQuark align_id = 0, block_id = 0, column_id = 0, featureset_id = 0, feature_id = 0 ;
        FooCanvasItem *current_item = NULL ;
        char *title = NULL ;
        gpointer search_func ;

        column_id = menu_data->container_set->unique_id ;
        align_id = featureset->parent->parent->unique_id ;
        block_id = featureset->parent->unique_id ;
        featureset_id = featureset->unique_id ;

        if (feature)
          current_item = menu_data->item ;

        title = (char *)g_quark_to_string(container->original_id) ;

        if (menu_item_id == ITEM_MENU_LIST_ALL_FEATURES)
          {
            /*
             * Set feature name to wild card to ensure we list all of them.
             *
             * (sm23) The feature pointer must also be NULL for this to work
             * AND the featureset_id has to be zero. Otherwise the search function
             * will only return features from the specified featureset (i.e. the
             * first one found in that column).
             */
            featureset_id = 0 ;
            feature_id = g_quark_from_string("*") ;
            feature = NULL ;
            search_func = (void *)zmapWindowFToIFindItemSetFull ;
          }
        else
          {
            /* Set feature name to original id to ensure we get all features in column with
             * same name. */
            feature_id = feature->original_id ;
            search_func = (void *)zmapWindowFToIFindSameNameItems ;
          }

        set_strand = zmapWindowContainerFeatureSetGetStrand(container) ;
        set_frame = zmapWindowContainerFeatureSetGetFrame(container) ;

        if ((search_data = zmapWindowFToISetSearchCreate(search_func,
                                                         feature,
                                                         align_id,
                                                         block_id,
                                                         column_id,
                                                         featureset_id,
                                                         feature_id,
                                                         zMapFeatureStrand2Str(set_strand),
                                                         zMapFeatureFrame2Str(set_frame))))
          zmapWindowListWindow(menu_data->window,
                               current_item, title,
                               NULL, NULL,
                               menu_data->window->context_map,
                               (ZMapWindowListSearchHashFunc)zmapWindowFToISetSearchPerform, search_data,
                               (GDestroyNotify)zmapWindowFToISetSearchDestroy, zoom_to_item) ;

        break ;
      }

    case ITEM_MENU_SEARCH:
      {
        zmapWindowCreateSearchWindow(menu_data->window,
                                     NULL, NULL,
                                     menu_data->window->context_map,
                                     menu_data->item) ;

        break ;
      }

    case ITEM_MENU_SEQUENCE_SEARCH_DNA:
      {
        zmapWindowCreateSequenceSearchWindow(menu_data->window, menu_data->item, ZMAPSEQUENCE_DNA) ;

        break ;
      }

    case ITEM_MENU_SEQUENCE_SEARCH_PEPTIDE:
      {
        zmapWindowCreateSequenceSearchWindow(menu_data->window, menu_data->item, ZMAPSEQUENCE_PEPTIDE) ;

        break ;
      }

    default:
      {
        zMapWarning("%s", "Unexpected search menu callback action\n") ;
        zMapWarnIfReached() ;
        break ;
      }
    }

  g_free(menu_data) ;

  return ;
}



