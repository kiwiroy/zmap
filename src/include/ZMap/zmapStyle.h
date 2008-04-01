/*  File: zmapStyle.h
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2007: Genome Research Ltd.
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
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *      Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk
 *
 * Description: Style and Style set handling functions.
 *
 * HISTORY:
 * Last edited: Mar 31 14:26 2008 (edgrif)
 * Created: Mon Feb 26 09:28:26 2007 (edgrif)
 * CVS info:   $Id: zmapStyle.h,v 1.19 2008-04-01 13:04:58 edgrif Exp $
 *-------------------------------------------------------------------
 */
#ifndef ZMAP_STYLE_H
#define ZMAP_STYLE_H


/* The opaque struct representing a style. */
typedef struct ZMapFeatureTypeStyleStruct_ *ZMapFeatureTypeStyle ;




/* Specifies how features that reference this style will be processed.
 *
 * ZMAPSTYLE_MODE_INVALID         invalid
 * ZMAPSTYLE_MODE_BASIC		  Basic box features
 * ZMAPSTYLE_MODE_TRANSCRIPT      Usual transcript like structure
 * ZMAPSTYLE_MODE_ALIGNMENT       Usual homology structure
 * ZMAPSTYLE_MODE_RAW_SEQUENCE    DNA sequence
 * ZMAPSTYLE_MODE_PEP_SEQUENCE    Peptide sequence
 * ZMAPSTYLE_MODE_TEXT            Text only display
 * ZMAPSTYLE_MODE_GRAPH		  Graphs of various types
 * ZMAPSTYLE_MODE_GLYPH		  Glyphs/shapes of various types
 * ZMAPSTYLE_MODE_META            Feature to be processed as meta object controlling other features
 */
#define ZMAP_MODE_METADEFS(DEF_MACRO) \
  DEF_MACRO(ZMAPSTYLE_MODE_INVALID)\
  DEF_MACRO(ZMAPSTYLE_MODE_BASIC)\
  DEF_MACRO(ZMAPSTYLE_MODE_ALIGNMENT)\
  DEF_MACRO(ZMAPSTYLE_MODE_TRANSCRIPT)\
  DEF_MACRO(ZMAPSTYLE_MODE_RAW_SEQUENCE)\
  DEF_MACRO(ZMAPSTYLE_MODE_PEP_SEQUENCE)\
  DEF_MACRO(ZMAPSTYLE_MODE_TEXT)\
  DEF_MACRO(ZMAPSTYLE_MODE_GRAPH)\
  DEF_MACRO(ZMAPSTYLE_MODE_GLYPH)\
  DEF_MACRO(ZMAPSTYLE_MODE_META)

#define ZMAP_MODE_ENUM( Name ) Name,

typedef enum
{
  ZMAP_MODE_METADEFS( ZMAP_MODE_ENUM )
} ZMapStyleMode ;


typedef enum
  {
    ZMAPSTYLE_COLDISPLAY_INVALID,
    ZMAPSTYLE_COLDISPLAY_HIDE,				    /* Never show. */
    ZMAPSTYLE_COLDISPLAY_SHOW_HIDE,			    /* Show according to zoom/mag, mark etc. */
    ZMAPSTYLE_COLDISPLAY_SHOW				    /* Always show. */
  } ZMapStyleColumnDisplayState ;


/* Specifies the style of graph. */
typedef enum
  {
    ZMAPSTYLE_GRAPH_INVALID,				    /* Initial setting. */
    ZMAPSTYLE_GRAPH_LINE,				    /* Just points joining a line. */
    ZMAPSTYLE_GRAPH_HISTOGRAM				    /* Usual blocky like graph. */
  } ZMapStyleGraphMode ;


/* Specifies the style of glyph. */
typedef enum
  {
    ZMAPSTYLE_GLYPH_INVALID,				    /* Initial setting. */
    ZMAPSTYLE_GLYPH_SPLICE
  } ZMapStyleGlyphMode ;



/* For drawing/colouring the various parts of a feature. */
typedef enum
  {
    ZMAPSTYLE_DRAW_INVALID,
    ZMAPSTYLE_DRAW_FILL,
    ZMAPSTYLE_DRAW_DRAW,
    ZMAPSTYLE_DRAW_BORDER
  } ZMapStyleDrawContext ;


