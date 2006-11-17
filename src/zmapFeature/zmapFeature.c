/*  File: zmapViewFeatures.c
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
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk
 *
 * Description: Implements feature contexts, sets and features themselves.
 *              Includes code to create/merge/destroy contexts and sets.
 *              
 * Exported functions: See zmapView_P.h
 * HISTORY:
 * Last edited: Nov 17 17:29 2006 (edgrif)
 * Created: Fri Jul 16 13:05:58 2004 (edgrif)
 * CVS info:   $Id: zmapFeature.c,v 1.48 2006-11-17 17:31:23 edgrif Exp $
 *-------------------------------------------------------------------
 */

#include <stdio.h>
#include <strings.h>
#include <glib.h>
#include <ZMap/zmapGLibUtils.h>
#include <ZMap/zmapUtils.h>
#include <zmapFeature_P.h>
#include <ZMap/zmapPeptide.h>




/*! @defgroup zmapfeatures   zMapFeatures: feature handling for ZMap
 * @{
 * 
 * \brief  Feature handling for ZMap.
 * 
 * zMapFeatures routines provide functions to create/modify/destroy individual
 * features, sets of features and feature contexts (contexts are sets of sets
 * of features with associated coordinate data for parent mapping etc.).
 *
 *  */


/* Could use just one struct...but one step at a time.... */


/* this may not be needed..... */
typedef struct
{
  GData **current_feature_sets ;
  GData **diff_feature_sets ;
  GData **new_feature_sets ;
} FeatureContextsStruct, *FeatureContexts ;



typedef struct
{
  GData **current_feature_sets ;
  GData **diff_feature_sets ;
  GData **new_feature_sets ;
} FeatureBlocksStruct, *FeatureBlocks ;


typedef struct
{
  GData **current_features ;
  GData **diff_features ;
  GData **new_features ;
} FeatureSetStruct, *FeatureSet ;



/* These are GData callbacks. */
static void doNewFeatureSets(GQuark key_id, gpointer data, gpointer user_data) ;
static void doNewFeatures(GQuark key_id, gpointer data, gpointer user_data) ;

static void destroyFeature(gpointer data) ;
static void removeNotFreeFeature(GQuark key_id, gpointer data, gpointer user_data) ;

static void destroyFeatureSet(gpointer data) ;
static void removeNotFreeFeatureSet(GQuark key_id, gpointer data, gpointer user_data) ;

static gboolean checkForPerfectAlign(GArray *gaps, unsigned int align_error) ;



/* !
 * A set of functions for allocating, populating and destroying features.
 * The feature create and add data are in two steps because currently the required
 * use is to have a struct that may need to be filled in in several steps because
 * in some data sources the data comes split up in the datastream (e.g. exons in
 * GFF). If there is a requirement for the two bundled then it should be implemented
 * via a simple new "create and add" function that merely calls both the create and
 * add functions from below. */



/*!
 * Function to do some validity checking on a ZMapFeatureAny struct. Always more you
 * could do but this is better than nothing.
 * 
 * Returns TRUE if the struct is OK, FALSE otherwise.
 * 
 * @param   any_feature    The feature to validate.
 * @return  gboolean       TRUE if feature is valid, FALSE otherwise.
 *  */
gboolean zMapFeatureIsValid(ZMapFeatureAny any_feature)
{
  gboolean result = FALSE ;

  if (any_feature
      && zMapFeatureTypeIsValid(any_feature->struct_type)
      && any_feature->unique_id != ZMAPFEATURE_NULLQUARK
      && any_feature->original_id != ZMAPFEATURE_NULLQUARK)
    result = TRUE ;

  return result ;
}

gboolean zMapFeatureTypeIsValid(ZMapFeatureStructType group_type)
{
  gboolean result = FALSE ;

  if (group_type >= ZMAPFEATURE_STRUCT_CONTEXT
      && group_type <= ZMAPFEATURE_STRUCT_FEATURE)
    result = TRUE ;

  return result ;
}



/*!
 * Returns the original name of any feature type. The returned string belongs
 * to the feature and must _NOT_ be free'd. This function can never return
 * NULL as all features must have valid names.
 * 
 * @param   any_feature    The feature.
 * @return  char *         The name of the feature.
 *  */
char *zMapFeatureName(ZMapFeatureAny any_feature)
{
  char *feature_name = NULL ;

  zMapAssert(zMapFeatureIsValid(any_feature)) ;

  feature_name = (char *)g_quark_to_string(any_feature->original_id) ;

  return feature_name ;
}



/*!
 * Function to return the _parent_ group of group_type of the feature any_feature.
 * This is a generalised function to stop all the poking about through the context
 * hierachy that is otherwise required. Note you can only go _UP_ the tree with
 * this function because going down is a one-to-many mapping.
 * 
 * Returns the feature group or NULL if there is no parent group or there is some problem
 * with the arguments like asking for a group at or below the level of any_feature.
 * 
 * @param   any_feature    The feature for which you wish to find the parent group.
 * @param   group_type     The type/level of the parent group you want to find.
 * @return  ZMapFeatureAny The parent group or NULL.
 *  */
ZMapFeatureAny zMapFeatureGetParentGroup(ZMapFeatureAny any_feature, ZMapFeatureStructType group_type)
{
  ZMapFeatureAny result = NULL ;

  zMapAssert(zMapFeatureIsValid(any_feature)
	     && group_type >= ZMAPFEATURE_STRUCT_CONTEXT
	     && group_type <= ZMAPFEATURE_STRUCT_FEATURE) ;

  if (any_feature->struct_type >= group_type)
    {
      ZMapFeatureAny group = any_feature ;

      while (group && group->struct_type > group_type)
	{
	  group = group->parent ;
	}

      result = group ;
    }

  return result ;
}





