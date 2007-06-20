/*  File: zmapWindowDraw.c
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
 *      Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk
 *
 * Description: General drawing functions for zmap window, e.g.
 *              repositioning columns after one has been bumped
 *              or removed etc.
 *
 * Exported functions: See zmapWindow_P.h
 * HISTORY:
 * Last edited: Jun 20 11:11 2007 (rds)
 * Created: Thu Sep  8 10:34:49 2005 (edgrif)
 * CVS info:   $Id: zmapWindowDraw.c,v 1.70 2007-06-20 12:02:56 rds Exp $
 *-------------------------------------------------------------------
 */

#include <ZMap/zmapUtils.h>
#include <ZMap/zmapGLibUtils.h>
#include <zmapWindow_P.h>
#include <zmapWindowContainer.h>



/* For straight forward bumping. */
typedef struct
{
  GHashTable *pos_hash ;
  GList *pos_list ;

  double offset ;
  double incr ;
  
  gboolean bump_all ;

  ZMapFeatureTypeStyle bumped_style ;
} BumpColStruct, *BumpCol ;


typedef struct
{
  double y1, y2 ;
  double offset ;
  double incr ;
} BumpColRangeStruct, *BumpColRange ;



/* THERE IS A BUG IN THE CODE THAT USES THIS...IT MOVES FEATURES OUTSIDE OF THE BACKGROUND SO
 * IT ALL LOOKS NAFF, PROBABLY WE GET AWAY WITH IT BECAUSE COLS ARE SO WIDELY SPACED OTHERWISE
 * THEY MIGHT OVERLAP. I THINK THE MOVING CODE MAY BE AT FAULT. */
/* For complex bump users seem to want columns to overlap a bit, if this is set to 1.0 there is
 * no overlap, if you set it to 0.5 they will overlap by a half and so on. */
#define COMPLEX_BUMP_COMPRESS 1.0


typedef gboolean (*OverLapListFunc)(GList *curr_features, GList *new_features) ;

typedef struct
{
  ZMapWindow window ;
  ZMapFeatureTypeStyle bumped_style ;
  gboolean bump_all ;
  GHashTable *name_hash ;
  GList *bumpcol_list ;
  double curr_offset ;
  double incr ;
  OverLapListFunc overlap_func ;
  gboolean protein ;
} ComplexBumpStruct, *ComplexBump ;


typedef struct
{
  ZMapWindow window ;
  double incr ;
  double offset ;
  GList *feature_list ;
} ComplexColStruct, *ComplexCol ;


typedef struct
{
  GList **name_list ;
  OverLapListFunc overlap_func ;
} FeatureDataStruct, *FeatureData ;

typedef struct
{
  gboolean overlap ;
  int start, end ;
  int feature_diff ;
  ZMapFeature feature ;
  double width ;
} RangeDataStruct, *RangeData ;



typedef struct
{
  ZMapWindow window ;
  double zoom ;

} ZoomDataStruct, *ZoomData ;


typedef struct execOnChildrenStruct_
{
  ZMapContainerLevelType stop ;

  GFunc                  down_func_cb ;
  gpointer               down_func_data ;

  GFunc                  up_func_cb ;
  gpointer               up_func_data ;

} execOnChildrenStruct, *execOnChildren ;


/* For 3 frame display/normal display. */
typedef struct
{
  FooCanvasGroup *forward_group, *reverse_group ;
  ZMapFeatureBlock block ;
  ZMapWindow window ;

  FooCanvasGroup *curr_forward_col ;
  FooCanvasGroup *curr_reverse_col ;

  int frame3_pos ;
  ZMapFrame frame ;

} RedrawDataStruct, *RedrawData ;



static void bumpColCB(gpointer data, gpointer user_data) ;

static void preZoomCB(FooCanvasGroup *data, FooCanvasPoints *points, 
                      ZMapContainerLevelType level, gpointer user_data) ;
static void resetWindowWidthCB(FooCanvasGroup *data, FooCanvasPoints *points, 
                               ZMapContainerLevelType level, gpointer user_data);
static void columnZoomChanged(FooCanvasGroup *container, double new_zoom, ZMapWindow window) ;


static void compareListOverlapCB(gpointer data, gpointer user_data) ;
static gint horizPosCompare(gconstpointer a, gconstpointer b) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
static void addToList(gpointer data, gpointer user_data);
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


static gboolean featureListCB(gpointer data, gpointer user_data) ;

static gint sortByScoreCB(gconstpointer a, gconstpointer b) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
static gint sortBySpanCB(gconstpointer a, gconstpointer b) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

static gint sortByStrandSpanCB(gconstpointer a, gconstpointer b) ;
static gint sortByOverlapCB(gconstpointer a, gconstpointer b, gpointer user_data) ;
static void bestFitCB(gpointer data, gpointer user_data) ;
static gboolean itemGetCoords(FooCanvasItem *item, double *x1_out, double *x2_out) ;
static void listScoreCB(gpointer data, gpointer user_data) ;
static void makeNameListCB(gpointer data, gpointer user_data) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
static void makeNameListStrandedCB(gpointer data, gpointer user_data) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

static void setOffsetCB(gpointer data, gpointer user_data) ;
static void setAllOffsetCB(gpointer data, gpointer user_data) ;
static void getMaxWidth(gpointer data, gpointer user_data) ;
static void sortListPosition(gpointer data, gpointer user_data) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
static void addBackgrounds(gpointer data, gpointer user_data) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
static void addMultiBackgrounds(gpointer data, gpointer user_data) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


static void NEWaddMultiBackgrounds(gpointer data, gpointer user_data) ;

static FooCanvasItem *makeMatchItem(FooCanvasGroup *parent, ZMapDrawObjectType shape,
				    double x1, double y1, double x2, double y2,
				    GdkColor *colour, gpointer item_data, gpointer event_data) ;
static gboolean listsOverlap(GList *curr_features, GList *new_features) ;
static gboolean listsOverlapNoInterleave(GList *curr_features, GList *new_features) ;
static void reverseOffsets(GList *bumpcol_list) ;
static void moveItemsCB(gpointer data, gpointer user_data) ;
static void moveItemCB(gpointer data, gpointer user_data) ;
static void freeExtraItems(gpointer data, gpointer user_data) ;
static void showItems(gpointer data, gpointer user_data) ;
static gboolean removeNameListsByRange(GList **names_list, int start, int end) ;
static void testRangeCB(gpointer data, gpointer user_data) ;
static void hideItemsCB(gpointer data, gpointer user_data_unused) ;
static void hashDataDestroyCB(gpointer data) ;
static void listDataDestroyCB(gpointer data, gpointer user_data) ;
static void getListFromHash(gpointer key, gpointer value, gpointer user_data) ;
static void setStyleBumpCB(ZMapFeatureTypeStyle style, gpointer user_data) ;
static gboolean bumpBackgroundEventCB(FooCanvasItem *item, GdkEvent *event, gpointer data) ;
static gboolean bumpBackgroundDestroyCB(FooCanvasItem *feature_item, gpointer data) ;



static void remove3Frame(ZMapWindow window) ;
static void remove3FrameCol(FooCanvasGroup *container, FooCanvasPoints *points, 
                            ZMapContainerLevelType level, gpointer user_data) ;

static void redraw3FrameNormal(ZMapWindow window) ;
static void redraw3FrameCol(FooCanvasGroup *container, FooCanvasPoints *points, 
                            ZMapContainerLevelType level, gpointer user_data) ;
static void createSetColumn(gpointer data, gpointer user_data) ;
static void drawSetFeatures(GQuark key_id, gpointer data, gpointer user_data) ;

static void redrawAs3Frames(ZMapWindow window) ;
static void redrawAs3FrameCols(FooCanvasGroup *container, FooCanvasPoints *points, 
                               ZMapContainerLevelType level, gpointer user_data) ;
static void create3FrameCols(gpointer data, gpointer user_data) ;
static void draw3FrameSetFeatures(GQuark key_id, gpointer data, gpointer user_data) ;
#ifdef UNUSED_FUNCTIONS
static gint compareNameToColumn(gconstpointer list_data, gconstpointer user_data) ;
#endif
static gint findItemInQueueCB(gconstpointer a, gconstpointer b) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
static void printChild(gpointer data, gpointer user_data) ;
static void printQuarks(gpointer data, gpointer user_data) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



/*! @addtogroup zmapwindow
 * @{
 *  */


/*!
 * Toggles the display of the Reading Frame columns, these columns show frame senstive
 * features in separate sets of columns according to their reading frame.
 *
 * @param window     The zmap window to display the reading frame in.
 * @return  <nothing>
 *  */
void zMapWindowToggle3Frame(ZMapWindow window)
{

  zMapWindowBusy(window, TRUE) ;

  /* Remove all col. configuration windows as columns will be destroyed/recreated and the column
   * list will be out of date. */
  zmapWindowColumnConfigureDestroy(window) ;

  /* Remove all frame sensitive cols as they must all be redisplayed. */
  remove3Frame(window) ;


  /* Now redraw the columns either as "normal" or as "3 frame" display.
   * NOTE the dna/protein call is a hack and the code needs to be changed to
   * draw the 3 frame stuff as 3 properly separate columns. */
  if (window->display_3_frame)
    {
      redraw3FrameNormal(window) ;

      zMapWindowToggleDNAProteinColumns(window, 0, 0, FALSE, TRUE, FALSE, TRUE) ;
    }
  else
    {
      redrawAs3Frames(window) ;

      //zMapWindowToggleDNAProteinColumns(window, 0, 0, FALSE, TRUE, TRUE, TRUE) ;
    }

  window->display_3_frame = !window->display_3_frame ;

  zMapWindowBusy(window, FALSE) ;

  return ;
}



/*! @} end of zmapwindow docs. */




/* Sorts the children of a group by horizontal position, sorts all children that
 * are groups and leaves children that are items untouched, this is because item children
 * are background items that do not need to be ordered.
 */
void zmapWindowCanvasGroupChildSort(FooCanvasGroup *group_inout)
{

  zMapAssert(FOO_IS_CANVAS_GROUP(group_inout)) ;

  group_inout->item_list = g_list_sort(group_inout->item_list, horizPosCompare) ;
  group_inout->item_list_end = g_list_last(group_inout->item_list);

  /* Now reset the last item as well !!!! */
  group_inout->item_list_end = g_list_last(group_inout->item_list) ;

  return ;
}



/* Set the hidden status of a column group, currently this depends on the mag setting in the
 * style for the column but we could allow the user to switch this on and off as well. */
void zmapWindowColumnSetMagState(ZMapWindow window, FooCanvasGroup *col_group)
{
  ZMapWindowItemFeatureSetData set_data ;
  ZMapFeatureTypeStyle style = NULL;

  zMapAssert(window && FOO_IS_CANVAS_GROUP(col_group)) ;

  /* These should go in container some time.... */
  set_data = g_object_get_data(G_OBJECT(col_group), ITEM_FEATURE_SET_DATA) ;
  zMapAssert(set_data) ;
  style = set_data->style ;

  /* Only check the mag factor if the column is visible. */
  if (zmapWindowItemIsShown(FOO_CANVAS_ITEM(col_group)))
    {
      double min_mag, max_mag ;

      min_mag = zMapStyleGetMinMag(style) ;
      max_mag = zMapStyleGetMaxMag(style) ;


      if (min_mag > 0.0 || max_mag > 0.0)
	{
	  double curr_zoom ;

	  curr_zoom = zMapWindowGetZoomMagnification(window) ;

	  if ((min_mag && curr_zoom < min_mag)
	      || (max_mag && curr_zoom > max_mag))
	    {
	      zmapWindowColumnHide(col_group) ;
	    }
	  else
	    {
	      zmapWindowColumnShow(col_group) ;
	    }
	}
    }

  return ;
}


/* These next two calls should be followed in the user's code
 * by a call to zmapWindowNewReposition()...
 * Doing 
 *  g_list_foreach(columns_to_hide, call_column_hide, NULL);
 *  zmapWindowNewReposition(window);
 * Is a lot quicker than putting the Reposition call in these.
 */
void zmapWindowColumnHide(FooCanvasGroup *column_group)
{
  zMapAssert(column_group && FOO_IS_CANVAS_GROUP(column_group)) ;

  zmapWindowContainerSetVisibility(column_group, FALSE);

  return ;
}

void zmapWindowColumnShow(FooCanvasGroup *column_group)
{
  zMapAssert(column_group && FOO_IS_CANVAS_GROUP(column_group)) ;

  zmapWindowContainerSetVisibility(column_group, TRUE);

  return ;
}



/* Bumps either the whole column represented by column_item, or if the item is a feature item
 * in the column then bumps just features in that column that share the same style. This
 * allows the user control over whether to bump all features in a column or just some features
 * in a column.
 * 
 * NOTE: this function bumps an individual column but it DOES NOT move any other columns,
 * to do that you need to use zmapWindowColumnReposition(). The split is made because
 * we don't always want to reposition all the columns following a bump, e.g. when we
 * are creating a zmap window.
 *  */
