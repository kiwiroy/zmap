/*  File: zmapFeatures.c
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
 * Last edited: Nov 22 11:50 2007 (rds)
 * Created: Fri Jul 16 13:05:58 2004 (edgrif)
 * CVS info:   $Id: zmapFeature.c,v 1.83 2007-11-22 11:55:29 rds Exp $
 *-------------------------------------------------------------------
 */

#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <glib.h>
#include <ZMap/zmapGLibUtils.h>
#include <ZMap/zmapUtils.h>
#include <zmapFeature_P.h>




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

typedef struct
{
  ZMapFeatureContext current_context;
  ZMapFeatureContext servers_context;
  ZMapFeatureContext diff_context;

  ZMapFeatureAlignment current_current_align;
  ZMapFeatureBlock     current_current_block;
  ZMapFeatureSet       current_current_set;

  ZMapFeatureAlignment current_diff_align;
  ZMapFeatureBlock     current_diff_block;
  ZMapFeatureSet       current_diff_set;

  ZMapFeatureContextExecuteStatus status;
  gboolean copied_align, copied_block, copied_set;
  gboolean destroy_align, destroy_block, destroy_set, destroy_feature;

  /* sizes for the servers children */
  gint aligns, blocks, sets, features;
  GList *destroy_aligns_list, *destroy_blocks_list, 
    *destroy_sets_list, *destroy_features_list;
} MergeContextDataStruct, *MergeContextData;

typedef struct
{
  ZMapFeatureContext context;
  ZMapFeatureAlignment align;
  ZMapFeatureBlock     block;
} EmptyCopyDataStruct, *EmptyCopyData ;

typedef struct
{
  int length;
} DataListLengthStruct, *DataListLength ;


typedef struct
{
  GData *styles ;
  gboolean result ;
} ReplaceStylesStruct, *ReplaceStyles ;




static ZMapFeatureAny featureAnyCreateFeature(ZMapFeatureStructType feature_type,
					      ZMapFeatureAny parent,
					      GQuark original_id, GQuark unique_id,
					      GHashTable *children) ;
static gboolean featureAnyAddFeature(ZMapFeatureAny feature_set, ZMapFeatureAny feature) ;
static gboolean destroyFeatureAnyWithChildren(ZMapFeatureAny feature_any, gboolean free_children) ;
static void featureAnyAddToDestroyList(ZMapFeatureContext context, ZMapFeatureAny feature_any) ;

static void destroyFeatureAny(gpointer data) ;
static void destroyFeature(ZMapFeature feature) ;
static void destroyContextSubparts(ZMapFeatureContext context) ;

static gboolean withdrawFeatureAny(gpointer key, gpointer value, gpointer user_data) ;

/* datalist debug stuff */

static void printDestroyDebugInfo(ZMapFeatureAny any, char *who) ;

static gboolean checkForPerfectAlign(GArray *gaps, unsigned int align_error) ;


static ZMapFeatureContextExecuteStatus emptyCopyCB(GQuark key, 
                                                   gpointer data, 
                                                   gpointer user_data,
                                                   char **err_out);
static ZMapFeatureContextExecuteStatus eraseContextCB(GQuark key, 
                                                      gpointer data, 
                                                      gpointer user_data,
                                                      char **err_out);
static ZMapFeatureContextExecuteStatus destroyIfEmptyContextCB(GQuark key, 
                                                               gpointer data, 
                                                               gpointer user_data,
                                                               char **err_out);
static ZMapFeatureContextExecuteStatus mergePreCB(GQuark key, 
                                                  gpointer data, 
                                                  gpointer user_data,
                                                  char **err_out);

static gboolean replaceStyles(ZMapFeatureAny feature_any, GData **styles) ;
static ZMapFeatureContextExecuteStatus replaceStyleCB(GQuark key_id, 
						      gpointer data, 
						      gpointer user_data,
						      char **error_out) ;
static void replaceFeatureStyleCB(gpointer key, gpointer data, gpointer user_data) ;


static ZMapFeatureContextExecuteStatus addModeCB(GQuark key_id, 
						 gpointer data, 
						 gpointer user_data,
						 char **error_out) ;
static void addFeatureModeCB(gpointer key, gpointer data, gpointer user_data) ;







static gboolean merge_debug_G   = FALSE;
static gboolean destroy_debug_G = FALSE;


/* Currently if we use this we get seg faults so we must not be cleaning up properly somewhere... */
static gboolean USE_SLICE_ALLOC = TRUE ;





/* !
 * A set of functions for allocating, populating and destroying features.
 * The feature create and add data are in two steps because currently the required
 * use is to have a struct that may need to be filled in in several steps because
 * in some data sources the data comes split up in the datastream (e.g. exons in
 * GFF). If there is a requirement for the two bundled then it should be implemented
 * via a simple new "create and add" function that merely calls both the create and
 * add functions from below. */



/* Make a copy of any feature, the feature is "stand alone", i.e. it has no parent
 * and no children, these are simply not copied, or any other dynamically allocated stuff ?? */
ZMapFeatureAny zMapFeatureAnyCopy(ZMapFeatureAny orig_feature_any)
{
  ZMapFeatureAny new_feature_any  = NULL ;

  new_feature_any  = zmapFeatureAnyCopy(orig_feature_any, destroyFeatureAny) ;

  return new_feature_any ;
}


/* Returns TRUE if the feature could be found in the feature_set, FALSE otherwise. */
gboolean zMapFeatureAnyFindFeature(ZMapFeatureAny feature_set, ZMapFeatureAny feature)
{
  gboolean result = FALSE ;
  ZMapFeature hash_feature ;

  zMapAssert(feature_set && feature) ;

  if ((hash_feature = g_hash_table_lookup(feature_set->children, zmapFeature2HashKey(feature))))
    result = TRUE ;

  return result ;
}


/* Returns the feature if found, NULL otherwise. */
ZMapFeatureAny zMapFeatureAnyGetFeatureByID(ZMapFeatureAny feature_set, GQuark feature_id)
{
  ZMapFeatureAny feature ;

  feature = g_hash_table_lookup(feature_set->children, GINT_TO_POINTER(feature_id)) ;

  return feature ;
}


gboolean zMapFeatureAnyRemoveFeature(ZMapFeatureAny feature_set, ZMapFeatureAny feature)
{
  gboolean result = FALSE;

  if (zMapFeatureAnyFindFeature(feature_set, feature))
    {
      result = g_hash_table_steal(feature_set->children, zmapFeature2HashKey(feature)) ;
      feature->parent = NULL;
  
      switch(feature->struct_type)
	{
	case ZMAPFEATURE_STRUCT_CONTEXT:
	  break ;
	case ZMAPFEATURE_STRUCT_ALIGN:
	  {
	    ZMapFeatureContext context = (ZMapFeatureContext)feature_set ;

	    if (context->master_align == (ZMapFeatureAlignment)feature)
	      context->master_align = NULL ;

	    break ;
	  }
	case ZMAPFEATURE_STRUCT_BLOCK:
	  break ;
	case ZMAPFEATURE_STRUCT_FEATURESET:
	  break ;
	case ZMAPFEATURE_STRUCT_FEATURE:
	  break ;
	default:
	  zMapAssertNotReached() ;
	  break ;
	}

      result = TRUE ;
    }

  return result ;
}




/* go through all the feature sets in the given AnyFeature (must be at least a feature set)
 * and set the style mode from that...a bit hacky really...think about this....
 * 
 * Really this is all acedb methods which are not rich enough for what we want to set
 * in our styles...
 * 
 *  */
gboolean zMapFeatureAnyAddModesToStyles(ZMapFeatureAny feature_any)
{
  gboolean result = TRUE;
  ZMapFeatureContextExecuteStatus status = ZMAP_CONTEXT_EXEC_STATUS_OK;

  zMapFeatureContextExecuteSubset(feature_any, 
                                  ZMAPFEATURE_STRUCT_FEATURESET,
                                  addModeCB,
                                  &status) ;

  if (status != ZMAP_CONTEXT_EXEC_STATUS_OK)
    result = FALSE ;

  return result;
}