/*!
 * Returns TRUE if feature context has DNA, FALSE otherwise.
 * If the context has DNA then the name, length and sequence are returned.
 * 
 * @param   seq_name_out  The name of the sequence (e.g. a clone name).
 * @param   seq_len_out   The length of the sequence in bases.
 * @param   sequence_out  The actual dna sequence as a C string.
 * @return  gboolean      TRUE if context contained a sequence.
 *  */
#ifdef RDS_DONT_INCLUDE
gboolean zmapFeatureContextDNA(ZMapFeatureContext context,
			       char **seq_name_out, int *seq_len_out, char **sequence_out)
{
  gboolean result = FALSE ;

  zMapAssert(context && seq_len_out && seq_name_out) ;
  if (context->sequence && context->sequence->sequence)
    {
      *seq_name_out = (char *)g_quark_to_string(context->sequence_name) ;
      *seq_len_out  = context->sequence->length ;
      *sequence_out = context->sequence->sequence ;
      result = TRUE ;
    }

  return result ;
}
#endif

gboolean zMapFeatureBlockDNA(ZMapFeatureBlock block,
                             char **seq_name_out, int *seq_len_out, char **sequence_out)
{
  gboolean result = FALSE;
  ZMapFeatureContext context = NULL;

  zMapAssert( block ) ;

  if(block->sequence.sequence && 
     block->sequence.type != ZMAPSEQUENCE_NONE &&
     block->sequence.type == ZMAPSEQUENCE_DNA  &&
     (context = (ZMapFeatureContext)zMapFeatureGetParentGroup((ZMapFeatureAny)block,
                                                              ZMAPFEATURE_STRUCT_CONTEXT)))
    {
      if(seq_name_out)
        *seq_name_out = (char *)g_quark_to_string(context->sequence_name) ;
      if(seq_len_out)
        *seq_len_out  = block->sequence.length ;
      if(sequence_out)
        *sequence_out = block->sequence.sequence ;
      result = TRUE ;
    }

  return result;
}


/*!
 * Returns a single feature correctly intialised to be a "NULL" feature.
 * 
 * @param   void  None.
 * @return  ZMapFeature  A pointer to the new ZMapFeature.
 *  */
ZMapFeature zMapFeatureCreateEmpty(void)
{
  ZMapFeature feature ;

  feature = g_new0(ZMapFeatureStruct, 1) ;
  feature->struct_type = ZMAPFEATURE_STRUCT_FEATURE ;
  feature->unique_id = ZMAPFEATURE_NULLQUARK ;
  feature->db_id = ZMAPFEATUREID_NULL ;
  feature->type = ZMAPFEATURE_INVALID ;

  return feature ;
}


/* ==================================================================
 * Because the contents of this are quite a lot of work. Useful for
 * creating single features, but be warned that usually you will need
 * to keep track of uniqueness, so for large parser the GFF style of
 * doing things is better (assuming we get a fix for g_datalist!).
 * ==================================================================
 */
ZMapFeature zMapFeatureCreateFromStandardData(char *name, char *sequence, char *ontology,
                                              ZMapFeatureType feature_type, 
                                              ZMapFeatureTypeStyle style,
                                              int start, int end,
                                              gboolean has_score, double score,
                                              ZMapStrand strand, ZMapPhase phase)
{
  ZMapFeature feature = NULL;
  gboolean       good = FALSE;

  if((feature = zMapFeatureCreateEmpty()))
    {
      char *feature_name_id = NULL;
      if((feature_name_id = zMapFeatureCreateName(feature_type, name, strand,
                                                  start, end, 0, 0)) != NULL)
        {
          if((good = zMapFeatureAddStandardData(feature, feature_name_id, 
                                                name, sequence, ontology,
                                                feature_type, style,
                                                start, end, has_score, score,
                                                strand, phase)))
            {
              /* Check I'm valid. Really worth it?? */
              if(!(good = zMapFeatureIsValid((ZMapFeatureAny)feature)))
                {
                  zmapFeatureDestroy(feature);
                  feature = NULL;
                }
            }
        }
    }

  return feature;
}




/*!
 * Adds the standard data fields to an empty feature.
 *  */
gboolean zMapFeatureAddStandardData(ZMapFeature feature, char *feature_name_id, char *name,
				    char *sequence, char *ontology,
				    ZMapFeatureType feature_type, ZMapFeatureTypeStyle style,
				    int start, int end,
				    gboolean has_score, double score,
				    ZMapStrand strand, ZMapPhase phase)
{
  gboolean result = FALSE ;

  /* Currently we don't overwrite features, they must be empty. */
  zMapAssert(feature) ;

  if (feature->unique_id == ZMAPFEATURE_NULLQUARK)
    {
      feature->unique_id = g_quark_from_string(feature_name_id) ;
      feature->original_id = g_quark_from_string(name) ;
      feature->type = feature_type ;
      feature->ontology = g_quark_from_string(ontology) ;
      feature->style = style ;
      feature->x1 = start ;
      feature->x2 = end ;
      feature->strand = strand ;
      feature->phase = phase ;
      if (has_score)
	{
	  feature->flags.has_score = 1 ;
	  feature->score = score ;
	}
    }

  result = TRUE ;

  return result ;
}




/*!
 * Adds data to a feature which may be empty or may already have partial features,
 * e.g. transcript that does not yet have all its exons.
 * 
 * NOTE that really we need this to be a polymorphic function so that the arguments
 * are different for different features.
 *  */
gboolean zMapFeatureAddTranscriptData(ZMapFeature feature,
				      gboolean cds, Coord cds_start, Coord cds_end,
				      gboolean start_not_found, ZMapPhase start_phase,
				      gboolean end_not_found,
				      GArray *exons, GArray *introns)
{
  gboolean result = FALSE ;

  zMapAssert(feature && feature->type == ZMAPFEATURE_TRANSCRIPT) ;

  if (cds)
    {
      feature->feature.transcript.flags.cds = 1 ;
      feature->feature.transcript.cds_start = cds_start ;
      feature->feature.transcript.cds_end = cds_end ;
    }

  if (start_not_found)
    {
      feature->feature.transcript.flags.start_not_found = 1 ;
      feature->feature.transcript.start_phase = start_phase ;
    }

  if (end_not_found)
    feature->feature.transcript.flags.end_not_found = 1 ;

  if (exons)
    feature->feature.transcript.exons = exons ;

  if (introns)
    feature->feature.transcript.introns = introns ;

  result = TRUE ;

  return result ;
}

