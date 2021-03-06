/*  File: zmapConfigStyleDefaults.h
 *  Author: Roy Storey (rds@sanger.ac.uk)
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
 * Description: Defines various strings etc. that are required by the
 *              configuration routines.
 *
 *-------------------------------------------------------------------
 */

#ifndef ZMAP_CONFIG_STYLE_DEFAULTS_H
#define ZMAP_CONFIG_STYLE_DEFAULTS_H


/*!
 * Here are a list of predefined/fixed styles which zmap uses. Styles with these names
 * _MUST_ be available from the server with the given name (case insensitive),
 * if they are not then various types of display (dna, 3 frame) are not possible.
 *
 * ZMap will fill in the style with some reasonable defaults, which
 * users may overload/override, so long as they use these names.
 *
 * - 3 Frame                  controls 3 frame display
 * - 3 Frame Translation      controls 3 frame protein translation
 *
 * - DNA                      controls dna sequence display
 *
 * - Locus                    controls display of a column of locus names
 *
 * - GeneFinderFeatures       controls fetching/display of gene finder features
 *
 * - Assembly_path            display of clone/tiling path for virtual sequence
 * 
 * - Annotation               display an annotation column for editing temporary features
 *
 * - hand_built               a column used by acedb as a default place for newly created features
 *
 */

#define TEXT_PREFIX "Predefined method: required for "

#define ZMAP_FIXED_STYLE_PLAIN_NAME   "Plain"
#define ZMAP_FIXED_STYLE_PLAIN_TEXT TEXT_PREFIX "miscellaneous."

#define ZMAP_FIXED_STYLE_3FRAME   "3 Frame"
#define ZMAP_FIXED_STYLE_3FRAME_TEXT TEXT_PREFIX "3 frame display."

#define ZMAP_FIXED_STYLE_3FT_NAME "3 Frame Translation"
#define ZMAP_FIXED_STYLE_3FT_NAME_TEXT TEXT_PREFIX "3 frame translation display."

#define ZMAP_FIXED_STYLE_DNA_NAME "DNA"
#define ZMAP_FIXED_STYLE_DNA_NAME_TEXT TEXT_PREFIX "dna display (and hence also for dna translation display)."

#define ZMAP_FIXED_STYLE_LOCUS_NAME "Locus"
#define ZMAP_FIXED_STYLE_LOCUS_NAME_TEXT TEXT_PREFIX "locus name text column display."

#define ZMAP_FIXED_STYLE_GFF_NAME "GeneFinderFeatures"
#define ZMAP_FIXED_STYLE_GFF_NAME_TEXT TEXT_PREFIX "Gene Finder Features display."

#define ZMAP_FIXED_STYLE_SCALE_NAME "scale"
#define ZMAP_FIXED_STYLE_SCALE_TEXT TEXT_PREFIX "displaying a scale bar"

#define ZMAP_FIXED_STYLE_SHOWTRANSLATION_NAME "Show Translation"
#define ZMAP_FIXED_STYLE_SHOWTRANSLATION_TEXT "show translation in zmap display"

#define ZMAP_FIXED_STYLE_STRAND_SEPARATOR "Strand Separator"
#define ZMAP_FIXED_STYLE_STRAND_SEPARATOR_TEXT TEXT_PREFIX "strand separator display"

#define ZMAP_FIXED_STYLE_SEARCH_MARKERS_NAME "Search Hit Marker"
#define ZMAP_FIXED_STYLE_SEARCH_MARKERS_TEXT TEXT_PREFIX "display location of matches to query."

#define ZMAP_FIXED_STYLE_ASSEMBLY_PATH_NAME "Assembly path"
#define ZMAP_FIXED_STYLE_ASSEMBLY_PATH_TEXT TEXT_PREFIX "assembly path for displayed sequence."

#define ZMAP_FIXED_STYLE_SCRATCH_NAME "Annotation"
#define ZMAP_FIXED_STYLE_SCRATCH_TEXT TEXT_PREFIX "annotation column for creating and editing temporary features."

#define ZMAP_FIXED_STYLE_HAND_BUILT_NAME "hand_built"
#define ZMAP_FIXED_STYLE_HAND_BUILT_TEXT TEXT_PREFIX "column for displaying hand-built features."