ZMapFeatureAny zmapFeatureAnyCopy(ZMapFeatureAny orig_feature_any, GDestroyNotify destroy_cb)
{
  ZMapFeatureAny new_feature_any  = NULL ;
  guint bytes ;

  zMapAssert(zMapFeatureIsValid(orig_feature_any)) ;


  /* Copy the original struct and set common fields. */
  switch(orig_feature_any->struct_type)
    {
    case ZMAPFEATURE_STRUCT_CONTEXT:
      bytes = sizeof(ZMapFeatureContextStruct) ;
      break ;
    case ZMAPFEATURE_STRUCT_ALIGN:
      bytes = sizeof(ZMapFeatureAlignmentStruct) ;
      break ;
    case ZMAPFEATURE_STRUCT_BLOCK:
      bytes = sizeof(ZMapFeatureBlockStruct) ;
      break ;
    case ZMAPFEATURE_STRUCT_FEATURESET:
      bytes = sizeof(ZMapFeatureSetStruct) ;
      break ;
    case ZMAPFEATURE_STRUCT_FEATURE:
      bytes = sizeof(ZMapFeatureStruct) ;
      break ;
    default:
      zMapAssertNotReached();
      break;
    }

  if (USE_SLICE_ALLOC)
    {
      new_feature_any = g_slice_alloc0(bytes) ;
      g_memmove(new_feature_any, orig_feature_any, bytes) ;
    }
  else
    new_feature_any = g_memdup(orig_feature_any, bytes) ;


  /* We DO NOT copy children or parents... */
  new_feature_any->parent = NULL ;
  if (new_feature_any->struct_type != ZMAPFEATURE_STRUCT_FEATURE)
    new_feature_any->children = g_hash_table_new_full(NULL, NULL, NULL, destroy_cb) ;


  /* Fill in the fields unique to each struct type. */
  switch(new_feature_any->struct_type)
    {
    case ZMAPFEATURE_STRUCT_CONTEXT:
      {
	ZMapFeatureContext new_context = (ZMapFeatureContext)new_feature_any ;

	new_context->elements_to_destroy = NULL ;

	new_context->feature_set_names = NULL ;

	new_context->styles = NULL ;

	new_context->master_align = NULL ;

	break ;
      }
    case ZMAPFEATURE_STRUCT_ALIGN:
      {
	break;
      }
    case ZMAPFEATURE_STRUCT_BLOCK:
      {
	ZMapFeatureBlock new_block = (ZMapFeatureBlock)new_feature_any ;	

	new_block->sequence.type = ZMAPSEQUENCE_NONE ;
	new_block->sequence.length = 0 ;
	new_block->sequence.sequence = NULL ;

	break;
      }
    case ZMAPFEATURE_STRUCT_FEATURESET:
      {
	ZMapFeatureSet new_set = (ZMapFeatureSet)new_feature_any ;

	new_set->style = NULL ;

	break;
      }
    case ZMAPFEATURE_STRUCT_FEATURE:
      {
	ZMapFeature new_feature = (ZMapFeature)new_feature_any,
	  orig_feature = (ZMapFeature)orig_feature_any ;

	if (new_feature->type == ZMAPFEATURE_ALIGNMENT)
	  {
	    ZMapAlignBlockStruct align;

	    if (orig_feature->feature.homol.align != NULL
		&& orig_feature->feature.homol.align->len > (guint)0)
	      {
		int i ;

		new_feature->feature.homol.align = 
		  g_array_sized_new(FALSE, TRUE, 
				    sizeof(ZMapAlignBlockStruct),
				    orig_feature->feature.homol.align->len);

		for (i = 0; i < orig_feature->feature.homol.align->len; i++)
		  {
		    align = g_array_index(orig_feature->feature.homol.align, ZMapAlignBlockStruct, i);
		    new_feature->feature.homol.align = 
		      g_array_append_val(new_feature->feature.homol.align, align);
		  }
	      }
	  }
	else if (new_feature->type == ZMAPFEATURE_TRANSCRIPT)
	  {
	    ZMapSpanStruct span;
	    int i ;

	    if (orig_feature->feature.transcript.exons != NULL
		&& orig_feature->feature.transcript.exons->len > (guint)0)
	      {
		new_feature->feature.transcript.exons = 
		  g_array_sized_new(FALSE, TRUE, 
				    sizeof(ZMapSpanStruct),
				    orig_feature->feature.transcript.exons->len);

		for (i = 0; i < orig_feature->feature.transcript.exons->len; i++)
		  {
		    span = g_array_index(orig_feature->feature.transcript.exons, ZMapSpanStruct, i);
		    new_feature->feature.transcript.exons = 
		      g_array_append_val(new_feature->feature.transcript.exons, span);
		  }
	      }

	    if (orig_feature->feature.transcript.introns != NULL
		&& orig_feature->feature.transcript.introns->len > (guint)0)
	      {
		new_feature->feature.transcript.introns = 
		  g_array_sized_new(FALSE, TRUE, 
				    sizeof(ZMapSpanStruct),
				    orig_feature->feature.transcript.introns->len);

		for (i = 0; i < orig_feature->feature.transcript.introns->len; i++)
		  {
		    span = g_array_index(orig_feature->feature.transcript.introns, ZMapSpanStruct, i);
		    new_feature->feature.transcript.introns = 
		      g_array_append_val(new_feature->feature.transcript.introns, span);
		  }
	      }
	  }

	break ;
      }
    default:
      zMapAssertNotReached();
      break;
    }

  return new_feature_any ;
}



void zMapFeatureAnyDestroy(ZMapFeatureAny feature_any)
{
  gboolean result ;

  zMapAssert(zMapFeatureIsValid(feature_any)) ;

  result = destroyFeatureAnyWithChildren(feature_any, TRUE) ;
  zMapAssert(result) ;

  return ;
}




