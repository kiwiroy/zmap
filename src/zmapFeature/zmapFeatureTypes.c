/*  File: zmapFeatureTypes.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2006-2010: Genome Research Ltd.
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
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk
 *
 * Description: Functions for manipulating Type structs and sets of
 *              type structs.c
 *
 * Exported functions: See ZMap/zmapFeature.h
 * HISTORY:
 * Last edited: Jan 26 12:02 2010 (edgrif)
 * Created: Tue Dec 14 13:15:11 2004 (edgrif)
 * CVS info:   $Id: zmapFeatureTypes.c,v 1.91 2010-03-30 13:59:46 mh17 Exp $
 *-------------------------------------------------------------------
 */

#include <stdio.h>
#include <memory.h>

#include <ZMap/zmapUtils.h>

/* This should go in the end..... */
//#include <zmapFeature_P.h>
#include <ZMap/zmapConfigStyleDefaults.h>

#include <zmapStyle_I.h>


/* Think about defaults, how should they be set, should we force user to set them ? */
#define ZMAPFEATURE_DEFAULT_WIDTH 10.0			    /* of features being displayed */


typedef struct
{
  GList **names ;
  GList **styles ;
  gboolean any_style_found ;
} CheckSetListStruct, *CheckSetList ;



typedef struct
{
  ZMapStyleMergeMode merge_mode ;

  GData *curr_styles ;
} MergeStyleCBStruct, *MergeStyleCB ;


typedef struct
{
  gboolean *error ;
  GData *copy_set ;
} CopyStyleCBStruct, *CopyStyleCB ;


typedef struct
{
  gboolean error ;
  ZMapFeatureTypeStyle inherited_style ;
} InheritStyleCBStruct, *InheritStyleCB ;


typedef struct
{
  gboolean errors ;
  GData *style_set ;
  GData *inherited_styles ;
  ZMapFeatureTypeStyle prev_style ;
} InheritAllCBStruct, *InheritAllCB ;



static void doTypeSets(GQuark key_id, gpointer data, gpointer user_data) ;

static void checkListName(gpointer data, gpointer user_data) ;
static gint compareNameToStyle(gconstpointer glist_data, gconstpointer user_data) ;

static void mergeStyle(GQuark style_id, gpointer data, gpointer user_data_unused) ;
static void destroyStyle(GQuark style_id, gpointer data, gpointer user_data_unused) ;

static void copySetCB(GQuark key_id, gpointer data, gpointer user_data) ;

static void inheritCB(GQuark key_id, gpointer data, gpointer user_data) ;
static gboolean doStyleInheritance(GData **style_set, GData **inherited_styles, ZMapFeatureTypeStyle curr_style) ;
static void inheritAllFunc(gpointer data, gpointer user_data) ;

static void setStrandFrameAttrs(ZMapFeatureTypeStyle type,
				gboolean *strand_specific_in,
				gboolean *show_rev_strand_in,
				ZMapStyle3FrameMode *frame_mode_in) ;
static void mergeColours(ZMapStyleFullColour curr, ZMapStyleFullColour new) ;


/*! @defgroup zmapstyles   zMapStyle: Feature Style handling for ZMap
 * @{
 *
 * \brief  Feature Style handling for ZMap.
 *
 * zMapStyle routines provide functions to create/modify/destroy individual
 * styles, the styles control how features are processed and displayed. They
 * control aspects such as foreground colour, column bumping mode etc.
 *
 *  */



/* Add a style to a set of styles, if the style is already there no action is taken
 * and FALSE is returned. */
gboolean zMapStyleSetAdd(GData **style_set, ZMapFeatureTypeStyle style)
{
  gboolean result = FALSE ;
  GQuark style_id ;

  zMapAssert(style_set && style) ;

  style_id = zMapStyleGetUniqueID(style) ;

  if (!(zMapFindStyle(*style_set, style_id)))
    {
      g_datalist_id_set_data(style_set, style_id, style) ;
      result = TRUE ;
    }

  return result ;
}



/* Sets up all the inheritance for the set of styles.
 *
 * The method is to record each style that we have inherited as we go, we then
 * check the set of inherited styles to see if we need to do one each time
 * we do through the list.
 *
 * If there are errors in trying to inherit styles (e.g. non-existent parents)
 * then this function returns FALSE and there will be log messages identifying
 * the errors.
 *
 *  */
gboolean zMapStyleInheritAllStyles(GData **style_set)
{
  gboolean result = FALSE ;
  GData *inherited_styles ;
  InheritAllCBStruct cb_data = {FALSE} ;

  g_datalist_init(&inherited_styles) ;
  cb_data.style_set = *style_set ;
  cb_data.inherited_styles = inherited_styles ;

  g_datalist_foreach(style_set, inheritCB, &cb_data) ;

  /* We always return the inherited set since the style set is inherited in place
   * but we signal that there were errors. */
  *style_set = cb_data.style_set ;
  result = !cb_data.errors ;
  g_datalist_clear(&inherited_styles) ;

  return result ;
}


/* Copies a set of styles.
 *
 * If there are errors in trying to copy styles then this function returns FALSE
 * and a GData set containing as many styles as it could copy, there will be
 * log messages identifying the errors. It returns TRUE if there were no errors
 * at all.
 *
 *  */
gboolean zMapStyleCopyAllStyles(GData **style_set, GData **copy_style_set_out)
{
  gboolean result = TRUE ;
  CopyStyleCBStruct cb_data = {NULL} ;

  cb_data.error = &result ;
  g_datalist_init(&(cb_data.copy_set)) ;

  g_datalist_foreach(style_set, copySetCB, &cb_data) ;

  *copy_style_set_out = cb_data.copy_set ;

  return result ;
}



/*!
 * Overload one style with another. Values in curr_style are overwritten with those
 * in the new_style. new_style is not altered.
 *
 * <b>NOTE</b> that both styles will have the same unique id so if you add the new_style
 * to a style set the reference to the old style will be removed.
 *
 * Returns TRUE if merge ok, FALSE if there was a problem.
 *
 * @param   curr_style          The style to be overwritten.
 * @param   new_style           The style to used for overwriting.
 * @return  gboolean            TRUE means successful merge.
 *  */
gboolean zMapStyleMerge(ZMapFeatureTypeStyle curr_style, ZMapFeatureTypeStyle new_style)
{
  gboolean result = TRUE ;				    /* There is nothing to fail currently. */
  ZMapStyleParam param = zmapStyleParams_G;
  int i;

  zMapAssert(curr_style && new_style) ;

  for(i = 1;i < _STYLE_PROP_N_ITEMS;i++,param++)
    {

      if(zMapStyleIsPropertySetId(new_style,param->id))      // something to merge?
        {

          switch(param->type)
            {
            case STYLE_PARAM_TYPE_FLAGS:   // must not merge this!
              continue;

            case STYLE_PARAM_TYPE_STRING:
               {
                 gchar **str;

                 if(zMapStyleIsPropertySetId(curr_style,param->id))
                  {

                    str = (gchar **) (((void *) curr_style) + param->offset);
                    g_free(*str) ;
                  }

                str = (gchar **) (((void *) new_style) + param->offset);
                * (gchar **) (((void *) curr_style) + param->offset) = g_strdup(*str);
                break;
              }

            case STYLE_PARAM_TYPE_COLOUR:
              {
                ZMapStyleFullColour src_colour,dst_colour;

                src_colour = (ZMapStyleFullColour) (((void *) new_style) + param->offset);
                dst_colour = (ZMapStyleFullColour) (((void *) curr_style) + param->offset);

                mergeColours(dst_colour,src_colour);
                break;
              }

            default:
              {
                void *src,*dst;

                src = ((void *) new_style) + param->offset;
                dst = ((void *) curr_style) + param->offset;

                memcpy(dst,src,param->size);
                break;
              }
            }

          zmapStyleSetIsSet(curr_style,param->id);
        }
    }

  return result ;
}


