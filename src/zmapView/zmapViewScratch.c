/*  Last edited: 30 Oct 16"27 2012 (gb10) */
/*  File: zmapViewScratchColumn.c
 *  Author: Gemma Barson (gb10@sanger.ac.uk)
 *  Copyright (c) 2012: Genome Research Ltd.
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
 * Description: Handles the 'scratch' or 'edit' column, which allows users to
 *              create and edit temporary features
 *
 * Exported functions: see zmapView_P.h
 *
 *-------------------------------------------------------------------
 */

#include <glib.h>

#include <ZMap/zmap.h>
#include <ZMap/zmapUtils.h>
#include <ZMap/zmapFeature.h>
#include <zmapView_P.h>


  
typedef struct _GetFeaturesetCBDataStruct
{
  GQuark set_id;
  ZMapFeatureSet featureset;
} GetFeaturesetCBDataStruct, *GetFeaturesetCBData;


typedef struct _ScratchMergeDataStruct
{
  ZMapView view;
  FooCanvasItem *src_item;
  ZMapFeature src_feature;     /* the new feature to be merged in, i.e. the source for the merge */ 
  ZMapFeatureSet dest_featureset; /* the scratch column featureset */
  ZMapFeature dest_feature;    /* the scratch column feature, i.e. the destination for the merge */
  double world_x;              /* clicked position */
  double world_y;              /* clicked position */
  gboolean use_subfeature;     /* if true, just use the clicked subfeature, otherwise use the whole feature */
  GError **error;              /* gets set if any problems */
} ScratchMergeDataStruct, *ScratchMergeData;
  



/*!
 * \brief Get the flag which indicates whether the start/end
 * of the feature have been set
 */
static gboolean scratchGetStartEndFlag(ZMapView view)
{
  gboolean value = FALSE;
  
  /* Update the relevant flag for the current strand */
  if (view->revcomped_features)
    value = view->scratch_start_end_set_rev;
  else
    value = view->scratch_start_end_set;
  
  return value;
}


/*!
 * \brief Update the flag which indicates whether the start/end
 * of the feature have been set
 */
static void scratchSetStartEndFlag(ZMapView view, gboolean value)
{
  /* Update the relevant flag for the current strand */
  if (view->revcomped_features)
    view->scratch_start_end_set_rev = value;
  else
    view->scratch_start_end_set = value;
}


/*!
 * \brief Callback called on every child in a FeatureAny.
 * 
 * For each featureset, compares its id to the id in the user
 * data and if it matches it sets the result in the user data.
 */
static ZMapFeatureContextExecuteStatus getFeaturesetFromIdCB(GQuark key,
                                                             gpointer data,
                                                             gpointer user_data,
                                                             char **err_out)
{
  ZMapFeatureContextExecuteStatus status = ZMAP_CONTEXT_EXEC_STATUS_OK;
  
  GetFeaturesetCBData cb_data = (GetFeaturesetCBData) user_data ;
  ZMapFeatureAny feature_any = (ZMapFeatureAny)data;

  switch(feature_any->struct_type)
    {
    case ZMAPFEATURE_STRUCT_FEATURESET:
      if (feature_any->unique_id == cb_data->set_id)
        cb_data->featureset = (ZMapFeatureSet)feature_any;
      break;
      
    default:
      break;
    };
  
  return status;
}


/*!
 * \brief Find the featureset with the given id in the given view's context
 */
static ZMapFeatureSet getFeaturesetFromId(ZMapView view, GQuark set_id)
{
  GetFeaturesetCBDataStruct cb_data = { set_id, NULL };
  
  zMapFeatureContextExecute((ZMapFeatureAny)view->features,
                            ZMAPFEATURE_STRUCT_FEATURESET,
                            getFeaturesetFromIdCB,
                            &cb_data);

  return cb_data.featureset ;
}


/*!
 * \brief Get the single featureset that resides in the scratch column
 *
 * \returns The ZMapFeatureSet, or NULL if there was a problem
 */