/* Specifies type of colour, e.g. normal or selected. */
typedef enum
  {
    ZMAPSTYLE_COLOURTYPE_NORMAL,
    ZMAPSTYLE_COLOURTYPE_SELECTED
  } ZMapStyleColourType ;


typedef enum
  {
    ZMAPSTYLE_COLOURTARGET_NORMAL,
    ZMAPSTYLE_COLOURTARGET_FRAME0,
    ZMAPSTYLE_COLOURTARGET_FRAME1,
    ZMAPSTYLE_COLOURTARGET_FRAME2,
    ZMAPSTYLE_COLOURTARGET_CDS
  } ZMapStyleColourTarget ;



/* Specifies how wide features should be in relation to their score. */
typedef enum
  {
    ZMAPSCORE_WIDTH,					    /* Use column width only - default. */
    ZMAPSCORE_OFFSET,
    ZMAPSCORE_HISTOGRAM,
    ZMAPSCORE_PERCENT
  } ZMapStyleScoreMode ;


/* Specifies how features in columns should be overlapped for compact display. */
typedef enum
  {
    ZMAPOVERLAP_START,
    ZMAPOVERLAP_INVALID = ZMAPOVERLAP_START,
    ZMAPOVERLAP_COMPLETE,				    /* draw on top - default */
    ZMAPOVERLAP_OVERLAP,				    /* bump if feature coords overlap. */
    ZMAPOVERLAP_POSITION,				    /* bump if features start at same coord. */
    ZMAPOVERLAP_NAME,					    /* one column per homol target */
    ZMAPOVERLAP_COMPLEX,				    /* all features with same name in a
							       single column, several names in one
							       column but no 2 features overlap. */
    ZMAPOVERLAP_COMPLEX_RANGE,				    /* All features with same name in a
							       single column if they overlap the
							       focus range, all other features in
							       a single column.  */
    ZMAPOVERLAP_NO_INTERLEAVE,				    /* all features with same name in a
							       single column, several names in one
							       column but no interleaving of sets
							       of features. */
    ZMAPOVERLAP_ENDS_RANGE,				    /* Sort by 5' and 3' best/biggest
							       matches, one match per column, very
							       fmap like but better... */

    ZMAPOVERLAP_COMPLEX_LIMIT,				    /* Constrain matches to be colinear
							       within range or are truncated. */

    ZMAPOVERLAP_OSCILLATE,                                  /* Oscillate between one column and another... 
                                                               Useful for displaying tile paths. */

    ZMAPOVERLAP_ITEM_OVERLAP,                               /* bump if item coords overlap in canvas space... */
    ZMAPOVERLAP_SIMPLE,					    /* one column per feature, for testing... */
    ZMAPOVERLAP_END
  } ZMapStyleOverlapMode ;



gboolean zMapStyleNameCompare(ZMapFeatureTypeStyle style, char *name) ;
gboolean zMapStyleDisplayValid(ZMapFeatureTypeStyle style, GError **error) ;
gboolean zMapStyleIsTrueFeature(ZMapFeatureTypeStyle style) ;
GQuark zMapStyleGetID(ZMapFeatureTypeStyle style) ;
GQuark zMapStyleGetUniqueID(ZMapFeatureTypeStyle style) ;
char *zMapStyleGetDescription(ZMapFeatureTypeStyle style) ;

gboolean zMapStyleGetColoursCDSDefault(ZMapFeatureTypeStyle style, 
				       GdkColor **background, GdkColor **foreground, GdkColor **outline) ;
gboolean zMapStyleIsColour(ZMapFeatureTypeStyle style, ZMapStyleDrawContext colour_context) ;
gboolean zMapStyleIsBackgroundColour(ZMapFeatureTypeStyle style) ;
gboolean zMapStyleIsForegroundColour(ZMapFeatureTypeStyle style) ;
gboolean zMapStyleIsOutlineColour(ZMapFeatureTypeStyle style) ;

