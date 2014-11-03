/*  File: zmapFeature_P.h
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2006-2014: Genome Research Ltd.
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
 *         Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk,
 *     Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *
 * Description: Internals for zmapFeature routines.
 *
 *-------------------------------------------------------------------
 */
#ifndef ZMAP_FEATURE_P_H
#define ZMAP_FEATURE_P_H

#include <ZMap/zmapFeature.h>



/* Used to construct a table of strings and corresponding enums to go between the two,
 * the table must end with an entry in which type_str is NULL. */
typedef struct
{
  char *type_str ;
  int type_enum ;
} ZMapFeatureStr2EnumStruct, *ZMapFeatureStr2Enum ;


#define zmapFeatureSwop(TYPE, FIRST, SECOND)   \
  { TYPE tmp = (FIRST) ; (FIRST) = (SECOND) ; (SECOND) = tmp ; }


#define zmapFeatureInvert(COORD, SEQ_START, SEQ_END)	\
  (COORD) = (SEQ_END) - (COORD) + 1


#define zmapFeatureRevComp(TYPE, SEQ_START, SEQ_END, COORD_1, COORD_2)  \
  { \
    zmapFeatureSwop(TYPE, COORD_1, COORD_2) ;              \
    zmapFeatureInvert(COORD_1, SEQ_START, SEQ_END) ;       \
    zmapFeatureInvert(COORD_2, SEQ_START, SEQ_END) ;       \
  }


#define zmapFeature2HashKey(FEATURE_ANY)  \
  GINT_TO_POINTER((FEATURE_ANY)->unique_id)

void zmapPrintFeatureContext(ZMapFeatureContext context) ;
gboolean zmapStr2Enum(ZMapFeatureStr2Enum type_table, char *type_str, int *type_out) ;


gboolean zmapFeatureBasicHasMatchingBoundary(ZMapFeature feature,
                                             int boundary_start, int boundary_end,
                                             int *boundary_start_out, int *boundary_end_out) ;
gboolean zmapFeatureAlignmentHasMatchingBoundary(ZMapFeature feature,
                                                 int boundary_start, int boundary_end,
                                                 int *boundary_start_out, int *boundary_end_out) ;
gboolean zmapFeatureTranscriptHasMatchingBoundary(ZMapFeature feature,
                                                  int boundary_start, int boundary_end,
                                                  int *boundary_start_out, int *boundary_end_out) ;
GList *zmapFeatureGetSubparts(ZMapFeature feature) ;

void zmapFeature3FrameTranslationSetRevComp(ZMapFeatureSet feature_set, int block_start, int block_end) ;
void zmapFeatureORFSetRevComp(ZMapFeatureSet feature_set, ZMapFeatureSet translation_fs) ;

gboolean zmapFeatureCoordsMatch(int slop, int boundary_start, int boundary_end,
                                int start, int end, int *match_start_out, int *match_end_out) ;

int zmapFeatureDNACalculateVariationDiff(const int start, 
                                         const int end,
                                         GList *variations) ;


#endif /* !ZMAP_FEATURE_P_H */