static ZMapFeatureSet scratchGetFeatureset(ZMapView view)
{
  ZMapFeatureSet feature_set = NULL;

  GQuark column_id = zMapFeatureSetCreateID(ZMAP_FIXED_STYLE_SCRATCH_NAME);
  GList *fs_list = zMapFeatureGetColumnFeatureSets(&view->context_map, column_id, TRUE);
  
  /* There should be one (and only one) featureset in the column */
  if (g_list_length(fs_list) > 0)
    {         
      GQuark set_id = (GQuark)(GPOINTER_TO_INT(fs_list->data));
      feature_set = getFeaturesetFromId(view, set_id);
    }
  else
    {
      zMapWarning("No featureset for column '%s'\n", ZMAP_FIXED_STYLE_SCRATCH_NAME);
    }

  return feature_set;
}


/*! 
 * \brief Get the single feature that resides in the scratch column featureset for this strand
 *
 * \returns The ZMapFeature, or NULL if there was a problem
 */
static ZMapFeature scratchGetFeature(ZMapFeatureSet feature_set, ZMapStrand strand)
{
  ZMapFeature feature = NULL;
  
  ZMapFeatureAny feature_any = (ZMapFeatureAny)feature_set;

  GHashTableIter iter;
  gpointer key, value;
      
  g_hash_table_iter_init (&iter, feature_any->children);
  
  while (g_hash_table_iter_next (&iter, &key, &value) && !feature)
    {
      feature = (ZMapFeature)(value);
      
      if (feature->strand != strand)
        feature = NULL;
    }

  return feature;
}


/*!
 * \brief Merge the given start/end coords into the scratch column feature
 */
static gboolean scratchMergeCoords(ScratchMergeData merge_data, const int coord1, const int coord2)
{
  gboolean result = FALSE;
  
  /* If the start and end have not been set and we're merging in a single subfeature
   * then set the start/end from the subfeature (for a single subfeature, this function
   * only gets called once, so it's safe to set it here; when merging a 'full' feature
   * e.g. a transcript this will get called for each exon, so we need to set the feature
   * extent from a higher level). */
  if (merge_data->use_subfeature && !scratchGetStartEndFlag(merge_data->view))
    {
      merge_data->dest_feature->x1 = coord1;
      merge_data->dest_feature->x2 = coord2;
    }

  zMapFeatureTranscriptMergeExon(merge_data->dest_feature, coord1, coord2);
  result = TRUE;
  
  return result;
}


/*!
 * \brief Merge a single coord into the scratch column feature
 */
static gboolean scratchMergeCoord(ScratchMergeData merge_data, const int coord, const ZMapBoundaryType boundary)
{
  gboolean merged = FALSE;

  /* Check that the scratch feature already contains at least one
   * exon, otherwise we cannot merge a single coord. */
  if (zMapFeatureTranscriptGetNumExons(merge_data->dest_feature) > 0)
    {
      zMapFeatureRemoveIntrons(merge_data->dest_feature);
      
      /* Merge in the new exons */
      merged = zMapFeatureTranscriptMergeCoord(merge_data->dest_feature, coord, boundary, merge_data->error);
    }
  else
    {
      g_set_error(merge_data->error, g_quark_from_string("ZMap"), 99, "Cannot merge a single coordinate");
    }

  zMapFeatureTranscriptRecreateIntrons(merge_data->dest_feature);
  
  return merged;
}


/*! 
 * \brief Merge a single exon into the scratch column feature
 */
static void scratchMergeExonCB(gpointer exon, gpointer user_data)
{
  ZMapSpan exon_span = (ZMapSpan)exon;
  ScratchMergeData merge_data = (ScratchMergeData)user_data;

  scratchMergeCoords(merge_data, exon_span->x1, exon_span->x2);
}


/*! 
 * \brief Merge a single alignment match into the scratch column feature
 */