#define ZMAP_FIXED_STYLE_ORF_NAME "ORF"
#define ZMAP_FIXED_STYLE_ORF_TEXT TEXT_PREFIX "open reading frame display"


/* Default glyphs used both internally and made externally available via the styles config. file. */
#define ZMAP_FIXED_STYLE_HOMOLOGY_GLYPH "Homology glyph"
#define ZMAP_FIXED_STYLE_HOMOLOGY_UP "up-tri"
#define ZMAP_FIXED_STYLE_HOMOLOGY_UP_SHAPE "<0,-4 ;-4,0 ;4,0 ;0,-4>"
#define ZMAP_FIXED_STYLE_HOMOLOGY_DOWN "dn-tri",
#define ZMAP_FIXED_STYLE_HOMOLOGY_DOWN_SHAPE "<0,4; -4,0 ;4,0; 0,4>"

#define ZMAP_FIXED_STYLE_SPLICE_MARKER_GLYPH "Gene Finder Splice Markers"
#define ZMAP_FIXED_STYLE_SPLICE_MARKER_UP "up-hook"
#define ZMAP_FIXED_STYLE_SPLICE_MARKER_UP_SHAPE "up-hook = <0,0; 8,0; 8,-8>"
#define ZMAP_FIXED_STYLE_SPLICE_MARKER_DOWN "dn-hook",
#define ZMAP_FIXED_STYLE_SPLICE_MARKER_DOWN_SHAPE "<0,0; 8,0; 8,8>"

#define ZMAP_FIXED_STYLE_SPLICE_MARKER_GLYPH "Gene Finder Splice Markers"
#define ZMAP_FIXED_STYLE_SPLICE_MARKER_UP "up-hook"
#define ZMAP_FIXED_STYLE_SPLICE_MARKER_UP_SHAPE "up-hook = <0,0; 8,0; 8,-8>"
#define ZMAP_FIXED_STYLE_SPLICE_MARKER_DOWN "dn-hook",
#define ZMAP_FIXED_STYLE_SPLICE_MARKER_DOWN_SHAPE "<0,0; 8,0; 8,8>"







/* The opts struct */
#define ZMAP_STYLE_DEFAULT_HIDE_INITIALLY  FALSE
#define ZMAP_STYLE_DEFAULT_SHOW_WHEN_EMPTY FALSE
#define ZMAP_STYLE_DEFAULT_SHOW_TEXT       FALSE
#define ZMAP_STYLE_DEFAULT_PARSE_GAPS      TRUE
#define ZMAP_STYLE_DEFAULT_ALIGN_GAPS      TRUE
#define ZMAP_STYLE_DEFAULT_STRAND_SPECIFIC TRUE
#define ZMAP_STYLE_DEFAULT_FRAME_SPECIFIC  FALSE
#define ZMAP_STYLE_DEFAULT_SHOW_REV_STRAND TRUE
#define ZMAP_STYLE_DEFAULT_DIRECTIONAL_END FALSE

/* The colours struct */
#define ZMAP_STYLE_DEFAULT_OUTLINE    "black"
#ifdef RDS_THESE_DEFAULT_TO_TRANSPARENT
#define ZMAP_STYLE_DEFAULT_FOREGROUND "white"
#define ZMAP_STYLE_DEFAULT_BACKGROUND "white"
#endif  /* RDS_THESE_DEFAULT_TO_TRANSPARENT */

/* Overlap */
#define ZMAP_STYLE_DEFAULT_OVERLAP_MODE 1

/* Magnification */
#define ZMAP_STYLE_DEFAULT_MIN_MAG 10.0
#define ZMAP_STYLE_DEFAULT_MAX_MAG 10.0

/* Widths */
#define ZMAP_STYLE_DEFAULT_WIDTH      10.0
#define ZMAP_STYLE_DEFAULT_BUMP_WIDTH 10.0

/* Scores */
#define ZMAP_STYLE_DEFAULT_SCORE_MODE 1
#define ZMAP_STYLE_DEFAULT_MIN_SCORE 10.0
#define ZMAP_STYLE_DEFAULT_MAX_SCORE 10.0




#endif  /* ZMAP_CONFIG_STYLE_DEFAULTS_H */