static void mergeColours(ZMapStyleFullColour curr, ZMapStyleFullColour new)
{

  if (new->normal.fields_set.fill)
    {
      curr->normal.fill = new->normal.fill ;
      curr->normal.fields_set.fill = TRUE ;
    }
  if (new->normal.fields_set.draw)
    {
      curr->normal.draw = new->normal.draw ;
      curr->normal.fields_set.draw = TRUE ;
    }
  if (new->normal.fields_set.border)
    {
      curr->normal.border = new->normal.border ;
      curr->normal.fields_set.border = TRUE ;
    }


  if (new->selected.fields_set.fill)
    {
      curr->selected.fill = new->selected.fill ;
      curr->selected.fields_set.fill = TRUE ;
    }
  if (new->selected.fields_set.draw)
    {
      curr->selected.draw = new->selected.draw ;
      curr->selected.fields_set.draw = TRUE ;
    }
  if (new->selected.fields_set.border)
    {
      curr->selected.border = new->selected.border ;
      curr->selected.fields_set.border = TRUE ;
    }


  return ;
}



GQuark zMapStyleGetID(ZMapFeatureTypeStyle style)
{
  return style->original_id ;      // is always set
}

GQuark zMapStyleGetUniqueID(ZMapFeatureTypeStyle style)
{
  return style->unique_id ;      // is always set
}

const gchar *zMapStyleGetName(ZMapFeatureTypeStyle style)
{
      // this is the original name, stored internally as a squark
  return g_quark_to_string(style->original_id) ;      // is always set
}

gchar *zMapStyleGetDescription(ZMapFeatureTypeStyle style)
{
  return style->description ;       // is always set
}


ZMapStyleBumpMode zMapStyleGetBumpMode(ZMapFeatureTypeStyle style)
{
  ZMapStyleBumpMode mode = ZMAPBUMP_INVALID;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_BUMP_MODE))
    mode = style->curr_bump_mode;

  return mode;
}

ZMapStyleBumpMode zMapStyleGetDefaultBumpMode(ZMapFeatureTypeStyle style)
{
  ZMapStyleBumpMode mode = ZMAPBUMP_INVALID;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_BUMP_DEFAULT))
    mode = style->default_bump_mode;

  return mode;

}



void zMapStyleSetParent(ZMapFeatureTypeStyle style, char *parent_name)
{

  zmapStyleSetIsSet(style,STYLE_PROP_PARENT_STYLE);
  style->parent_id = zMapStyleCreateID(parent_name) ;

  return ;
}

void zMapStyleSetDescription(ZMapFeatureTypeStyle style, char *description)
{

  zmapStyleSetIsSet(style,STYLE_PROP_DESCRIPTION);
  style->description = g_strdup(description) ;

  return ;
}

void zMapStyleSetWidth(ZMapFeatureTypeStyle style, double width)
{

  zmapStyleSetIsSet(style,STYLE_PROP_WIDTH);
  style->width = width ;

  return ;
}

double zMapStyleGetBumpWidth(ZMapFeatureTypeStyle style)
{
  double bump_spacing = 0.0 ;

  if (zMapStyleIsPropertySetId(style,STYLE_PROP_BUMP_SPACING))
    bump_spacing = style->bump_spacing ;

  return  bump_spacing ;
}




/* Set magnification limits for displaying columns. */
void zMapStyleSetMag(ZMapFeatureTypeStyle style, double min_mag, double max_mag)
{

  if (min_mag && min_mag > 0.0)
    {
      style->min_mag = min_mag ;
      zmapStyleSetIsSet(style,STYLE_PROP_MIN_MAG);
    }

  if (max_mag && max_mag > 0.0)
    {
      style->max_mag = max_mag ;
      zmapStyleSetIsSet(style,STYLE_PROP_MAX_MAG);
    }


  return ;
}



gboolean zMapStyleIsMinMag(ZMapFeatureTypeStyle style, double *min_mag)
{
  gboolean mag_set = FALSE ;

  if (zMapStyleIsPropertySetId(style,STYLE_PROP_MIN_MAG))
    {
      mag_set = TRUE ;

      if (min_mag)
	*min_mag = style->min_mag ;
    }

  return mag_set ;
}


gboolean zMapStyleIsMaxMag(ZMapFeatureTypeStyle style, double *max_mag)
{
  gboolean mag_set = FALSE ;

  if (zMapStyleIsPropertySetId(style,STYLE_PROP_MIN_MAG))
    {
      mag_set = TRUE ;

      if (max_mag)
	*max_mag = style->max_mag ;
    }

  return mag_set ;
}


void zMapStyleSetMode(ZMapFeatureTypeStyle style, ZMapStyleMode mode)
{
  style->mode = mode;
  zmapStyleSetIsSet(style,STYLE_PROP_MODE);

  return ;
}


gboolean zMapStyleHasMode(ZMapFeatureTypeStyle style)
{
  gboolean result ;

  result = zMapStyleIsPropertySetId(style, STYLE_PROP_MODE) ;

  return result ;
}


ZMapStyleMode zMapStyleGetMode(ZMapFeatureTypeStyle style)
{
  ZMapStyleMode mode = ZMAPSTYLE_MODE_INVALID ;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_MODE))
      mode = style->mode;

  return mode;
}



void zMapStyleSetGlyphMode(ZMapFeatureTypeStyle style, ZMapStyleGlyphMode glyph_mode)
{
  switch (glyph_mode)
    {
    case ZMAPSTYLE_GLYPH_SPLICE:
    case ZMAPSTYLE_GLYPH_MARKER:
      style->mode_data.glyph.mode = glyph_mode ;
      zmapStyleSetIsSet(style,STYLE_PROP_GLYPH_MODE);

      break ;

    default:
      zMapAssertNotReached() ;
      break ;
    }

  return ;
}

ZMapStyleGlyphMode zMapStyleGetGlyphMode(ZMapFeatureTypeStyle style)
{
  ZMapStyleGlyphMode glyph_mode = ZMAPSTYLE_GLYPH_INVALID;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_GLYPH_MODE))
      glyph_mode = style->mode_data.glyph.mode;

  return glyph_mode ;
}



void zMapStyleSetPfetch(ZMapFeatureTypeStyle style, gboolean pfetchable)
{

  style->mode_data.alignment.pfetchable = pfetchable ;
  zmapStyleSetIsSet(style,STYLE_PROP_ALIGNMENT_PFETCHABLE);

  return ;
}




const gchar *zMapStyleGetGFFSource(ZMapFeatureTypeStyle style)
{
  const gchar *gff_source = NULL;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_GFF_SOURCE))
      gff_source = g_quark_to_string(style->gff_source);

  return gff_source ;
}

const gchar *zMapStyleGetGFFFeature(ZMapFeatureTypeStyle style)
{
  const gchar *gff_feature = NULL ;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_GFF_FEATURE))
      gff_feature = g_quark_to_string(style->gff_feature);

  return gff_feature ;
}


gboolean zMapStyleIsDirectionalEnd(ZMapFeatureTypeStyle style)
{
  gboolean ends = FALSE ;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_DIRECTIONAL_ENDS))
      ends = style->directional_end;

  return ends ;
}


unsigned int zmapStyleGetWithinAlignError(ZMapFeatureTypeStyle style)
{
  unsigned int error = 0;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_ALIGNMENT_BETWEEN_ERROR))
      error = style->mode_data.alignment.between_align_error;

  return error ;
}


gboolean zMapStyleIsParseGaps(ZMapFeatureTypeStyle style)
{
  gboolean parse_gaps = FALSE;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_ALIGNMENT_PARSE_GAPS))
      parse_gaps = style->mode_data.alignment.parse_gaps;

  return parse_gaps ;
}