static void scratchMergeMatchCB(gpointer match, gpointer user_data)
{
  ZMapAlignBlock match_block = (ZMapAlignBlock)match;
  ScratchMergeData merge_data = (ScratchMergeData)user_data;

  scratchMergeCoords(merge_data, match_block->q1, match_block->q2);
}


/*! 
 * \brief Add/merge a single base into the scratch column
 */
static gboolean scratchMergeBase(ScratchMergeData merge_data)
{
  gboolean merged = FALSE;
  long seq_start=0.0, seq_end=0.0;      

  /* Convert the world coords to sequence coords */
  if (zMapWindowItemGetSeqCoord(merge_data->src_item, TRUE, merge_data->world_x, merge_data->world_y, &seq_start, &seq_end))
    {
      merged = scratchMergeCoord(merge_data, seq_start, ZMAPBOUNDARY_NONE);
    }
  else
    {
      g_set_error(merge_data->error, g_quark_from_string("ZMap"), 99, "Program error: not a featureset item");
    }


  return merged;
}


/*! 
 * \brief Add/merge a basic feature to the scratch column
 */
static gboolean scratchMergeBasic(ScratchMergeData merge_data)
{
  gboolean merged = TRUE;
  
  zMapFeatureRemoveIntrons(merge_data->dest_feature);

  /* Just merge the start/end of the feature */
  merged = scratchMergeCoords(merge_data, merge_data->src_feature->x1, merge_data->src_feature->x2);

  /* Recreate the introns */
  zMapFeatureTranscriptRecreateIntrons(merge_data->dest_feature);
  
  return merged;
}


/*! 
 * \brief Add/merge a splice feature to the scratch column
 */
static gboolean scratchMergeSplice(ScratchMergeData merge_data)
{
  gboolean merged = FALSE;
  
  zMapFeatureRemoveIntrons(merge_data->dest_feature);

  /* Just merge the start/end of the feature */
  merged = scratchMergeCoord(merge_data, merge_data->src_feature->x1, merge_data->src_feature->boundary_type);

  /* Recreate the introns */
  zMapFeatureTranscriptRecreateIntrons(merge_data->dest_feature);
  
  return merged;
}


/*! 
 * \brief Add/merge an alignment feature to the scratch column
 */
static gboolean scratchMergeAlignment(ScratchMergeData merge_data)
{
  gboolean merged = FALSE;
  
  zMapFeatureRemoveIntrons(merge_data->dest_feature);

  /* Loop through each match block and merge it in */
  if (!zMapFeatureAlignmentMatchForeach(merge_data->src_feature, scratchMergeMatchCB, merge_data))
    {
      /* The above returns false if no match blocks were found. If the alignment is 
       * gapped but we don't have this data then we can't process it. Otherwise, it's
       * an ungapped alignment so add the whole thing. */
      if (zMapFeatureAlignmentIsGapped(merge_data->src_feature))
        {
          g_set_error(merge_data->error, g_quark_from_string("ZMap"), 99, "Cannot copy feature; gapped alignment data is not available");
        }
      else
        {
          merged = scratchMergeCoords(merge_data, merge_data->src_feature->x1, merge_data->src_feature->x2);
        }
    }

  /* Copy CDS, if set */
  //  zMapFeatureMergeTranscriptCDS(merge_data->src_feature, merge_data->dest_feature);

  /* Recreate the introns */
  zMapFeatureTranscriptRecreateIntrons(merge_data->dest_feature);

  return merged;
}


/*! 
 * \brief Add/merge a transcript feature to the scratch column
 */
static gboolean scratchMergeTranscript(ScratchMergeData merge_data)
{
  gboolean merged = TRUE;
  
  zMapFeatureRemoveIntrons(merge_data->dest_feature);

  /* Merge in all of the new exons from the new feature */
  zMapFeatureTranscriptExonForeach(merge_data->src_feature, scratchMergeExonCB, merge_data);
  
  /* Copy CDS, if set */
  zMapFeatureMergeTranscriptCDS(merge_data->src_feature, merge_data->dest_feature);

  /* Recreate the introns */
  zMapFeatureTranscriptRecreateIntrons(merge_data->dest_feature);

  return merged;
}