void zmapWindowColumnBump(FooCanvasItem *column_item, ZMapStyleOverlapMode bump_mode)
{
  BumpColStruct bump_data = {NULL} ;
  FooCanvasGroup *column_features ;
  ZMapFeatureTypeStyle column_style ;
  double spacing, width ;
  ZMapFeatureTypeStyle style ;
  ZMapWindowItemFeatureType feature_type ;
  ZMapContainerLevelType container_type ;
  FooCanvasGroup *column_group =  NULL ;
  ZMapWindowItemFeatureSetData set_data ;
  gboolean column = FALSE ;
  gboolean bumped = TRUE ;


  /* Decide if the column_item is a column group or a feature within that group. */
  if ((feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(column_item), ITEM_FEATURE_TYPE)))
      != ITEM_FEATURE_INVALID)
    {
      column = FALSE ;
      column_group = zmapWindowContainerGetParentContainerFromItem(column_item) ;
    }
  else if ((container_type = zmapWindowContainerGetLevel(FOO_CANVAS_GROUP(column_item)))
	   == ZMAPCONTAINER_LEVEL_FEATURESET)
    {
      column = TRUE ;
      column_group = FOO_CANVAS_GROUP(column_item) ;
    }
  else
    zMapAssertNotReached() ;


  column_features = zmapWindowContainerGetFeatures(column_group) ;

  spacing = zmapWindowContainerGetSpacing(column_group) ;

  set_data = g_object_get_data(G_OBJECT(column_group), ITEM_FEATURE_SET_DATA) ;
  zMapAssert(set_data) ;

  zMapWindowBusy(set_data->window, TRUE) ;


  /* We may have created extra items for some bump modes to show all matches from the same query
   * etc. so now we need to get rid of them before redoing the bump. */
  if (set_data->extra_items)
    {
      g_list_foreach(set_data->extra_items, freeExtraItems, NULL) ;

      g_list_free(set_data->extra_items) ;
      set_data->extra_items = NULL ;
    }


  /* Some features may have been hidden for bumping, unhide them now. */
  if (set_data->hidden_bump_features)
    {
      g_list_foreach(column_features->item_list, showItems, set_data) ;
      set_data->hidden_bump_features = FALSE ;
    }


  /* Get the style for the selected item. */
  if (!column)
    {
      ZMapFeature feature ;

      feature = g_object_get_data(G_OBJECT(column_item), ITEM_FEATURE_DATA) ;
      zMapAssert(feature) ;

      style = zmapWindowStyleTableFind(set_data->style_table, zMapStyleGetUniqueID(feature->style)) ;
    }
  else
    style = set_data->style ;


  /* Set bump mode in the style. */
  zMapStyleSetOverlapMode(style, bump_mode) ;


  /* If user clicked on the column, not a feature within a column then we need to bump all styles
   * and features within the column, otherwise we just bump the specific features. */
  column_style = zmapWindowContainerGetStyle(column_group) ;
  if (column)
    {
      zmapWindowStyleTableForEach(set_data->style_table, setStyleBumpCB, GINT_TO_POINTER(bump_mode)) ;

      bump_data.bump_all = TRUE ;
    }


  /* All bump modes except ZMAPOVERLAP_COMPLEX share common data/code as they are essentially
   * simple variants. The complex mode requires more processing so uses its own structs/lists. */
  bump_data.bumped_style = style ;
  width = zMapStyleGetWidth(style) ;
  bump_data.incr = width + spacing ;

  switch (bump_mode)
    {
    case ZMAPOVERLAP_COMPLETE:
      bump_data.incr = 0.0 ;
      break ;
    case ZMAPOVERLAP_SIMPLE:
      break ;
    case ZMAPOVERLAP_POSITION:
      bump_data.pos_hash = g_hash_table_new_full(NULL, NULL, /* NULL => use direct hash */
						 NULL, hashDataDestroyCB) ;
      break ;
    case ZMAPOVERLAP_OVERLAP:
      break ;
    case ZMAPOVERLAP_NAME:
      bump_data.pos_hash = g_hash_table_new_full(NULL, NULL, /* NULL => use direct hash */
						 NULL, hashDataDestroyCB) ;
      break ;
    case ZMAPOVERLAP_ITEM_OVERLAP:
      break;
    case ZMAPOVERLAP_COMPLEX:
    case ZMAPOVERLAP_COMPLEX_RANGE:
    case ZMAPOVERLAP_NO_INTERLEAVE:
    case ZMAPOVERLAP_ENDS_RANGE:
      {
	ComplexBumpStruct complex = {NULL} ;
	GList *names_list = NULL ;
	gboolean mark_set ;

	mark_set = zmapWindowMarkIsSet(set_data->window->mark) ;

	complex.bumped_style = style ;

	if (column)
	  complex.bump_all = TRUE ;

	/* Make a hash table of feature names which are hashed to lists of features with that name. */
	complex.name_hash = g_hash_table_new_full(NULL, NULL, /* NULL => use direct hash/comparison. */
						  NULL, hashDataDestroyCB) ;

	g_list_foreach(column_features->item_list, makeNameListCB, &complex) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	g_list_foreach(column_features->item_list, makeNameListStrandedCB, &complex) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	/* Extract the lists of features into a list of lists for sorting. */
	g_hash_table_foreach(complex.name_hash, getListFromHash, &names_list) ;


	/* Sort each sub list of features by position for simpler position sorting later. */
	g_list_foreach(names_list, sortListPosition, NULL) ;


	/* Sort the top list using the combined normalised scores of the sublists so higher
	 * scoring matches come first. */

	/* Lets try different sorting for proteins vs. dna. */
	if (complex.protein)
	  names_list = g_list_sort(names_list, sortByScoreCB) ;
	else

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	  names_list = g_list_sort(names_list, sortBySpanCB) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
	names_list = g_list_sort(names_list, sortByStrandSpanCB) ;


	/* Remove any lists that do not overlap with the range set by the user. */
	if (mark_set)
	  {
	    int start, end ;

	    /* we know mark is set so no need to check result of range check. But should check
	     * that col to be bumped and mark are in same block ! */
	    zmapWindowMarkGetSequenceRange(set_data->window->mark, &start, &end) ;

	    if (bump_mode == ZMAPOVERLAP_ENDS_RANGE)
	      {
		RangeDataStruct range = {FALSE} ;

		range.start = start ;
		range.end = end ;

		names_list = g_list_sort_with_data(names_list, sortByOverlapCB, &range) ;
	      }

	    if (removeNameListsByRange(&names_list, start, end))
	      set_data->hidden_bump_features = TRUE ;
	  }

	if (!names_list)
	  {
	    bumped = FALSE ;
	  }
	else
	  {
	    /* Merge the lists into the min. number of non-overlapping lists of features arranged
	     * by name and to some extent by score. */
	    complex.window = set_data->window ;
	    complex.curr_offset = 0.0 ;
	    complex.incr = (width * COMPLEX_BUMP_COMPRESS) ;

	    if (bump_mode == ZMAPOVERLAP_COMPLEX)
	      complex.overlap_func = listsOverlap ;
	    else if (bump_mode == ZMAPOVERLAP_NO_INTERLEAVE || bump_mode == ZMAPOVERLAP_ENDS_RANGE
		     || bump_mode == ZMAPOVERLAP_COMPLEX_RANGE)
	      complex.overlap_func = listsOverlapNoInterleave ;

	    if (bump_mode == ZMAPOVERLAP_ENDS_RANGE)
	      g_list_foreach(names_list, setAllOffsetCB, &complex) ;
	    else
	      g_list_foreach(names_list, setOffsetCB, &complex) ;


	    /* we reverse the offsets for reverse strand cols so as to mirror the forward strand. */
	    set_data = g_object_get_data(G_OBJECT(column_group), ITEM_FEATURE_SET_DATA) ;
	    if (set_data->strand == ZMAPSTRAND_REVERSE)
	      reverseOffsets(complex.bumpcol_list) ;


	    /* Bump all the features to their correct offsets. */
	    g_list_foreach(complex.bumpcol_list, moveItemsCB, NULL) ;


	    /* for no interleave add background items.... */

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	    g_list_foreach(complex.bumpcol_list, addBackgrounds, &set_data->extra_items) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	    g_list_foreach(complex.bumpcol_list, addMultiBackgrounds, &set_data->extra_items) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	    g_list_foreach(complex.bumpcol_list, NEWaddMultiBackgrounds, &set_data->extra_items) ;

	  }

	/* Clear up. */
	if (complex.bumpcol_list)
	  g_list_foreach(complex.bumpcol_list, listDataDestroyCB, NULL) ;
	if (complex.name_hash)
	  g_hash_table_destroy(complex.name_hash) ;
	if (names_list)
	  g_list_free(names_list) ;
	 
	break ;
      }
    default:
      zMapAssertNotReached() ;
      break ;
    }


  /* bump all the features for all modes except complex and then clear up. */
  if (bumped)
    {
      if (bump_mode != ZMAPOVERLAP_COMPLEX && bump_mode != ZMAPOVERLAP_NO_INTERLEAVE
	  && bump_mode != ZMAPOVERLAP_ENDS_RANGE && bump_mode != ZMAPOVERLAP_COMPLEX_RANGE)
	{

	  /* Need to add data to test for correct feature here.... */

	  g_list_foreach(column_features->item_list, bumpColCB, (gpointer)&bump_data) ;

	  if (bump_mode == ZMAPOVERLAP_POSITION || bump_mode == ZMAPOVERLAP_NAME)
	    g_hash_table_destroy(bump_data.pos_hash) ;
	  else if (bump_mode == ZMAPOVERLAP_OVERLAP)
	    g_list_foreach(bump_data.pos_list, listDataDestroyCB, NULL) ;
	}

      /* Make the parent groups bounding box as large as the group.... */
#ifdef NOT_REQUIRED_AND_TIME_CONSUMING
      zmapWindowContainerSetBackgroundSize(column_group, 0.0) ;
      /* We don't set the background size like this as it results in another item_get_bounds cycle */
#endif
      /* Rely on user calling the Reposition call which ContainerExecutes and
       * also sets the scroll region so that the user can see all the canvas.
       */
    }

  zMapWindowBusy(set_data->window, FALSE) ;

  return ;
}

void zmapWindowContainerMoveEvent(FooCanvasGroup *super_root, ZMapWindow window)
{
  ContainerType type = CONTAINER_INVALID ;

  type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(super_root), CONTAINER_TYPE_KEY)) ;
  zMapAssert(type = CONTAINER_ROOT);
  /* pre callback was set to zmapWindowContainerRegionChanged */
  zmapWindowContainerExecuteFull(FOO_CANVAS_GROUP(super_root),
                                 ZMAPCONTAINER_LEVEL_FEATURESET,
                                 NULL, NULL,
                                 NULL, NULL, TRUE) ;
  return ;
}

/* Makes sure all the things that need to be redrawn when the canvas needs redrawing. */
void zmapWindowFullReposition(ZMapWindow window)
{
  FooCanvasGroup *super_root ;
  ContainerType type = CONTAINER_INVALID ;

  super_root = FOO_CANVAS_GROUP(zmapWindowFToIFindItemFull(window->context_to_item,
							   0,0,0,
							   ZMAPSTRAND_NONE, ZMAPFRAME_NONE,
							   0)) ;
  zMapAssert(super_root) ;

  type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(super_root), CONTAINER_TYPE_KEY)) ;
  zMapAssert(type = CONTAINER_ROOT) ;

  /* This could probably call the col order stuff as pre recurse function... */
  zmapWindowContainerExecuteFull(FOO_CANVAS_GROUP(super_root),
                                 ZMAPCONTAINER_LEVEL_FEATURESET,
                                 NULL,
                                 NULL,
                                 resetWindowWidthCB,
                                 window, TRUE) ;

  zmapWindowReFocusHighlights(window);

  return ;
}


/* Reset scrolled region width so that user can scroll across whole of canvas. */
void zmapWindowResetWidth(ZMapWindow window)
{
  FooCanvasGroup *root ;
  double x1, x2, y1, y2 ;
  double root_x1, root_x2, root_y1, root_y2 ;
  double scr_reg_width, root_width ;


  foo_canvas_get_scroll_region(window->canvas, &x1, &y1, &x2, &y2);

  scr_reg_width = x2 - x1 + 1.0 ;

  root = foo_canvas_root(window->canvas) ;

  foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(root), &root_x1, &root_y1, &root_x2, &root_y2) ;

  root_width = root_x2 - root_x1 + 1 ;

  if (root_width != scr_reg_width)
    {
      double excess ;

      excess = root_width - scr_reg_width ;

      x2 = x2 + excess;

      foo_canvas_set_scroll_region(window->canvas, x1, y1, x2, y2) ;
    }


  return ;
}



/* Makes sure all the things that need to be redrawn for zooming get redrawn. */
void zmapWindowDrawZoom(ZMapWindow window)
{
  FooCanvasGroup *super_root ;
  ContainerType type = CONTAINER_INVALID ;
  ZoomDataStruct zoom_data = {NULL} ;

  super_root = FOO_CANVAS_GROUP(zmapWindowFToIFindItemFull(window->context_to_item,
							   0,0,0,
							   ZMAPSTRAND_NONE, ZMAPFRAME_NONE,
							   0)) ;
  zMapAssert(super_root) ;


  type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(super_root), CONTAINER_TYPE_KEY)) ;
  zMapAssert(type = CONTAINER_ROOT) ;

  zoom_data.window = window ;
  zoom_data.zoom = zMapWindowGetZoomFactor(window) ;

  zmapWindowContainerExecuteFull(FOO_CANVAS_GROUP(super_root),
                                 ZMAPCONTAINER_LEVEL_FEATURESET,
                                 preZoomCB,
                                 &zoom_data,
                                 resetWindowWidthCB,
                                 window, TRUE) ;

  return ;
}


/* Some features have different widths according to their score, this helps annotators
 * guage whether something like an alignment is worth taking notice of.
 * 
 * Currently we only implement the algorithm given by acedb's Score_by_width but it maybe
 * that we will need to do others to support their display.
 *
 * I've done this because for James Havana DBs all methods that mention score have:
 *
 * Score_by_width	
 * Score_bounds	 70.000000 130.000000
 *
 * (the bounds are different for different data sources...)
 * 
 * The interface is slightly tricky here in that we use the style->width to calculate
 * width, not (x2 - x1), hence x2 is only used to return result.
 * 
 */
void zmapWindowGetPosFromScore(ZMapFeatureTypeStyle style, 
			       double score,
			       double *curr_x1_inout, double *curr_x2_out)
{
  double dx = 0.0 ;
  double numerator, denominator ;
  double max_score, min_score ;

  zMapAssert(style && curr_x1_inout && curr_x2_out) ;

  min_score = zMapStyleGetMinScore(style) ;
  max_score = zMapStyleGetMaxScore(style) ;

  /* We only do stuff if there are both min and max scores to work with.... */
  if (max_score && min_score)
    {
      double start = *curr_x1_inout ;
      double width, half_width, mid_way ;

      width = zMapStyleGetWidth(style) ;

      half_width = width / 2 ;
      mid_way = start + half_width ;

      numerator = score - min_score ;
      denominator = max_score - min_score ;

      if (denominator == 0)				    /* catch div by zero */
	{
	  if (numerator < 0)
	    dx = 0.25 ;
	  else if (numerator > 0)
	    dx = 1 ;
	}
      else
	{
	  dx = 0.25 + (0.75 * (numerator / denominator)) ;
	}

      if (dx < 0.25)
	dx = 0.25 ;
      else if (dx > 1)
	dx = 1 ;
      
      *curr_x1_inout = mid_way - (half_width * dx) ;
      *curr_x2_out = mid_way + (half_width * dx) ;
    }

  return ;
}



/* Sort the column list according to the feature names order. */
void zmapWindowSortCols(GList *col_names, FooCanvasGroup *col_container, gboolean reverse)

{
#ifdef RDS_DONT_INCLUDE
  FooCanvasGroup *column_group ;
  GList *column_list ;
  GList *next ;
  int last_position ;

  zMapAssert(col_names && FOO_IS_CANVAS_GROUP(col_container)) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  g_list_foreach(col_names, printQuarks, NULL) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  column_group = zmapWindowContainerGetFeatures(col_container) ;
  column_list = column_group->item_list ;

  if (reverse)
    next = g_list_last(col_names) ;
  else
    next = g_list_first(col_names) ;

  last_position = 0 ;

  do
    {
      GList *column_element ;

      if ((column_element = g_list_find_custom(column_list, next->data, compareNameToColumn)))
	{
	  FooCanvasGroup *col_group ;

	  col_group = FOO_CANVAS_GROUP(column_element->data) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	  printf("moving column %s to position %d\n",
		 g_quark_to_string(GPOINTER_TO_INT(next->data)), last_position) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	  column_list = zMap_g_list_move(column_list, column_element->data, last_position) ;

	  last_position++ ;
	}

      if (reverse)
	next = g_list_previous(next) ;
      else
	next = g_list_next(next) ;

    } while (next) ;

  /* Note how having rearranged the list we MUST reset the cached last item pointer !! */
  column_group->item_list = column_list ;
  column_group->item_list_end = g_list_last(column_group->item_list) ;

#endif
  return ;
}





/* 
 *                Internal routines.
 */


/* Is called once for each item in a column and sets the horizontal position of that
 * item under various "bumping" modes. */