/*!
 * Adds a single exon and/or intron to a feature which may be empty or may already have
 * some exons/introns.
 *  */
gboolean zMapFeatureAddTranscriptExonIntron(ZMapFeature feature,
					    ZMapSpanStruct *exon, ZMapSpanStruct *intron)
{
  gboolean result = FALSE ;

  zMapAssert(feature && feature->type == ZMAPFEATURE_TRANSCRIPT) ;

  if (exon)
    {
      if (!feature->feature.transcript.exons)
	feature->feature.transcript.exons = g_array_sized_new(FALSE, TRUE,
							      sizeof(ZMapSpanStruct), 30) ;

      g_array_append_val(feature->feature.transcript.exons, *exon) ;

      result = TRUE ;
    }
  else if (intron)
    {
      if (!feature->feature.transcript.introns)
	feature->feature.transcript.introns = g_array_sized_new(FALSE, TRUE,
								sizeof(ZMapSpanStruct), 30) ;

      g_array_append_val(feature->feature.transcript.introns, *intron) ;

      result = TRUE ;
    }

  return result ;
}



/*!
 * Adds homology data to a feature which may be empty or may already have partial features.
 *  */
gboolean zMapFeatureAddSplice(ZMapFeature feature, ZMapBoundaryType boundary)
{
  gboolean result = FALSE ;

  zMapAssert(feature && (boundary == ZMAPBOUNDARY_5_SPLICE || boundary == ZMAPBOUNDARY_3_SPLICE)) ;

  feature->flags.has_boundary = TRUE ;
  feature->boundary_type = boundary ;

  return result ;
}



/*!
 * Adds homology data to a feature which may be empty or may already have partial features.
 *  */
gboolean zMapFeatureAddAlignmentData(ZMapFeature feature,
				     ZMapHomolType homol_type,
				     ZMapStrand target_strand, ZMapPhase target_phase,
				     int query_start, int query_end,
				     GArray *gaps)
{
  gboolean result = TRUE ;				    /* Not used at the moment. */

  zMapAssert(feature && feature->type == ZMAPFEATURE_ALIGNMENT) ;

  feature->feature.homol.type = homol_type ;
  feature->feature.homol.target_strand = target_strand ;
  feature->feature.homol.target_phase = target_phase ;
  feature->feature.homol.y1 = query_start ;
  feature->feature.homol.y2 = query_end ;

  if (gaps)
    {
      zMapFeatureSortGaps(gaps) ;

      feature->feature.homol.align = gaps ;

      feature->feature.homol.flags.perfect = checkForPerfectAlign(feature->feature.homol.align,
								  feature->style->within_align_error) ;
    }
	  
  return result ;
}

/*!
 * Adds a URL to the object, n.b. we add this as a string that must be freed, this.
 * is because I don't want the global GQuark table to be expanded by these...
 *  */
gboolean zMapFeatureAddURL(ZMapFeature feature, char *url)
{
  gboolean result = TRUE ;				    /* We may add url checking sometime. */

  zMapAssert(feature && url && *url) ;

  feature->url = g_strdup_printf(url) ;

  return result ;
}


/*!
 * Adds a Locus to the object.
 *  */
gboolean zMapFeatureAddLocus(ZMapFeature feature, GQuark locus_id)
{
  gboolean result = TRUE ;

  zMapAssert(feature && locus_id) ;

  feature->locus_id = locus_id ;

  return result ;
}


/*!
 * Returns the length of a feature. For a simple feature this is just (end - start + 1),
 * for transcripts and alignments the exons or blocks must be totalled up.
 * 
 * @param   feature      Feature for which length is required.
 * @return               nothing.
 *  */
int zMapFeatureLength(ZMapFeature feature)
{
  int length = 0 ;

  zMapAssert(zMapFeatureIsValid((ZMapFeatureAny)feature)) ;

  length = (feature->x2 - feature->x1 + 1) ;

  if (feature->type == ZMAPFEATURE_TRANSCRIPT && feature->feature.transcript.exons)
    {
      int i ;
      ZMapSpan span ;
      GArray *exons = feature->feature.transcript.exons ;

      length = 0 ;

      for (i = 0 ; i < exons->len ; i++)
	{
	  span = &g_array_index(exons, ZMapSpanStruct, i) ;

	  length += (span->x2 - span->x1 + 1) ;
	}

    }
  else if (feature->type == ZMAPFEATURE_ALIGNMENT && feature->feature.homol.align)
    {
      int i ;
      ZMapAlignBlock align ;
      GArray *gaps = feature->feature.homol.align ;

      length = 0 ;

      for (i = 0 ; i < gaps->len ; i++)
	{
	  align = &g_array_index(gaps, ZMapAlignBlockStruct, i) ;

	  length += (align->q2 - align->q1 + 1) ;
	}
    }

  return length ;
}

/*!
 * Destroys a feature, freeing up all of its resources.
 * 
 * @param   feature      The feature to be destroyed.
 * @return               nothing.
 *  */
void zmapFeatureDestroy(ZMapFeature feature)
{
  zMapAssert(feature) ;

  if (feature->url)
    g_free(feature->url) ;

  if (feature->type == ZMAPFEATURE_TRANSCRIPT)
    {
      if (feature->feature.transcript.exons)
	g_array_free(feature->feature.transcript.exons, TRUE) ;

      if (feature->feature.transcript.introns)
	g_array_free(feature->feature.transcript.introns, TRUE) ;
    }
  else if (feature->type == ZMAPFEATURE_ALIGNMENT)
    {
      if (feature->feature.homol.align)
	g_array_free(feature->feature.homol.align, TRUE) ;
    }

  /* We could memset to zero the feature struct for safety here.... */
  g_free(feature) ;

  return ;
}