/*! 
 * \brief Add/merge a feature to the scratch column
 */
static void scratchMergeFeature(ScratchMergeData merge_data)
{
  gboolean merged = FALSE;

  if (merge_data->use_subfeature)
    {
      /* Just merge the clicked subfeature */
      ZMapFeatureSubPartSpan sub_feature = NULL;
      zMapWindowItemGetInterval(merge_data->src_item, merge_data->world_x, merge_data->world_y, &sub_feature);
      
      if (sub_feature)
        {
          if (sub_feature->start == sub_feature->end)
            merged = scratchMergeCoord(merge_data, sub_feature->start, ZMAPBOUNDARY_NONE);
          else
            merged = scratchMergeCoords(merge_data, sub_feature->start, sub_feature->end);

          zMapFeatureTranscriptRecreateIntrons(merge_data->dest_feature);
        }
      else
        {
          g_set_error(merge_data->error, g_quark_from_string("ZMap"), 99, 
                      "Could not find subfeature in feature '%s' at coords %d, %d", 
                      g_quark_to_string(merge_data->src_feature->original_id),
                      (int)merge_data->world_x, (int)merge_data->world_y);
        }
    }
  else
    {
      /* If the start/end is not set, set it now from the feature extent */
      if (!scratchGetStartEndFlag(merge_data->view))
        {
          merge_data->dest_feature->x1 = merge_data->src_feature->x1;
          merge_data->dest_feature->x2 = merge_data->src_feature->x2;
        }

      switch (merge_data->src_feature->type)
        {
        case ZMAPSTYLE_MODE_BASIC:
          merged = scratchMergeBasic(merge_data);
          break;
        case ZMAPSTYLE_MODE_ALIGNMENT:
          merged = scratchMergeAlignment(merge_data);
          break;
        case ZMAPSTYLE_MODE_TRANSCRIPT:
          merged = scratchMergeTranscript(merge_data);
          break;
        case ZMAPSTYLE_MODE_SEQUENCE:
          merged = scratchMergeBase(merge_data);
          break;
        case ZMAPSTYLE_MODE_GLYPH:
          merged = scratchMergeSplice(merge_data);
          break;

        case ZMAPSTYLE_MODE_INVALID:       /* fall through */
        case ZMAPSTYLE_MODE_ASSEMBLY_PATH: /* fall through */
        case ZMAPSTYLE_MODE_TEXT:          /* fall through */
        case ZMAPSTYLE_MODE_GRAPH:         /* fall through */
        case ZMAPSTYLE_MODE_PLAIN:         /* fall through */
        case ZMAPSTYLE_MODE_META:          /* fall through */
        default:
          g_set_error(merge_data->error, g_quark_from_string("ZMap"), 99, "copy of feature of type %d is not implemented.\n", merge_data->src_feature->type);
          break;
        };
    }
  
  /* Once finished merging, the start/end should now be set */
  if (merged)
    {
      scratchSetStartEndFlag(merge_data->view, TRUE);
    }
  
  if (*(merge_data->error))
    {
      if (merged)
        zMapWarning("Warning while copying feature: %s", (*(merge_data->error))->message);
      else
        zMapCritical("Error copying feature: %s", (*(merge_data->error))->message);
      
      g_error_free(*(merge_data->error));
      merge_data->error = NULL;
    }
}


/*!
 * \brief Utility to delete all exons from the given feature
 */
static void scratchDeleteFeatureExons(ZMapView view, ZMapFeature feature, ZMapFeatureSet feature_set)
{
  if (feature)
    {
      zMapFeatureRemoveExons(feature);
      zMapFeatureRemoveIntrons(feature);
    }  
}