static void bumpColCB(gpointer data, gpointer user_data)
{
  FooCanvasItem *item = (FooCanvasItem *)data ;
  BumpCol bump_data   = (BumpCol)user_data ;
  ZMapWindowItemFeatureType item_feature_type ;
  ZMapFeature feature ;
  double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0 ;
  gpointer y1_ptr = 0 ;
  gpointer key = NULL, value = NULL ;
  double offset = 0.0, dx = 0.0 ;


  if(!(zmapWindowItemIsShown(item)))
    return ;

  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
  zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE || item_feature_type == ITEM_FEATURE_PARENT) ;

  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;

  /* If we not bumping all features, then only bump the features who have the bumped style. */
  {
    FooCanvasGroup *column_group =  NULL ;
    ZMapWindowItemFeatureSetData set_data ;
    ZMapFeatureTypeStyle style ;

    column_group = zmapWindowContainerGetParentContainerFromItem(item) ;

    set_data = g_object_get_data(G_OBJECT(column_group), ITEM_FEATURE_SET_DATA) ;
    zMapAssert(set_data) ;

    style = zmapWindowStyleTableFind(set_data->style_table, zMapStyleGetUniqueID(feature->style)) ;

    if (!(bump_data->bump_all) && bump_data->bumped_style != style)
      return ;
  }

  /* x1, x2 always needed so might as well get y coords as well because foocanvas will have
   * calculated them anyway. */
  foo_canvas_item_get_bounds(item, &x1, &y1, &x2, &y2) ;

  switch (zMapStyleGetOverlapMode(bump_data->bumped_style))
    {
    case ZMAPOVERLAP_POSITION:
      {
	/* Bump features over if they have the same start coord. */

	y1_ptr = GINT_TO_POINTER((int)y1) ;

	if (g_hash_table_lookup_extended(bump_data->pos_hash, y1_ptr, &key, &value))
	  {
	    offset = *(double *)value + bump_data->incr ;
	    
	    *(double *)value = offset ;
	  }
	else
	  {
	    value = g_new(double, 1) ;
	    *(double *)value = offset ;
	    
	    g_hash_table_insert(bump_data->pos_hash, y1_ptr, value) ;
	  }
	
	break ;
      }


    case ZMAPOVERLAP_OVERLAP:
      {
	/* Bump features over if they overlap at all. */
	BumpColRange new_range ;

	new_range = g_new0(BumpColRangeStruct, 1) ;
	new_range->y1 = y1 ;
	new_range->y2 = y2 ;
	new_range->offset = 0.0 ;
	new_range->incr = bump_data->incr ;

	g_list_foreach(bump_data->pos_list, compareListOverlapCB, new_range) ;

	bump_data->pos_list = g_list_append(bump_data->pos_list, new_range) ;

	offset = new_range->offset ;
	
	break ;
      }
    case ZMAPOVERLAP_NAME:
      {
	/* Bump features that have the same name, i.e. are the same feature, so that each
	 * vertical subcolumn is composed of just one feature in different positions. */
	ZMapFeature feature ;

	feature = (ZMapFeature)(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA)) ;

	if (g_hash_table_lookup_extended(bump_data->pos_hash,
					 GINT_TO_POINTER(feature->original_id), &key, &value))
	  {
	    offset = *(double *)value ;
	  }
	else
	  {
	    offset = bump_data->offset ;

	    value = g_new(double, 1) ;
	    *(double *)value = offset ;
	    
	    g_hash_table_insert(bump_data->pos_hash, GINT_TO_POINTER(feature->original_id), value) ;

	    bump_data->offset += bump_data->incr ;
	  }
	
	break ;
      }
    case ZMAPOVERLAP_COMPLETE:
    case ZMAPOVERLAP_SIMPLE:
      {
	offset = bump_data->offset ;
	bump_data->offset += bump_data->incr ;

	break ;
      }
    case ZMAPOVERLAP_ITEM_OVERLAP:
      {
	/* Bump features over if they overlap at all. */
	BumpColRange new_range ;

	new_range = g_new0(BumpColRangeStruct, 1) ;
	new_range->y1 = y1 ;
	new_range->y2 = y2 ;
	new_range->offset = 0.0 ;
	new_range->incr = x2 - x1 + 1.0;//bump_data->incr ;

	g_list_foreach(bump_data->pos_list, compareListOverlapCB, new_range) ;

	bump_data->pos_list = g_list_append(bump_data->pos_list, new_range) ;

	offset = new_range->offset ;
	
	break ;
      }
      break;
    default:
      zMapAssertNotReached() ;
      break ;
    }


  /* Some features are drawn with different widths to indicate things like score. In this case
   * their offset needs to be corrected to place them centrally. (We always do this which
   * seems inefficient but its a toss up whether it would be quicker to test (dx == 0). */
  dx = (zMapStyleGetWidth(feature->style) - (x2 - x1)) / 2 ;
  offset += dx ;

  /* Not having something like this appears to be part of the cause of the oddness. Not all though */
  if(offset < 0.0)
    offset = 0.0;

  /* This does a item_get_bounds... don't we already have them? 
   * Might be missing something though. Why doesn't the "Bump" bit calculate offsets? */
  my_foo_canvas_item_goto(item, &(offset), NULL) ; 

  return ;
}


/* GDestroyNotify() function for freeing the data part of a hash association. */
static void hashDataDestroyCB(gpointer data)
{
  g_free(data) ;

  return ;
}


/* GFunc callback func, called from g_list_foreach_find() to test whether current
 * element matches supplied overlap coords. */
static void compareListOverlapCB(gpointer data, gpointer user_data)
{
  BumpColRange curr_range = (BumpColRange)data ;
  BumpColRange new_range = (BumpColRange)user_data ;

  /* Easier to test no overlap and negate. */
  if (!(new_range->y1 > curr_range->y2 || new_range->y2 < curr_range->y1))
    {
      new_range->offset = curr_range->offset + curr_range->incr ;
    }

  return ;
}


/* GFunc callback func, called from g_list_foreach_find() to free list resources. */
static void listDataDestroyCB(gpointer data, gpointer user_data)
{
  g_free(data) ;

  return ;
}



/* MAY NEED TO INGNORE BACKGROUND BOXES....WHICH WILL BE ITEMS.... */

/* A GCompareFunc() called from g_list_sort(), compares groups on their horizontal position. */
static gint horizPosCompare(gconstpointer a, gconstpointer b)
{
  gint result = 0 ;
  FooCanvasGroup *group_a = (FooCanvasGroup *)a, *group_b = (FooCanvasGroup *)b ;

  if (!FOO_IS_CANVAS_GROUP(group_a) || !FOO_IS_CANVAS_GROUP(group_b))
    {
      result = 0 ;
    }
  else
    {
      if (group_a->xpos < group_b->xpos)
	result = -1 ;
      else if (group_a->xpos > group_b->xpos)
	result = 1 ;
      else
	result = 0 ;
    }

  return result ;
}




#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
static void printItem(FooCanvasItem *item, int indent, char *prefix)
{
  int i ;

  for (i = 0 ; i < indent ; i++)
    {
      printf("  ") ;
    }
  printf("%s", prefix) ;

  zmapWindowPrintItemCoords(item) ;

  return ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */





#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* UNUSED */
static void addToList(gpointer data, gpointer user_data)
{
  FooCanvasGroup *grp = FOO_CANVAS_GROUP(data);
  GList **list = (GList **)user_data;
  *list = g_list_append(*list, grp);
  return ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



/* We need the window in here.... */
/* GFunc to call on potentially all container groups.
 */
static void preZoomCB(FooCanvasGroup *data, FooCanvasPoints *points, 
                      ZMapContainerLevelType level, gpointer user_data)
{
  FooCanvasGroup *container = (FooCanvasGroup *)data ;
  ZoomData zoom_data = (ZoomData)user_data ;

  switch(level)
    {
    case ZMAPCONTAINER_LEVEL_FEATURESET:
      columnZoomChanged(container, zoom_data->zoom, zoom_data->window) ;
      break ;
    default:
      break ;
    }

  return ;
}



/* A version of zmapWindowResetWidth which uses the points from the recursion to set the width */
static void resetWindowWidthCB(FooCanvasGroup *data, FooCanvasPoints *points, 
                               ZMapContainerLevelType level, gpointer user_data)
{
  ZMapWindow window = NULL;
  double x1, x2, y1, y2 ;       /* scroll region positions */
  double scr_reg_width, root_width ;

  if(level == ZMAPCONTAINER_LEVEL_ROOT)
    {
      window = (ZMapWindow)user_data ;

      foo_canvas_get_scroll_region(window->canvas, &x1, &y1, &x2, &y2);

      scr_reg_width = x2 - x1 + 1.0 ;

      root_width = points->coords[2] - points->coords[0] + 1.0 ;

      if (root_width != scr_reg_width)
        {
          double excess ;
          
          excess = root_width - scr_reg_width ;
          /* the spacing should be a border width from somewhere. */
          x2 = x2 + excess + window->config.strand_spacing;

          foo_canvas_set_scroll_region(window->canvas, x1, y1, x2, y2) ;
        }
    }
  
  return ;
}


static void columnZoomChanged(FooCanvasGroup *container, double new_zoom, ZMapWindow window)
{

  zmapWindowColumnSetMagState(window, container) ;

  return ;
}






/* 
 *                       Functions for the complex bump...
 * 
 * If you alter this code you should note the careful use of GList**, list addresses
 * can change so its vital to return the address of the list pointer, not the list
 * pointer itself.
 * 
 */

/* Is called once for each item in a column and constructs a hash table in which each entry
 * is a list of all the features with the same name. */
static void makeNameListCB(gpointer data, gpointer user_data)
{
  FooCanvasItem *item = (FooCanvasItem *)data ;
  ComplexBump complex = (ComplexBump)user_data ;
  GHashTable *name_hash = complex->name_hash ;
  ZMapWindowItemFeatureType item_feature_type ;
  ZMapFeatureTypeStyle style ;
  ZMapFeature feature ;
  gpointer key = NULL, value = NULL ;
  GList **feature_list_ptr ;
  GList *feature_list ;

  /* don't bother if something is not displayed. */
  if(!(zmapWindowItemIsShown(item)))
    return ;

  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
  zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE || item_feature_type == ITEM_FEATURE_PARENT) ;

  style = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_ITEM_STYLE) ;
  zMapAssert(style) ;

  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;

  /* If we are not bumping all the features then only bump those for the specified style. */
  {
    FooCanvasGroup *column_group =  NULL ;
    ZMapWindowItemFeatureSetData set_data ;
    ZMapFeatureTypeStyle style ;

    column_group = zmapWindowContainerGetParentContainerFromItem(item) ;

    set_data = g_object_get_data(G_OBJECT(column_group), ITEM_FEATURE_SET_DATA) ;
    zMapAssert(set_data) ;

    style = zmapWindowStyleTableFind(set_data->style_table, zMapStyleGetUniqueID(feature->style)) ;

    if (!(complex->bump_all) && complex->bumped_style != style)
      return ;
  }



  /* Try doing this here.... */
  if (feature->type == ZMAPFEATURE_ALIGNMENT
      && feature->feature.homol.type == ZMAPHOMOL_X_HOMOL)
    complex->protein = TRUE ;



  /* If a list of features with this features name already exists in the hash then simply
   * add this feature to it. Otherwise make a new entry in the hash. */
  if (g_hash_table_lookup_extended(name_hash,
				   GINT_TO_POINTER(feature->original_id), &key, &value))
    {
      feature_list_ptr = (GList **)value ;

      feature_list = *feature_list_ptr ;

      feature_list = g_list_append(feature_list, item) ;

      *feature_list_ptr = feature_list ;
    }
  else
    {
      feature_list_ptr = (GList **)g_malloc0(sizeof(GList **)) ;

      feature_list = NULL ;

      feature_list = g_list_append(feature_list, item) ;

      *feature_list_ptr = feature_list ;

      g_hash_table_insert(name_hash, GINT_TO_POINTER(feature->original_id), feature_list_ptr) ;
    }

  
  return ;
}



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* UNUSED */
static void makeNameListStrandedCB(gpointer data, gpointer user_data)
{
  FooCanvasItem *item = (FooCanvasItem *)data ;
  ComplexBump complex = (ComplexBump)user_data ;
  GHashTable *name_hash = complex->name_hash ;
  ZMapWindowItemFeatureType item_feature_type ;
  ZMapFeatureTypeStyle style ;
  ZMapFeature feature ;
  gpointer key = NULL, value = NULL ;
  GList **feature_list_ptr ;
  GList *feature_list ;
  char *feature_strand_name = NULL ;
  GQuark name_quark ;

  /* don't bother if something is not displayed. */
  if(!(zmapWindowItemIsShown(item)))
    return ;

  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
  zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE || item_feature_type == ITEM_FEATURE_PARENT) ;

  style = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_ITEM_STYLE) ;
  zMapAssert(style) ;


  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;

  /* If we not bumping all features, then only bump the features who have the bumped style. */
  {
    FooCanvasGroup *column_group =  NULL ;
    ZMapWindowItemFeatureSetData set_data ;
    ZMapFeatureTypeStyle style ;

    column_group = zmapWindowContainerGetParentContainerFromItem(item) ;

    set_data = g_object_get_data(G_OBJECT(column_group), ITEM_FEATURE_SET_DATA) ;
    zMapAssert(set_data) ;

    style = zmapWindowStyleTableFind(set_data->style_table, zMapStyleGetUniqueID(feature->style)) ;

    if (!(bump_data->bump_all) && bump_data->bumped_style != style)
      return ;
  }


  /* Try doing this here.... */
  if (feature->type == ZMAPFEATURE_ALIGNMENT
      && feature->feature.homol.type == ZMAPHOMOL_X_HOMOL)
    complex->protein = TRUE ;


  /* Make a stranded name... */
  feature_strand_name = g_strdup_printf("%s-%s",
					g_quark_to_string(feature->original_id),
					(feature->strand == ZMAPSTRAND_NONE ? "none"
					 : feature->strand == ZMAPSTRAND_FORWARD ? "+"
					 : "-")) ;
  name_quark = g_quark_from_string(feature_strand_name) ;


  /* If a list of features with this features name already exists in the hash then simply
   * add this feature to it. Otherwise make a new entry in the hash. */
  if (g_hash_table_lookup_extended(name_hash,
				   GINT_TO_POINTER(name_quark), &key, &value))
    {
      feature_list_ptr = (GList **)value ;

      feature_list = *feature_list_ptr ;

      feature_list = g_list_append(feature_list, item) ;

      *feature_list_ptr = feature_list ;
    }
  else
    {
      feature_list_ptr = (GList **)g_malloc0(sizeof(GList **)) ;

      feature_list = NULL ;

      feature_list = g_list_append(feature_list, item) ;

      *feature_list_ptr = feature_list ;

      g_hash_table_insert(name_hash, GINT_TO_POINTER(name_quark), feature_list_ptr) ;
    }

  g_free(feature_strand_name) ;

  
  return ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */




/* GHFunc(), gets the list stored in the hash and adds it to the supplied list of lists.
 * Essentially this routine makes a list out of a hash. */
static void getListFromHash(gpointer key, gpointer value, gpointer user_data)
{
  GList **this_list_ptr = (GList **)value ;
  GList **names_list_out = (GList **)user_data ;
  GList *names_list = *names_list_out ;

  names_list = g_list_append(names_list, this_list_ptr) ;

  *names_list_out = names_list ;

  return ;
}