/* Features can be NULL if there are no features yet..... */
ZMapFeatureSet zMapFeatureSetCreate(char *source, GData *features)
{
  ZMapFeatureSet feature_set ;
  GQuark original_id, unique_id ;

  unique_id = zMapStyleCreateID(source) ;
  original_id = g_quark_from_string(source) ;

  feature_set = zMapFeatureSetIDCreate(original_id, unique_id, NULL, features) ;

  return feature_set ;
}



void zMapFeatureSetStyle(ZMapFeatureSet feature_set, ZMapFeatureTypeStyle style)
{
  zMapAssert(feature_set && style) ;

  feature_set->style = style ;

  return ;
}




/* Features can be NULL if there are no features yet.....
 * original_id  the original name of the feature set
 * unique_id    some derivation of the original name or otherwise unique id to identify this
 *              feature set. */
ZMapFeatureSet zMapFeatureSetIDCreate(GQuark original_id, GQuark unique_id,
				      ZMapFeatureTypeStyle style, GData *features)
{
  ZMapFeatureSet feature_set ;

  feature_set = g_new0(ZMapFeatureSetStruct, 1) ;
  feature_set->struct_type = ZMAPFEATURE_STRUCT_FEATURESET ;
  feature_set->unique_id = unique_id ;
  feature_set->original_id = original_id ;
  feature_set->style = style ;

  if (!features)
    g_datalist_init(&(feature_set->features)) ;
  else
    feature_set->features = features ;

  return feature_set ;
}





/* Feature must not be null to be added we need at least the feature id and probably should.
 * check for more.
 * 
 * Returns FALSE if feature is already in set.
 *  */
gboolean zMapFeatureSetAddFeature(ZMapFeatureSet feature_set, ZMapFeature feature)
{
  gboolean result = FALSE ;

  zMapAssert(feature_set && feature && feature->unique_id != ZMAPFEATURE_NULLQUARK) ;

  if (!zMapFeatureSetFindFeature(feature_set, feature))
    {
      feature->parent = (ZMapFeatureAny)feature_set ;

      g_datalist_id_set_data_full(&(feature_set->features), feature->unique_id, feature,
				  destroyFeature) ;

      result = TRUE ;
    }

  return result ;
}


/* Returns TRUE if the feature could be found in the feature_set, FALSE otherwise. */
gboolean zMapFeatureSetFindFeature(ZMapFeatureSet feature_set, ZMapFeature feature)
{
  gboolean result = FALSE ;
  gpointer stored = NULL ;
  zMapAssert(feature_set && feature) ;

  if ((stored = g_datalist_id_get_data(&(feature_set->features), feature->unique_id)) != NULL)
    result = TRUE ;

  return result ;
}


/* Feature must exist in set to be removed.
 * 
 * Returns FALSE if feature is not in set.
 *  */
gboolean zMapFeatureSetRemoveFeature(ZMapFeatureSet feature_set, ZMapFeature feature)
{
  gboolean result = FALSE ;

  zMapAssert(feature_set && feature && feature->unique_id != ZMAPFEATURE_NULLQUARK) ;

  if (zMapFeatureSetFindFeature(feature_set, feature))
    {
      g_datalist_id_remove_data(&(feature_set->features), feature->unique_id) ;

      result = TRUE ;
    }

  return result ;
}


void zMapFeatureSetDestroy(ZMapFeatureSet feature_set, gboolean free_data)
{

  /* Delete the feature list, freeing the features if required. */
  if (!free_data)
    {
      /* Unfortunately there is no "clear but don't free the data call" so have to do it
       * the long way by removing each item but _not_ calling our destructor function. */
      g_datalist_foreach(&(feature_set->features), removeNotFreeFeature, (gpointer)feature_set) ;
    }
  g_datalist_clear(&(feature_set->features)) ;

  g_free(feature_set) ;

  return ;
}


ZMapFeatureAlignment zMapFeatureAlignmentCreate(char *align_name, gboolean master_alignment)
{
  ZMapFeatureAlignment alignment ;
  char *unique_name ;

  zMapAssert(align_name) ;

  alignment = g_new0(ZMapFeatureAlignmentStruct, 1) ;

  alignment->struct_type = ZMAPFEATURE_STRUCT_ALIGN ;

  if (master_alignment)
    unique_name = g_strdup_printf("%s_master", align_name) ;
  else
    unique_name = g_strdup(align_name) ;

  alignment->original_id = g_quark_from_string(align_name) ;

  alignment->unique_id = g_quark_from_string(unique_name) ;

  g_free(unique_name) ;

  return alignment ;
}


void zMapFeatureAlignmentAddBlock(ZMapFeatureAlignment alignment, ZMapFeatureBlock block)
{
  block->parent = (ZMapFeatureAny)alignment ;

  alignment->blocks = g_list_append(alignment->blocks, block) ;

  return ;
}


void zMapFeatureAlignmentDestroy(ZMapFeatureAlignment alignment)
{

  /* Need to destroy the blocks here.... */

  g_free(alignment) ;

  return ;
}