/*!
 * A Blocks DNA
 */
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

  feature = (ZMapFeature)featureAnyCreateFeature(ZMAPFEATURE_STRUCT_FEATURE, NULL,
						 ZMAPFEATURE_NULLQUARK, ZMAPFEATURE_NULLQUARK,
						 NULL) ;
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

  if ((feature = zMapFeatureCreateEmpty()))
    {
      char *feature_name_id = NULL;
      if((feature_name_id = zMapFeatureCreateName(feature_type, name, strand,
                                                  start, end, 0, 0)) != NULL)
        {
          if ((good = zMapFeatureAddStandardData(feature, feature_name_id, 
						 name, sequence, ontology,
						 feature_type, style,
						 start, end, has_score, score,
						 strand, phase)))
            {
              /* Check I'm valid. Really worth it?? */
              if(!(good = zMapFeatureIsValid((ZMapFeatureAny)feature)))
                {
                  zMapFeatureDestroy(feature);
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
gboolean zMapFeatureAddKnownName(ZMapFeature feature, char *known_name)
{
  gboolean result = FALSE ;
  GQuark known_id ;

  zMapAssert(feature && (feature->type == ZMAPFEATURE_BASIC || feature->type == ZMAPFEATURE_TRANSCRIPT)) ;

  known_id = g_quark_from_string(known_name) ;

  if (feature->type == ZMAPFEATURE_BASIC)
    feature->feature.basic.known_name = known_id ;
  else
    feature->feature.transcript.known_name = known_id ;

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

  if (exons)
    feature->feature.transcript.exons = exons ;

  if (introns)
    feature->feature.transcript.introns = introns ;

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
gboolean zMapFeatureAddTranscriptStartEnd(ZMapFeature feature,
					  gboolean start_not_found, ZMapPhase start_phase,
					  gboolean end_not_found)
{
  gboolean result = TRUE ;

  zMapAssert(feature && feature->type == ZMAPFEATURE_TRANSCRIPT
	     && ((!start_not_found && start_phase == ZMAPPHASE_NONE)
		 || (start_phase >= ZMAPPHASE_0 && start_phase <= ZMAPPHASE_2))) ;

  if (start_not_found)
    {
      feature->feature.transcript.flags.start_not_found = 1 ;
      feature->feature.transcript.start_phase = start_phase ;
    }

  if (end_not_found)
    feature->feature.transcript.flags.end_not_found = 1 ;

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
  gboolean result = TRUE ;

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
				     ZMapPhase target_phase,
				     int query_start, int query_end, int query_length,
				     GArray *gaps, gboolean has_local_sequence)
{
  gboolean result = TRUE ;				    /* Not used at the moment. */

  zMapAssert(feature && feature->type == ZMAPFEATURE_ALIGNMENT) ;

  feature->feature.homol.type = homol_type ;
  feature->feature.homol.target_phase = target_phase ;
  feature->feature.homol.y1 = query_start ;
  feature->feature.homol.y2 = query_end ;
  feature->feature.homol.length = query_length ;
  feature->feature.homol.flags.has_sequence = has_local_sequence ;

  if (gaps)
    {
      zMapFeatureSortGaps(gaps) ;

      feature->feature.homol.align = gaps ;

      feature->feature.homol.flags.perfect = checkForPerfectAlign(feature->feature.homol.align,
								  zmapStyleGetWithinAlignError(feature->style)) ;
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
 * @param   length_type  Length in target sequence coords or query sequence coords or spliced length.
 * @return               nothing.
 *  */
int zMapFeatureLength(ZMapFeature feature, ZMapFeatureLengthType length_type)
{
  int length = 0 ;

  zMapAssert(zMapFeatureIsValid((ZMapFeatureAny)feature)) ;

  switch (length_type)
    {
    case ZMAPFEATURELENGTH_TARGET:
      {
	/* We just want the total length of the feature as located on the target sequence. */

	length = (feature->x2 - feature->x1 + 1) ;

	break ;
      }
    case ZMAPFEATURELENGTH_QUERY:
      {
	/* We want the length of the feature as it is in its original query sequence, this is
	 * only different from ZMAPFEATURELENGTH_TARGET for alignments. */

	if (feature->type == ZMAPFEATURE_ALIGNMENT)
	  {
	    length = abs(feature->feature.homol.y2 - feature->feature.homol.y1) + 1 ;
	  }
	else
	  {
	    length = feature->x2 - feature->x1 + 1 ;
	  }

	break ;
      }
    case ZMAPFEATURELENGTH_SPLICED:
      {
	/* We want the actual length of the feature blocks, only different for transcripts and alignments. */

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
	else
	  {
	    length = (feature->x2 - feature->x1 + 1) ;
	  }

	break ;
      }
    default:
      {
	zMapAssertNotReached() ;

	break ;
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
void zMapFeatureDestroy(ZMapFeature feature)
{
  gboolean result ;

  zMapAssert(feature) ;

  result = destroyFeatureAnyWithChildren((ZMapFeatureAny)feature, FALSE) ;
  zMapAssert(result) ;

  return ;
}




/* 
 *                      Feature Set functions.
 */

/* Features can be NULL if there are no features yet..... */
ZMapFeatureSet zMapFeatureSetCreate(char *source, GHashTable *features)
{
  ZMapFeatureSet feature_set ;
  GQuark original_id, unique_id ;

  unique_id = zMapFeatureSetCreateID(source) ;
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
				      ZMapFeatureTypeStyle style, GHashTable *features)
{
  ZMapFeatureSet feature_set ;

  feature_set = (ZMapFeatureSet)featureAnyCreateFeature(ZMAPFEATURE_STRUCT_FEATURESET, NULL,
							original_id, unique_id,
							features) ;
  feature_set->style = style ;

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

  result = featureAnyAddFeature((ZMapFeatureAny)feature_set, (ZMapFeatureAny)feature) ;

  return result ;
}


/* Returns TRUE if the feature could be found in the feature_set, FALSE otherwise. */
gboolean zMapFeatureSetFindFeature(ZMapFeatureSet feature_set, 
                                   ZMapFeature    feature)
{
  gboolean result = FALSE ;

  zMapAssert(feature_set && feature) ;

  result = zMapFeatureAnyFindFeature((ZMapFeatureAny)feature_set, (ZMapFeatureAny)feature) ;

  return result ;
}

ZMapFeature zMapFeatureSetGetFeatureByID(ZMapFeatureSet feature_set, GQuark feature_id)
{
  ZMapFeature feature ;

  feature = (ZMapFeature)zMapFeatureAnyGetFeatureByID((ZMapFeatureAny)feature_set, feature_id) ;

  return feature ;
}


/* Feature must exist in set to be removed.
 * 
 * Returns FALSE if feature is not in set.
 *  */
gboolean zMapFeatureSetRemoveFeature(ZMapFeatureSet feature_set, ZMapFeature feature)
{
  gboolean result = FALSE ;

  zMapAssert(feature_set && feature && feature->unique_id != ZMAPFEATURE_NULLQUARK) ;

  result = zMapFeatureAnyRemoveFeature((ZMapFeatureAny)feature_set, (ZMapFeatureAny)feature) ;

  return result ;
}


void zMapFeatureSetDestroy(ZMapFeatureSet feature_set, gboolean free_data)
{
  gboolean result ;

  zMapAssert(feature_set);
  result = destroyFeatureAnyWithChildren((ZMapFeatureAny)feature_set, free_data) ;
  zMapAssert(result) ;

  return ;
}


void zMapFeatureSetDestroyFeatures(ZMapFeatureSet feature_set)
{
  zMapAssert(feature_set) ;

  g_hash_table_destroy(feature_set->features) ;
  feature_set->features = NULL ;

  return ;
}




/* 
 *                Alignment functions
 */

GQuark zMapFeatureAlignmentCreateID(char *align_name, gboolean master_alignment)
{
  GQuark id = 0;
  char *unique_name;

  if (master_alignment)
    unique_name = g_strdup_printf("%s_master", align_name) ;
  else
    unique_name = g_strdup(align_name) ;

  id = g_quark_from_string(unique_name) ;
  
  g_free(unique_name);

  return id;
}


ZMapFeatureAlignment zMapFeatureAlignmentCreate(char *align_name, gboolean master_alignment)
{
  ZMapFeatureAlignment alignment ;

  zMapAssert(align_name) ;

  alignment = (ZMapFeatureAlignment)featureAnyCreateFeature(ZMAPFEATURE_STRUCT_ALIGN,
							    NULL,
							    g_quark_from_string(align_name),
							    zMapFeatureAlignmentCreateID(align_name, master_alignment),
							    NULL) ;

  return alignment ;
}


gboolean zMapFeatureAlignmentAddBlock(ZMapFeatureAlignment alignment, ZMapFeatureBlock block)
{
  gboolean result = FALSE  ;

  zMapAssert(alignment && block) ;

  result = featureAnyAddFeature((ZMapFeatureAny)alignment, (ZMapFeatureAny)block) ;

  return result ;
}


gboolean zMapFeatureAlignmentFindBlock(ZMapFeatureAlignment feature_align, 
                                       ZMapFeatureBlock     feature_block)
{
  gboolean result = FALSE;

  zMapAssert(feature_align && feature_block);

  result = zMapFeatureAnyFindFeature((ZMapFeatureAny)feature_align, (ZMapFeatureAny)feature_block) ;

  return result;
}

ZMapFeatureBlock zMapFeatureAlignmentGetBlockByID(ZMapFeatureAlignment feature_align, GQuark block_id)
{
  ZMapFeatureBlock feature_block = NULL;
  
  feature_block = (ZMapFeatureBlock)zMapFeatureAnyGetFeatureByID((ZMapFeatureAny)feature_align, block_id) ;

  return feature_block ;
}


gboolean zMapFeatureAlignmentRemoveBlock(ZMapFeatureAlignment feature_align,
                                         ZMapFeatureBlock     feature_block)
{
  gboolean result = FALSE;

  result = zMapFeatureAnyRemoveFeature((ZMapFeatureAny)feature_align, (ZMapFeatureAny)feature_block) ;

  return result;
}

void zMapFeatureAlignmentDestroy(ZMapFeatureAlignment alignment, gboolean free_data)
{
  gboolean result ;

  zMapAssert(alignment);
  result = destroyFeatureAnyWithChildren((ZMapFeatureAny)alignment, free_data) ;
  zMapAssert(result) ;

  return ;
}



ZMapFeatureBlock zMapFeatureBlockCreate(char *block_seq,
					int ref_start, int ref_end, ZMapStrand ref_strand,
					int non_start, int non_end, ZMapStrand non_strand)
{
  ZMapFeatureBlock new_block ;

  zMapAssert((ref_strand == ZMAPSTRAND_FORWARD || ref_strand == ZMAPSTRAND_REVERSE)
	     && (non_strand == ZMAPSTRAND_FORWARD || non_strand == ZMAPSTRAND_REVERSE)) ;

  new_block = (ZMapFeatureBlock)featureAnyCreateFeature(ZMAPFEATURE_STRUCT_BLOCK,
							NULL,
							g_quark_from_string(block_seq),
							zMapFeatureBlockCreateID(ref_start, ref_end, ref_strand,
										 non_start, non_end, non_strand),
							NULL) ;

  new_block->block_to_sequence.t1 = ref_start ;
  new_block->block_to_sequence.t2 = ref_end ;
  new_block->block_to_sequence.t_strand = ref_strand ;

  new_block->block_to_sequence.q1 = non_start ;
  new_block->block_to_sequence.q2 = non_end ;
  new_block->block_to_sequence.q_strand = non_strand ;

  return new_block ;
}



gboolean zMapFeatureBlockAddFeatureSet(ZMapFeatureBlock feature_block, 
				       ZMapFeatureSet   feature_set)
{
  gboolean result = FALSE  ;

  zMapAssert(feature_block && feature_set) ;

  result = featureAnyAddFeature((ZMapFeatureAny)feature_block, (ZMapFeatureAny)feature_set) ;

  return result ;
}



gboolean zMapFeatureBlockFindFeatureSet(ZMapFeatureBlock feature_block,
                                        ZMapFeatureSet   feature_set)
{
  gboolean result = FALSE ;

  zMapAssert(feature_set && feature_block);

  result = zMapFeatureAnyFindFeature((ZMapFeatureAny)feature_block, (ZMapFeatureAny)feature_set) ;

  return result;
}

ZMapFeatureSet zMapFeatureBlockGetSetByID(ZMapFeatureBlock feature_block, GQuark set_id)
{
  ZMapFeatureSet feature_set ;

  feature_set = (ZMapFeatureSet)zMapFeatureAnyGetFeatureByID((ZMapFeatureAny)feature_block, set_id) ;

  return feature_set ;
}


gboolean zMapFeatureBlockRemoveFeatureSet(ZMapFeatureBlock feature_block, 
                                          ZMapFeatureSet   feature_set)
{
  gboolean result = FALSE;

  result = zMapFeatureAnyRemoveFeature((ZMapFeatureAny)feature_block, (ZMapFeatureAny)feature_set) ;

  return result;
}

void zMapFeatureBlockDestroy(ZMapFeatureBlock block, gboolean free_data)
{
  gboolean result ;

  zMapAssert(block);

  result = destroyFeatureAnyWithChildren((ZMapFeatureAny)block, free_data) ;
  zMapAssert(result) ;

  return ;
}

ZMapFeatureContext zMapFeatureContextCreate(char *sequence, int start, int end,
					    GData *styles, GList *set_names)
{
  ZMapFeatureContext feature_context ;
  GQuark original_id = 0, unique_id = 0 ;

  if (sequence && *sequence)
    unique_id = original_id = g_quark_from_string(sequence) ;

  feature_context = (ZMapFeatureContext)featureAnyCreateFeature(ZMAPFEATURE_STRUCT_CONTEXT,
								NULL,
								original_id, unique_id,
								NULL) ;

  if (sequence && *sequence)
    {
      feature_context->sequence_name = g_quark_from_string(sequence) ;
      feature_context->sequence_to_parent.c1 = start ;
      feature_context->sequence_to_parent.c2 = end ;
    }

  feature_context->styles = styles ;
  feature_context->feature_set_names = set_names ;

  return feature_context ;
}

ZMapFeatureContext zMapFeatureContextCreateEmptyCopy(ZMapFeatureContext feature_context)
{
  EmptyCopyDataStruct   empty_data = {NULL};
  ZMapFeatureContext empty_context = NULL;

  if((empty_data.context = zMapFeatureContextCreate(NULL, 0, 0, NULL, NULL)))
    {
      char *tmp;
      tmp = g_strdup_printf("HACK in %s:%d", __FILE__, __LINE__);
      empty_data.context->unique_id =
        empty_data.context->original_id = g_quark_from_string(tmp);
      g_free(tmp);

      empty_data.context->parent_span        = feature_context->parent_span; /* struct copy */
      empty_data.context->sequence_to_parent = feature_context->sequence_to_parent; /* struct copy */
      empty_data.context->length             = feature_context->length;

      zMapFeatureContextExecute((ZMapFeatureAny)feature_context,
                                ZMAPFEATURE_STRUCT_FEATURESET,
                                emptyCopyCB,
                                &empty_data);
      
      empty_context = empty_data.context;
    }

  return empty_context;
}

gboolean zMapFeatureContextAddAlignment(ZMapFeatureContext feature_context,
					ZMapFeatureAlignment alignment, gboolean master)
{
  gboolean result = FALSE  ;

  zMapAssert(feature_context && alignment) ;

  if ((result = featureAnyAddFeature((ZMapFeatureAny)feature_context, (ZMapFeatureAny)alignment)))
    {
      if (master)
	feature_context->master_align = alignment ;
    }

  return result ;
}

gboolean zMapFeatureContextFindAlignment(ZMapFeatureContext   feature_context,
                                         ZMapFeatureAlignment feature_align)
{
  gboolean result = FALSE ;

  zMapAssert(feature_context && feature_align );

  result = zMapFeatureAnyFindFeature((ZMapFeatureAny)feature_context, (ZMapFeatureAny)feature_align) ;

  return result;  
}

ZMapFeatureAlignment zMapFeatureContextGetAlignmentByID(ZMapFeatureContext feature_context, 
                                                        GQuark align_id)
{
  ZMapFeatureAlignment feature_align ;

  feature_align = (ZMapFeatureAlignment)zMapFeatureAnyGetFeatureByID((ZMapFeatureAny)feature_context, align_id) ;

  return feature_align ;
}


gboolean zMapFeatureContextRemoveAlignment(ZMapFeatureContext feature_context,
                                           ZMapFeatureAlignment feature_alignment)
{
  gboolean result = FALSE;

  zMapAssert(feature_context && feature_alignment);

  if ((result = zMapFeatureAnyRemoveFeature((ZMapFeatureAny)feature_context, (ZMapFeatureAny)feature_alignment)))
    {
      if(feature_context->master_align == feature_alignment)
        feature_context->master_align = NULL; 
    }

  return result;
}






/* Context merging is complicated when it comes to destroying the contexts
 * because the diff_context actually points to feature structs in the merged
 * context. It has to do this because we want to pass to the drawing code
 * only new features that need drawing. To do this though means that some of
 * of the "parent" parts of the context (aligns, blocks, sets) must be duplicates
 * of those in the current context. Hence we end up with a context where we
 * want to destroy some features (the duplicates) but not others (the ones that
 * are just pointers to features in the current context).
 * 
 * So for the diff_context we don't set destroy functions when the context
 * is created, instead we keep a separate hash of duplicate features to be destroyed.
 * 
 * If hashtables supported setting a destroy function for each element we
 * wouldn't need to do this, but they don't (unlike g_datalists, we don't
 * use those because they are too slow).
 * 
 */

/* N.B. under new scheme, new_context_inout will be always be destroyed && NULL'd.... */
gboolean zMapFeatureContextMerge(ZMapFeatureContext *merged_context_inout,
                                 ZMapFeatureContext *new_context_inout,
				 ZMapFeatureContext *diff_context_out)
{
  gboolean result = FALSE ;
  ZMapFeatureContext current_context, new_context, diff_context = NULL ;

  zMapAssert(merged_context_inout && new_context_inout && diff_context_out) ;

  current_context = *merged_context_inout ;
  new_context = *new_context_inout ;

  /* If there are no current features we just return the new ones and the diff is
   * set to NULL, otherwise we need to do a merge of the new and current feature sets. */
  if (!current_context)
    {
      if(merge_debug_G)
        zMapLogWarning("%s", "No current context, returning complete new...") ;

      diff_context = current_context = new_context ;
      new_context = NULL ;
      result = TRUE ;
    }
  else
    {
      /* Here we need to merge for all alignments and all blocks.... */
      MergeContextDataStruct merge_data = {NULL} ;


      /* Note we make the diff context point at the feature list and styles of the new context,
       * I guess we could copy them but doesn't seem worth it...see code below where we NULL them
       * in new_context so they are not thrown away.... */
      diff_context = (ZMapFeatureContext)zmapFeatureAnyCopy((ZMapFeatureAny)new_context, NULL) ;
      diff_context->diff_context = TRUE ;
      diff_context->elements_to_destroy = g_hash_table_new_full(NULL, NULL, NULL, destroyFeatureAny) ;
      diff_context->feature_set_names = new_context->feature_set_names ;
      diff_context->styles = new_context->styles ;

      merge_data.current_context = current_context;
      merge_data.servers_context = new_context;
      merge_data.diff_context = diff_context ;
      merge_data.status = ZMAP_CONTEXT_EXEC_STATUS_OK;
      
      current_context->feature_set_names = g_list_concat(current_context->feature_set_names,
                                                         new_context->feature_set_names);

      /* Merge the styles from the new context into the existing context. */
      current_context->styles = zMapStyleMergeStyles(current_context->styles, new_context->styles);

      /* Make the diff_context point at the merged styles, not its own copies... */
      replaceStyles((ZMapFeatureAny)new_context, &(current_context->styles)) ;

      if(merge_debug_G)
        zMapLogWarning("%s", "merging ...");

      /* Do the merge ! */
      zMapFeatureContextExecuteComplete((ZMapFeatureAny)new_context, ZMAPFEATURE_STRUCT_FEATURE,
                                        mergePreCB, NULL, &merge_data) ;

      if(merge_debug_G)
        zMapLogWarning("%s", "finished ...");

      if (merge_data.status == ZMAP_CONTEXT_EXEC_STATUS_OK)
	{
	  /* Set these to NULL as diff_context references them. */
	  new_context->feature_set_names = NULL ;
	  new_context->styles = NULL ;

	  current_context = merge_data.current_context ;
	  new_context = merge_data.servers_context ;
	  diff_context = merge_data.diff_context ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	  {
	    /* Debug stuff... */
	    GError *err = NULL ;

	    printf("diff context:\n") ;
	    zMapFeatureDumpStdOutFeatures(diff_context, &err) ;

	    printf("full context:\n") ;
	    zMapFeatureDumpStdOutFeatures(current_context, &err) ;

	  }
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	  result = TRUE ;
	}
    }

  /* Clear up new_context whatever happens. If there is an error, what can anyone do with it...nothing. */
  if (new_context)
    {
      zMapFeatureContextDestroy(new_context, TRUE);
      new_context = NULL ;
    }

  if (result)
    {
      *merged_context_inout = current_context ;
      *new_context_inout = new_context ;
      *diff_context_out = diff_context ;
    }

  return result ;
}



/* creates a context of features of matches between the remove and
 * current, removing from the current as it goes. */
gboolean zMapFeatureContextErase(ZMapFeatureContext *current_context_inout,
				 ZMapFeatureContext remove_context,
				 ZMapFeatureContext *diff_context_out)
{
  gboolean erased = FALSE;
  /* Here we need to merge for all alignments and all blocks.... */
  MergeContextDataStruct merge_data = {NULL};
  char *diff_context_string  = NULL;
  ZMapFeatureContext current_context ;
  ZMapFeatureContext diff_context ;

  zMapAssert(current_context_inout && remove_context && diff_context_out) ;

  current_context = *current_context_inout ;
  
  merge_data.current_context = current_context;
  merge_data.servers_context = remove_context;
  merge_data.diff_context    = diff_context = zMapFeatureContextCreate(NULL, 0, 0, NULL, NULL);
  merge_data.status          = ZMAP_CONTEXT_EXEC_STATUS_OK;
  
  diff_context->feature_set_names    = remove_context->feature_set_names;
  current_context->feature_set_names = g_list_concat(current_context->feature_set_names,
                                                     remove_context->feature_set_names);
  
  /* Set the original and unique ids so that the context passes the feature validity checks */
  diff_context_string = g_strdup_printf("%s vs %s\n", 
                                        g_quark_to_string(current_context->unique_id),
                                        g_quark_to_string(remove_context->unique_id));
  diff_context->original_id = 
    diff_context->unique_id = g_quark_from_string(diff_context_string);

  diff_context->alignments  = g_hash_table_new_full(NULL, NULL, NULL, destroyFeatureAny) ;
  
  g_free(diff_context_string);
  
  zMapFeatureContextExecuteComplete((ZMapFeatureAny)remove_context, ZMAPFEATURE_STRUCT_FEATURE,
                                    eraseContextCB, destroyIfEmptyContextCB, &merge_data);
  

  if(merge_data.status == ZMAP_CONTEXT_EXEC_STATUS_OK)
    erased = TRUE;

  if (erased)
    {
      *current_context_inout = current_context ;
      *diff_context_out      = diff_context ;
    }

  return erased;
}


void zMapFeatureContextDestroy(ZMapFeatureContext feature_context, gboolean free_data)
{
  gboolean result ;

  zMapAssert(zMapFeatureIsValid((ZMapFeatureAny)feature_context)) ;

  if (feature_context->diff_context)
    {
      destroyFeatureAny(feature_context) ;
    }
  else
    {
      result = destroyFeatureAnyWithChildren((ZMapFeatureAny)feature_context, free_data) ;
      zMapAssert(result) ;
    }

  return ;
}


/*! @} end of zmapfeatures docs. */



/* 
 *            Internal routines. 
 */



/* Frees resources that are unique to a context, resources common to all features
 * are freed by a common routine. */
static void destroyContextSubparts(ZMapFeatureContext context)
{

  /* diff contexts are different because they are a mixture of pointers to subtrees in another
   * context, individual features in another context and copied elements in subtrees. */
  if (context->diff_context)
    {
      /* Remove the copied elements. */
      g_hash_table_destroy(context->elements_to_destroy) ;
    }


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  if (context->feature_set_names)
    {
      g_list_free(context->feature_set_names) ;
      context->feature_set_names = NULL ;
    }


  if (context->styles)
    zMapStyleDestroyStyles(&(context->styles)) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  return ;
}


static void destroyFeature(ZMapFeature feature)
{
  if (feature->url)
    g_free(feature->url) ;

  if(feature->text)
    g_free(feature->text) ;

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

  return ;
}



/* A GDestroyNotify() function called from g_hash_table_destroy() to get rid of all children
 * in the hash. */
static void destroyFeatureAny(gpointer data)
{
  ZMapFeatureAny feature_any = (ZMapFeatureAny)data ;
  gulong nbytes ;


  if (destroy_debug_G && feature_any->struct_type != ZMAPFEATURE_STRUCT_FEATURE)
    printDestroyDebugInfo(feature_any, __PRETTY_FUNCTION__) ;

  switch(feature_any->struct_type)
    {
    case ZMAPFEATURE_STRUCT_CONTEXT:
      nbytes = sizeof(ZMapFeatureContextStruct) ;
      destroyContextSubparts((ZMapFeatureContext)feature_any) ;
      break ;
    case ZMAPFEATURE_STRUCT_ALIGN:
      nbytes = sizeof(ZMapFeatureAlignmentStruct) ;
      break ;
    case ZMAPFEATURE_STRUCT_BLOCK:
      nbytes = sizeof(ZMapFeatureBlockStruct) ;
      break ;
    case ZMAPFEATURE_STRUCT_FEATURESET:
      nbytes = sizeof(ZMapFeatureSetStruct) ;
      break;
    case ZMAPFEATURE_STRUCT_FEATURE:
      nbytes = sizeof(ZMapFeatureStruct) ;
      destroyFeature((ZMapFeature)feature_any) ;
      break;
    default:
      zMapAssertNotReached();
      break;
    }

  if (feature_any->struct_type != ZMAPFEATURE_STRUCT_FEATURE)
    g_hash_table_destroy(feature_any->children) ;

  memset(feature_any, (char )0, nbytes);

  /* We could memset to zero the feature struct for safety here.... */
  if (USE_SLICE_ALLOC)
    g_slice_free1(nbytes, feature_any) ;
  else
    g_free(feature_any) ;

  return ;
}







static void printDestroyDebugInfo(ZMapFeatureAny feature_any, char *who)
{
  int length = 0 ;

  if (feature_any->struct_type != ZMAPFEATURE_STRUCT_FEATURE)
    length = g_hash_table_size(feature_any->children) ;

  if(destroy_debug_G)
    zMapLogWarning("%s: (%p) '%s' datalist size %d", who, feature_any, g_quark_to_string(feature_any->unique_id), length) ;

  return ;
}





/* A GHRFunc() called by hash steal to remove items from hash _without_ calling the item destroy routine. */
static gboolean withdrawFeatureAny(gpointer key, gpointer value, gpointer user_data)
{
  gboolean remove = TRUE ;


  /* We want to remove all items so always return TRUE. */

  return remove ;
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



static ZMapFeatureContextExecuteStatus emptyCopyCB(GQuark key, 
                                                   gpointer data, 
                                                   gpointer user_data,
                                                   char **err_out)
{
  ZMapFeatureAny feature_any = (ZMapFeatureAny)data;
  EmptyCopyData    copy_data = (EmptyCopyData)user_data;
  ZMapFeatureAlignment align, copy_align;
  ZMapFeatureBlock     block, copy_block;
  ZMapFeatureSet feature_set, copy_feature_set;
  ZMapFeatureContextExecuteStatus status = ZMAP_CONTEXT_EXEC_STATUS_OK;

  switch(feature_any->struct_type)
    {
    case ZMAPFEATURE_STRUCT_CONTEXT:
      {
	copy_data->context = zMapFeatureContextCreate(NULL, 0, 0, NULL, NULL);
	break;
      }

    case ZMAPFEATURE_STRUCT_ALIGN:
      {
        char *align_name;
        gboolean is_master = FALSE;
        
        align = (ZMapFeatureAlignment)feature_any;

        if(((ZMapFeatureContext)(feature_any->parent))->master_align == align)
          is_master = TRUE;

        align_name = (char *)g_quark_to_string(align->original_id);
        copy_align = zMapFeatureAlignmentCreate(align_name, is_master);
        zMapFeatureContextAddAlignment(copy_data->context, copy_align, is_master);
        copy_data->align = copy_align;

	break;
      }

    case ZMAPFEATURE_STRUCT_BLOCK:
      {
        char *block_seq;
        block = (ZMapFeatureBlock)feature_any;
        block_seq  = (char *)g_quark_to_string(block->original_id);
        copy_block = zMapFeatureBlockCreate(block_seq, 
                                            block->block_to_sequence.t1, 
                                            block->block_to_sequence.t2,
                                            block->block_to_sequence.t_strand,
                                            block->block_to_sequence.q1,
                                            block->block_to_sequence.q2,
                                            block->block_to_sequence.q_strand);
        copy_block->unique_id = block->unique_id;
        zMapFeatureAlignmentAddBlock(copy_data->align, copy_block);
        copy_data->block = copy_block;
      }
      break;
    case ZMAPFEATURE_STRUCT_FEATURESET:
      {
        char *set_name;
        feature_set = (ZMapFeatureSet)feature_any;

        set_name    = (char *)g_quark_to_string(feature_set->original_id);

        copy_feature_set = zMapFeatureSetCreate(set_name, NULL);

	copy_feature_set->style = feature_set->style ;

        zMapFeatureBlockAddFeatureSet(copy_data->block, copy_feature_set);
      }
      break;
    case ZMAPFEATURE_STRUCT_FEATURE:
      /* We don't copy features, hence the name emptyCopyCB!! */
    default:
      zMapAssertNotReached();
      break;
    }

  return status;
}

static ZMapFeatureContextExecuteStatus eraseContextCB(GQuark key, 
                                                      gpointer data, 
                                                      gpointer user_data,
                                                      char **err_out)
{
  ZMapFeatureContextExecuteStatus status = ZMAP_CONTEXT_EXEC_STATUS_OK;
  MergeContextData merge_data = (MergeContextData)user_data;
  ZMapFeatureAny  feature_any = (ZMapFeatureAny)data;
  ZMapFeature feature, erased_feature;
  gboolean remove_status = FALSE;

  if(merge_debug_G)
    zMapLogWarning("checking %s", g_quark_to_string(feature_any->unique_id));

  switch(feature_any->struct_type)
    {
    case ZMAPFEATURE_STRUCT_CONTEXT:
      break;
    case ZMAPFEATURE_STRUCT_ALIGN:
      merge_data->current_current_align = zMapFeatureContextGetAlignmentByID(merge_data->current_context, 
                                                                             key);
      break;
    case ZMAPFEATURE_STRUCT_BLOCK:
      merge_data->current_current_block = zMapFeatureAlignmentGetBlockByID(merge_data->current_current_align, 
                                                                           key);
      break;
    case ZMAPFEATURE_STRUCT_FEATURESET:
      merge_data->current_current_set   = zMapFeatureBlockGetSetByID(merge_data->current_current_block, 
                                                                     key);
      break;      
    case ZMAPFEATURE_STRUCT_FEATURE:
      /* look up in the current */
      feature = (ZMapFeature)feature_any;

      if(merge_data->current_current_set &&
         (erased_feature = zMapFeatureSetGetFeatureByID(merge_data->current_current_set, key)))
        {
          if(merge_debug_G)
            zMapLogWarning("%s","\tFeature in erase and current contexts...");

          /* insert into the diff context. 
           * BUT, need to check if the parents exist in the diff context first.
           */
          if (!merge_data->current_diff_align) 
            {
              if(merge_debug_G)
                zMapLogWarning("%s","\tno parent align... creating align in diff");

	      merge_data->current_diff_align
		= (ZMapFeatureAlignment)featureAnyCreateFeature(merge_data->current_current_align->struct_type,
								NULL,
								merge_data->current_current_align->original_id, 
								merge_data->current_current_align->unique_id,
								NULL) ;

              zMapFeatureContextAddAlignment(merge_data->diff_context, merge_data->current_diff_align, FALSE);
            }
          if (!merge_data->current_diff_block)
            {
              if(merge_debug_G)
                zMapLogWarning("%s","\tno parent block... creating block in diff");

	      merge_data->current_diff_block
		= (ZMapFeatureBlock)featureAnyCreateFeature(merge_data->current_current_block->struct_type,
							    NULL,
							    merge_data->current_current_block->original_id,
							    merge_data->current_current_block->unique_id,
							    NULL) ;

              zMapFeatureAlignmentAddBlock(merge_data->current_diff_align, merge_data->current_diff_block);
            }
          if(!merge_data->current_diff_set)
            {
              if(merge_debug_G)
                zMapLogWarning("%s","\tno parent set... creating set in diff");

	      merge_data->current_diff_set
		= (ZMapFeatureSet)featureAnyCreateFeature(merge_data->current_current_set->struct_type,
							  NULL,
							  merge_data->current_current_set->original_id,
							  merge_data->current_current_set->unique_id,
							  NULL) ;

              zMapFeatureBlockAddFeatureSet(merge_data->current_diff_block, merge_data->current_diff_set); 
            }

          if(merge_debug_G)
            zMapLogWarning("%s","\tmoving feature from current to diff context ... removing ... and inserting.");

          /* remove from the current context.*/
          remove_status = zMapFeatureSetRemoveFeature(merge_data->current_current_set, erased_feature);
          zMapAssert(remove_status);

          zMapFeatureSetAddFeature(merge_data->current_diff_set, erased_feature);

          /* destroy from the erase context.*/
          if(merge_debug_G)
            zMapLogWarning("%s","\tdestroying feature in erase context");
          zMapFeatureDestroy(feature);
        }
      else
        {
          if(merge_debug_G)
            zMapLogWarning("%s","\tFeature absent from current context, nothing to do...");
          /* no ...
           * leave in the erase context. 
           */
        }
      break;
    default:
      break;
    }

  return status ;
}

static ZMapFeatureContextExecuteStatus destroyIfEmptyContextCB(GQuark key, 
                                                               gpointer data, 
                                                               gpointer user_data,
                                                               char **err_out)
{
  ZMapFeatureContextExecuteStatus status = ZMAP_CONTEXT_EXEC_STATUS_OK;
  MergeContextData        merge_data = (MergeContextData)user_data;
  ZMapFeatureAny         feature_any = (ZMapFeatureAny)data;
  ZMapFeatureAlignment feature_align;
  ZMapFeatureBlock     feature_block;
  ZMapFeatureSet         feature_set;

  if(merge_debug_G)
    zMapLogWarning("checking %s", g_quark_to_string(feature_any->unique_id));

  switch(feature_any->struct_type)
    {
    case ZMAPFEATURE_STRUCT_CONTEXT:
      break;
    case ZMAPFEATURE_STRUCT_ALIGN:
      feature_align = (ZMapFeatureAlignment)feature_any;
      if (g_hash_table_size(feature_align->blocks) == 0)
        {
          if(merge_debug_G)
            zMapLogWarning("%s","\tempty align ... destroying");
          zMapFeatureAlignmentDestroy(feature_align, TRUE);
        }
      merge_data->current_diff_align = 
        merge_data->current_current_align = NULL;
      merge_data->copied_align = FALSE;
      break;
    case ZMAPFEATURE_STRUCT_BLOCK:
      feature_block = (ZMapFeatureBlock)feature_any;
      if (g_hash_table_size(feature_block->feature_sets) == 0)
        {
          if(merge_debug_G)
            zMapLogWarning("%s","\tempty block ... destroying");
          zMapFeatureBlockDestroy(feature_block, TRUE);
        }
      merge_data->current_diff_block =
        merge_data->current_current_block = NULL;
      merge_data->copied_block = FALSE;
      break;
    case ZMAPFEATURE_STRUCT_FEATURESET:
      feature_set = (ZMapFeatureSet)feature_any;
      if (g_hash_table_size(feature_set->features) == 0)
        {
          if(merge_debug_G)
            zMapLogWarning("%s","\tempty set ... destroying");
          zMapFeatureSetDestroy(feature_set, TRUE);
        }
      merge_data->current_diff_set =
        merge_data->current_current_set = NULL;
      merge_data->copied_set = FALSE;
      break;
    case ZMAPFEATURE_STRUCT_FEATURE:
      break;
    default:
      break;
    }


  return status ;
}



/* It's very important to note that the diff context hash tables _do_not_ have destroy functions,
 * this is what prevents them from freeing their children. */
static ZMapFeatureContextExecuteStatus mergePreCB(GQuark key, 
                                                  gpointer data, 
                                                  gpointer user_data,
                                                  char **err_out)
{
  ZMapFeatureContextExecuteStatus status = ZMAP_CONTEXT_EXEC_STATUS_OK ;
  MergeContextData merge_data = (MergeContextData)user_data ;
  ZMapFeatureAny feature_any = (ZMapFeatureAny)data ;
  gboolean new = FALSE, children = FALSE ;


  /* THE CODE BELOW ALL NEEDS FACTORISING, START WITH THE SWITCH SETTING SOME
     COMMON POINTERS FOR THE FOLLOWING CODE..... */

  
  switch(feature_any->struct_type)
    {
      gboolean result ;

    case ZMAPFEATURE_STRUCT_CONTEXT:
      {
	break ;
      }

    case ZMAPFEATURE_STRUCT_ALIGN:
      {
	ZMapFeatureAlignment feature_align = (ZMapFeatureAlignment)feature_any ;
	ZMapFeatureContext server_context = (ZMapFeatureContext)(feature_any->parent) ;

	/* Always remove from the server context */
	result = zMapFeatureAnyRemoveFeature((ZMapFeatureAny)server_context, feature_any) ;
	zMapAssert(result) ;

	/* If there are no children then we don't add it, we don't keep empty aligns etc. */
	if (feature_any->children)
	  {
	    ZMapFeatureAny diff_align ;
	    children = TRUE ;

	    merge_data->aligns++ ;

	    /* If its not in the current context then add it, otherwise copy it so we can add
	     * features further down the tree. */
	    if (!(merge_data->current_current_align
		  = (ZMapFeatureAlignment)zMapFeatureAnyGetFeatureByID((ZMapFeatureAny)(merge_data->current_context),
								       feature_any->unique_id)))
	      {

		/* If its new we can simply copy a pointer over to the diff context
		 * and stop recursing.... */
		zMapFeatureContextAddAlignment(merge_data->current_context, feature_align, FALSE);
		merge_data->current_current_align = feature_align ;

		new = TRUE ;

		diff_align = (ZMapFeatureAny)feature_align ;

                /* but we need to reset parent pointer....*/
		diff_align->parent = (ZMapFeatureAny)(merge_data->current_context) ;
		
		status = ZMAP_CONTEXT_EXEC_STATUS_DONT_DESCEND ;
	      }
	    else
	      {
		/* If the feature is there we need to copy it and then recurse down until
		 * we get to the individual feature level. */
		diff_align = zmapFeatureAnyCopy((ZMapFeatureAny)feature_align, NULL) ;
		featureAnyAddToDestroyList(merge_data->diff_context, diff_align) ;
	      }

	    /* Add align to diff context, n.b. we do _not_ set a destroy function. */
	    featureAnyAddFeature((ZMapFeatureAny)(merge_data->diff_context), (ZMapFeatureAny)diff_align) ;
	    merge_data->current_diff_align = (ZMapFeatureAlignment)diff_align ;

	    if (new)
	      diff_align->parent = (ZMapFeatureAny)(merge_data->current_context) ;

	    if (merge_data->current_current_align == merge_data->current_context->master_align)
	      {
		merge_data->diff_context->master_align = (ZMapFeatureAlignment)diff_align ;
	      }
	  }

	break;
      }

    case ZMAPFEATURE_STRUCT_BLOCK:
      {
	ZMapFeatureBlock feature_block = (ZMapFeatureBlock)feature_any ;
	ZMapFeatureAlignment server_align = (ZMapFeatureAlignment)(feature_any->parent) ;

	/* remove from server align */
	result = zMapFeatureAnyRemoveFeature((ZMapFeatureAny)server_align, feature_any) ;
	zMapAssert(result) ;

	/* If there are no children then we don't add it, we don't keep empty aligns etc. */
	if (feature_any->children)
	  {
	    ZMapFeatureAny diff_block ;

	    children = TRUE ;

	    merge_data->blocks++ ;

	    /* If its not in the current context then add it. */
	    if (!(merge_data->current_current_block
		  = (ZMapFeatureBlock)zMapFeatureAnyGetFeatureByID((ZMapFeatureAny)(merge_data->current_current_align),
								   feature_any->unique_id)))
	      {
		/* add to the full context align */
		zMapFeatureAlignmentAddBlock(merge_data->current_current_align, feature_block);
		merge_data->current_current_block = feature_block ;
		
		new = TRUE ;

		/* If its new we can simply copy a pointer over to the diff context
		 * and stop recursing.... */
		diff_block = (ZMapFeatureAny)feature_block ;

		/* but we need to reset parent pointer....*/
		diff_block->parent = (ZMapFeatureAny)(merge_data->current_current_align) ;
				
		status = ZMAP_CONTEXT_EXEC_STATUS_DONT_DESCEND ;

	      }
	    else
	      {
		/* Add to diff align. */
		diff_block = zmapFeatureAnyCopy((ZMapFeatureAny)feature_block, NULL) ;
		featureAnyAddToDestroyList(merge_data->diff_context, (ZMapFeatureAny)diff_block) ;
	      }

	    featureAnyAddFeature((ZMapFeatureAny)(merge_data->current_diff_align), diff_block) ;
	    merge_data->current_diff_block = (ZMapFeatureBlock)diff_block ;

	    if (new)
	      diff_block->parent = (ZMapFeatureAny)(merge_data->current_current_align) ;

	  }

	break;
      }

    case ZMAPFEATURE_STRUCT_FEATURESET:
      {
	ZMapFeatureSet feature_set = (ZMapFeatureSet)feature_any ;
	ZMapFeatureBlock server_block = (ZMapFeatureBlock)(feature_any->parent);

	/* remove from server align */
	result = zMapFeatureAnyRemoveFeature((ZMapFeatureAny)server_block, feature_any) ;
	zMapAssert(result) ;

	/* If there are no children then we don't add it, we don't keep empty aligns etc. */
	if (feature_any->children)
	  {
	    ZMapFeatureAny diff_set ;

	    children = TRUE ;

	    merge_data->sets++;

	    if (!(merge_data->current_current_set
		  = (ZMapFeatureSet)zMapFeatureAnyGetFeatureByID((ZMapFeatureAny)(merge_data->current_current_block),
								 feature_any->unique_id)))
	      {
		/* add to the full context block */
		zMapFeatureBlockAddFeatureSet(merge_data->current_current_block, (ZMapFeatureSet)feature_set);
		merge_data->current_current_set = feature_set;

		new = TRUE ;

		/* If its new we can simply copy a pointer over to the diff context
		 * and stop recursing.... */
		diff_set = (ZMapFeatureAny)feature_set ;
		               
		/* but we need to reset parent pointer....*/
		diff_set->parent = (ZMapFeatureAny)(merge_data->current_current_block) ;
				
		status = ZMAP_CONTEXT_EXEC_STATUS_DONT_DESCEND ;

	      }
	    else
	      {
		diff_set = zmapFeatureAnyCopy((ZMapFeatureAny)feature_set, NULL) ;
		((ZMapFeatureSet)diff_set)->style = feature_set->style ;

		featureAnyAddToDestroyList(merge_data->diff_context, (ZMapFeatureAny)diff_set) ;
	      }

	    featureAnyAddFeature((ZMapFeatureAny)(merge_data->current_diff_block), diff_set) ;
	    merge_data->current_diff_set = (ZMapFeatureSet)diff_set ;

	    if (new)
	      diff_set->parent = (ZMapFeatureAny)(merge_data->current_current_block) ;

	  }

	break;
      }

    case ZMAPFEATURE_STRUCT_FEATURE:
      {
	ZMapFeature feature = (ZMapFeature)(feature_any) ;
	ZMapFeatureSet server_set = (ZMapFeatureSet)(feature_any->parent) ;
	ZMapFeatureAny diff_feature ;

	/* remove from the server set */
	result = zMapFeatureAnyRemoveFeature((ZMapFeatureAny)server_set, feature_any) ;
	zMapAssert(result) ;

	/* Note that features do not have children _ever_ so we are only concerned with copying
	 * pointers if we find new features. */

	merge_data->features++ ;

	/* If its not in the current context then add it. */

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	if (!(zMapFeatureSetGetFeatureByID(merge_data->current_current_set, feature_any->unique_id)))
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
	if (!(zMapFeatureAnyGetFeatureByID((ZMapFeatureAny)(merge_data->current_current_set),
					   feature_any->unique_id)))
	  {
	    new = TRUE ;

	    /* If its new we can simply copy a pointer over to the diff context
	     * and stop recursing.... */
	    diff_feature = (ZMapFeatureAny)feature ;

               
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	    /* I don't think we need to do this because features do not have children. */

	    featureAnyAddToRemoveList(merge_data->diff_context, diff_feature) ;
	    status = ZMAP_CONTEXT_EXEC_STATUS_DONT_DESCEND ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	    /* We need to reset parent pointer....and the style pointer.... 
	     * order is critical here since featureany call resets parent... */
	    featureAnyAddFeature((ZMapFeatureAny)merge_data->current_diff_set, diff_feature) ;

	    /* add to the full context set */
	    featureAnyAddFeature((ZMapFeatureAny)(merge_data->current_current_set), diff_feature);


	    if (merge_debug_G)
	      zMapLogWarning("feature(%p)->parent = %p. current_current_set = %p",
			     feature, feature->parent, merge_data->current_current_set);
	  }
        
	break;
      }

    default:
      {
	zMapAssertNotReached() ;
	break;
      }
    }


  if (merge_debug_G)
    zMapLogWarning("%s (%p) '%s' is %s and has %s",
		   zMapFeatureStructType2Str(feature_any->struct_type),
		   feature_any,
		   g_quark_to_string(feature_any->unique_id),
		   (new == TRUE ? "new" : "old"),
		   (children ? "children and was added" : "no children and was not added"));

  return status;
}



/* 
 *              Following functions all operate on any feature type,
 *              they were written to reduce duplication of code.
 */


/* Allocate a feature structure of the requested type, filling in the feature any fields. */
static ZMapFeatureAny featureAnyCreateFeature(ZMapFeatureStructType struct_type,
					      ZMapFeatureAny parent,
					      GQuark original_id, GQuark unique_id,
					      GHashTable *children)
{
  ZMapFeatureAny feature_any = NULL ;
  gulong nbytes ;

  switch(struct_type)
    {
    case ZMAPFEATURE_STRUCT_CONTEXT:
      nbytes = sizeof(ZMapFeatureContextStruct) ;
      break ;
    case ZMAPFEATURE_STRUCT_ALIGN:
      nbytes = sizeof(ZMapFeatureAlignmentStruct) ;
      break ;
    case ZMAPFEATURE_STRUCT_BLOCK:
      nbytes = sizeof(ZMapFeatureBlockStruct) ;
      break ;
    case ZMAPFEATURE_STRUCT_FEATURESET:
      nbytes = sizeof(ZMapFeatureSetStruct) ;
      break ;
    case ZMAPFEATURE_STRUCT_FEATURE:
      nbytes = sizeof(ZMapFeatureStruct) ;
      break ;
    default:
      zMapAssertNotReached() ;
      break ;
    }

  if (USE_SLICE_ALLOC)
    feature_any = (ZMapFeatureAny)g_slice_alloc0(nbytes) ;
  else
    feature_any = (ZMapFeatureAny)g_malloc0(nbytes) ;


  feature_any->struct_type = struct_type ;

  if (struct_type != ZMAPFEATURE_STRUCT_CONTEXT)
    feature_any->parent = parent ;

  feature_any->original_id = original_id ;
  feature_any->unique_id = unique_id ;

  if (struct_type != ZMAPFEATURE_STRUCT_FEATURE)
    {
      if (children)
	feature_any->children = children ;
      else
	feature_any->children = g_hash_table_new_full(NULL, NULL, NULL, destroyFeatureAny) ;
    }

  return feature_any ;
}



/* Hooks up the feature into the feature_sets children and makes feature_set the parent
 * of feature _if_ the feature is not already in feature_set. */
static gboolean featureAnyAddFeature(ZMapFeatureAny feature_set, ZMapFeatureAny feature)
{
  gboolean result = FALSE ;

  if (!zMapFeatureAnyFindFeature(feature_set, feature))
    {
      g_hash_table_insert(feature_set->children, zmapFeature2HashKey(feature), feature) ;

      feature->parent = feature_set ;

      result = TRUE ;
    }

  return result ;
}


static gboolean destroyFeatureAnyWithChildren(ZMapFeatureAny feature_any, gboolean free_children)
{
  gboolean result = TRUE ;

  zMapAssert(feature_any);

  /* I think this is equivalent to the below code.... */

  /* If we have a parent then remove ourselves from the parent. */
  if (result && feature_any->parent)
    {
      /* splice out the feature_any from parent */
      result = g_hash_table_steal(feature_any->parent->children, zmapFeature2HashKey(feature_any)) ;
    }

  /* If we have children but they should not be freed, then remove them before destroying the
   * feature, otherwise the children will be destroyed. */
  if (result && (feature_any->children && !free_children))
    {
      if ((g_hash_table_foreach_steal(feature_any->children,
				      withdrawFeatureAny, (gpointer)feature_any)))
	{
	  zMapAssert(g_hash_table_size(feature_any->children) == 0) ;
	  result = TRUE ;
	}
    }  

  /* Now destroy the feature. */
  if (result)
    {
      destroyFeatureAny((gpointer)feature_any) ;
      result = TRUE ;
    }

  return result ;
}



static void featureAnyAddToDestroyList(ZMapFeatureContext context, ZMapFeatureAny feature_any)
{
  zMapAssert(context && feature_any) ;

  g_hash_table_insert(context->elements_to_destroy, zmapFeature2HashKey(feature_any), feature_any) ;

  return ;
}








static gboolean replaceStyles(ZMapFeatureAny feature_any, GData **styles)
{
  ReplaceStylesStruct replace_data = {*styles, TRUE} ;

  zMapFeatureContextExecute(feature_any,
			    ZMAPFEATURE_STRUCT_FEATURESET,
			    replaceStyleCB,
			    &replace_data) ;

  return replace_data.result ;
}


static ZMapFeatureContextExecuteStatus replaceStyleCB(GQuark key_id, 
						      gpointer data, 
						      gpointer user_data,
						      char **error_out)
{
  ZMapFeatureAny feature_any = (ZMapFeatureAny)data ;
  ReplaceStyles replace_data = (ReplaceStyles)user_data ;
  ZMapFeatureStructType feature_type ;
  ZMapFeatureContextExecuteStatus status = ZMAP_CONTEXT_EXEC_STATUS_OK;

  feature_type = feature_any->struct_type ;

  switch(feature_type)
    {
    case ZMAPFEATURE_STRUCT_CONTEXT:
    case ZMAPFEATURE_STRUCT_ALIGN:
    case ZMAPFEATURE_STRUCT_BLOCK:
      {
	break ;
      }
    case ZMAPFEATURE_STRUCT_FEATURESET:
      {
	ZMapFeatureSet feature_set ;

        feature_set = (ZMapFeatureSet)feature_any ;

	if (!(feature_set->style = zMapFindStyle(replace_data->styles,
						 zMapStyleGetUniqueID(feature_set->style))))
	  printf("agh, no style...\n") ;

	g_hash_table_foreach(feature_set->features, replaceFeatureStyleCB, replace_data) ;

	break;
      }
    case ZMAPFEATURE_STRUCT_FEATURE:
    case ZMAPFEATURE_STRUCT_INVALID:
    default:
      {
	status = ZMAP_CONTEXT_EXEC_STATUS_ERROR;
	zMapAssertNotReached();
	break;
      }
    }

  return status ;
}



/* A GHashForeachFunc() to add a mode to the styles for all features in a set, note that
 * this is not efficient as we go through all features but we would need more information
 * stored in the feature set to avoid this. */
static void replaceFeatureStyleCB(gpointer key, gpointer data, gpointer user_data)
{
  ZMapFeature feature = (ZMapFeature)data ;
  ReplaceStyles replace_data = (ReplaceStyles)user_data ;

  feature->style = zMapFindStyle(replace_data->styles, zMapStyleGetUniqueID(feature->style)) ;

  return ;
}




static ZMapFeatureContextExecuteStatus addModeCB(GQuark key_id, 
						 gpointer data, 
						 gpointer user_data,
						 char **error_out)
{
  ZMapFeatureAny feature_any = (ZMapFeatureAny)data ;
  ZMapFeatureStructType feature_type ;
  ZMapFeatureContextExecuteStatus status = ZMAP_CONTEXT_EXEC_STATUS_OK;

  feature_type = feature_any->struct_type ;

  switch(feature_type)
    {
    case ZMAPFEATURE_STRUCT_CONTEXT:
    case ZMAPFEATURE_STRUCT_ALIGN:
    case ZMAPFEATURE_STRUCT_BLOCK:
      {
	break ;
      }
    case ZMAPFEATURE_STRUCT_FEATURESET:
      {
	ZMapFeatureSet feature_set ;

        feature_set = (ZMapFeatureSet)feature_any ;

	g_hash_table_foreach(feature_set->features, addFeatureModeCB, feature_set) ;

	break;
      }
    case ZMAPFEATURE_STRUCT_FEATURE:
    case ZMAPFEATURE_STRUCT_INVALID:
    default:
      {
	status = ZMAP_CONTEXT_EXEC_STATUS_ERROR;
	zMapAssertNotReached();
	break;
      }
    }

  return status ;
}



/* A GDataForeachFunc() to add a mode to the styles for all features in a set, note that
 * this is not efficient as we go through all features but we would need more information
 * stored in the feature set to avoid this as there may be several different types of
 * feature stored in a feature set.
 * 
 * Note that I'm setting some other style data here because we need different default bumping
 * modes etc for different feature types....
 * 
 *  */
static void addFeatureModeCB(gpointer key, gpointer data, gpointer user_data)
{
  ZMapFeature feature = (ZMapFeature)data ;
  ZMapFeatureSet feature_set = (ZMapFeatureSet)user_data ;
  ZMapFeatureTypeStyle style ;

  style = feature->style ;

  if (!zMapStyleHasMode(feature->style))
    {
      ZMapStyleMode mode ;

      switch (feature->type)
	{
	case ZMAPFEATURE_BASIC:
	  mode = ZMAPSTYLE_MODE_BASIC ;

	  if (g_ascii_strcasecmp(g_quark_to_string(zMapStyleGetID(style)), "GF_splice") == 0)
	    {
	      mode = ZMAPSTYLE_MODE_GLYPH ;
	      zMapStyleSetGlyphMode(style, ZMAPSTYLE_GLYPH_SPLICE) ;

	      zMapStyleSetColours(style, ZMAPSTYLE_COLOURTARGET_FRAME0, ZMAPSTYLE_COLOURTYPE_NORMAL,
				  "red", NULL, NULL) ;
	      zMapStyleSetColours(style, ZMAPSTYLE_COLOURTARGET_FRAME1, ZMAPSTYLE_COLOURTYPE_NORMAL,
				  "blue", NULL, NULL) ;
	      zMapStyleSetColours(style, ZMAPSTYLE_COLOURTARGET_FRAME2, ZMAPSTYLE_COLOURTYPE_NORMAL,
				  "green", NULL, NULL) ;
	    }
	  break ;
	case ZMAPFEATURE_ALIGNMENT:
	  {
	    mode = ZMAPSTYLE_MODE_ALIGNMENT ;

	    /* Initially alignments should not be bumped. */
	    zMapStyleInitOverlapMode(style, ZMAPOVERLAP_COMPLEX_LIMIT, ZMAPOVERLAP_COMPLETE) ;

	    break ;
	  }
	case ZMAPFEATURE_TRANSCRIPT:
	  {
	    mode = ZMAPSTYLE_MODE_TRANSCRIPT ;

	    /* We simply never want transcripts to overlap. */
	    zMapStyleInitOverlapMode(style, ZMAPOVERLAP_COMPLEX, ZMAPOVERLAP_COMPLEX) ;
	    /* We also never need them to be hidden when they don't overlap the marked region. */
	    zMapStyleSetBumpSensitivity(style, TRUE);
	    break ;
	  }
	case ZMAPFEATURE_RAW_SEQUENCE:
	  mode = ZMAPSTYLE_MODE_TEXT ;
	  break ;
	case ZMAPFEATURE_PEP_SEQUENCE:
	  mode = ZMAPSTYLE_MODE_TEXT ;
	  break ;
	  /* What about glyph and graph..... */

	default:
	  zMapAssertNotReached() ;
	  break ;
	}

      /* Tricky....we can have features within a single feature set that have _different_
       * styles, if this is the case we must be sure to set the mode in feature_set style
       * (where in fact its kind of useless as this is a style for the whole column) _and_
       * we must set it in the features own style. */
      zMapStyleSetMode(feature_set->style, mode) ;

      if (feature_set->style != style)
	zMapStyleSetMode(style, mode) ;
    }


  return ;
}