static void handBuiltInit(ZMapView zmap_view, ZMapFeatureSequenceMap sequence, ZMapFeatureContext context)
{
  ZMapFeatureSet featureset = NULL ;
  ZMapFeatureTypeStyle style = NULL ;
  ZMapFeatureSetDesc f2c;
  ZMapFeatureSource src;
  GList *list;
  ZMapFeatureColumn column;

  ZMapFeatureContextMap context_map = &zmap_view->context_map;

  if((style = zMapFindStyle(context_map->styles, zMapStyleCreateID(ZMAP_FIXED_STYLE_HAND_BUILT_NAME))))
    {
      /* Create the featureset */
      featureset = zMapFeatureSetCreate(ZMAP_FIXED_STYLE_HAND_BUILT_NAME, NULL);
      style = zMapFeatureStyleCopy(style);
      featureset->style = style ;

      /* Create the context, align and block, and add the featureset to it */
      if (!context)
        context = zmapViewCreateContext(zmap_view, NULL, featureset);

	/* set up featureset2_column and anything else needed */
      f2c = g_hash_table_lookup(context_map->featureset_2_column, GUINT_TO_POINTER(featureset->unique_id));
      if(!f2c)	/* these just accumulate  and should be removed from the hash table on clear */
	{
		f2c = g_new0(ZMapFeatureSetDescStruct,1);

		f2c->column_id = zMapFeatureSetCreateID(ZMAP_FIXED_STYLE_HAND_BUILT_NAME);
		f2c->column_ID = g_quark_from_string(ZMAP_FIXED_STYLE_HAND_BUILT_NAME);
		f2c->feature_src_ID = g_quark_from_string(ZMAP_FIXED_STYLE_HAND_BUILT_NAME);
		f2c->feature_set_text = ZMAP_FIXED_STYLE_HAND_BUILT_TEXT;
		g_hash_table_insert(context_map->featureset_2_column, GUINT_TO_POINTER(featureset->unique_id), f2c);
	}

      src = g_hash_table_lookup(context_map->source_2_sourcedata, GUINT_TO_POINTER(featureset->unique_id));
      if(!src)
	{
		src = g_new0(ZMapFeatureSourceStruct,1);
		src->source_id = f2c->feature_src_ID;
		src->source_text = g_quark_from_string(ZMAP_FIXED_STYLE_HAND_BUILT_TEXT);
		src->style_id = style->unique_id;
		src->maps_to = f2c->column_id;
		g_hash_table_insert(context_map->source_2_sourcedata, GUINT_TO_POINTER(featureset->unique_id), src);
	}

      list = g_hash_table_lookup(context_map->column_2_styles,GUINT_TO_POINTER(f2c->column_id));
      if(!list)
	{
		list = g_list_prepend(list,GUINT_TO_POINTER(src->style_id));
		g_hash_table_insert(context_map->column_2_styles,GUINT_TO_POINTER(f2c->column_id), list);
	}

      column = g_hash_table_lookup(context_map->columns,GUINT_TO_POINTER(f2c->column_id));
      if(!column)
	{
		column = g_new0(ZMapFeatureColumnStruct,1);
		column->unique_id = f2c->column_id;
		column->style_table = g_list_prepend(NULL, (gpointer)  style);
		/* the rest shoudl get filled in elsewhere */
		g_hash_table_insert(context_map->columns, GUINT_TO_POINTER(f2c->column_id), column);
	}
    }  
}


/*!
 * \brief Initialise the Scratch column
 *
 * \param[in] zmap_view
 * \param[in] sequence
 *
 * \return void
 */