ZMapFeatureBlock zMapFeatureBlockCreate(char *block_seq,
					int ref_start, int ref_end, ZMapStrand ref_strand,
					int non_start, int non_end, ZMapStrand non_strand)
{
  ZMapFeatureBlock new_block ;

  zMapAssert((ref_strand == ZMAPSTRAND_FORWARD || ref_strand == ZMAPSTRAND_REVERSE)
	     && (non_strand == ZMAPSTRAND_FORWARD || non_strand == ZMAPSTRAND_REVERSE)) ;


  new_block = g_new0(ZMapFeatureBlockStruct, 1) ;

  new_block->struct_type = ZMAPFEATURE_STRUCT_BLOCK ;

  new_block->unique_id = zMapFeatureBlockCreateID(ref_start, ref_end, ref_strand,
                                                  non_start, non_end, non_strand
                                                  );
  /* Use the sequence name for the original_id */
  new_block->original_id = g_quark_from_string(block_seq) ; 

  new_block->block_to_sequence.t1 = ref_start ;
  new_block->block_to_sequence.t2 = ref_end ;
  new_block->block_to_sequence.t_strand = ref_strand ;

  new_block->block_to_sequence.q1 = non_start ;
  new_block->block_to_sequence.q2 = non_end ;
  new_block->block_to_sequence.q_strand = non_strand ;

  return new_block ;
}



void zMapFeatureBlockAddFeatureSet(ZMapFeatureBlock feature_block, ZMapFeatureSet feature_set)
{

  zMapAssert(feature_block && feature_set && feature_set->unique_id) ;

  feature_set->parent = (ZMapFeatureAny)feature_block ;

  g_datalist_id_set_data_full(&(feature_block->feature_sets), feature_set->unique_id, feature_set,
			      destroyFeatureSet) ;

  /* NEED TO DO SOMETHING ABOUT DESTROYING LIST.... */

  return ;
}


void zMapFeatureBlockDestroy(ZMapFeatureBlock block, gboolean free_data)
{

  block->original_id = block->unique_id = 0 ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* WE PROBABLY NEED TO DO THIS CALL HERE.... */

  /* Other stuff will need freeing here.... */
  zMapFeatureSetDestroy(ZMapFeatureSet feature_set, gboolean free_data) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  g_free(block) ;

  return ;
}

gboolean zMapFeatureBlockThreeFrameTranslation(ZMapFeatureBlock block, ZMapFeatureSet *set_out)
{
  ZMapFeatureSet feature_set = NULL;
  ZMapFeatureTypeStyle style = NULL;
  ZMapFeatureContext context = NULL;
  GQuark style_id = 0;
  gboolean still_good = FALSE, created = FALSE;

  style_id = zMapStyleCreateID(ZMAP_FIXED_STYLE_3FT_NAME);

  if(block->sequence.length)
    still_good = TRUE;

  if(!(context = (ZMapFeatureContext)(zMapFeatureGetParentGroup((ZMapFeatureAny)block, 
                                                                ZMAPFEATURE_STRUCT_CONTEXT))))
    still_good = FALSE;

  if((feature_set = (ZMapFeatureSet)(g_datalist_id_get_data(&(block->feature_sets), style_id))))
    still_good = FALSE;         /* We've already got one */

  if(still_good &&
     (style = zMapFindStyle(context->styles, style_id)) != NULL)
    {
      feature_set = zMapFeatureSetCreate(ZMAP_FIXED_STYLE_3FT_NAME, NULL);
      feature_set->style = style;
      created = TRUE;
    }

  if(still_good && feature_set)
    {
      int i;
      char *seq = NULL, *f_name = NULL, *s_name;
      ZMapFeature threeft = NULL; 
      ZMapPeptide pep = NULL;

      s_name = (char *)g_quark_to_string(block->original_id);

      seq = block->sequence.sequence ;
      seq += 2;                 /* We do it in this order so it looks sensible on the display */

      for(i = 2; seq && *seq && i >= 0; i--, seq--)
        {
          threeft = zMapFeatureCreateEmpty();

          f_name = g_strdup_printf("%s_phase_%d",
				   g_quark_to_string(zMapStyleCreateID(ZMAP_FIXED_STYLE_3FT_NAME)),
				   i);

          pep = zMapPeptideCreateSafely(NULL, NULL, seq, NULL, FALSE);
          
          threeft->text = g_strdup(zMapPeptideSequence(pep));
          
          zMapFeatureAddStandardData(threeft, f_name, f_name,
                                     s_name, "sequence",
                                     ZMAPFEATURE_PEP_SEQUENCE, style,
                                     i+1, zMapPeptideLength(pep) * 3 + i + 1,
                                     FALSE, 0.0,
                                     ZMAPSTRAND_NONE, ZMAPPHASE_NONE);

          zMapFeatureSetAddFeature(feature_set, threeft);
        }
    }

  if(set_out)
    *set_out = feature_set;

  return created;
}


ZMapFeatureContext zMapFeatureContextCreate(char *sequence, int start, int end,
					    GList *types, GList *set_names)
{
  ZMapFeatureContext feature_context ;

  feature_context = g_new0(ZMapFeatureContextStruct, 1) ;

  feature_context->struct_type = ZMAPFEATURE_STRUCT_CONTEXT ;

  if (sequence && *sequence)
    {
      feature_context->unique_id = feature_context->original_id
	= feature_context->sequence_name = g_quark_from_string(sequence) ;
      feature_context->sequence_to_parent.c1 = start ;
      feature_context->sequence_to_parent.c2 = end ;
    }

  feature_context->styles = types ;
  feature_context->feature_set_names = set_names ;

  g_datalist_init(&(feature_context->alignments)) ;

  return feature_context ;
}


void zMapFeatureContextAddAlignment(ZMapFeatureContext feature_context,
				    ZMapFeatureAlignment alignment, gboolean master)
{
  zMapAssert(feature_context && alignment) ;

  alignment->parent = (ZMapFeatureAny)feature_context ;

  g_datalist_id_set_data(&(feature_context->alignments), alignment->unique_id, alignment) ;

  if (master)
    feature_context->master_align = alignment ;

  return ;
}