gboolean zMapStyleIsShowGaps(ZMapFeatureTypeStyle style)
{
  gboolean show_gaps =FALSE ;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_ALIGNMENT_SHOW_GAPS))
      show_gaps = style->mode_data.alignment.show_gaps;

  return show_gaps ;
}

void zMapStyleSetShowGaps(ZMapFeatureTypeStyle style, gboolean show_gaps)
{

  if (style->mode == ZMAPSTYLE_MODE_ALIGNMENT)
    {
      style->mode_data.alignment.show_gaps = show_gaps;
      zmapStyleSetIsSet(style,STYLE_PROP_ALIGNMENT_SHOW_GAPS);
    }
  return ;
}




void zMapStyleSetGappedAligns(ZMapFeatureTypeStyle style, gboolean parse_gaps, gboolean show_gaps)
{

  if (style->mode == ZMAPSTYLE_MODE_ALIGNMENT)
    {
      style->mode_data.alignment.show_gaps = show_gaps;
      zmapStyleSetIsSet(style,STYLE_PROP_ALIGNMENT_SHOW_GAPS);
      style->mode_data.alignment.parse_gaps = parse_gaps;
      zmapStyleSetIsSet(style,STYLE_PROP_ALIGNMENT_PARSE_GAPS);
    }
  return ;
}


void zMapStyleGetGappedAligns(ZMapFeatureTypeStyle style, gboolean *parse_gaps, gboolean *show_gaps)
{
  if(zMapStyleIsPropertySetId(style,STYLE_PROP_ALIGNMENT_PARSE_GAPS))
      *parse_gaps = style->mode_data.alignment.parse_gaps;
  if(zMapStyleIsPropertySetId(style,STYLE_PROP_ALIGNMENT_SHOW_GAPS))
      *show_gaps = style->mode_data.alignment.show_gaps;

  return ;
}



void zMapStyleSetJoinAligns(ZMapFeatureTypeStyle style, unsigned int between_align_error)
{
  if(style->mode == ZMAPSTYLE_MODE_ALIGNMENT)
    {
      zmapStyleSetIsSet(style,STYLE_PROP_ALIGNMENT_BETWEEN_ERROR);
      style->mode_data.alignment.between_align_error = between_align_error ;
    }

  return ;
}


/* Returns TRUE and returns the between_align_error if join_aligns is TRUE for the style,
 * otherwise returns FALSE. */
gboolean zMapStyleGetJoinAligns(ZMapFeatureTypeStyle style, unsigned int *between_align_error)
{
  gboolean result = FALSE ;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_ALIGNMENT_PARSE_GAPS))
    {
      *between_align_error = style->mode_data.alignment.between_align_error;
      result = TRUE ;
    }

  return result ;
}


void zMapStyleSetDisplayable(ZMapFeatureTypeStyle style, gboolean displayable)
{
  style->displayable = displayable ;
  zmapStyleSetIsSet(style,STYLE_PROP_DISPLAYABLE);

  return ;
}

gboolean zMapStyleIsDisplayable(ZMapFeatureTypeStyle style)
{
  gboolean result = FALSE ;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_DISPLAYABLE))
      result = style->displayable;

  return result ;
}


void zMapStyleSetDeferred(ZMapFeatureTypeStyle style, gboolean deferred)
{
  style->deferred = deferred;
  zmapStyleSetIsSet(style,STYLE_PROP_DEFERRED);
  return ;
}

gboolean zMapStyleIsDeferred(ZMapFeatureTypeStyle style)
{
  gboolean result = FALSE;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_DEFERRED))
      result = style->deferred;

   return result ;
}

void zMapStyleSetLoaded(ZMapFeatureTypeStyle style, gboolean loaded)
{
  style->loaded = loaded;
  zmapStyleSetIsSet(style,STYLE_PROP_LOADED);

  return ;
}

gboolean zMapStyleIsLoaded(ZMapFeatureTypeStyle style)
{
  gboolean result = FALSE;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_LOADED))
      result = style->loaded;

  return result ;
}


/* Controls whether the feature set is displayed. */
void zMapStyleSetDisplay(ZMapFeatureTypeStyle style, ZMapStyleColumnDisplayState col_show)
{
  zMapAssert(style
           && col_show > ZMAPSTYLE_COLDISPLAY_INVALID && col_show <= ZMAPSTYLE_COLDISPLAY_SHOW) ;

  zmapStyleSetIsSet(style,STYLE_PROP_COLUMN_DISPLAY_MODE);
  style->col_display_state = col_show ;

  return ;
}

ZMapStyleColumnDisplayState zMapStyleGetDisplay(ZMapFeatureTypeStyle style)
{
  ZMapStyleColumnDisplayState mode = ZMAPSTYLE_COLDISPLAY_INVALID;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_COLUMN_DISPLAY_MODE))
      mode = style->col_display_state;

  return mode ;
}


gboolean zMapStyleIsHidden(ZMapFeatureTypeStyle style)
{
  gboolean result = FALSE ;
  ZMapStyleColumnDisplayState mode = ZMAPSTYLE_COLDISPLAY_INVALID;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_COLUMN_DISPLAY_MODE))
      mode = style->col_display_state;

  if (mode == ZMAPSTYLE_COLDISPLAY_HIDE)
    result = TRUE ;

  return result ;
}


/* Controls whether the feature set is displayed initially. */
void zMapStyleSetShowWhenEmpty(ZMapFeatureTypeStyle style, gboolean show_when_empty)
{
  g_return_if_fail(ZMAP_IS_FEATURE_STYLE(style));

  style->show_when_empty = show_when_empty ;
  zmapStyleSetIsSet(style,STYLE_PROP_SHOW_WHEN_EMPTY);

  return ;
}

gboolean zMapStyleGetShowWhenEmpty(ZMapFeatureTypeStyle style)
{
  gboolean show = FALSE;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_COLUMN_DISPLAY_MODE))
      show = style->show_when_empty;

  return show ;
}



double zMapStyleGetWidth(ZMapFeatureTypeStyle style)
{
  double width = 0.0 ;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_WIDTH))
      width = style->width;

  return width ;
}

double zMapStyleGetMaxScore(ZMapFeatureTypeStyle style)
{
  double max_score = 0.0 ;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_MAX_SCORE))
      max_score = style->max_score;

  return max_score ;
}

double zMapStyleGetMinScore(ZMapFeatureTypeStyle style)
{
  double min_score = 0.0 ;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_MIN_SCORE))
      min_score = style->min_score;

   return min_score ;
}


double zMapStyleGetMinMag(ZMapFeatureTypeStyle style)
{
  double min_mag = 0.0 ;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_MIN_MAG))
      min_mag = style->min_mag;

   return min_mag ;
}


double zMapStyleGetMaxMag(ZMapFeatureTypeStyle style)
{
  double max_mag = 0.0 ;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_MAX_MAG))
      max_mag = style->max_mag;

  return max_mag ;
}


double zMapStyleBaseline(ZMapFeatureTypeStyle style)
{
  double baseline = 0.0 ;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_GRAPH_BASELINE))
      baseline = style->mode_data.graph.baseline;

  return baseline ;
}

ZMapStyleGlyphType zMapStyleGlyphMode(ZMapFeatureTypeStyle style)
{
  int glyph_mode = ZMAPSTYLE_GLYPH_INVALID;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_GLYPH_MODE))
      glyph_mode = style->mode_data.glyph.mode;

   return glyph_mode ;
}

ZMapStyleGlyphType zMapStyleGlyphType(ZMapFeatureTypeStyle style)
{
  int glyph_type = ZMAPSTYLE_GLYPH_TYPE_INVALID;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_GLYPH_TYPE))
      glyph_type = style->mode_data.glyph.type;

  return glyph_type ;
}