/* GFunc() to sort each sub list of features in this list by position. */
static void sortListPosition(gpointer data, gpointer user_data)
{
  GList **name_list_ptr = (GList **)data ;
  GList *name_list = *name_list_ptr ;			    /* Single list of named features. */

  /* sort the sublist of features. */
  name_list = zmapWindowItemSortByPostion(name_list) ;

  *name_list_ptr = name_list ;

  return ;
}



/* This may be a candidate to be a more general function to filter sets of features by range....
 * Checks all the features in each feature list within names_list to see if they overlap the
 * given feature item, removes from names_list any that don't.
 * Returns TRUE if any items were _removed_. */
static gboolean removeNameListsByRange(GList **names_list, int start, int end)
{
  gboolean result = FALSE ;
  GList *new_list, *list_item ;
  RangeDataStruct range_data ;

  range_data.overlap = FALSE ;

  range_data.start = start ;
  range_data.end = end ;

  new_list = NULL ;
  list_item = *names_list ;
  while (list_item)
    {
      /* testRangeCB() sets overlap to TRUE when it detects one. */
      g_list_foreach(*((GList **)(list_item->data)), testRangeCB, &range_data) ;

      /* Tricky list logic here, g_list_delete_link() has the effect of moving us on to the next
       * list item so we mustn't do g_list_next() as well. */
      if (!range_data.overlap)
	{
	  /* Now hide these items as they don't overlap... */
	  g_list_foreach(*((GList **)(list_item->data)), hideItemsCB, NULL) ;

	  list_item = g_list_delete_link(list_item, list_item) ;

	  result = TRUE ;
	}
      else
	{
	  new_list = list_item ;			    /* Hang on to valid list ptr. */
	  list_item = g_list_next(list_item) ;
	  range_data.overlap = FALSE ;
	}
    }


  *names_list = g_list_first(new_list) ;		    /* Hand back the _start_ of the list. */

  return result ;
}



/* GFunc() to test whether a set of features overlaps the range given in user_data, sets
 * TRUE when an overlap is detected. Features that don't ovelap are hidden. */
static void testRangeCB(gpointer data, gpointer user_data)
{
  FooCanvasItem *item = (FooCanvasItem *)data ;
  RangeData range_data = (RangeData)user_data ;
  ZMapFeature feature ;

  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;

  if (!(feature->x1 > range_data->end || feature->x2 < range_data->start))
    range_data->overlap = TRUE ;

  return ;
}


/* GFunc() to hide all items in a list. */
static void hideItemsCB(gpointer data, gpointer user_data_unused)
{
  FooCanvasItem *item = (FooCanvasItem *)data ;

  foo_canvas_item_hide(item) ;

  return ;
}