/* Merge two feature contexts, the contexts must be for the same sequence.
 * The merge works by:
 *
 * - Feature sets and/or features in new_context that already exist in current_context
 *   are simply removed from new_context.
 * 
 * - Feature sets and/or features in new_context that do not exist in current_context
 *   are transferred from new_context to current_context and pointers to these new
 *   features/sets are returned in diff_context.
 *
 * Hence at the start of the merge we have:
 * 
 * current_context  - contains the current feature sets and features
 *     new_context  - contains the new feature sets and features (some of which
 *                    may already be in the current_context)
 *    diff_context  - is empty
 * 
 * and at the end of the merge we will have:
 * 
 * current_context  - contains the merge of all feature sets and features
 *     new_context  - is now empty
 *    diff_context  - contains just the new features sets and features that
 *                    were added to current_context by this merge.
 * 
 * We return diff_context so that the caller has the option to update its display
 * by simply adding the features in diff_context rather than completely redrawing
 * with current_context which may be very expensive.
 * 
 * If called with current_context_inout == NULL (i.e. there are no "current
 * features"), current_context_inout just gets set to new_context and
 * diff_context will be NULL as there are no differences.
 * 
 * NOTE that diff_context may also be NULL if there are no differences
 * between the current and new contexts.
 * 
 * NOTE that diff_context does _not_ have its own copies of features, it points
 * at the ones in current_context so anything you do to diff_context will be
 * reflected in current_context. In particular you should free diff_context
 * using the free context function provided in this interface.
 * 
 * 
 * Returns TRUE if the merge was successful, FALSE otherwise. */
gboolean zMapFeatureContextMerge(ZMapFeatureContext *current_context_inout,
				 ZMapFeatureContext new_context,
				 ZMapFeatureContext *diff_context_out)
{
  gboolean result = FALSE ;
  ZMapFeatureContext current_context ;
  ZMapFeatureContext diff_context ;

  zMapAssert(current_context_inout && new_context && diff_context_out) ;

  current_context = *current_context_inout ;

  /* If there are no current features we just return the new ones and the diff is
   * set to NULL, otherwise we need to do a merge of the new and current feature sets. */
  if (!current_context)
    {
      current_context = new_context ;
      diff_context = NULL ;

      result = TRUE ;
    }
  else
    {

      /* AGH...THIS ALL SEEMS TO BE DISABLED....MAKE IT WORK QUICK.... */

      /* Here we need to merge for all alignments and all blocks.... */


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      /* THIS WILL HAVE TO CHANGE..... */

      /* Check that certain basic things about the sequences to be merged are true....
       * this will probably have to be revised, for now we check that the name and length of the
       * sequences are the same. */
      if (current_context->unique_id == new_context->unique_id
	  && current_context->features_to_sequence.p1 == new_context->features_to_sequence.p1
	  && current_context->features_to_sequence.p2 == new_context->features_to_sequence.p2)
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	{
	  FeatureContextsStruct contexts ;

	  diff_context = zMapFeatureContextCreate(NULL, 0, 0, NULL) ;

	  contexts.current_feature_sets = &(current_context->feature_sets) ;
	  contexts.diff_feature_sets = &(diff_context->feature_sets) ;
	  contexts.new_feature_sets = &(new_context->feature_sets) ;

	  /* OK, this is where we do the merge, the real work is in the functions that get called
	   * from here. */
	  g_datalist_foreach(&(new_context->feature_sets),
			     doNewFeatureSets, (void *)&contexts) ;

	  if (diff_context->feature_sets)
	    {
	      /* Fill in the sequence/mapping details. */
	      diff_context->unique_id = current_context->unique_id ;
	      diff_context->original_id = current_context->original_id ;
	      diff_context->sequence_name = current_context->sequence_name ;
	      diff_context->parent_name = current_context->parent_name ;
	      diff_context->parent_span = current_context->parent_span ; /* n.b. struct copies. */
	      diff_context->sequence_to_parent = current_context->sequence_to_parent ;
	      diff_context->features_to_sequence = current_context->features_to_sequence ;
	    }
	  else
	    {
	      zMapFeatureContextDestroy(diff_context, FALSE) ; /* No need to free data, there
								  isn't any. */
	      diff_context = NULL ;
	    }


	  /* Somewhere around here we should get rid of the new_context..... */


	  result = TRUE ;
	}

#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
    }




  if (result)
    {
      *current_context_inout = current_context ;
      *diff_context_out = diff_context ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      /* debugging.... */
      printf("\ncurrent context\n") ;
      zmapPrintFeatureContext(*current_context_inout) ;


      printf("\nnew context\n") ;
      zmapPrintFeatureContext(new_context) ;

      if (*diff_context_out)
	{
	  printf("\ndiff context\n") ;
	  zmapPrintFeatureContext(*diff_context_out) ;
	}

      zMapAssert(fflush(stdout) == 0) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      /* delete the new_context.... */
      zMapFeatureContextDestroy(new_context, TRUE) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

    }


  return result ;
}



void zMapFeatureContextDestroy(ZMapFeatureContext feature_context, gboolean free_data)
{


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* We need to call routines to free blocks etc. */


  /* Delete the feature set list, freeing the feature sets if required. */
  if (!free_data)
    {
      /* Unfortunately there is no "clear but don't free the data call" so have to do it
       * the long way by removing each item but _not_ calling our destructor function. */
      g_datalist_foreach(&(feature_context->feature_sets), removeNotFreeFeatureSet,
			 (gpointer)feature_context) ;
    }
  g_datalist_clear(&(feature_context->feature_sets)) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  g_free(feature_context) ;

  return ;
}


/*! @} end of zmapfeatures docs. */



/* 
 *            Internal routines. 
 */


/* This is a GDestroyNotify() and is called for each element in a GData list when
 * the list is cleared with g_datalist_clear () and also if g_datalist_id_remove_data()
 * is called. */
static void destroyFeature(gpointer data)
{
  ZMapFeature feature = (ZMapFeature)data ;

  zmapFeatureDestroy(feature) ;

  return ;
}



/* This is GDataForeachFunc(), it is called to remove all the elements of the datalist
 * without removing the corresponding Feature data. */
static void removeNotFreeFeature(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapFeatureSet feature_set = (ZMapFeatureSet)user_data ;

  g_datalist_id_remove_no_notify(&(feature_set->features), key_id) ;

  return ;
}