/* Returns TRUE if bumping has been fixed to one type, FALSE otherwise. */
gboolean zmapStyleBumpIsFixed(ZMapFeatureTypeStyle style)
{
  gboolean is_fixed = FALSE ;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_BUMP_FIXED))
      is_fixed = style->bump_fixed;

  return is_fixed ;
}






/* Set up graphing stuff, currentlyends the basic code is copied from acedb but this will
 * change if we add different graphing types.... */
void zMapStyleSetGraph(ZMapFeatureTypeStyle style, ZMapStyleGraphMode mode,
		       double min_score, double max_score, double baseline)
{

  style->mode_data.graph.mode = mode ;
  zmapStyleSetIsSet(style,STYLE_PROP_GRAPH_MODE);
  style->mode_data.graph.baseline = baseline ;
  zmapStyleSetIsSet(style,STYLE_PROP_GRAPH_BASELINE);

  style->min_score = min_score ;
  zmapStyleSetIsSet(style,STYLE_PROP_MIN_SCORE);
  style->max_score = max_score ;
  zmapStyleSetIsSet(style,STYLE_PROP_MAX_SCORE);


  /* normalise the baseline */
  if (style->min_score == style->max_score)
    style->mode_data.graph.baseline = style->min_score ;
  else
    style->mode_data.graph.baseline = (style->mode_data.graph.baseline - style->min_score) / (style->max_score - style->min_score) ;

  if (style->mode_data.graph.baseline < 0)
    style->mode_data.graph.baseline = 0 ;
  if (style->mode_data.graph.baseline > 1)
    style->mode_data.graph.baseline = 1 ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* fmax seems only to be used to obtain the final column width in acedb, we can get this from its size... */

  bc->fmax = (bc->width * bc->histBase) + 0.2 ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  return ;
}




/* Set score bounds for displaying column with width related to score. */
void zMapStyleSetScore(ZMapFeatureTypeStyle style, double min_score, double max_score)
{

  style->min_score = min_score ;
  zmapStyleSetIsSet(style,STYLE_PROP_MIN_SCORE);
  style->max_score = max_score ;
  zmapStyleSetIsSet(style,STYLE_PROP_MAX_SCORE);

  return ;
}




/*!
 * \brief sets the end style of exons
 *
 * Controls the end of exons at the moment.  They are either pointy,
 * strand sensitive (when true) or square when false (default) */
void zMapStyleSetEndStyle(ZMapFeatureTypeStyle style, gboolean directional)
{
  style->directional_end = directional;
  zmapStyleSetIsSet(style,STYLE_PROP_DIRECTIONAL_ENDS);

  return;
}


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE

/* NOT USING THIS CURRENTLY, WILL BE NEEDED IN FUTURE... */