void zmapViewScratchInit(ZMapView zmap_view, ZMapFeatureSequenceMap sequence, ZMapFeatureContext context_in, ZMapFeatureBlock block_in)
{
  ZMapFeatureSet scratch_featureset = NULL ;
  ZMapFeatureTypeStyle style = NULL ;
  ZMapFeatureSetDesc f2c;
  ZMapFeatureSource src;
  GList *list;
  ZMapFeatureColumn column;

  ZMapFeatureContextMap context_map = &zmap_view->context_map;
  ZMapFeatureContext context = context_in;
  ZMapFeatureBlock block = block_in;

  if((style = zMapFindStyle(context_map->styles, zMapStyleCreateID(ZMAP_FIXED_STYLE_SCRATCH_NAME))))
    {
      /* Create the featureset */
      scratch_featureset = zMapFeatureSetCreate(ZMAP_FIXED_STYLE_SCRATCH_NAME, NULL);
      style = zMapFeatureStyleCopy(style);
      scratch_featureset->style = style ;

      /* Create the context, align and block, and add the featureset to it */
      if (!context || !block)
        context = zmapViewCreateContext(zmap_view, NULL, scratch_featureset);
      else
        zMapFeatureBlockAddFeatureSet(block, scratch_featureset);
      
	/* set up featureset2_column and anything else needed */
      f2c = g_hash_table_lookup(context_map->featureset_2_column, GUINT_TO_POINTER(scratch_featureset->unique_id));
      if(!f2c)	/* these just accumulate  and should be removed from the hash table on clear */
	{
		f2c = g_new0(ZMapFeatureSetDescStruct,1);

		f2c->column_id = zMapFeatureSetCreateID(ZMAP_FIXED_STYLE_SCRATCH_NAME);
		f2c->column_ID = g_quark_from_string(ZMAP_FIXED_STYLE_SCRATCH_NAME);
		f2c->feature_src_ID = g_quark_from_string(ZMAP_FIXED_STYLE_SCRATCH_NAME);
		f2c->feature_set_text = ZMAP_FIXED_STYLE_SCRATCH_TEXT;
		g_hash_table_insert(context_map->featureset_2_column, GUINT_TO_POINTER(scratch_featureset->unique_id), f2c);
	}

      src = g_hash_table_lookup(context_map->source_2_sourcedata, GUINT_TO_POINTER(scratch_featureset->unique_id));
      if(!src)
	{
		src = g_new0(ZMapFeatureSourceStruct,1);
		src->source_id = f2c->feature_src_ID;
		src->source_text = g_quark_from_string(ZMAP_FIXED_STYLE_SCRATCH_TEXT);
		src->style_id = style->unique_id;
		src->maps_to = f2c->column_id;
		g_hash_table_insert(context_map->source_2_sourcedata, GUINT_TO_POINTER(scratch_featureset->unique_id), src);
	}

      list = g_hash_table_lookup(context_map->column_2_styles,GUINT_TO_POINTER(f2c->column_id));
      if(!list)
	{
		list = g_list_prepend(list,GUINT_TO_POINTER(src->style_id));
		g_hash_table_insert(context_map->column_2_styles,GUINT_TO_POINTER(f2c->column_id), list);
	}

      column = g_hash_table_lookup(context_map->columns,GUINT_TO_POINTER(f2c->column_id));
      if(!column)
	{
		column = g_new0(ZMapFeatureColumnStruct,1);
		column->unique_id = f2c->column_id;
		column->style_table = g_list_prepend(NULL, (gpointer)  style);
		/* the rest shoudl get filled in elsewhere */
		g_hash_table_insert(context_map->columns, GUINT_TO_POINTER(f2c->column_id), column);
	}

      /* Create two empty features, one for each strand */
      ZMapFeature feature_fwd = zMapFeatureCreateEmpty() ;
      ZMapFeature feature_rev = zMapFeatureCreateEmpty() ;
      char *feature_name = "scratch_feature";
      
      zMapFeatureAddStandardData(feature_fwd, "scratch_feature_fwd", feature_name,
                                 NULL, NULL,
                                 ZMAPSTYLE_MODE_TRANSCRIPT, &scratch_featureset->style,
                                 0, 0, FALSE, 0.0,
                                 ZMAPSTRAND_FORWARD);

      zMapFeatureAddStandardData(feature_rev, "scratch_feature_rev", feature_name,
                                 NULL, NULL,
                                 ZMAPSTYLE_MODE_TRANSCRIPT, &scratch_featureset->style,
                                 0, 0, FALSE, 0.0,
                                 ZMAPSTRAND_REVERSE);

      zMapFeatureTranscriptInit(feature_fwd);
      zMapFeatureTranscriptInit(feature_rev);
      zMapFeatureAddTranscriptStartEnd(feature_fwd, FALSE, 0, FALSE);
      zMapFeatureAddTranscriptStartEnd(feature_rev, FALSE, 0, FALSE);
      
      //zMapFeatureSequenceSetType(feature, ZMAPSEQUENCE_PEPTIDE);
      //zMapFeatureAddFrame(feature, ZMAPFRAME_NONE);
      
      zMapFeatureSetAddFeature(scratch_featureset, feature_fwd);      
      zMapFeatureSetAddFeature(scratch_featureset, feature_rev);      
    }  

  /* Also initialise the "hand_built" column. xace puts newly created
   * features in this column. */
  handBuiltInit(zmap_view, sequence, context);

  /* Merge our context into the view's context and view the diff context */
  if (!context_in)
    {
      ZMapFeatureContext diff_context = zmapViewMergeInContext(zmap_view, context);
      zmapViewDrawDiffContext(zmap_view, &diff_context, NULL);
    }
}