/* This is a GDestroyNotify() and is called for each element in a GData list when
 * the list is cleared with g_datalist_clear () and also if g_datalist_id_remove_data()
 * is called. */
static void destroyFeatureSet(gpointer data)
{
  ZMapFeatureSet feature_set = (ZMapFeatureSet)data ;

  /* Note, if this routine is called it means that we have been called to free the data so
   * make sure the feature set frees its data as well. */
  zMapFeatureSetDestroy(feature_set, TRUE) ;

  return ;
}



/* This is GDataForeachFunc(), it is called to remove all the elements of the datalist
 * without removing the corresponding Feature data. */
static void removeNotFreeFeatureSet(GQuark key_id, gpointer data, gpointer user_data)
{
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  ZMapFeatureContext feature_context = (ZMapFeatureContext)user_data ;

  /* need to call funcs to free blocks etc. here.... */
  g_datalist_id_remove_no_notify(&(feature_context->feature_sets), key_id) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  return ;
}






#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* Merge two feature blocks, the blocks must be for the same sequence.
 * The merge works by:
 *
 * - Feature sets and/or features in new_block that already exist in current_block
 *   are simply removed from new_block.
 * 
 * - Feature sets and/or features in new_block that do not exist in current_block
 *   are transferred from new_block to current_block and pointers to these new
 *   features/sets are returned in diff_block.
 *
 * Hence at the start of the merge we have:
 * 
 * current_block  - contains the current feature sets and features
 *     new_block  - contains the new feature sets and features (some of which
 *                    may already be in the current_block)
 *    diff_block  - is empty
 * 
 * and at the end of the merge we will have:
 * 
 * current_block  - contains the merge of all feature sets and features
 *     new_block  - is now empty
 *    diff_block  - contains just the new features sets and features that
 *                    were added to current_block by this merge.
 * 
 * We return diff_block so that the caller has the option to update its display
 * by simply adding the features in diff_block rather than completely redrawing
 * with current_block which may be very expensive.
 * 
 * If called with current_block_inout == NULL (i.e. there are no "current
 * features"), current_block_inout just gets set to new_block and
 * diff_block will be NULL as there are no differences.
 * 
 * NOTE that diff_block may also be NULL if there are no differences
 * between the current and new blocks.
 * 
 * NOTE that diff_block does _not_ have its own copies of features, it points
 * at the ones in current_block so anything you do to diff_block will be
 * reflected in current_block. In particular you should free diff_block
 * using the free block function provided in this interface.
 * 
 * 
 * Returns TRUE if the merge was successful, FALSE otherwise. */
gboolean zMapFeatureBlockMerge(ZMapFeatureBlock *current_block_inout,
				 ZMapFeatureBlock new_block,
				 ZMapFeatureBlock *diff_block_out)
{
  gboolean result = FALSE ;
  ZMapFeatureBlock current_block ;
  ZMapFeatureBlock diff_block ;

  zMapAssert(current_block_inout && new_block && diff_block_out) ;

  current_block = *current_block_inout ;

  /* If there are no current features we just return the new ones and the diff is
   * set to NULL, otherwise we need to do a merge of the new and current feature sets. */
  if (!current_block)
    {
      current_block = new_block ;
      diff_block = NULL ;

      result = TRUE ;
    }
  else
    {
      /* Check that certain basic things about the sequences to be merged are true....
       * this will probably have to be revised, for now we check that the name and length of the
       * sequences are the same. */
      if (current_block->unique_id == new_block->unique_id
	  && current_block->features_to_sequence.p1 == new_block->features_to_sequence.p1
	  && current_block->features_to_sequence.p2 == new_block->features_to_sequence.p2)
	{
	  FeatureBlocksStruct blocks ;

	  diff_block = zMapFeatureBlockCreate(NULL) ;

	  blocks.current_feature_sets = &(current_block->feature_sets) ;
	  blocks.diff_feature_sets = &(diff_block->feature_sets) ;
	  blocks.new_feature_sets = &(new_block->feature_sets) ;

	  /* OK, this is where we do the merge, the real work is in the functions that get called
	   * from here. */
	  g_datalist_foreach(&(new_block->feature_sets),
			     doNewFeatureSets, (void *)&blocks) ;

	  if (diff_block->feature_sets)
	    {

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	      /* Fill in the sequence/mapping details. */
	      diff_context->unique_id = current_context->unique_id ;
	      diff_context->original_id = current_context->original_id ;
	      diff_block->sequence_name = current_block->sequence_name ;
	      diff_block->parent_name = current_block->parent_name ;
	      diff_block->parent_span = current_block->parent_span ; /* n.b. struct copies. */
	      diff_block->sequence_to_parent = current_block->sequence_to_parent ;
	      diff_block->features_to_sequence = current_block->features_to_sequence ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	      diff_block->unique_id = current_block->unique_id ;
	      diff_block->original_id = current_block->original_id ;
	      diff_block->features_to_sequence = current_block->features_to_sequence ;
							    /* n.b. struct copies. */
	    }
	  else
	    {
	      zMapFeatureBlockDestroy(diff_block, FALSE) ; /* No need to free data, there
								  isn't any. */
	      diff_block = NULL ;
	    }


	  /* Somewhere around here we should get rid of the new_block..... */


	  result = TRUE ;
	}
    }


  if (result)
    {
      *current_block_inout = current_block ;
      *diff_block_out = diff_block ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      /* debugging.... */
      printf("\ncurrent block\n") ;
      zmapPrintFeatureBlock(*current_block_inout) ;


      printf("\nnew block\n") ;
      zmapPrintFeatureBlock(new_block) ;

      if (*diff_block_out)
	{
	  printf("\ndiff block\n") ;
	  zmapPrintFeatureBlock(*diff_block_out) ;
	}

      zMapAssert(fflush(stdout) == 0) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      /* delete the new_block.... */
      zMapFeatureBlockDestroy(new_block, TRUE) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

    }


  return result ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */










/* Note that user_data is the _address_ of the pointer to a GData, we need this because
 * we need to set the GData address in the original struct to be the new GData address
 * after we have added elements to it. */
static void doNewFeatureSets(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapFeatureSet new_set = (ZMapFeatureSet)data ;

  FeatureBlocks blocks = (FeatureBlocks)user_data ;
  GData **current_result = blocks->current_feature_sets ;
  GData *current_feature_sets = *current_result ;
  GData **diff_result = blocks->diff_feature_sets ;
  GData *diff_feature_sets = *diff_result ;
  GData **new_result = blocks->new_feature_sets ;
  GData *new_feature_sets = *new_result ;

  ZMapFeatureSet current_set, diff_set ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  char *feature_set_name = NULL ;

  feature_set_name = (char *)g_quark_to_string(key_id) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  zMapAssert(key_id == new_set->unique_id) ;


  /* If the feature set is not in current then we can simply add it 
   * otherwise we must do a merge of the individual features of the new and current sets. */
  if (!(current_set = g_datalist_id_get_data(&(current_feature_sets), key_id)))
    {
      /* Entire feature set is not in current context so add it to current and to diff
       * and remove it from new. */
      ZMapFeatureSet unused ;

      g_datalist_id_set_data(&(current_feature_sets), key_id, new_set) ;
      g_datalist_id_set_data(&(diff_feature_sets), key_id, new_set) ;

      /* Should remove from new set here but not sure if it will
       * work as part of a foreach loop....which is where this routine is called from... */
      unused = (ZMapFeatureSet)g_datalist_id_remove_no_notify(&(new_feature_sets), key_id) ;


      /* Need to dump here and see if the new feature set is still valid.... */

    }
  else
    {
      FeatureSetStruct features ;

      diff_set = zMapFeatureSetCreate((char *)g_quark_to_string(key_id), NULL) ;

      features.current_features = &(current_set->features);
      features.diff_features = &(diff_set->features) ;
      features.new_features = &(new_set->features) ;

      g_datalist_foreach(&(new_set->features),
			 doNewFeatures, (void *)&features) ;

      /* after we have done this, if there are any features in the diff set then we must
       * add it to the diff_feature_set as above.... */
      if (diff_set->features)
	g_datalist_id_set_data(&(diff_feature_sets), key_id, diff_set) ;
      else
	zMapFeatureSetDestroy(diff_set, FALSE) ;
    }


  /* Poke the results back in to the original feature context, seems hokey but remember that
   * the address of GData will change as new elements are added so we must poke back the new
   * address. */
  *current_result = current_feature_sets ;
  *diff_result = diff_feature_sets ;
  *new_result = new_feature_sets ;

  return ;
}


static void doNewFeatures(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapFeature new_feature = (ZMapFeature)data ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  GData **current_result = (GData **)user_data ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  FeatureSet sets = (FeatureSet)user_data ;
  GData **current_result = sets->current_features ;
  GData *current_features = *current_result ;
  GData **diff_result = sets->diff_features ;
  GData *diff_features = *diff_result ;
  GData **new_result = sets->new_features ;
  GData *new_features = *new_result ;
  ZMapFeature current_feature;


  zMapAssert(key_id == new_feature->unique_id) ;

  /* If the feature is not in the current feature set then we can simply add it, if its
   * already there then we don't do anything, i.e. we don't try and merge two features. */
  if (!(current_feature = g_datalist_id_get_data(&(current_features), key_id)))
    {
      ZMapFeature unused ;

      /* Feature is not in current, so add it to current and to diff and remove it
       * from new. */
      g_datalist_id_set_data(&(current_features), key_id, new_feature) ;
      g_datalist_id_set_data(&(diff_features), key_id, new_feature) ;


      /* Should remove from new set here but not sure if it will
       * work as part of a foreach loop....which is where this routine is called from... */
      unused = (ZMapFeature)g_datalist_id_remove_no_notify(&(new_features), key_id) ;

    }


  /* Poke the results back in to the original feature set, seems hokey but remember that
   * the address of GData will change as new elements are added so we must poke back the new
   * address. */
  *current_result = current_features ;
  *diff_result = diff_features ;
  *new_result = new_features ;

  return ;
}


/* Returns TRUE if the target blocks match coords are within align_error bases of each other, if
 * there are less than two blocks then FALSE is returned.
 * 
 * Sometimes, for reasons I don't understand, its possible to have two butting matches, i.e. they
 * should really be one continuous match. It may be that this happens at a clone boundary, I don't
 * try to correct this because really its a data entry problem.
 * 
 *  */
static gboolean checkForPerfectAlign(GArray *gaps, unsigned int align_error)
{
  gboolean perfect_align = FALSE ;
  int i ;
  ZMapAlignBlock align, last_align ;

  if (gaps->len > 1)
    {
      perfect_align = TRUE ;
      last_align = &g_array_index(gaps, ZMapAlignBlockStruct, 0) ;

      for (i = 1 ; i < gaps->len ; i++)
	{
	  int prev_end, curr_start ;

	  align = &g_array_index(gaps, ZMapAlignBlockStruct, i) ;


	  /* The gaps array gets sorted by target coords, this can have the effect of reversing
	     the order of the query coords if the match is to the reverse strand of the target sequence. */
	  if (align->q2 < last_align->q1)
	    {
	      prev_end = align->q2 ;
	      curr_start = last_align->q1 ;
	    }
	  else
	    {
	      prev_end = last_align->q2 ;
	      curr_start = align->q1 ;
	    }


	  /* The "- 1" is because the default align_error is zero, i.e. zero _missing_ bases,
	     which is true when sub alignment follows on directly from the next. */
	  if ((curr_start - prev_end - 1) <= align_error)
	    {
	      last_align = align ;
	    }
	  else
	    {
	      perfect_align = FALSE ;
	      break ;
	    }
	}
    }

  return perfect_align ;
}