#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* UNUSED */
/* GFunc() to add a background item to each set of items in a list. */
static void addBackgrounds(gpointer data, gpointer user_data)
{
  ComplexCol col_data = (ComplexCol)data ;
  GList **extras_ptr = (GList **)user_data ;
  GList *extra_items = *extras_ptr ;
  GList *name_list = col_data->feature_list ;			    /* Single list of named features. */
  GList *first, *last ;
  FooCanvasItem *first_item, *last_item, *background ;
  static gboolean colour_init = FALSE ;
  static GdkColor perfect, colinear, noncolinear ;
  char *perfect_colour = ZMAP_WINDOW_MATCH_PERFECT ;
  char *colinear_colour = ZMAP_WINDOW_MATCH_COLINEAR ;
  char *noncolinear_colour = ZMAP_WINDOW_MATCH_NOTCOLINEAR ;
  double x1, y1, x2, y2 ;

  if (!colour_init)
    {
      gdk_color_parse(perfect_colour, &perfect) ;
      gdk_color_parse(colinear_colour, &colinear) ;
      gdk_color_parse(noncolinear_colour, &noncolinear) ;

      colour_init = TRUE ;
    }


  /* SHOULD FILTER ON TYPE AS WELL...WE DON'T WANT ALL TYPES TO HAVE BACKGROUNDS....???? */
  if (g_list_length(name_list) > 1)
    {
      GList *backgrounds = NULL ;
      GList *list_item ;
      GQuark first_id = 0 ;
      ZMapFeatureTypeStyle first_style = NULL ;
      double mid, half_width = COLUMN_BACKGROUND_SPACING ;

      first = last = NULL ;
      list_item = g_list_first(name_list) ;

      while (list_item)
	{
	  FooCanvasItem *item ;
	  ZMapFeature feature ;
	  GQuark feature_id = 0 ;

	  item = (FooCanvasItem *)list_item->data ;
	  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
	  zMapAssert(feature) ;
	  feature_id = feature->original_id ;

	  if (!first)
	    {
	      first = last = list_item ;
	      first_item = (FooCanvasItem *)first->data ;
	      
	      /* N.B. this works because the items have already been positioned in the column. */
	      foo_canvas_item_get_bounds(first_item, &x1, &y1, &x2, NULL) ;
	      mid = (x2 + x1) * 0.5 ;

	      first_id = feature_id ;
	      first_style = feature->style ;
	      list_item = g_list_next(list_item) ;
	    }
	  else
	    {
	      if (feature_id == first_id)
		{
		  last = list_item ;
		  list_item = g_list_next(list_item) ;
		}

	      if (feature_id != first_id || !list_item)
		{
		  /* Only do boxes where there is more than one item. */
		  if (first != last)
		    {
		      ZMapWindowItemFeatureBumpData bump_data ;

		      last_item = (FooCanvasItem *)last->data ;

		      foo_canvas_item_get_bounds(last_item, NULL, NULL, NULL, &y2);

		      bump_data = g_new0(ZMapWindowItemFeatureBumpDataStruct, 1) ;
		      bump_data->first_item = first_item ;
		      bump_data->feature_id = first_id ;
		      bump_data->style = first_style ;

		      background = zMapDrawBox(FOO_CANVAS_GROUP(first_item->parent),
					       (mid - half_width), y1, (mid + half_width), y2,
					       &perfect, &perfect, 0.0) ;

		      zmapWindowLongItemCheck(col_data->window->long_items, background, y1, y2) ;

		      extra_items = g_list_append(extra_items, background) ;

		      g_object_set_data(G_OBJECT(background), ITEM_FEATURE_TYPE,
					GINT_TO_POINTER(ITEM_FEATURE_GROUP_BACKGROUND)) ;
		      g_object_set_data(G_OBJECT(background), ITEM_FEATURE_BUMP_DATA, bump_data) ;
		      g_signal_connect(GTK_OBJECT(background), "event",
				       GTK_SIGNAL_FUNC(bumpBackgroundEventCB), col_data->window) ;
		      g_signal_connect(GTK_OBJECT(background), "destroy",
				       GTK_SIGNAL_FUNC(bumpBackgroundDestroyCB), col_data->window) ;


		      /* Make sure these backgrounds are drawn behind the features items. */
		      foo_canvas_item_lower_to_bottom(background) ;

		      backgrounds = g_list_append(backgrounds, background) ;
		    }
		  
		  /* N.B. ensure we don't move on in the list so that we pick up this item
		   * as the next "first" item... */
		  first = NULL ;
		}

	    }
	}


      /* Need to add all backgrounds to col list here and lower them in group.... */
      name_list = g_list_concat(name_list, backgrounds) ;


      col_data->feature_list = name_list ;

      *extras_ptr = extra_items ;
    }

  return ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */




/* GFunc() to add background items indicating goodness of match to each set of items in a list.
 * 
 * THIS FUNCTION HAS SOME HARD CODED STUFF AND QUITE A BIT OF CODE THAT CAN BE REFACTORED TO
 * SIMPLIFY THINGS, I'VE CHECKED IT IN NOW BECAUSE I WANT IT TO BE IN CVS OVER XMAS.
 * 
 *  */
static void NEWaddMultiBackgrounds(gpointer data, gpointer user_data)
{
  ComplexCol col_data = (ComplexCol)data ;
  GList **extras_ptr = (GList **)user_data ;
  GList *extra_items = *extras_ptr ;
  GList *name_list = col_data->feature_list ;			    /* Single list of named features. */
  static gboolean colour_init = FALSE ;
  static GdkColor perfect, colinear, noncolinear ;
  char *perfect_colour = ZMAP_WINDOW_MATCH_PERFECT ;
  char *colinear_colour = ZMAP_WINDOW_MATCH_COLINEAR ;
  char *noncolinear_colour = ZMAP_WINDOW_MATCH_NOTCOLINEAR ;
  GList *backgrounds = NULL ;
  GList *list_item ;
  double width, mid, half_width ;
  FooCanvasItem *item ;
  FooCanvasItem *background ;
  ZMapFeature prev_feature = NULL, curr_feature = NULL ;
  ZMapFeatureTypeStyle prev_style = NULL, curr_style = NULL ;
  GQuark prev_id = 0, curr_id = 0 ;
  int prev_end = 0, curr_start = 0 ;
  double x1, x2, prev_y2, curr_y1, curr_y2 ;
  double start_x1, start_x2, end_x1, end_x2 ;



  if (!colour_init)
    {
      gdk_color_parse(perfect_colour, &perfect) ;
      gdk_color_parse(colinear_colour, &colinear) ;
      gdk_color_parse(noncolinear_colour, &noncolinear) ;

      colour_init = TRUE ;
    }


  /* Get the first item. */
  list_item = g_list_first(name_list) ;
  item = (FooCanvasItem *)list_item->data ;
  foo_canvas_item_get_bounds(item, &x1, &curr_y1, &x2, &curr_y2) ;
  curr_feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(curr_feature) ;
  curr_id = curr_feature->original_id ;
  curr_style = curr_feature->style ;


  /* Calculate mid point of this column of matches from the first item,
   * N.B. this works because the items have already been positioned in the column. */
  mid = (x2 + x1) * 0.5 ;

  width = zMapStyleGetWidth(curr_style) ;
  half_width = width * 0.5 ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  start_x1 = end_x1 = (mid - (half_width - 2)) ;
  start_x2 = end_x2 = (mid + (half_width - 2)) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

 
  /* CODE HERE WORKS BUT IS NOT CORRECT IN THAT IT IS USING THE CANVAS BOX COORDS WHEN IT
   * SHOULD BE USING THE FEATURE->X1/X2 COORDS...i'LL FIX IT LATER... */
  do
    {
      unsigned int match_threshold ;
      GdkColor *box_colour ;
      ZMapWindowItemFeatureBumpData bump_data ;
      int start, end ;


      /* mark start of curr item if its incomplete. */
      if (curr_feature->type == ZMAPFEATURE_ALIGNMENT)
	{
	  if (curr_feature->feature.homol.y1 > curr_feature->feature.homol.y2)
	    {
	      start = curr_feature->feature.homol.y2 ;
	      end = curr_feature->feature.homol.y1 ;
	    }
	  else
	    {
	      start = curr_feature->feature.homol.y1 ;
	      end = curr_feature->feature.homol.y2 ;
	    }
	  if (start > 1)
	    {
	      double test_1, test_2 ;

	      bump_data = g_new0(ZMapWindowItemFeatureBumpDataStruct, 1) ;
	      bump_data->first_item = item ;
	      bump_data->feature_id = curr_id ;
	      bump_data->style = curr_style ;


	      foo_canvas_item_get_bounds(item, &test_1, NULL, &test_2, NULL) ;



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	      if (zmapWindowItemIsCompound(item))
		{
		  true_item = zmapWindowItemGetNthChild(FOO_CANVAS_GROUP(item), 0) ;
		}
	      else
		true_item = item ;

              g_object_get(G_OBJECT(true_item),
                           "x1", &start_x1,
                           "x2", &start_x2,
                           NULL) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	      itemGetCoords(item, &start_x1, &start_x2) ;


	      box_colour = &noncolinear ;
	      background = makeMatchItem(FOO_CANVAS_GROUP(item->parent), ZMAPDRAW_OBJECT_BOX,
					 start_x1, curr_y1,
					 start_x2, (curr_y1 + 10),
					 box_colour, bump_data, col_data->window) ;

	      backgrounds = g_list_append(backgrounds, background) ;
		      
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	      zmapWindowLongItemCheck(col_data->window->long_items, background, curr_y2, curr_y1) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	      extra_items = g_list_append(extra_items, background) ;
	    }
	}


      /* make curr in to prev item */
      prev_feature = curr_feature ;
      prev_id = curr_feature->original_id ;

      if (col_data->window->revcomped_features)
	prev_end = curr_feature->feature.homol.y1 ;
      else
	prev_end = curr_feature->feature.homol.y2 ;

      prev_style = curr_feature->style ;
      prev_y2 = curr_y2 ;


      /* Mark in between matches for their colinearity. */
      while ((list_item = g_list_next(list_item)))
	{
	  /* get new curr item */
	  item = (FooCanvasItem *)list_item->data ;
	  foo_canvas_item_get_bounds(item, NULL, &curr_y1, NULL, &curr_y2);

	  curr_feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
	  zMapAssert(curr_feature) ;
	  curr_id = curr_feature->original_id ;

	  if (col_data->window->revcomped_features)
	    curr_start = curr_feature->feature.homol.y2 ;
	  else
	    curr_start = curr_feature->feature.homol.y1 ;


	  curr_style = curr_feature->style ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	  printf("%s :  %d, %d\n", g_quark_to_string(feature->original_id), feature->x1, feature->x2) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


	  /* We only do aligns (remember that there can be different types in a single col)
	   * and only those for which joining of homols was requested. */
	  if (curr_feature->type != ZMAPFEATURE_ALIGNMENT
	      || !(zMapStyleGetJoinAligns(curr_style, &match_threshold)))
	    continue ;


	  /* If we've changed to a new match then break, otherwise mark for colinearity. */
	  if (curr_id != prev_id)
	    break ;
	  else
	    {
	      int diff ;

	      /* Peverse stuff...only draw the box if the matches do not overlap, it can happen
	       * that there can be overlapping matches for a single piece of evidence....sigh... */
	      if (curr_y1 > prev_y2)
		{
		  bump_data = g_new0(ZMapWindowItemFeatureBumpDataStruct, 1) ;
		  bump_data->first_item = item ;
		  bump_data->feature_id = prev_id ;
		  bump_data->style = prev_style ;

		  diff = abs(prev_end - curr_start) - 1 ;
		  if (diff > match_threshold)
		    {
		      if (curr_start < prev_end)
			box_colour = &noncolinear ;
		      else
			box_colour = &colinear ;
		    }
		  else
		    box_colour = &perfect ;


		  /* Make line... */
		  background = makeMatchItem(FOO_CANVAS_GROUP(item->parent), ZMAPDRAW_OBJECT_LINE,
					     mid, prev_y2, mid, curr_y1,
					     box_colour, bump_data, col_data->window) ;

		  zmapWindowLongItemCheck(col_data->window->long_items, background, prev_y2, curr_y1) ;

		  extra_items = g_list_append(extra_items, background) ;

		  /* Make sure these backgrounds are drawn behind the feature items. */
		  foo_canvas_item_lower_to_bottom(background) ;

		  backgrounds = g_list_append(backgrounds, background) ;


		  /* Make invisible box.... */
		  bump_data = g_new0(ZMapWindowItemFeatureBumpDataStruct, 1) ;
		  bump_data->first_item = item ;
		  bump_data->feature_id = prev_id ;
		  bump_data->style = prev_style ;

		  background = makeMatchItem(FOO_CANVAS_GROUP(item->parent), ZMAPDRAW_OBJECT_BOX,
					     (mid - half_width), prev_y2, (mid + half_width), curr_y1,
					     NULL,
					     bump_data, col_data->window) ;

		  zmapWindowLongItemCheck(col_data->window->long_items, background, prev_y2, curr_y1) ;

		  extra_items = g_list_append(extra_items, background) ;

		  /* Make sure these backgrounds are drawn behind the feature items. */
		  foo_canvas_item_lower_to_bottom(background) ;

		  backgrounds = g_list_append(backgrounds, background) ;



		}

	      /* make curr into prev */
	      prev_feature = curr_feature ;
	      prev_id = curr_feature->original_id ;

	      if (col_data->window->revcomped_features)
		prev_end = curr_feature->feature.homol.y1 ;
	      else
		prev_end = curr_feature->feature.homol.y2 ;

	      prev_style = curr_feature->style ;
	      prev_y2 = curr_y2 ;
	    }
	}


      /* mark end of prev item if premature ending. */
      if (curr_feature->type == ZMAPFEATURE_ALIGNMENT)
	{
	  if (prev_feature->feature.homol.y1 > prev_feature->feature.homol.y2)
	    {
	      start = prev_feature->feature.homol.y2 ;
	      end = prev_feature->feature.homol.y1 ;
	    }
	  else
	    {
	      start = prev_feature->feature.homol.y1 ;
	      end = prev_feature->feature.homol.y2 ;
	    }

	  if (end < prev_feature->feature.homol.length)
	    {
	      bump_data = g_new0(ZMapWindowItemFeatureBumpDataStruct, 1) ;
	      bump_data->first_item = item ;
	      bump_data->feature_id = prev_feature->original_id ;
	      bump_data->style = prev_feature->style ;

	      itemGetCoords(item, &end_x1, &end_x2) ;

	      box_colour = &noncolinear ;
	      background = makeMatchItem(FOO_CANVAS_GROUP(item->parent), ZMAPDRAW_OBJECT_BOX,
					 end_x1, (prev_y2 - 10),
					 end_x2, prev_y2, 
					 box_colour, bump_data, col_data->window) ;

	      backgrounds = g_list_append(backgrounds, background) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	      zmapWindowLongItemCheck(col_data->window->long_items, background, prev_y1, (prev_y1 + 10)) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	      extra_items = g_list_append(extra_items, background) ;
	    }
	}


    } while (list_item) ;


  /* Need to add all backgrounds to col list here and lower them in group.... */
  name_list = g_list_concat(name_list, backgrounds) ;

  col_data->feature_list = name_list ;

  *extras_ptr = extra_items ;


  return ;
}






#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* UNUSED */
/* GFunc() to add background items indicating goodness of match to each set of items in a list. */
static void addMultiBackgrounds(gpointer data, gpointer user_data)
{
  ComplexCol col_data = (ComplexCol)data ;
  GList **extras_ptr = (GList **)user_data ;
  GList *extra_items = *extras_ptr ;
  GList *name_list = col_data->feature_list ;			    /* Single list of named features. */
  static gboolean colour_init = FALSE ;
  static GdkColor perfect, colinear, noncolinear ;
  char *perfect_colour = ZMAP_WINDOW_MATCH_PERFECT ;
  char *colinear_colour = ZMAP_WINDOW_MATCH_COLINEAR ;
  char *noncolinear_colour = ZMAP_WINDOW_MATCH_NOTCOLINEAR ;


  if (!colour_init)
    {
      gdk_color_parse(perfect_colour, &perfect) ;
      gdk_color_parse(colinear_colour, &colinear) ;
      gdk_color_parse(noncolinear_colour, &noncolinear) ;

      colour_init = TRUE ;
    }


  /* No point in doing lists that are only 1 long....actually not true if we want to mark start/end.... */
  if (g_list_length(name_list) > 1)
    {
      GList *backgrounds = NULL ;
      GList *list_item ;
      ZMapFeatureTypeStyle prev_style = NULL, curr_style = NULL ;
      double mid, half_width = COLUMN_BACKGROUND_SPACING ;
      double x1, prev_y1, x2, prev_y2, curr_y1, curr_y2 ;
      int prev_end = 0, curr_start = 0 ;
      FooCanvasItem *item ;
      FooCanvasItem *background ;
      ZMapFeature prev_feature, feature ;
      GQuark prev_id = 0, curr_id = 0 ;


      /* Get the first item. */
      list_item = g_list_first(name_list) ;
      item = (FooCanvasItem *)list_item->data ;
      foo_canvas_item_get_bounds(item, &x1, &prev_y1, &x2, &prev_y2) ;

      prev_feature = feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
      zMapAssert(feature) ;
      prev_id = feature->original_id ;
      prev_end = feature->feature.homol.y2 ;
      prev_style = feature->style ;


      /* N.B. this works because the items have already been positioned in the column. */
      mid = (x2 + x1) * 0.5 ;


      /* CODE HERE WORKS BUT IS NOT CORRECT IN THAT IT IS USING THE CANVAS BOX COORDS WHEN IT
	 SHOULD
	 * BE USING THE FEATURE->X1/X2 COORDS...i'LL FIX IT LATER... */

      do
	{
	  if ((list_item = g_list_next(list_item)))
	    {
	      unsigned int match_threshold ;
	      GdkColor *box_colour ;
	      ZMapWindowItemFeatureBumpData bump_data ;

	      item = (FooCanvasItem *)list_item->data ;

	      foo_canvas_item_get_bounds(item, NULL, &curr_y1, NULL, &curr_y2);

	      feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
	      zMapAssert(feature) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	      printf("%s :  %d, %d\n", g_quark_to_string(feature->original_id), feature->x1, feature->x2) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	      curr_id = feature->original_id ;
	      curr_style = feature->style ;

	      /* We only do aligns (remember that there can be different types in a single col)
	       * and only those for which joining of homols was requested. */
	      if (feature->type != ZMAPFEATURE_ALIGNMENT
		  || !(zMapStyleGetJoinAligns(curr_style, &match_threshold)))
		continue ;

	      if (curr_id != prev_id)
		{
		  int start, end ;

		  if (prev_feature->feature.homol.y1 > prev_feature->feature.homol.y2)
		    {
		      start = prev_feature->feature.homol.y2 ;
		      end = prev_feature->feature.homol.y1 ;
		    }
		  else
		    {
		      start = prev_feature->feature.homol.y1 ;
		      end = prev_feature->feature.homol.y2 ;
		    }

		  if (end < prev_feature->feature.homol.length)
		    {
		      bump_data = g_new0(ZMapWindowItemFeatureBumpDataStruct, 1) ;
		      bump_data->feature_id = prev_id ;
		      bump_data->style = prev_style ;

		      box_colour = &noncolinear ;
		      background = makeMatchItem(FOO_CANVAS_GROUP(item->parent), ZMAPDRAW_OBJECT_BOX,
						 (mid - (3 * half_width)), (prev_y2 - 10),
						 (mid + (3 * half_width)), prev_y2,
						 box_colour, bump_data, col_data->window) ;
		      
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
		      zmapWindowLongItemCheck(col_data->window->long_items, background, prev_y2, curr_y1) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

		      extra_items = g_list_append(extra_items, background) ;
		    }

		  prev_feature = feature ;
		  prev_id = curr_id ;
		  prev_style = curr_style ;
		  prev_y2 = curr_y2 ;
		  prev_end = feature->feature.homol.y2 ;

		  curr_id = 0 ;


		  if (feature->feature.homol.y1 > feature->feature.homol.y2)
		    {
		      start = feature->feature.homol.y2 ;
		      end = feature->feature.homol.y1 ;
		      printf("found one\n") ;
		    }
		  else
		    {
		      start = feature->feature.homol.y1 ;
		      end = feature->feature.homol.y2 ;
		    }

		  if (start > 1)
		    {
		      bump_data = g_new0(ZMapWindowItemFeatureBumpDataStruct, 1) ;
		      bump_data->feature_id = feature->original_id ;
		      bump_data->style = feature->style ;

		      box_colour = &noncolinear ;
		      background = makeMatchItem(FOO_CANVAS_GROUP(item->parent), ZMAPDRAW_OBJECT_BOX,
						 (mid - (3 * half_width)), curr_y1,
						 (mid + (3 * half_width)), (curr_y1 + 10), 
						 box_colour, bump_data, col_data->window) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
		      zmapWindowLongItemCheck(col_data->window->long_items, background, prev_y2, curr_y1) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


		      extra_items = g_list_append(extra_items, background) ;
		    }
		}
	      else
		{
		  int diff ;

		  curr_start = feature->feature.homol.y1 ;

		  /* Peverse stuff...it can happen that there can be overlapping matches for a
		   * single piece of evidence in which case we cannot draw a box between them....sigh... */
		  if (curr_y1 > prev_y2)
		    {
		      bump_data = g_new0(ZMapWindowItemFeatureBumpDataStruct, 1) ;
		      bump_data->feature_id = prev_id ;
		      bump_data->style = prev_style ;

		      diff = abs(prev_end - curr_start) ;
		      if (diff > match_threshold)
			{
			  if (curr_start < prev_end)
			    box_colour = &noncolinear ;
			  else
			    box_colour = &colinear ;
			}
		      else
			box_colour = &perfect ;

		      background = makeMatchItem(FOO_CANVAS_GROUP(item->parent), ZMAPDRAW_OBJECT_BOX,
						 (mid - half_width), prev_y2, (mid + half_width), curr_y1,
						 box_colour, bump_data, col_data->window) ;

		      zmapWindowLongItemCheck(col_data->window->long_items, background, prev_y2, curr_y1) ;

		      extra_items = g_list_append(extra_items, background) ;

		      /* Make sure these backgrounds are drawn behind the feature items. */
		      foo_canvas_item_lower_to_bottom(background) ;

		      backgrounds = g_list_append(backgrounds, background) ;
		    }

		  prev_y2 = curr_y2 ;
		  prev_end = feature->feature.homol.y2 ;
		}
	    }
	} while (list_item) ;


      /* Need to add all backgrounds to col list here and lower them in group.... */
      name_list = g_list_concat(name_list, backgrounds) ;


      col_data->feature_list = name_list ;

      *extras_ptr = extra_items ;
    }

  return ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



static FooCanvasItem *makeMatchItem(FooCanvasGroup *parent, ZMapDrawObjectType shape,
				    double x1, double y1, double x2, double y2,
				    GdkColor *colour,
				    gpointer item_data, gpointer event_data)
{
  FooCanvasItem *match_item ;

  if (shape == ZMAPDRAW_OBJECT_LINE)
    {
      match_item = zMapDrawLine(parent, x1, y1, x2, y2, 
				colour, 2.0) ;
    }
  else
    {
      match_item = zMapDrawBox(parent,
			       x1, y1, x2, y2,
			       colour, colour, 0.0) ;
    }

  g_object_set_data(G_OBJECT(match_item), ITEM_FEATURE_TYPE,
		    GINT_TO_POINTER(ITEM_FEATURE_GROUP_BACKGROUND)) ;
  g_object_set_data(G_OBJECT(match_item), ITEM_FEATURE_BUMP_DATA, item_data) ;
  g_signal_connect(GTK_OBJECT(match_item), "event",
		   GTK_SIGNAL_FUNC(bumpBackgroundEventCB), event_data) ;
  g_signal_connect(GTK_OBJECT(match_item), "destroy",
		   GTK_SIGNAL_FUNC(bumpBackgroundDestroyCB), event_data) ;

  return match_item ;
}


static void freeExtraItems(gpointer data, gpointer user_data_unused)
{
  FooCanvasItem *item = (FooCanvasItem *)data ;

  gtk_object_destroy(GTK_OBJECT(item)) ;

  return ;
}



/* We only reshow items if they were hidden by our code, any user hidden items are kept hidden.
 * Code is slightly complex here because the user hidden items are a stack of lists of hidden items. */
static void showItems(gpointer data, gpointer user_data)
{
  FooCanvasItem *item = (FooCanvasItem *)data ;
  ZMapWindowItemFeatureSetData set_data = (ZMapWindowItemFeatureSetData)user_data ;
  GList *found_item = NULL ;

  if (!(found_item = g_queue_find_custom( set_data->user_hidden_stack, item, findItemInQueueCB)))
    foo_canvas_item_show(item) ;

  return ;
}

static gint findItemInQueueCB(gconstpointer a, gconstpointer b)
{
  gint result = -1 ;
  GList *item_list = (GList *)a ;
  FooCanvasItem *item = (FooCanvasItem *) b ;
  GList *found_item = NULL ;

  if ((found_item = g_list_find(item_list, item)))
    result = 0 ;

  return result ;
}







/* GCompareFunc() to sort two lists of features by the average score of all their features. */
static gint sortByScoreCB(gconstpointer a, gconstpointer b)
{
  gint result = 0 ;					    /* make a == b default. */
  GList **feature_list_out_1 = (GList **)a ;
  GList *feature_list_1 = *feature_list_out_1 ;		    /* Single list of named features. */
  GList **feature_list_out_2 = (GList **)b ;
  GList *feature_list_2 = *feature_list_out_2 ;		    /* Single list of named features. */
  double list_score_1 = 0.0, list_score_2 = 0.0 ;

  /* Add up the scores for all features in the sublists. */
  g_list_foreach(feature_list_1, listScoreCB, &list_score_1) ;
  g_list_foreach(feature_list_2, listScoreCB, &list_score_2) ;

  /* average them by the number of features. */
  list_score_1 = list_score_1 / (double)(g_list_length(feature_list_1)) ;
  list_score_2 = list_score_2 / (double)(g_list_length(feature_list_2)) ;

  /* Highest scores go first. */
  if (list_score_1 > list_score_2)
    result = -1 ;
  else if (list_score_1 < list_score_2)
    result = 1 ;

  return result ;
}




/* GFunc() to return a features score. */
static void listScoreCB(gpointer data, gpointer user_data)
{
  FooCanvasItem *item = (FooCanvasItem *)data ;
  double score = *((double *)user_data) ;

  /* don't bother if something is not displayed. */
  if((zmapWindowItemIsShown(item)))
    {
      ZMapWindowItemFeatureType item_feature_type ;
      ZMapFeature feature ;

      /* Sanity checks... */
      item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
      zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE || item_feature_type == ITEM_FEATURE_PARENT) ;

      feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
      zMapAssert(feature) ;
      
      /* Can only compare by score if features have a score... */
      if (feature->flags.has_score)
	{
	  score += feature->score ;
	      
	  *((double *)user_data) = score ;
	}
    }

  return ;
}




#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* UNUSED */
/* GCompareFunc() to sort two lists of features by the overall span of all their features. */
static gint sortBySpanCB(gconstpointer a, gconstpointer b)
{
  gint result = 0 ;					    /* make a == b default. */
  GList **feature_list_out_1 = (GList **)a ;
  GList *feature_list_1 = *feature_list_out_1 ;		    /* Single list of named features. */
  GList **feature_list_out_2 = (GList **)b ;
  GList *feature_list_2 = *feature_list_out_2 ;		    /* Single list of named features. */
  FooCanvasItem *item ;
  ZMapWindowItemFeatureType item_feature_type ;
  ZMapFeature feature ;
  int list_span_1 = 0, list_span_2 = 0 ;
  

  feature_list_1 = g_list_first(feature_list_1) ;
  item = (FooCanvasItem *)feature_list_1->data ;
  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
  zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE || item_feature_type == ITEM_FEATURE_PARENT) ;
  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;
  list_span_1 = feature->x1 ;

  feature_list_1 = g_list_last(feature_list_1) ;
  item = (FooCanvasItem *)feature_list_1->data ;
  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
  zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE || item_feature_type == ITEM_FEATURE_PARENT) ;
  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;
  list_span_1 = feature->x1 - list_span_1 + 1 ;


  feature_list_2 = g_list_first(feature_list_2) ;
  item = (FooCanvasItem *)feature_list_2->data ;
  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
  zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE || item_feature_type == ITEM_FEATURE_PARENT) ;
  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;
  list_span_2 = feature->x1 ;

  feature_list_2 = g_list_last(feature_list_2) ;
  item = (FooCanvasItem *)feature_list_2->data ;
  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
  zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE || item_feature_type == ITEM_FEATURE_PARENT) ;
  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;
  list_span_2 = feature->x1 - list_span_2 + 1 ;
  
  /* Highest spans go first. */
  if (list_span_1 > list_span_2)
    result = -1 ;
  else if (list_span_1 < list_span_2)
    result = 1 ;

  return result ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */




/* GCompareFunc() to sort two lists of features by strand and then the overall span of all their features. */
static gint sortByStrandSpanCB(gconstpointer a, gconstpointer b)
{
  gint result = 0 ;					    /* make a == b default. */
  GList **feature_list_out_1 = (GList **)a ;
  GList *feature_list_1 = *feature_list_out_1 ;		    /* Single list of named features. */
  GList **feature_list_out_2 = (GList **)b ;
  GList *feature_list_2 = *feature_list_out_2 ;		    /* Single list of named features. */
  FooCanvasItem *item ;
  ZMapWindowItemFeatureType item_feature_type ;
  ZMapFeature feature ;
  int list_span_1 = 0, list_span_2 = 0 ;
  ZMapStrand strand_1, strand_2 ;

  feature_list_1 = g_list_first(feature_list_1) ;
  item = (FooCanvasItem *)feature_list_1->data ;
  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
  zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE || item_feature_type == ITEM_FEATURE_PARENT) ;
  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;
  list_span_1 = feature->x1 ;
  strand_1 = feature->strand ;

  feature_list_1 = g_list_last(feature_list_1) ;
  item = (FooCanvasItem *)feature_list_1->data ;
  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
  zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE || item_feature_type == ITEM_FEATURE_PARENT) ;
  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;
  list_span_1 = feature->x1 - list_span_1 + 1 ;


  feature_list_2 = g_list_first(feature_list_2) ;
  item = (FooCanvasItem *)feature_list_2->data ;
  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
  zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE || item_feature_type == ITEM_FEATURE_PARENT) ;
  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;
  list_span_2 = feature->x1 ;
  strand_2 = feature->strand ;

  feature_list_2 = g_list_last(feature_list_2) ;
  item = (FooCanvasItem *)feature_list_2->data ;
  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
  zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE || item_feature_type == ITEM_FEATURE_PARENT) ;
  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;
  list_span_2 = feature->x1 - list_span_2 + 1 ;
  

  /* Strand first, then span. */
  if (strand_1 < strand_2)
    result = -1 ;
  else if (strand_1 > strand_2)
    result = 1 ;
  else
    {
      /* Highest spans go first. */
      if (list_span_1 > list_span_2)
	result = -1 ;
      else if (list_span_1 < list_span_2)
	result = 1 ;
    }

  return result ;
}


/* GCompareFunc() to sort two lists of features by strand and then by how close their 5' and 3'
 * matches are to the range start/end. */
static gint sortByOverlapCB(gconstpointer a, gconstpointer b, gpointer user_data)
{
  gint result = 0 ;					    /* make a == b default. */
  GList **feature_list_out_1 = (GList **)a ;
  GList *feature_list_1 = *feature_list_out_1 ;		    /* Single list of named features. */
  GList **feature_list_out_2 = (GList **)b ;
  GList *feature_list_2 = *feature_list_out_2 ;		    /* Single list of named features. */
  RangeData range = (RangeData)user_data ;
  FooCanvasItem *item ;
  ZMapWindowItemFeatureType item_feature_type ;
  ZMapFeature feature ;
  int top_1, top_2, bot_1, bot_2 ;
  ZMapStrand strand_1, strand_2 ;

  feature_list_1 = g_list_first(feature_list_1) ;
  item = (FooCanvasItem *)feature_list_1->data ;
  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
  zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE || item_feature_type == ITEM_FEATURE_PARENT) ;
  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;
  top_1 = feature->x1 ;
  strand_1 = feature->strand ;
  bot_1 = feature->x2 ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  feature_list_1 = g_list_last(feature_list_1) ;
  item = (FooCanvasItem *)feature_list_1->data ;
  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
  zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE || item_feature_type == ITEM_FEATURE_PARENT) ;
  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;
  bot_1 = feature->x2 ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



  feature_list_2 = g_list_first(feature_list_2) ;
  item = (FooCanvasItem *)feature_list_2->data ;
  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
  zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE || item_feature_type == ITEM_FEATURE_PARENT) ;
  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;
  top_2 = feature->x1 ;
  strand_2 = feature->strand ;
  bot_2 = feature->x2 ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  feature_list_2 = g_list_last(feature_list_2) ;
  item = (FooCanvasItem *)feature_list_2->data ;
  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
  zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE || item_feature_type == ITEM_FEATURE_PARENT) ;
  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;
  bot_2 = feature->x2 ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  

  /* Strand first, then overlap. */
  if (strand_1 < strand_2)
    result = -1 ;
  else if (strand_1 > strand_2)
    result = 1 ;
  else
    {
      RangeDataStruct range_1 = {FALSE}, range_2 = {FALSE} ;

      range_1.start = range_2.start = range->start ;
      range_1.end = range_2.end = range->end ;

      /* Find the closest 5' or 3' match (for forward or reverse aligns) to the start or end
       * respectively of the mark range, */
      g_list_foreach(feature_list_1, bestFitCB, &range_1) ;

      g_list_foreach(feature_list_2, bestFitCB, &range_2) ;

      /* sort by nearest to 5' or 3' and then by longest match after that. */
      if (range_1.feature_diff < range_2.feature_diff)
	result = -1 ;
      else if (range_1.feature_diff == range_2.feature_diff)
	{
	  if (abs(range_1.feature->x1 - range_1.feature->x2) > abs(range_2.feature->x1 - range_2.feature->x2))
	    result = -1 ;
	  else
	    result = 1 ;
	}
      else
	result = 1 ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      /* LEAVE UNTIL I'VE FNISHED TESTING THE BUMP STUFF.... */
      if (strand_1 == ZMAPSTRAND_FORWARD)
	{
	  fit_1 = abs(range->start - top_1) + abs(range->start - bot_1) ;
	  fit_2 = abs(range->start - top_2) + abs(range->start - bot_2) ;
	}
      else if (strand_1 == ZMAPSTRAND_REVERSE)
	{
	  fit_1 = abs(range->end - top_1) + abs(range->end - bot_1) ;
	  fit_2 = abs(range->end - top_2) + abs(range->end - bot_2) ;
	}
      else
	{
	  fit_1 = abs(range->start - top_1) + abs(range->end - bot_1) ;
	  fit_2 = abs(range->start - top_2) + abs(range->end - bot_2) ;
	}

      /* Best fit to range goes first. */
      if (fit_1 < fit_2)
	result = -1 ;
      else if (fit_1 > fit_2)
	result = 1 ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

    }

  return result ;
}


/* A GFunc() to find the best fitting match to the start or end of the range in a list. */
static void bestFitCB(gpointer data, gpointer user_data)
{
  FooCanvasItem *item = (FooCanvasItem *)data ;
  RangeData range = (RangeData)user_data ;
  ZMapWindowItemFeatureType item_feature_type ;
  ZMapFeature feature ;
  int diff ;
  
  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
  zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE || item_feature_type == ITEM_FEATURE_PARENT) ;
  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;

  if (feature->strand == ZMAPSTRAND_FORWARD || feature->strand == ZMAPSTRAND_NONE)
    diff = abs(range->start - feature->x1) ;
  else
    diff = abs(range->end - feature->x2) ;


  if (!(range->feature))
    {
      range->feature_diff = diff ;
      range->feature = feature ;
    }
  else if (diff < range->feature_diff)
    {
      range->feature_diff = diff ;
      range->feature = feature ;
    }

  return ;
}







#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* GFunc() to return a features span. */
static void listSpanCB(gpointer data, gpointer user_data)
{
  FooCanvasItem *item = (FooCanvasItem *)data ;
  double span = *((double *)user_data) ;

  /* don't bother if something is not displayed. */
  if((zmapWindowItemIsShown(item)))
    {
      ZMapWindowItemFeatureType item_feature_type ;
      ZMapFeature feature ;

      /* Sanity checks... */
      item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
      zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE || item_feature_type == ITEM_FEATURE_PARENT) ;

      feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
      zMapAssert(feature) ;
      
      /* Can only compare by span if features have a span... */
      if (feature->flags.has_score)
	{
	  score += feature->score ;
	      
	  *((double *)user_data) = score ;
	}
    }

  return ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */





/* GFunc() to try to combine lists of features, if the lists have any overlapping features,
 * then the new list must be placed at a new offset in the column. */
static void setOffsetCB(gpointer data, gpointer user_data)
{
  GList **name_list_ptr = (GList **)data ;
  GList *name_list = *name_list_ptr ;			    /* Single list of named features. */
  ComplexBump complex = (ComplexBump)user_data ;
  FeatureDataStruct feature_data ;

  feature_data.name_list = &name_list ;
  feature_data.overlap_func = complex->overlap_func ;

  /* If theres already a list of columns then try to add this set of name features to one of them
   * otherwise create a new column. */
  if (!complex->bumpcol_list
      || zMap_g_list_cond_foreach(complex->bumpcol_list, featureListCB, &feature_data))
    {
      ComplexCol col ;

      col = g_new0(ComplexColStruct, 1) ;
      col->window = complex->window ;
      col->offset = complex->curr_offset ;
      col->feature_list = name_list ;

      complex->bumpcol_list = g_list_append(complex->bumpcol_list, col) ;
      complex->curr_offset += complex->incr ;
    }

  return ;
}

/* Check to see if two lists overlap, if they don't then merge the two lists.
 * NOTE that this is non-destructive merge, features are not moved from one list to the
 * other, see the glib docs for _list_concat(). */
static gboolean featureListCB(gpointer data, gpointer user_data)
{
  gboolean call_again = TRUE ;
  ComplexCol col = (ComplexCol)data ;
  FeatureData feature_data = (FeatureData)user_data ;
  GList *name_list = *(feature_data->name_list) ;


  if (!(feature_data->overlap_func)(col->feature_list, name_list))
    {
      col->feature_list = g_list_concat(col->feature_list, name_list) ;

      /* Having done the merge we _must_ resort the list by position otherwise subsequent
       * overlap tests will fail. */
      col->feature_list = zmapWindowItemSortByPostion(col->feature_list) ;

      call_again = FALSE ;
    }

  return call_again ;
}



/* GFunc() to try to combine lists of features, if the lists have any overlapping features,
 * then the new list must be placed at a new offset in the column. */
static void setAllOffsetCB(gpointer data, gpointer user_data)
{
  GList **name_list_ptr = (GList **)data ;
  GList *name_list = *name_list_ptr ;			    /* Single list of named features. */
  ComplexBump complex = (ComplexBump)user_data ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  FeatureDataStruct feature_data ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  feature_data.name_list = &name_list ;
  feature_data.overlap_func = complex->overlap_func ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  /* If theres already a list of columns then try to add this set of name features to one of them
   * otherwise create a new column. */

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  if (!complex->bumpcol_list
      || zMap_g_list_cond_foreach(complex->bumpcol_list, feature2ListCB, &feature_data))
    {
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

      ComplexCol col ;
      double max_width = 0.0 ;

      g_list_foreach(name_list, getMaxWidth, &max_width) ;


      col = g_new0(ComplexColStruct, 1) ;
      col->window = complex->window ;
      col->offset = complex->curr_offset ;
      col->feature_list = name_list ;

      complex->bumpcol_list = g_list_append(complex->bumpcol_list, col) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      complex->curr_offset += complex->incr ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

      complex->curr_offset += max_width ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
    }
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  return ;
}


/* GFunc() to find max width of canvas items in a list. */
static void getMaxWidth(gpointer data, gpointer user_data)
{
  FooCanvasItem *item = (FooCanvasItem *)data ;
  double *max_width = (double *)user_data ;
  double x1, x2, width ;
  ZMapFeature feature = NULL ;
  ZMapWindowItemFeatureType feature_type ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  foo_canvas_item_get_bounds(item, &x1, &y1, &x2, &y2) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA);
  feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  if (zmapWindowItemIsCompound(item))
    {
      true_item = zmapWindowItemGetNthChild(FOO_CANVAS_GROUP(item), 0) ;
    }
  else
    true_item = item ;


  if (FOO_IS_CANVAS_GROUP(true_item))
    {
      printf("we shouldn't be here\n") ;
    }


  if (FOO_IS_CANVAS_POLYGON(true_item))
    {
      FooCanvasPoints *points = NULL ;
      int i ;

      g_object_get(G_OBJECT(true_item),
		   "points", &points,
		   NULL) ;

      x1 = x2 = points->coords[0] ;
      for (i = 0 ; i < points->num_points ; i += 2)
	{
	  if (points->coords[i] < x1)
	    x1 = points->coords[i] ;
	  if (points->coords[i] > x2)
	    x2 = points->coords[i] ;
	}
    }
  else if (FOO_IS_CANVAS_RE(true_item))
    {
      g_object_get(G_OBJECT(true_item),
		   "x1", &x1,
		   "x2", &x2,
		   NULL) ;
    }
  else
    {
      printf("agh....can't cope with this\n") ;
    }

#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  itemGetCoords(item, &x1, &x2) ;

  width = x2 - x1 + 1.0 ;

  if (width > *max_width)
    *max_width = width ;

  return ;
}



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* Check to see if two lists overlap, if they don't then merge the two lists.
 * NOTE that this is non-destructive merge, features are not moved from one list to the
 * other, see the glib docs for _list_concat(). */