gboolean zMapStyleSetColours(ZMapFeatureTypeStyle style, ZMapStyleColourTarget target, ZMapStyleColourType type,
			     char *fill, char *draw, char *border) ;
gboolean zMapStyleGetColours(ZMapFeatureTypeStyle style, ZMapStyleColourTarget target, ZMapStyleColourType type,
			     GdkColor **fill, GdkColor **draw, GdkColor **border) ;

ZMapStyleMode zMapStyleGetMode(ZMapFeatureTypeStyle style) ;
const char *zMapStyleMode2Str(ZMapStyleMode mode) ;

void zMapStyleSetGFF(ZMapFeatureTypeStyle style, char *gff_source, char *gff_feature) ;
char *zMapStyleGetGFFSource(ZMapFeatureTypeStyle style) ;
char *zMapStyleGetGFFFeature(ZMapFeatureTypeStyle style) ;

gboolean zMapStyleIsDirectionalEnd(ZMapFeatureTypeStyle style) ;

void zMapStyleSetDisplayable(ZMapFeatureTypeStyle style, gboolean displayable) ;
gboolean zMapStyleIsDisplayable(ZMapFeatureTypeStyle style) ;
void zMapStyleSetDisplay(ZMapFeatureTypeStyle style, ZMapStyleColumnDisplayState col_show) ;
ZMapStyleColumnDisplayState zMapStyleGetDisplay(ZMapFeatureTypeStyle style) ;
gboolean zMapStyleIsHidden(ZMapFeatureTypeStyle style) ;


void zMapStyleSetShowWhenEmpty(ZMapFeatureTypeStyle style, gboolean show_when_empty) ;


gboolean zMapStyleIsFrameSpecific(ZMapFeatureTypeStyle style) ;
gboolean zMapStyleIsStrandSpecific(ZMapFeatureTypeStyle style) ;
gboolean zMapStyleIsShowReverseStrand(ZMapFeatureTypeStyle style) ;

double zMapStyleGetWidth(ZMapFeatureTypeStyle style) ;
double zMapStyleGetMaxScore(ZMapFeatureTypeStyle style) ;
double zMapStyleGetMinScore(ZMapFeatureTypeStyle style) ;

double zMapStyleBaseline(ZMapFeatureTypeStyle style) ;

gboolean zMapStyleIsMinMag(ZMapFeatureTypeStyle style, double *min_mag) ;
gboolean zMapStyleIsMaxMag(ZMapFeatureTypeStyle style, double *max_mag) ;
void zMapStyleSetMag(ZMapFeatureTypeStyle style, double min_mag, double max_mag) ;
double zMapStyleGetMinMag(ZMapFeatureTypeStyle style) ;
double zMapStyleGetMaxMag(ZMapFeatureTypeStyle style) ;




/* Lets change all these names to just be zmapStyle, i.e. lose the featuretype bit..... */

ZMapFeatureTypeStyle zMapFeatureTypeCreate(char *name, char *description) ;

void zMapStyleSetDescription(ZMapFeatureTypeStyle style, char *description) ;
void zMapStyleSetParent(ZMapFeatureTypeStyle style, char *parent_name) ;
void zMapStyleSetMode(ZMapFeatureTypeStyle style, ZMapStyleMode mode) ;
gboolean zMapStyleHasMode(ZMapFeatureTypeStyle style);
ZMapStyleMode zMapStyleGetMode(ZMapFeatureTypeStyle style) ;
void zMapStyleSetWidth(ZMapFeatureTypeStyle style, double width) ;
double zMapStyleGetWidth(ZMapFeatureTypeStyle style) ;
void zMapStyleSetBumpWidth(ZMapFeatureTypeStyle style, double bump_width) ;
double zMapStyleGetBumpWidth(ZMapFeatureTypeStyle style) ;

gboolean zMapStyleFormatMode(char *mode_str, ZMapStyleMode *mode_out) ;



void zMapStyleSetScore(ZMapFeatureTypeStyle style, double min_score, double max_score) ;
void zMapStyleSetGraph(ZMapFeatureTypeStyle style, ZMapStyleGraphMode mode, double min, double max, double baseline) ;
void zMapStyleSetStrandAttrs(ZMapFeatureTypeStyle type,
			     gboolean strand_specific, gboolean frame_specific,
			     gboolean show_rev_strand, gboolean show_as_3_frame) ;