/*!
 * \brief Does the work to update any changes to the given featureset
 */
void scratchRedrawFeature(ZMapView zmap_view, 
                          ZMapFeature feature,
                          ZMapFeatureSet feature_set,
                          ZMapFeatureContext context)
{
  zmapViewDisplayDataWindows(zmap_view, zmap_view->features, zmap_view->features, NULL, TRUE, NULL, NULL, FALSE) ;
  zmapViewDisplayDataWindows(zmap_view, zmap_view->features, zmap_view->features, NULL, FALSE, NULL, NULL, FALSE) ;
}


/*!
 * \brief Update any changes to the given featureset
 */
gboolean zmapViewScratchCopyFeature(ZMapView view, 
                                    ZMapFeature feature,
                                    FooCanvasItem *item,
                                    const double world_x,
                                    const double world_y,
                                    const gboolean use_subfeature)
{

  ZMapStrand strand = ZMAPSTRAND_FORWARD;
  ZMapFeatureSet scratch_featureset = scratchGetFeatureset(view);
  ZMapFeature scratch_feature = scratchGetFeature(scratch_featureset, strand);
  
  if (scratch_featureset && scratch_feature)
    {
      GError *error = NULL;
      ScratchMergeDataStruct merge_data = {view, item, feature, scratch_featureset, scratch_feature, world_x, world_y, use_subfeature, &error};
      scratchMergeFeature(&merge_data);
    }
  
  scratchRedrawFeature(view, scratch_feature, scratch_featureset, view->features);
  return TRUE;
}


/*!
 * \brief Clear the scratch column
 *
 * This removes the exons and introns from the feature.
 * It doesn't remove the feature itself because this 
 * singleton feature always exists.
 */
gboolean zmapViewScratchClear(ZMapView view)
{ 
  /* Get the singleton features that exist in each strand of the scatch column */
  ZMapFeatureSet scratch_featureset = scratchGetFeatureset(view);
  ZMapFeature scratch_feature = scratchGetFeature(scratch_featureset, ZMAPSTRAND_FORWARD);

  /* Delete the exons and introns (the singleton feature always
   * exists so we don't delete the whole thing). */
  scratchDeleteFeatureExons(view, scratch_feature, scratch_featureset);

  /* Reset the start-/end-set flag */
  scratchSetStartEndFlag(view, FALSE);

  if (scratch_feature)
    {
      scratchRedrawFeature(view, scratch_feature, scratch_featureset, view->features);
    }

  return TRUE;
}