static gboolean featureList2CB(gpointer data, gpointer user_data)
{
  gboolean call_again = TRUE ;
  ComplexCol col = (ComplexCol)data ;
  FeatureData feature_data = (FeatureData)user_data ;
  GList *name_list = *(feature_data->name_list) ;


  if (!(feature_data->overlap_func)(col->feature_list, name_list))
    {
      col->feature_list = g_list_concat(col->feature_list, name_list) ;

      /* Having done the merge we _must_ resort the list by position otherwise subsequent
       * overlap tests will fail. */
      col->feature_list = zmapWindowItemSortByPostion(col->feature_list) ;

      call_again = FALSE ;
    }

  return call_again ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */





/* Compare two lists of features, return TRUE if any two features in the two lists overlap,
 * return FALSE otherwise.
 * 
 * NOTE that this function _completely_ relies on the two feature lists having been sorted
 * into ascending order on their coord positions, it will fail dismally if this is not true.
 *  */
static gboolean listsOverlap(GList *curr_features, GList *new_features)
{
  gboolean overlap = FALSE ;
  GList *curr_ptr, *new_ptr ;

  curr_ptr = g_list_first(curr_features) ;
  new_ptr = g_list_first(new_features) ;
  do
    {
      FooCanvasItem *curr_item = (FooCanvasItem *)(curr_ptr->data) ;
      FooCanvasItem *new_item = (FooCanvasItem *)(new_ptr->data) ;
      ZMapFeature curr_feature ;
      ZMapFeature new_feature ;

      curr_feature = g_object_get_data(G_OBJECT(curr_item), ITEM_FEATURE_DATA) ;
      zMapAssert(curr_feature) ;

      new_feature = g_object_get_data(G_OBJECT(new_item), ITEM_FEATURE_DATA) ;
      zMapAssert(new_feature) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      printf("%s (%d, %d) vs. %s (%d, %d)",
	     g_quark_to_string(curr_feature->original_id), curr_feature->x1, curr_feature->x2,
	     g_quark_to_string(new_feature->original_id), new_feature->x1, new_feature->x2) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


      if (new_feature->x2 < curr_feature->x1)
	{
	  new_ptr = g_list_next(new_ptr) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	  printf("-->  no overlap\n") ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	}
      else if (new_feature->x1 > curr_feature->x2)
	{
	  curr_ptr = g_list_next(curr_ptr) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	  printf("-->  no overlap\n") ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	}
      else
	{
	  overlap = TRUE ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	  printf("-->  OVERLAP\n") ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	}

    } while (!overlap && curr_ptr && new_ptr) ;


  return overlap ;
}



/* Compare two lists of features, return TRUE if the two lists of features are interleaved
 * in position, return FALSE otherwise.
 * 
 * NOTE that this function _completely_ relies on the two feature lists having been sorted
 * into ascending order on their coord positions, it will fail dismally if this is not true.
 *  */
static gboolean listsOverlapNoInterleave(GList *curr_features, GList *new_features)
{
  gboolean overlap = TRUE ;
  GList *curr_ptr, *new_ptr ;
  FooCanvasItem *curr_item ;
  FooCanvasItem *new_item ;
  ZMapFeature curr_first, curr_last ;
  ZMapFeature new_first, new_last ;



  curr_ptr = g_list_first(curr_features) ;
  curr_item = (FooCanvasItem *)(curr_ptr->data) ;
  curr_first = g_object_get_data(G_OBJECT(curr_item), ITEM_FEATURE_DATA) ;
  zMapAssert(curr_first) ;

  curr_ptr = g_list_last(curr_features) ;
  curr_item = (FooCanvasItem *)(curr_ptr->data) ;
  curr_last = g_object_get_data(G_OBJECT(curr_item), ITEM_FEATURE_DATA) ;
  zMapAssert(curr_last) ;

  new_ptr = g_list_first(new_features) ;
  new_item = (FooCanvasItem *)(new_ptr->data) ;
  new_first = g_object_get_data(G_OBJECT(new_item), ITEM_FEATURE_DATA) ;
  zMapAssert(new_first) ;

  new_ptr = g_list_last(new_features) ;
  new_item = (FooCanvasItem *)(new_ptr->data) ;
  new_last = g_object_get_data(G_OBJECT(new_item), ITEM_FEATURE_DATA) ;
  zMapAssert(new_last) ;

  /* We just want to make sure the two lists do not overlap in any of their positions. */
  if (new_first->x1 > curr_last->x2
      || new_last->x2 < curr_first->x1)
    overlap = FALSE ;

  return overlap ;
}



/* Reverses the offsets for the supplied list...for displaying on the reverse strand. */
static void reverseOffsets(GList *bumpcol_list)
{
  GList *first = g_list_first(bumpcol_list) ;
  GList *last = g_list_last(bumpcol_list) ;
  ComplexCol col_data_first, col_data_last ;
  int i ;
  int length = g_list_length(bumpcol_list) / 2 ;

  for (i = 0 ; i < length ; i++)
    {
      double tmp ;

      col_data_first = (ComplexCol)(first->data) ;
      col_data_last = (ComplexCol)(last->data) ;

      tmp = col_data_first->offset ;
      col_data_first->offset = col_data_last->offset ;
      col_data_last->offset = tmp ;

      first = g_list_next(first) ;
      last = g_list_previous(last) ;
    }

  return ;
}




/* These two callbacks go through the list of lists of features moving all their canvas items
 * into the correct place within their columns. */
static void moveItemsCB(gpointer data, gpointer user_data)
{
  ComplexCol col_data = (ComplexCol)data ;

  g_list_foreach(col_data->feature_list, moveItemCB, col_data) ;

  return ;
}


static void moveItemCB(gpointer data, gpointer user_data)
{
  FooCanvasItem *item = (FooCanvasItem *)data ;
  ComplexCol col_data = (ComplexCol)user_data ;
  ZMapFeature feature ;
  double dx = 0.0, offset = 0.0 ;
  double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0 ;

  feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;

  /* x1, x2 always needed so might as well get y coords as well because foocanvas will have
   * calculated them anyway. */
  foo_canvas_item_get_bounds(item, &x1, &y1, &x2, &y2) ;

  /* Some features are drawn with different widths to indicate things like score. In this case
   * their offset needs to be corrected to place them centrally. (We always do this which
   * seems inefficient but its a toss up whether it would be quicker to test (dx == 0). */
  dx = ((zMapStyleGetWidth(feature->style) * COMPLEX_BUMP_COMPRESS) - (x2 - x1)) / 2 ;

  offset = col_data->offset + dx ;

  my_foo_canvas_item_goto(item, &offset, NULL) ; 

  return ;
}


/* Called for styles in a column hash set of styles, just sets bump mode. */
static void setStyleBumpCB(ZMapFeatureTypeStyle style, gpointer user_data)
{
  ZMapStyleOverlapMode bump_mode = GPOINTER_TO_INT(user_data) ;

  zMapStyleSetOverlapMode(style, bump_mode) ;

  return ;
}




#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* UNUSED */
static gint compareNameToColumn(gconstpointer list_data, gconstpointer user_data)
{
  gint result = -1 ;
  FooCanvasGroup *col_group = (FooCanvasGroup *)list_data ;
  GQuark col_name = GPOINTER_TO_INT(user_data) ;
  ZMapFeatureSet feature_set ;

  /* We can have empty columns with no feature set so maynot be able to get feature set name.... */
  if ((feature_set = (ZMapFeatureSet)g_object_get_data(G_OBJECT(col_group), ITEM_FEATURE_DATA)))
    {
      if (feature_set->unique_id == col_name)
	result = 0 ;
    }

  return result ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */






/* 
 *             3 Frame Display functions.
 * 
 * 3 frame display is complex in that some columns should only be shown in "3 frame"
 * mode, whereas others are shown as a single column normally and then as the three
 * "frame" columns in "3 frame" mode. This involves deleting columns, moving them,
 * recreating them etc.
 * 
 */


/*
 *      Functions to remove frame sensitive columns from the canvas.
 */

static void remove3Frame(ZMapWindow window)
{
  FooCanvasGroup *super_root ;

  super_root = FOO_CANVAS_GROUP(zmapWindowFToIFindItemFull(window->context_to_item,
							   0,0,0,
							   ZMAPSTRAND_NONE, ZMAPFRAME_NONE,
							   0)) ;
  zMapAssert(super_root) ;


  zmapWindowContainerExecute(super_root,
                             ZMAPCONTAINER_LEVEL_FEATURESET,
                             remove3FrameCol, window);

  return ;
}

static void remove3FrameCol(FooCanvasGroup *container, FooCanvasPoints *points, 
                            ZMapContainerLevelType level, gpointer user_data)
{
  ZMapWindow window = (ZMapWindow)user_data ;

  if (level == ZMAPCONTAINER_LEVEL_FEATURESET)
    {
      ZMapFeatureTypeStyle style ;
      gboolean frame_specific = FALSE ;

      style = zmapWindowContainerGetStyle(container) ;
      zMapAssert(style) ;
      zMapStyleGetStrandAttrs(style, NULL, &frame_specific, NULL, NULL ) ;

      if (frame_specific)
	{
	  ZMapWindowItemFeatureSetData set_data ;
	  FooCanvasGroup *parent ;

	  set_data = g_object_get_data(G_OBJECT(container), ITEM_FEATURE_SET_DATA) ;
	  zMapAssert(set_data) ;

	  if (set_data->strand != ZMAPSTRAND_REVERSE
	      || (set_data->strand == ZMAPSTRAND_REVERSE && window->show_3_frame_reverse))
	    {
	      parent = zmapWindowContainerGetSuperGroup(container) ;
	      parent = zmapWindowContainerGetFeatures(parent) ;

	      zmapWindowContainerDestroy(container) ;
	    }
	}
    }

  return ;
}




/*
 *   Functions to draw as "normal" (i.e. not read frame) the frame sensitive columns
 *   from the canvas.
 */

static void redraw3FrameNormal(ZMapWindow window)
{
  FooCanvasGroup *super_root ;

  super_root = FOO_CANVAS_GROUP(zmapWindowFToIFindItemFull(window->context_to_item,
							   0,0,0,
							   ZMAPSTRAND_NONE, ZMAPFRAME_NONE,
							   0)) ;
  zMapAssert(super_root) ;

  zmapWindowContainerExecute(super_root,
                             ZMAPCONTAINER_LEVEL_BLOCK,
                             redraw3FrameCol, window);

  zmapWindowColOrderColumns(window);
  
  zmapWindowFullReposition(window) ;

  return ;
}


static void redraw3FrameCol(FooCanvasGroup *container, FooCanvasPoints *points, 
                            ZMapContainerLevelType level, gpointer user_data)
{
  ZMapWindow window = (ZMapWindow)user_data ;

  if (level == ZMAPCONTAINER_LEVEL_BLOCK)
    {
      RedrawDataStruct redraw_data = {NULL} ;
      FooCanvasGroup *block_children ;
      FooCanvasGroup *forward, *reverse ;

      redraw_data.block = g_object_get_data(G_OBJECT(container), ITEM_FEATURE_DATA) ;
      redraw_data.window = window ;

      block_children = zmapWindowContainerGetFeatures(container) ;

      forward = zmapWindowContainerGetStrandGroup(container, ZMAPSTRAND_FORWARD) ;
      redraw_data.forward_group = zmapWindowContainerGetFeatures(forward) ;

      if (window->show_3_frame_reverse)
	{
	  reverse = zmapWindowContainerGetStrandGroup(container, ZMAPSTRAND_REVERSE) ;
	  redraw_data.reverse_group = zmapWindowContainerGetFeatures(reverse) ;
	}

      /* Recreate all the frame sensitive columns as single columns and populate them with features. */
      g_list_foreach(window->feature_set_names, createSetColumn, &redraw_data) ;

      /* Now make sure the columns are in the right order. */
      zmapWindowSortCols(window->feature_set_names, forward, FALSE) ;
      if (window->show_3_frame_reverse)
	zmapWindowSortCols(window->feature_set_names, reverse, TRUE) ;

      /* Now remove any empty columns. */
      if (!(window->keep_empty_cols))
	{
	  zmapWindowRemoveEmptyColumns(window,
				       redraw_data.forward_group, redraw_data.reverse_group) ;
	}

    }

  return ;
}

static void createSetColumn(gpointer data, gpointer user_data)
{
  GQuark feature_set_id = GPOINTER_TO_UINT(data) ;
  RedrawData redraw_data = (RedrawData)user_data ;
  ZMapWindow window = redraw_data->window ;
  FooCanvasGroup *forward_col = NULL, *reverse_col = NULL ;
  ZMapFeatureTypeStyle style ;
  ZMapFeatureSet feature_set ;


  /* need to get style and check for 3 frame..... */
  if (!(style = zMapFindStyle(window->feature_context->styles, feature_set_id)))
    {
      char *name = (char *)g_quark_to_string(feature_set_id) ;

      zMapLogCritical("feature set \"%s\" not displayed because its style (\"%s\") could not be found.",
		      name, name) ;
    }
  else if (feature_set_id != zMapStyleCreateID(ZMAP_FIXED_STYLE_3FRAME)
	   && (!zMapStyleIsHiddenAlways(style) && zMapStyleIsFrameSpecific(style))
	   && ((feature_set = zMapFeatureBlockGetSetByID(redraw_data->block, feature_set_id))))
    {
      /* Make the forward/reverse columns for this feature set. */
      zmapWindowCreateSetColumns(window,
                                 redraw_data->forward_group, 
                                 redraw_data->reverse_group,
				 redraw_data->block, 
                                 feature_set,
                                 ZMAPFRAME_NONE,
				 &forward_col, &reverse_col) ;

      redraw_data->curr_forward_col = forward_col ;
      redraw_data->curr_reverse_col = reverse_col ;

      /* Now put the features in the columns. */
      drawSetFeatures(feature_set_id, feature_set, redraw_data) ;
    }


  return ;
}


static void drawSetFeatures(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapFeatureSet feature_set = (ZMapFeatureSet)data ;
  RedrawData redraw_data = (RedrawData)user_data ;
  ZMapWindow window = redraw_data->window ;
  GQuark type_quark ;
  FooCanvasGroup *forward_col, *reverse_col ;
  ZMapFeatureTypeStyle style ;

  /* Each column is known by its type/style name. */
  type_quark = feature_set->unique_id ;
  zMapAssert(type_quark == key_id) ;			    /* sanity check. */

  forward_col = redraw_data->curr_forward_col ;
  reverse_col = redraw_data->curr_reverse_col ;

  /* If neither column can be found it means that there is a mismatch between the requested
   * feature_sets and the styles. This happens when dealing with gff sources read from
   * files for instance because the features must be retrieved using the style names but then
   * filtered by feature_set name, so if extra styles are provided this will result in the
   * mismatch.
   *  */
  if (!forward_col && !reverse_col)
    {
      /* There is a memory leak here in that we do not delete the feature set from the context. */
      char *name = (char *)g_quark_to_string(feature_set->original_id) ;

      /* Temporary...probably user will not want to see this in the end but for now its good for debugging. */
      zMapShowMsg(ZMAP_MSG_WARNING, 
		  "Feature set \"%s\" not displayed because although it was retrieved from the server,"
		  " it is not in the list of feature sets to be displayed."
		  " Please check the list of styles and feature sets in the ZMap configuration file.",
		  name) ;

      zMapLogCritical("Feature set \"%s\" not displayed because although it was retrieved from the server,"
		      " it is not in the list of feature sets to be displayed."
		      " Please check the list of styles and feature sets in the ZMap configuration file.",
		      name) ;

      return ;
    }


  /* Record the feature sets on the column groups. */
  if (forward_col)
    {
      zmapWindowContainerSetData(forward_col, ITEM_FEATURE_DATA, feature_set) ;
    }

  if (reverse_col)
    {
      zmapWindowContainerSetData(reverse_col, ITEM_FEATURE_DATA, feature_set) ;
    }


  /* Check from the style whether this column is a 3 frame column and whether it should
   * ever be shown, if so then draw the features, note that setting frame to ZMAPFRAME_NONE
   * means the frame will be ignored and the features will all be in one column on the.
   * forward or reverse strand. */
  style = feature_set->style ;
  if (!zMapStyleIsHiddenAlways(style) && zMapStyleIsFrameSpecific(style))
    {
      zmapWindowDrawFeatureSet(window, feature_set,
                               forward_col, reverse_col, ZMAPFRAME_NONE) ;
    }

  return ;
}



/* 
 *       Functions to draw the 3 frame columns.
 * 
 * precursors: all existing frame sensitive cols have been removed
 *             all other cols are left intact in their original order 
 * 
 * processing is like this:
 * 
 * find the position where the 3 frame cols should be shown
 * 
 * for (all cols)
 *      for (all frames)
 *         for (all cols)
 *            if (col frame sensitive)
 *                create col
 *                insert col in list of cols at 3 frame position
 *                add features that are in correct frame to that col.
 * 
 * reposition all cols
 * 
 */


static void redrawAs3Frames(ZMapWindow window)
{
  FooCanvasGroup *super_root ;

  super_root = FOO_CANVAS_GROUP(zmapWindowFToIFindItemFull(window->context_to_item,
							   0,0,0,
							   ZMAPSTRAND_NONE, ZMAPFRAME_NONE,
							   0)) ;
  zMapAssert(super_root) ;

  /* possibly change nxt 3 calls to. 
   * zmapWindowContainerExecuteFull(super_root, 
   *                                ZMAPCONTAINER_LEVEL_STRAND,
   *                                redrawAs3FrameCols, window,
   *                                orderColumnCB, order_data, NULL);
   * means moving column code around though. Basically need to decide 
   * on how much NewReposition does...
   */
  zmapWindowContainerExecute(super_root,
                             ZMAPCONTAINER_LEVEL_BLOCK,
                             redrawAs3FrameCols, window);

  zmapWindowColOrderColumns(window);
  
  zmapWindowFullReposition(window) ;

  return ;
}


static void redrawAs3FrameCols(FooCanvasGroup *container, FooCanvasPoints *points, 
                               ZMapContainerLevelType level, gpointer user_data)
{
  ZMapWindow window = (ZMapWindow)user_data ;

  if (level == ZMAPCONTAINER_LEVEL_BLOCK)
    {
      RedrawDataStruct redraw_data = {NULL} ;
      FooCanvasGroup *block_children ;
      FooCanvasGroup *forward, *reverse ;

      redraw_data.block = g_object_get_data(G_OBJECT(container), ITEM_FEATURE_DATA) ;
      redraw_data.window = window ;

      block_children = zmapWindowContainerGetFeatures(container) ;


      forward = zmapWindowContainerGetStrandGroup(container, ZMAPSTRAND_FORWARD) ;
      redraw_data.forward_group = zmapWindowContainerGetFeatures(forward) ;

      if (window->show_3_frame_reverse)
	{
	  reverse = zmapWindowContainerGetStrandGroup(container, ZMAPSTRAND_REVERSE) ;
	  redraw_data.reverse_group = zmapWindowContainerGetFeatures(reverse) ;
	}


      /* We need to find the 3 frame position and use this to insert stuff.... */
      if ((redraw_data.frame3_pos = g_list_index(window->feature_set_names,
						 GINT_TO_POINTER(zMapStyleCreateID(ZMAP_FIXED_STYLE_3FRAME))))
	  == -1)
	{
	  zMapShowMsg(ZMAP_MSG_WARNING,
		      "Could not find 3 frame (\"%s\") in requested list of feature sets"
		      " so 3 Frame columns cannot be displayed.", ZMAP_FIXED_STYLE_3FRAME) ;

	  zMapLogCritical("Could not find 3 frame (\"%s\") in requested list of feature sets"
			  " so 3 Frame columns cannot be displayed.", ZMAP_FIXED_STYLE_3FRAME) ;
	}
      else
	{
          FooCanvasGroup *translation;
	  for (redraw_data.frame = ZMAPFRAME_0 ; redraw_data.frame <= ZMAPFRAME_2 ; redraw_data.frame++)
	    {
	      g_list_foreach(window->feature_set_names, create3FrameCols, &redraw_data) ;
	    }

          /* We need to draw the translation column in here too */
          if((translation = zmapWindowItemGetTranslationColumnFromBlock(window, redraw_data.block)))
            {
              zmapWindowColumnShow(translation);
            }

	  /* Remove empty cols.... */
	  zmapWindowRemoveEmptyColumns(window, redraw_data.forward_group, redraw_data.reverse_group) ;
	}

    }

  return ;
}



static void create3FrameCols(gpointer data, gpointer user_data)
{
  GQuark feature_set_id = GPOINTER_TO_UINT(data) ;
  RedrawData redraw_data = (RedrawData)user_data ;
  ZMapWindow window = redraw_data->window ;
  FooCanvasGroup *forward_col = NULL, *reverse_col = NULL ;
  ZMapFeatureTypeStyle style ;
  ZMapFeatureSet feature_set ;

  /* need to get style and check for 3 frame..... */
  if (!(style = zMapFindStyle(window->feature_context->styles, feature_set_id)))
    {
      char *name = (char *)g_quark_to_string(feature_set_id) ;

      zMapLogCritical("feature set \"%s\" not displayed because its style (\"%s\") could not be found.",
		      name, name) ;
    }
  else if(feature_set_id == zMapStyleCreateID(ZMAP_FIXED_STYLE_3FT_NAME))
    zMapLogMessage("%s this is message", "I say");
  else if (feature_set_id != zMapStyleCreateID(ZMAP_FIXED_STYLE_3FRAME)
	   && (!zMapStyleIsHiddenAlways(style) && zMapStyleIsFrameSpecific(style))
	   && ((feature_set = zMapFeatureBlockGetSetByID(redraw_data->block, feature_set_id))))
    {
      /* Create both forward and reverse columns. */
      zmapWindowCreateSetColumns(window,
                                 redraw_data->forward_group, 
                                 redraw_data->reverse_group,
				 redraw_data->block, 
                                 feature_set, 
                                 redraw_data->frame,
				 &forward_col, &reverse_col) ;

      /* There was some column ordering code here, but that's now in the
       * zmapWindowColOrderColumns code instead. */

      /* Set the current columns */
      redraw_data->curr_forward_col = forward_col ;

      if (window->show_3_frame_reverse)
        redraw_data->curr_reverse_col = reverse_col ;

      /* Now draw the features into the forward and reverse columns, empty columns are removed
       * at this stage. */
      draw3FrameSetFeatures(feature_set_id, feature_set, redraw_data) ;

      redraw_data->frame3_pos++ ;
    }


  return ;
}


static void draw3FrameSetFeatures(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapFeatureSet feature_set = (ZMapFeatureSet)data ;
  RedrawData redraw_data = (RedrawData)user_data ;
  ZMapWindow window = redraw_data->window ;
  GQuark type_quark ;
  FooCanvasGroup *forward_col, *reverse_col ;
  ZMapFeatureTypeStyle style ;


  /* Each column is known by its type/style name. */
  type_quark = feature_set->unique_id ;
  zMapAssert(type_quark == key_id) ;			    /* sanity check. */

  forward_col = redraw_data->curr_forward_col ;
  reverse_col = redraw_data->curr_reverse_col ;

  /* If neither column can be found it means that there is a mismatch between the requested
   * feature_sets and the styles. This happens when dealing with gff sources read from
   * files for instance because the features must be retrieved using the style names but then
   * filtered by feature_set name, so if extra styles are provided this will result in the
   * mismatch.
   *  */
  if (!forward_col && !reverse_col)
    {
      /* There is a memory leak here in that we do not delete the feature set from the context. */
      char *name = (char *)g_quark_to_string(feature_set->original_id) ;

      /* Temporary...probably user will not want to see this in the end but for now its good for debugging. */
      zMapShowMsg(ZMAP_MSG_WARNING, 
		  "Feature set \"%s\" not displayed because although it was retrieved from the server,"
		  " it is not in the list of feature sets to be displayed."
		  " Please check the list of styles and feature sets in the ZMap configuration file.",
		  name) ;

      zMapLogCritical("Feature set \"%s\" not displayed because although it was retrieved from the server,"
		      " it is not in the list of feature sets to be displayed."
		      " Please check the list of styles and feature sets in the ZMap configuration file.",
		      name) ;

      return ;
    }


  /* Set the feature set data on the canvas group so we can get it from event handlers. */
  if (forward_col)
    {
      zmapWindowContainerSetData(forward_col, ITEM_FEATURE_DATA, feature_set) ;
    }

  if (reverse_col)
    {
      zmapWindowContainerSetData(reverse_col, ITEM_FEATURE_DATA, feature_set) ;
    }


  /* need to get style and check for 3 frame and if column should be displayed and then
   * draw features. */
  style = feature_set->style ;
  if (!zMapStyleIsHiddenAlways(style) && zMapStyleIsFrameSpecific(style))
    {
      zmapWindowDrawFeatureSet(window, feature_set,
                               forward_col, reverse_col, 
                               redraw_data->frame) ;
    }

  return ;
}




/* Callback for any events that happen on bump background items.
 * 
 *  */
static gboolean bumpBackgroundEventCB(FooCanvasItem *item, GdkEvent *event, gpointer data)
{
  gboolean event_handled = FALSE ;
  ZMapWindow window = (ZMapWindowStruct*)data ;

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
      {
	GdkEventButton *but_event = (GdkEventButton *)event ;

	/* Button 1 is handled, button 3 is not as there is no applicable item menu,
	 * 2 is left for a general handler which could be the root handler. */
	if (but_event->button == 1)
	  {
	    ZMapWindowItemFeatureType item_feature_type ;
	    ZMapWindowItemFeatureBumpData bump_data ;
	    ZMapWindowSelectStruct select = {0} ;
	    gboolean replace_highlight = TRUE ;

	    if (zMapGUITestModifiers(but_event, GDK_SHIFT_MASK))
	      {
		if (zmapWindowFocusIsItemInHotColumn(window->focus, item))
		  replace_highlight = FALSE ;
	      }

	    item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item),
								  ITEM_FEATURE_TYPE)) ;
	    zMapAssert(item_feature_type == ITEM_FEATURE_GROUP_BACKGROUND) ;


	    /* Retrieve the feature item info from the canvas item. */
	    bump_data = (ZMapWindowItemFeatureBumpData)g_object_get_data(G_OBJECT(item), ITEM_FEATURE_BUMP_DATA) ;  
	    zMapAssert(bump_data) ;

	    /* Pass back details for display to the user to our caller. */
	    select.feature_desc.feature_name = (char *)g_quark_to_string(bump_data->feature_id) ;

	    select.feature_desc.feature_set = (char *)g_quark_to_string(zMapStyleGetID(bump_data->style)) ;

	    select.secondary_text = zmapWindowFeatureSetDescription(zMapStyleGetID(bump_data->style),
								    bump_data->style) ;

	    select.highlight_item = bump_data->first_item ;
	    select.replace_highlight_item = replace_highlight ;
            select.type = ZMAPWINDOW_SELECT_SINGLE;

	    (*(window->caller_cbs->select))(window, window->app_data, (void *)&select) ;

	    g_free(select.secondary_text) ;

	    event_handled = TRUE ;
	  }

	break ;
      }
    default:
      {
	/* By default we _don't_ handle events. */
	event_handled = FALSE ;

	break ;
      }
    }
  
  return event_handled ;
}