void zMapStyleGetStrandAttrs(ZMapFeatureTypeStyle type,
			     gboolean *strand_specific, gboolean *frame_specific,
			     gboolean *show_rev_strand, gboolean *show_as_3_frame) ;
void zMapStyleSetEndStyle(ZMapFeatureTypeStyle style, gboolean directional) ;

void zMapStyleSetGappedAligns(ZMapFeatureTypeStyle style, gboolean parse_gaps, unsigned int within_align_error) ;
gboolean zMapStyleGetGappedAligns(ZMapFeatureTypeStyle style, unsigned int *within_align_error) ;
void zMapStyleSetJoinAligns(ZMapFeatureTypeStyle style, unsigned int between_align_error) ;
gboolean zMapStyleGetJoinAligns(ZMapFeatureTypeStyle style, unsigned int *between_align_error) ;
gboolean zMapStyleGetParseGaps(ZMapFeatureTypeStyle style) ;
unsigned int zmapStyleGetWithinAlignError(ZMapFeatureTypeStyle style) ;
gboolean zMapStyleIsParseGaps(ZMapFeatureTypeStyle style) ;
gboolean zMapStyleIsAlignGaps(ZMapFeatureTypeStyle style) ;
void zMapStyleSetAlignGaps(ZMapFeatureTypeStyle style, gboolean show_gaps) ;




void zMapStyleSetGlyphMode(ZMapFeatureTypeStyle style, ZMapStyleGlyphMode glyph_mode) ;

char *zMapStyleCreateName(char *style_name) ;
GQuark zMapStyleCreateID(char *style_name) ;
char *zMapStyleGetName(ZMapFeatureTypeStyle style) ;
GQuark zMapStyleGetID(ZMapFeatureTypeStyle style) ;

void zMapFeatureTypeDestroy(ZMapFeatureTypeStyle type) ;
ZMapFeatureTypeStyle zMapStyleGetPredefined(char *style_name) ;
gboolean zMapFeatureTypeSetAugment(GData **current, GData **new) ;

void zMapStyleInitOverlapMode(ZMapFeatureTypeStyle style, 
			      ZMapStyleOverlapMode default_overlap_mode, ZMapStyleOverlapMode curr_overlap_mode) ;
void zMapStyleSetBump(ZMapFeatureTypeStyle type, char *bump) ;
ZMapStyleOverlapMode zMapStyleGetOverlapMode(ZMapFeatureTypeStyle style) ;
void zMapStyleSetOverlapMode(ZMapFeatureTypeStyle style, ZMapStyleOverlapMode overlap_mode) ;
ZMapStyleOverlapMode zMapStyleResetOverlapMode(ZMapFeatureTypeStyle style) ;

ZMapFeatureTypeStyle zMapFeatureStyleCopy(ZMapFeatureTypeStyle style) ;
gboolean zMapStyleMerge(ZMapFeatureTypeStyle curr_style, ZMapFeatureTypeStyle new_style) ;

unsigned int zmapStyleGetWithinAlignError(ZMapFeatureTypeStyle style) ;


GData *zMapFeatureTypeGetFromFile(char *types_file) ;



/* Style set functions... */

gboolean zMapStyleSetAdd(GData **style_set, ZMapFeatureTypeStyle style) ;
void zMapFeatureTypePrintAll(GData *type_set, char *user_string) ;
void zMapFeatureStylePrintAll(GList *styles, char *user_string) ;
gboolean zMapStyleInheritAllStyles(GData **style_set) ;
gboolean zMapStyleNameExists(GList *style_name_list, char *style_name) ;
ZMapFeatureTypeStyle zMapFindStyle(GData *styles, GQuark style_id) ;
GList *zMapStylesGetNames(GData *styles) ;
GData *zMapStyleGetAllPredefined(void) ;
GData *zMapStyleMergeStyles(GData *curr_styles, GData *new_styles) ;
void zMapStyleDestroyStyles(GData **styles) ;



#endif /* ZMAP_STYLE_H */