/* Score stuff is all quite interdependent, hence this function to set it all up. */
void zMapStyleSetScore(ZMapFeatureTypeStyle style, char *score_str,
		       double min_score, double max_score)
{
  ZMapStyleScoreMode score = ZMAPSCORE_WIDTH ;

  zMapAssert(style) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* WE ONLY SCORE BY WIDTH AT THE MOMENT..... */
  if (bump_str && *bump_str)
    {
      if (g_ascii_strcasecmp(bump_str, "bump") == 0)
	bump = ZMAPBUMP_OVERLAP ;
      else if (g_ascii_strcasecmp(bump_str, "position") == 0)
	bump = ZMAPBUMP_POSITION ;
      else if (g_ascii_strcasecmp(bump_str, "name") == 0)
	bump = ZMAPBUMP_NAME ;
      else if (g_ascii_strcasecmp(bump_str, "simple") == 0)
	bump = ZMAPBUMP_SIMPLE ;
    }
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  style->score_mode = score ;

  /* Make sure we have some kind of score. */
  if (style->max_score == style->min_score)
    style->max_score = style->min_score + 1 ;

  /* Make sure we have kind of width. */
  if (!(style->width))
    style->width = 2.0 ;

  zmapStyleSetIsSet(style,STYLE_PROP_MIN_SCORE);
  zmapStyleSetIsSet(style,STYLE_PROP_MAX_SCORE);
  zmapStyleSetIsSet(style,STYLE_PROP_SCORE_MODE);

  return ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



void zMapStyleSetStrandSpecific(ZMapFeatureTypeStyle type, gboolean strand_specific)
{
  setStrandFrameAttrs(type, &strand_specific, NULL, NULL) ;

  return ;
}

void zMapStyleSetStrandShowReverse(ZMapFeatureTypeStyle type, gboolean show_reverse)
{
  setStrandFrameAttrs(type, NULL, &show_reverse, NULL) ;

  return ;
}

void zMapStyleSetFrameMode(ZMapFeatureTypeStyle type, ZMapStyle3FrameMode frame_mode)
{
  setStrandFrameAttrs(type, NULL, NULL, &frame_mode) ;

  return ;
}


/* These attributes are not independent hence the bundling into this call, only one input can be
 * set at a time. */
static void setStrandFrameAttrs(ZMapFeatureTypeStyle type,
                        gboolean *strand_specific_in,
                        gboolean *show_rev_strand_in,
                        ZMapStyle3FrameMode *frame_mode_in)
{
  if (strand_specific_in)
    {
      zmapStyleSetIsSet(type,STYLE_PROP_STRAND_SPECIFIC);

      if (*strand_specific_in)
      {
        type->strand_specific = TRUE ;
      }
      else
      {
        type->strand_specific = FALSE ;

        zmapStyleUnsetIsSet(type,STYLE_PROP_SHOW_REV_STRAND);

        if (zMapStyleIsPropertySetId(type,STYLE_PROP_FRAME_MODE))
          type->frame_mode = ZMAPSTYLE_3_FRAME_NEVER ;
      }
    }
  else if (show_rev_strand_in)
    {
      zmapStyleSetIsSet(type,STYLE_PROP_SHOW_REV_STRAND);
      type->show_rev_strand = *show_rev_strand_in ;

      if (*show_rev_strand_in)
      {
         zmapStyleSetIsSet(type,STYLE_PROP_STRAND_SPECIFIC);
         type->strand_specific = TRUE ;
      }
    }
  else if (frame_mode_in)
    {
      if (*frame_mode_in != ZMAPSTYLE_3_FRAME_NEVER)
      {
        zmapStyleSetIsSet(type,STYLE_PROP_STRAND_SPECIFIC);
        type->strand_specific = TRUE ;

        zmapStyleSetIsSet(type,STYLE_PROP_FRAME_MODE);
        type->frame_mode = *frame_mode_in ;
      }
      else
      {
        zmapStyleUnsetIsSet(type,STYLE_PROP_STRAND_SPECIFIC);
        type->strand_specific = FALSE ;

        if (zMapStyleIsPropertySetId(type,STYLE_PROP_SHOW_REV_STRAND) && type->show_rev_strand)
            zmapStyleUnsetIsSet(type,STYLE_PROP_SHOW_REV_STRAND);

        zmapStyleSetIsSet(type,STYLE_PROP_FRAME_MODE);
        type->frame_mode = *frame_mode_in ;
      }
    }

  return ;
}

void zMapStyleGetStrandAttrs(ZMapFeatureTypeStyle type,
			     gboolean *strand_specific, gboolean *show_rev_strand, ZMapStyle3FrameMode *frame_mode)
{

  if (strand_specific && zMapStyleIsPropertySetId(type,STYLE_PROP_STRAND_SPECIFIC))
    *strand_specific = type->strand_specific ;
  if (show_rev_strand && zMapStyleIsPropertySetId(type,STYLE_PROP_SHOW_REV_STRAND))
    *show_rev_strand = type->show_rev_strand ;
  if (frame_mode && zMapStyleIsPropertySetId(type,STYLE_PROP_FRAME_MODE))
    *frame_mode = type->frame_mode ;

  return ;
}


gboolean zMapStyleIsFrameSpecific(ZMapFeatureTypeStyle style)
{
  gboolean frame_specific = FALSE ;

  if (zMapStyleIsPropertySetId(style,STYLE_PROP_FRAME_MODE) && style->frame_mode != ZMAPSTYLE_3_FRAME_NEVER)
    frame_specific = TRUE ;

  return frame_specific ;
}

gboolean zMapStyleIsFrameOneColumn(ZMapFeatureTypeStyle style)
{
  gboolean one_column = FALSE ;

  if (zMapStyleIsPropertySetId(style,STYLE_PROP_FRAME_MODE) && style->frame_mode == ZMAPSTYLE_3_FRAME_ONLY_1)
    one_column = TRUE ;

  return one_column ;
}


gboolean zMapStyleIsStrandSpecific(ZMapFeatureTypeStyle style)
{
  gboolean strand_specific = FALSE ;

  if (zMapStyleIsPropertySetId(style,STYLE_PROP_STRAND_SPECIFIC))
    strand_specific = style->strand_specific ;

  return strand_specific ;
}

gboolean zMapStyleIsShowReverseStrand(ZMapFeatureTypeStyle style)
{
  gboolean show_rev_strand = FALSE ;

  if (zMapStyleIsPropertySetId(style,STYLE_PROP_SHOW_REV_STRAND))
    show_rev_strand = style->show_rev_strand ;

  return show_rev_strand ;
}


void zMapStyleSetGFF(ZMapFeatureTypeStyle style, char *gff_source, char *gff_feature)
{

  if (gff_source && *gff_source)
    {
      style->gff_source = g_quark_from_string(gff_source) ;
      zmapStyleSetIsSet(style,STYLE_PROP_GFF_SOURCE);
    }

  if (gff_feature && *gff_feature)
    {
      style->gff_feature = g_quark_from_string(gff_feature) ;
      zmapStyleSetIsSet(style,STYLE_PROP_GFF_SOURCE);
    }

  return ;
}




void zMapStyleSetBumpMode(ZMapFeatureTypeStyle style, ZMapStyleBumpMode bump_mode)
{
  if(!(bump_mode >= ZMAPBUMP_START && bump_mode <= ZMAPBUMP_END))
      bump_mode  = ZMAPBUMP_UNBUMP;       // better than going Assert

  if (!zmapStyleBumpIsFixed(style))
    {
      // MH17: this looked wrong, refer to old version
      if(!zMapStyleIsPropertySetId(style,STYLE_PROP_BUMP_DEFAULT))
	{
	  zmapStyleSetIsSet(style,STYLE_PROP_BUMP_DEFAULT);
	  style->default_bump_mode = bump_mode ;
	}

      style->curr_bump_mode = bump_mode ;
      zmapStyleSetIsSet(style,STYLE_PROP_BUMP_MODE);
    }

  return ;
}

void zMapStyleSetBumpSpace(ZMapFeatureTypeStyle style, double bump_spacing)
{
  zmapStyleSetIsSet(style,STYLE_PROP_BUMP_SPACING);
  style->bump_spacing = bump_spacing ;

  return ;
}


double zMapStyleGetBumpSpace(ZMapFeatureTypeStyle style)
{
  double spacing = 0.0 ;


  if (zMapStyleIsPropertySetId(style,STYLE_PROP_BUMP_SPACING))
    spacing = style->bump_spacing ;

  return spacing ;
}



/* Reset bump mode to default and returns the default mode. */
ZMapStyleBumpMode zMapStyleResetBumpMode(ZMapFeatureTypeStyle style)
{
  ZMapStyleBumpMode default_mode = ZMAPBUMP_INVALID ;

  if (!zmapStyleBumpIsFixed(style))
    {
      default_mode = style->curr_bump_mode = style->default_bump_mode ;
    }

  return default_mode ;
}


/* Re/init bump mode. */
void zMapStyleInitBumpMode(ZMapFeatureTypeStyle style,
			   ZMapStyleBumpMode default_bump_mode, ZMapStyleBumpMode curr_bump_mode)
{
  zMapAssert(style
	     && (default_bump_mode ==  ZMAPBUMP_INVALID
		 || (default_bump_mode >= ZMAPBUMP_START && default_bump_mode <= ZMAPBUMP_END))
	     && (curr_bump_mode ==  ZMAPBUMP_INVALID
		 || (curr_bump_mode >= ZMAPBUMP_START && curr_bump_mode <= ZMAPBUMP_END))) ;

  if (!zmapStyleBumpIsFixed(style))
    {
      if (curr_bump_mode != ZMAPBUMP_INVALID)
	{
        zmapStyleSetIsSet(style,STYLE_PROP_BUMP_MODE);
	  style->curr_bump_mode = curr_bump_mode ;
	}

      if (default_bump_mode != ZMAPBUMP_INVALID)
	{
        zmapStyleSetIsSet(style,STYLE_PROP_BUMP_DEFAULT);
	  style->default_bump_mode = default_bump_mode ;
	}
    }

  return ;
}



/* Pretty brain dead but we need some way to deal with the situation where a style may differ in
 * case only betweens servers...perhaps we should not be merging....???
 * Caller must free returned string.
 *  */
char *zMapStyleCreateName(char *style)
{
  char *style_name ;
  GString *unique_style_name ;

  unique_style_name = g_string_new(style) ;
  unique_style_name = g_string_ascii_down(unique_style_name) ; /* does it in place */

  style_name = unique_style_name->str ;

  g_string_free(unique_style_name, FALSE) ;		    /* Do not free returned string. */

  return style_name ;
}


/* Like zMapStyleCreateName() but returns a quark representing the style name. */
GQuark zMapStyleCreateID(char *style)
{
  GQuark style_id = 0 ;
  char *style_name ;

  style_name = zMapStyleCreateName(style) ;

  style_id = g_quark_from_string(style_name) ;

  g_free(style_name) ;

  return style_id ;
}


gboolean zMapStyleDisplayInSeparator(ZMapFeatureTypeStyle style)
{
  gboolean separator_style = FALSE;

  if(zMapStyleIsPropertySetId(style,STYLE_PROP_SHOW_ONLY_IN_SEPARATOR))
      separator_style = style->show_only_in_separator;

  return separator_style;
}




gboolean zMapFeatureTypeSetAugment(GData **current, GData **new)
{
  gboolean result = FALSE ;

  zMapAssert(new && *new) ;

  if (!current || !*current)
    g_datalist_init(current) ;

  g_datalist_foreach(new, doTypeSets, (void *)current) ;

  result = TRUE ;					    /* currently shouldn't fail.... */

  return result ;
}


/* Check that every name has a style, if the style can't be found, remove the name from the list. */
gboolean zMapSetListEqualStyles(GList **feature_set_names, GList **styles)
{
  gboolean result = FALSE ;
  CheckSetListStruct check_list ;

  check_list.names = feature_set_names ;
  check_list.styles = styles ;
  check_list.any_style_found = FALSE ;

  g_list_foreach(*feature_set_names, checkListName, (gpointer)&check_list) ;

  if (check_list.any_style_found)
    result = TRUE ;

  return result ;
}


/* Merge new_styles into curr_styles. Rules are:
 *
 * if new_style is not in curr_styles its simply added, otherwise new_style
 * overloads curr_style.
 *
 *  */
GData *zMapStyleMergeStyles(GData *curr_styles, GData *new_styles, ZMapStyleMergeMode merge_mode)
{
  GData *merged_styles = NULL ;
  MergeStyleCBStruct merge_data = {FALSE} ;

  merge_data.merge_mode = merge_mode ;
  merge_data.curr_styles = curr_styles ;

  g_datalist_foreach(&new_styles, mergeStyle, &merge_data) ;

  merged_styles = merge_data.curr_styles ;

  return merged_styles ;
}


/* Returns a GData of all predefined styles, the user should free the list AND the styles when
 * they have finished with them. */
GData *zMapStyleGetAllPredefined(void)
{
  GData *style_list = NULL ;
  ZMapFeatureTypeStyle curr = NULL ;


  /* In some of the examples below we don't set strand_specific even though it seems like
   * we should (e.g. DNA), this is because what we really want is to only show the forward
   * version of the column. This points to a flaw or lack of a column type. */


  /* 3 Frame - meta mode controlling whether we do 3 frame display or not. */
  curr = zMapStyleCreate(ZMAP_FIXED_STYLE_3FRAME, ZMAP_FIXED_STYLE_3FRAME_TEXT) ;
  g_object_set(G_OBJECT(curr),
	       ZMAPSTYLE_PROPERTY_MODE,              ZMAPSTYLE_MODE_META,
	       ZMAPSTYLE_PROPERTY_BUMP_MODE,         ZMAPBUMP_UNBUMP,
	       ZMAPSTYLE_PROPERTY_DEFAULT_BUMP_MODE, ZMAPBUMP_UNBUMP,
	       ZMAPSTYLE_PROPERTY_DISPLAYABLE,       FALSE,
	       NULL) ;
  g_datalist_id_set_data(&style_list, curr->unique_id, curr) ;


  /* 3 Frame Translation */
  curr = zMapStyleCreate(ZMAP_FIXED_STYLE_3FT_NAME, ZMAP_FIXED_STYLE_3FT_NAME_TEXT);
  /* The translation width is the width for the whole column if
   * all three frames are displayed in one column.  When displayed
   * in the frame specfic mode the width of each of the columns
   * will be a third of this whole column value.  This is contrary
   * to all other columns.  This way the translation takes up the
   * same screen space whether it's displayed frame specific or
   * not.  I thought this was important considering the column is
   * very wide. */
  /* Despite seeming to be frame specific, they all get drawn in the
   * same column at the moment so it's not frame specific! */
  {
    char *colours = "normal fill white ; normal draw black ;" ;

    /* we need draw colour here as well.... */
    g_object_set(G_OBJECT(curr),
		 ZMAPSTYLE_PROPERTY_MODE,                 ZMAPSTYLE_MODE_PEP_SEQUENCE,
		 ZMAPSTYLE_PROPERTY_DISPLAYABLE,          TRUE,
		 ZMAPSTYLE_PROPERTY_DISPLAY_MODE,         ZMAPSTYLE_COLDISPLAY_HIDE,
		 ZMAPSTYLE_PROPERTY_BUMP_MODE,            ZMAPBUMP_ALL,
		 ZMAPSTYLE_PROPERTY_DEFAULT_BUMP_MODE,    ZMAPBUMP_ALL,
		 ZMAPSTYLE_PROPERTY_WIDTH,                300.0,
		 ZMAPSTYLE_PROPERTY_BUMP_SPACING,         10.0,
		 ZMAPSTYLE_PROPERTY_FRAME_MODE,           ZMAPSTYLE_3_FRAME_AS_WELL,

		 /* This seems unsupported actually.... */
		 ZMAPSTYLE_PROPERTY_BUMP_FIXED,           TRUE,

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
		 ZMAPSTYLE_PROPERTY_STRAND_SPECIFIC,      TRUE,
		 ZMAPSTYLE_PROPERTY_SHOW_REVERSE_STRAND,  FALSE,
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

		 ZMAPSTYLE_PROPERTY_COLOURS,              colours,
		 NULL);
  }
  g_datalist_id_set_data(&style_list, curr->unique_id, curr);


  /* DNA */
  curr = zMapStyleCreate(ZMAP_FIXED_STYLE_DNA_NAME,
			       ZMAP_FIXED_STYLE_DNA_NAME_TEXT);
  {
    char *colours = "normal fill white ; normal draw black ; selected fill red" ;

    g_object_set(G_OBJECT(curr),
		 ZMAPSTYLE_PROPERTY_MODE,                 ZMAPSTYLE_MODE_RAW_SEQUENCE,
		 ZMAPSTYLE_PROPERTY_DISPLAYABLE,          TRUE,
		 ZMAPSTYLE_PROPERTY_DISPLAY_MODE,         ZMAPSTYLE_COLDISPLAY_HIDE,
		 ZMAPSTYLE_PROPERTY_WIDTH,                300.0,
		 ZMAPSTYLE_PROPERTY_BUMP_MODE,            ZMAPBUMP_UNBUMP,
		 ZMAPSTYLE_PROPERTY_DEFAULT_BUMP_MODE,    ZMAPBUMP_UNBUMP,
		 ZMAPSTYLE_PROPERTY_BUMP_FIXED,           TRUE,

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
		 ZMAPSTYLE_PROPERTY_STRAND_SPECIFIC,      TRUE,
		 ZMAPSTYLE_PROPERTY_SHOW_REVERSE_STRAND,  FALSE,
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

		 ZMAPSTYLE_PROPERTY_COLOURS,              colours,
		 NULL);
  }
  g_datalist_id_set_data(&style_list, curr->unique_id, curr);


  /* Locus */
  curr = zMapStyleCreate(ZMAP_FIXED_STYLE_LOCUS_NAME,
			 ZMAP_FIXED_STYLE_LOCUS_NAME_TEXT) ;
  {
    char *colours = "normal fill white ; normal draw black" ;

    g_object_set(G_OBJECT(curr),
		 ZMAPSTYLE_PROPERTY_MODE,                 ZMAPSTYLE_MODE_TEXT,
		 ZMAPSTYLE_PROPERTY_DISPLAYABLE,          TRUE,
		 ZMAPSTYLE_PROPERTY_DISPLAY_MODE,         ZMAPSTYLE_COLDISPLAY_HIDE,
		 ZMAPSTYLE_PROPERTY_BUMP_MODE,         ZMAPBUMP_UNBUMP,
		 ZMAPSTYLE_PROPERTY_DEFAULT_BUMP_MODE, ZMAPBUMP_UNBUMP,
		 ZMAPSTYLE_PROPERTY_STRAND_SPECIFIC,      TRUE,
		 ZMAPSTYLE_PROPERTY_COLOURS,              colours,
		 NULL);
    g_datalist_id_set_data(&style_list, curr->unique_id, curr);
  }


  /* GeneFinderFeatures */
  curr = zMapStyleCreate(ZMAP_FIXED_STYLE_GFF_NAME,
			       ZMAP_FIXED_STYLE_GFF_NAME_TEXT);
  g_object_set(G_OBJECT(curr),
	       ZMAPSTYLE_PROPERTY_MODE,                 ZMAPSTYLE_MODE_META,
	       ZMAPSTYLE_PROPERTY_DISPLAYABLE,          FALSE,
	       ZMAPSTYLE_PROPERTY_DISPLAY_MODE,         ZMAPSTYLE_COLDISPLAY_HIDE,
	       ZMAPSTYLE_PROPERTY_BUMP_MODE,         ZMAPBUMP_UNBUMP,
	       ZMAPSTYLE_PROPERTY_DEFAULT_BUMP_MODE, ZMAPBUMP_UNBUMP,
		 ZMAPSTYLE_PROPERTY_BUMP_FIXED,         TRUE,
	       NULL);
  g_datalist_id_set_data(&style_list, curr->unique_id, curr);


  /* Scale Bar */
  curr = zMapStyleCreate(ZMAP_FIXED_STYLE_SCALE_NAME,
			       ZMAP_FIXED_STYLE_SCALE_TEXT);
  g_object_set(G_OBJECT(curr),
	       ZMAPSTYLE_PROPERTY_MODE,                 ZMAPSTYLE_MODE_META,
	       ZMAPSTYLE_PROPERTY_DISPLAYABLE,          FALSE,
	       ZMAPSTYLE_PROPERTY_DISPLAY_MODE,         ZMAPSTYLE_COLDISPLAY_HIDE,
	       ZMAPSTYLE_PROPERTY_BUMP_MODE,         ZMAPBUMP_UNBUMP,
	       ZMAPSTYLE_PROPERTY_DEFAULT_BUMP_MODE, ZMAPBUMP_UNBUMP,
		 ZMAPSTYLE_PROPERTY_BUMP_FIXED,         TRUE,
	       NULL);
  g_datalist_id_set_data(&style_list, curr->unique_id, curr);


  /* NEED TO CHECK THAT THIS ACTUALLY WORKS...DOESN'T SEEM TO JUST NOW...... */
  /* show translation in zmap */
  curr = zMapStyleCreate(ZMAP_FIXED_STYLE_SHOWTRANSLATION_NAME, ZMAP_FIXED_STYLE_SHOWTRANSLATION_TEXT);
  {
    char *colours = "normal fill white ; normal draw black ; selected fill light green ; selected draw black" ;

    g_object_set(G_OBJECT(curr),
		 ZMAPSTYLE_PROPERTY_MODE,                 ZMAPSTYLE_MODE_TEXT,
		 ZMAPSTYLE_PROPERTY_DISPLAYABLE,          TRUE,
		 ZMAPSTYLE_PROPERTY_DISPLAY_MODE,         ZMAPSTYLE_COLDISPLAY_HIDE,
		 ZMAPSTYLE_PROPERTY_BUMP_MODE,            ZMAPBUMP_UNBUMP,
		 ZMAPSTYLE_PROPERTY_DEFAULT_BUMP_MODE,    ZMAPBUMP_UNBUMP,
		 ZMAPSTYLE_PROPERTY_BUMP_FIXED,           TRUE,
		 ZMAPSTYLE_PROPERTY_WIDTH,                300.0,
		 ZMAPSTYLE_PROPERTY_STRAND_SPECIFIC,      TRUE,
		 ZMAPSTYLE_PROPERTY_COLOURS,              colours,
		 NULL);
  }
  g_datalist_id_set_data(&style_list, curr->unique_id, curr);


  /* strand separator */
  curr = zMapStyleCreate(ZMAP_FIXED_STYLE_STRAND_SEPARATOR,
			       ZMAP_FIXED_STYLE_STRAND_SEPARATOR_TEXT);
  g_object_set(G_OBJECT(curr),
	       ZMAPSTYLE_PROPERTY_MODE,                 ZMAPSTYLE_MODE_META,
	       ZMAPSTYLE_PROPERTY_DISPLAYABLE,          FALSE,
	       ZMAPSTYLE_PROPERTY_DISPLAY_MODE,         ZMAPSTYLE_COLDISPLAY_HIDE,
	       ZMAPSTYLE_PROPERTY_BUMP_MODE,         ZMAPBUMP_UNBUMP,
	       ZMAPSTYLE_PROPERTY_DEFAULT_BUMP_MODE, ZMAPBUMP_UNBUMP,
		 ZMAPSTYLE_PROPERTY_BUMP_FIXED,         TRUE,
	       NULL);
  g_datalist_id_set_data(&style_list, curr->unique_id, curr);


  /* Search results hits */
  curr = zMapStyleCreate(ZMAP_FIXED_STYLE_SEARCH_MARKERS_NAME,
			       ZMAP_FIXED_STYLE_SEARCH_MARKERS_TEXT);
  {
    char *colours = "normal fill red ; normal draw black" ;
    char *strand_colours = "normal fill green ; normal draw black" ;

    g_object_set(G_OBJECT(curr),
		 ZMAPSTYLE_PROPERTY_MODE,                   ZMAPSTYLE_MODE_BASIC,
		 ZMAPSTYLE_PROPERTY_DISPLAYABLE,            TRUE,
		 ZMAPSTYLE_PROPERTY_DISPLAY_MODE,           ZMAPSTYLE_COLDISPLAY_HIDE,
		 ZMAPSTYLE_PROPERTY_BUMP_MODE,           ZMAPBUMP_UNBUMP,
		 ZMAPSTYLE_PROPERTY_DEFAULT_BUMP_MODE,   ZMAPBUMP_UNBUMP,
		 ZMAPSTYLE_PROPERTY_BUMP_FIXED,         TRUE,
		 ZMAPSTYLE_PROPERTY_WIDTH,                  15.0,
		 ZMAPSTYLE_PROPERTY_STRAND_SPECIFIC,        FALSE,
		 ZMAPSTYLE_PROPERTY_SHOW_ONLY_IN_SEPARATOR, TRUE,
		 ZMAPSTYLE_PROPERTY_COLOURS,                colours,
		 ZMAPSTYLE_PROPERTY_REV_COLOURS,            strand_colours,
		 NULL);
  }
  g_datalist_id_set_data(&style_list, curr->unique_id, curr);


  /* Assembly path */
  curr = zMapStyleCreate(ZMAP_FIXED_STYLE_ASSEMBLY_PATH_NAME,
			 ZMAP_FIXED_STYLE_ASSEMBLY_PATH_TEXT) ;
  {
    char *colours = "normal fill gold ; normal border black ; selected fill orange ; selected border blue" ;
    char *non_path_colours = "normal fill brown ; normal border black ; selected fill red ; selected border black" ;

    g_object_set(G_OBJECT(curr),
		 ZMAPSTYLE_PROPERTY_MODE,                 ZMAPSTYLE_MODE_ASSEMBLY_PATH,
		 ZMAPSTYLE_PROPERTY_DISPLAYABLE,          TRUE,
		 ZMAPSTYLE_PROPERTY_DISPLAY_MODE,         ZMAPSTYLE_COLDISPLAY_SHOW,
		 ZMAPSTYLE_PROPERTY_WIDTH,                10.0,
		 ZMAPSTYLE_PROPERTY_BUMP_MODE,            ZMAPBUMP_ALTERNATING,
		 ZMAPSTYLE_PROPERTY_DEFAULT_BUMP_MODE,    ZMAPBUMP_ALTERNATING,
		 ZMAPSTYLE_PROPERTY_BUMP_FIXED,           TRUE,
		 ZMAPSTYLE_PROPERTY_COLOURS,              colours,
		 ZMAPSTYLE_PROPERTY_ASSEMBLY_PATH_NON_COLOURS, non_path_colours,
		 NULL);
  }
  g_datalist_id_set_data(&style_list, curr->unique_id, curr);


  return style_list ;
}



/* need a func to free a styles list here..... */
void zMapStyleDestroyStyles(GData **styles)
{
  g_datalist_foreach(styles, destroyStyle, NULL) ;

  g_datalist_clear(styles) ;

  return ;
}




/*! @} end of zmapstyles docs. */






/*
 *                  Internal routines
 */



static void doTypeSets(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapFeatureTypeStyle new_type = (ZMapFeatureTypeStyle)data ;
  GData **current_out = (GData **)user_data ;
  GData *current = *current_out ;

  if (!g_datalist_id_get_data(&current, key_id))
    {
      /* copy the struct and then add it to the current set. */
      ZMapFeatureTypeStyle type ;

      type = zMapFeatureStyleCopy(new_type) ;

      g_datalist_id_set_data(&current, key_id, type) ;
    }

  *current_out = current ;

  return ;
}


/* A GFunc() called from zMapSetListEqualStyles() to check that a given feature_set name
 * has a corresponding style. */
static void checkListName(gpointer data, gpointer user_data)
{
  CheckSetList check_list = (CheckSetList)user_data ;
  GList *style_item ;
  ZMapFeatureTypeStyle style ;

  if ((style_item = g_list_find_custom(*(check_list->styles), data, compareNameToStyle)))
    {
      style = (ZMapFeatureTypeStyle)(style_item->data) ;
      check_list->any_style_found = TRUE ;
    }
  else
    *(check_list->names) = g_list_remove(*(check_list->names), data) ;

  return ;
}


/* GCompareFunc () calling the given function which should return 0 when the desired element is found.
   The function takes two gconstpointer arguments, the GList element's data and the given user data.*/
static gint compareNameToStyle(gconstpointer glist_data, gconstpointer user_data)
{
  gint result = -1 ;
  ZMapFeatureTypeStyle style = (ZMapFeatureTypeStyle)glist_data ;
  GQuark set_name = GPOINTER_TO_INT(user_data) ;

  if (set_name == style->unique_id)
    result = 0 ;

  return result ;
}



/* Either merges a new style or adds it to current list. */
static void mergeStyle(GQuark style_id, gpointer data, gpointer user_data)
{
  ZMapFeatureTypeStyle new_style = (ZMapFeatureTypeStyle)data ;
  MergeStyleCB merge_data = (MergeStyleCB)user_data ;
  GData *curr_styles = merge_data->curr_styles ;
  ZMapFeatureTypeStyle curr_style = NULL ;


  /* If we find the style then process according to merge_mode, if not then add a copy to the curr_styles. */
  if ((curr_style = zMapFindStyle(curr_styles, new_style->unique_id)))
    {
      switch (merge_data->merge_mode)
	{
	case ZMAPSTYLE_MERGE_PRESERVE:
	  {
	    /* Leave the existing style untouched. */
	    break ;
	  }
	case ZMAPSTYLE_MERGE_REPLACE:
	  {
	    /* Remove the existing style and put the new one in its place. */
	    ZMapFeatureTypeStyle copied_style = NULL ;

	    g_datalist_id_remove_data(&curr_styles, curr_style->unique_id) ;
	    zMapStyleDestroy(curr_style) ;

	    copied_style = zMapFeatureStyleCopy(new_style);
          g_datalist_id_set_data(&curr_styles, new_style->unique_id, copied_style) ;

          merge_data->curr_styles = curr_styles ;

          break ;
	  }
	case ZMAPSTYLE_MERGE_MERGE:
	  {
	    /* Merge the existing and new styles. */
	    zMapStyleMerge(curr_style, new_style) ;

	    break ;
	  }
	default:
	  {
	    zMapLogFatalLogicErr("switch(), unknown value: %d", merge_data->merge_mode) ;

	    break ;
	  }

	}
    }
  else
    {
      ZMapFeatureTypeStyle copied_style = NULL ;

      copied_style = zMapFeatureStyleCopy(new_style);
	g_datalist_id_set_data(&curr_styles, new_style->unique_id, copied_style) ;
	merge_data->curr_styles = curr_styles ;
    }

  return ;
}



/* Destroy the given style. */
static void destroyStyle(GQuark style_id, gpointer data, gpointer user_data_unused)
{
  ZMapFeatureTypeStyle style = (ZMapFeatureTypeStyle)data ;

  zMapStyleDestroy(style);

  return ;
}



/* A GDataForeachFunc() to copy styles into a GDatalist */
static void copySetCB(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapFeatureTypeStyle curr_style = (ZMapFeatureTypeStyle)data ;
  CopyStyleCB cb_data = (CopyStyleCB)user_data ;
  ZMapFeatureTypeStyle copy_style ;
  GData *style_set = cb_data->copy_set ;

  zMapAssert(key_id == curr_style->unique_id) ;

  if ((copy_style = zMapFeatureStyleCopy(curr_style)))
    g_datalist_id_set_data(&style_set, key_id, copy_style) ;
  else
    *(cb_data->error) = TRUE ;

  cb_data->copy_set = style_set ;

  return ;
}





/* Functions to sort out the inheritance of styles by copying and overloading. */

/* A GDataForeachFunc() to ..... */
static void inheritCB(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapFeatureTypeStyle curr_style = (ZMapFeatureTypeStyle)data ;
  InheritAllCB cb_data = (InheritAllCB)user_data ;

  zMapAssert(key_id == curr_style->unique_id) ;

  /* If we haven't done this style yet then we need to do its inheritance. */
  if (!(g_datalist_id_get_data(&(cb_data->inherited_styles), key_id)))
    {
      if (!doStyleInheritance(&(cb_data->style_set), &(cb_data->inherited_styles), curr_style))
	{
	  cb_data->errors = TRUE ;
	}

      /* record that we have now done this style. */
      g_datalist_id_set_data(&(cb_data->inherited_styles), key_id, curr_style) ;
    }

  return ;
}



static gboolean doStyleInheritance(GData **style_set_inout, GData **inherited_styles_inout,
				   ZMapFeatureTypeStyle style)
{
  gboolean result = TRUE ;
  GData *style_set, *inherited_styles ;
  GQueue *style_queue ;
  gboolean parent ;
  ZMapFeatureTypeStyle curr_style ;
  GQuark prev_id, curr_id ;

  style_set = *style_set_inout ;
  inherited_styles = *inherited_styles_inout ;

  style_queue = g_queue_new() ;

  prev_id = curr_id = style->unique_id ;
  parent = TRUE ;
  do
    {
      if ((curr_style = zMapFindStyle(style_set, curr_id)))
	{
	  g_queue_push_head(style_queue, curr_style) ;

	  if (!zMapStyleIsPropertySetId(curr_style,STYLE_PROP_PARENT_STYLE))
	    parent = FALSE ;
	  else
	    {
	      prev_id = curr_id ;
	      curr_id = curr_style->parent_id ;
	    }
	}
      else
	{
	  result = FALSE ;

	  zMapLogWarning("Style \"%s\" has the the parent style \"%s\" "
			 "but the latter cannot be found in the styles set so cannot be inherited.",
			 g_quark_to_string(prev_id), g_quark_to_string(curr_id)) ;
	}
    } while (parent && result) ;

  /* If we only recorded the current style in our inheritance queue then there is no inheritance tree
   * so no need to do the inheritance. */
  if (result && g_queue_get_length(style_queue) > 1)
    {
      InheritAllCBStruct new_style = {FALSE} ;

      new_style.style_set = style_set ;
      new_style.inherited_styles = inherited_styles ;
      g_queue_foreach(style_queue, inheritAllFunc, &new_style) ;
      style_set = new_style.style_set ;
      inherited_styles = new_style.inherited_styles ;

      result = !new_style.errors ;
    }

  g_queue_free(style_queue) ;			    /* We only hold pointers here so no
						       data to free. */

  *style_set_inout = style_set ;
  *inherited_styles_inout = inherited_styles ;

  return result ;
}


/* A GFunc to take a style and replace it with one inherited from its parent.
 *
 * Note that if there is an error at any stage in processing the styles then we return NULL. */
static void inheritAllFunc(gpointer data, gpointer user_data)
{
  ZMapFeatureTypeStyle curr_style = (ZMapFeatureTypeStyle)data ;
  InheritAllCB inherited = (InheritAllCB)user_data ;

  if (!inherited->errors)
    {
      ZMapFeatureTypeStyle prev_style = inherited->prev_style, tmp_style ;

      if (!prev_style)
	{
	  inherited->prev_style = curr_style ;
	}
      else
	{
	  tmp_style = zMapFeatureStyleCopy(prev_style) ;

	  if (zMapStyleMerge(tmp_style, curr_style))
	    {
	      inherited->prev_style = tmp_style ;

	      /* The g_datalist call overwrites the old style reference with the new one, we then
	       * delete the old one. */
	      g_datalist_id_set_data(&(inherited->style_set), tmp_style->unique_id, tmp_style) ;
	      tmp_style = curr_style ;			    /* Make sure we destroy the old style. */

	    }
	  else
	    {
	      inherited->errors = TRUE ;

	      zMapLogWarning("Merge of style \"%s\" into style \"%s\" failed.",
			     g_quark_to_string(curr_style->original_id), g_quark_to_string(tmp_style->original_id)) ;
	    }

	  zMapStyleDestroy(tmp_style) ;
	}
    }

  return ;
}