/* Callback for destroy of feature items... */
static gboolean bumpBackgroundDestroyCB(FooCanvasItem *feature_item, gpointer data)
{
  gboolean event_handled = FALSE ;			    /* Make sure any other callbacks also get run. */
  ZMapWindowItemFeatureType item_feature_type ;
  ZMapWindow window = (ZMapWindowStruct*)data ;
  ZMapWindowItemFeatureBumpData bump_data ;

  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(feature_item), ITEM_FEATURE_TYPE)) ;
  zMapAssert(item_feature_type == ITEM_FEATURE_GROUP_BACKGROUND) ;

  /* Retrieve the feature item info from the canvas item. */
  bump_data = (ZMapWindowItemFeatureBumpData)g_object_get_data(G_OBJECT(feature_item), ITEM_FEATURE_BUMP_DATA) ;  
  zMapAssert(bump_data) ;
  g_free(bump_data) ;

  zmapWindowLongItemRemove(window->long_items, feature_item) ;  /* Ignore boolean result. */

  return event_handled ;
}



/* Get the x coords of a drawn feature item, this is more complicated for a compound item where
 * we have to get a child and then transform its coords back to the compound items space.
 * the parent. */
static gboolean itemGetCoords(FooCanvasItem *item, double *x1_out, double *x2_out)
{
  gboolean result = TRUE ;
  FooCanvasItem *true_item ;
  double x1, x2, parent_x1, parent_x2 ;

  if (zmapWindowItemIsCompound(item))
    {
      true_item = zmapWindowItemGetNthChild(FOO_CANVAS_GROUP(item), 0) ;

      g_object_get(G_OBJECT(item),
		   "x", &parent_x1,
		   "y", &parent_x2,
		   NULL) ;
    }
  else
    true_item = item ;


  if (FOO_IS_CANVAS_POLYGON(true_item))
    {
      FooCanvasPoints *points = NULL ;
      int i ;

      g_object_get(G_OBJECT(true_item),
		   "points", &points,
		   NULL) ;

      x1 = x2 = points->coords[0] ;
      for (i = 0 ; i < points->num_points ; i += 2)
	{
	  if (points->coords[i] < x1)
	    x1 = points->coords[i] ;
	  if (points->coords[i] > x2)
	    x2 = points->coords[i] ;
	}
    }
  else if (FOO_IS_CANVAS_RE(true_item))
    {
      g_object_get(G_OBJECT(true_item),
		   "x1", &x1,
		   "x2", &x2,
		   NULL) ;
    }
  else if(FOO_IS_CANVAS_TEXT(true_item))
    {
      
    }
  else
    {
      zMapAssertNotReached() ;
    }

  if (item != true_item)
    {
      x1 += parent_x1 ;
      x2 += parent_x1 ;
    }

  *x1_out = x1 ;
  *x2_out = x2 ;

  return result ;
}



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE

/* 
 *             debug functions, please leave here for future use.
 */


static void printChild(gpointer data, gpointer user_data)
{
  FooCanvasGroup *container = (FooCanvasGroup *)data ;
  ZMapFeatureAny any_feature ;

  any_feature = g_object_get_data(G_OBJECT(container), ITEM_FEATURE_DATA) ;

  printf("%s ", g_quark_to_string(any_feature->unique_id)) ;

  return ;
}



static void printPtr(gpointer data, gpointer user_data)
{
  FooCanvasGroup *container = (FooCanvasGroup *)data ;

  printf("%p ", container) ;

  return ;
}


static void printQuarks(gpointer data, gpointer user_data)
{


  printf("quark str: %s\n", g_quark_to_string(GPOINTER_TO_INT(data))) ;


  return ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */




