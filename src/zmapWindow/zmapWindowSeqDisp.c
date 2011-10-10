/*  Last edited: Jul 13 14:33 2011 (edgrif) */
/*  File: zmapWindowItem.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2006-2011: Genome Research Ltd.
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
 * Description: Functions to manipulate canvas items.
 *
 * Exported functions: See zmapWindow_P.h
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.h>

#include <math.h>
#include <ZMap/zmapUtils.h>
#include <ZMap/zmapGLibUtils.h>
#include <ZMap/zmapSequence.h>
#include <zmapWindow_P.h>
#include <zmapWindowContainerUtils.h>
#include <zmapWindowItemTextFillColumn.h>
#include <zmapWindowFeatures.h>
#include <zmapWindowContainerFeatureSet_I.h>



static void highlightSequenceItems(ZMapWindow window, ZMapFeatureBlock block,
				   FooCanvasItem *focus_item,
				   ZMapSequenceType seq_type, ZMapFrame frame, int start, int end,
				   gboolean centre_on_region) ;

static void handleHightlightDNA(gboolean on, gboolean item_highlight,
				ZMapWindow window, FooCanvasItem *item,
				ZMapFrame required_frame,
				ZMapSequenceType coords_type, int region_start, int region_end) ;

static void highlightTranslationRegion(ZMapWindow window,
				       gboolean highlight, gboolean item_highlight,
				       FooCanvasItem *item,
				       char *required_col, ZMapFrame required_frame,
				       ZMapSequenceType coords_type, int region_start, int region_end) ;
static void unHighlightTranslation(ZMapWindow window, FooCanvasItem *item,
				   char *required_col, ZMapFrame required_frame) ;
static void handleHighlightTranslation(gboolean highlight,  gboolean item_highlight,
				       ZMapWindow window, FooCanvasItem *item,
				       char *required_col, ZMapFrame required_frame,
				       ZMapSequenceType coords_type, int region_start, int region_end) ;
static void handleHighlightTranslationSeq(gboolean highlight, gboolean item_highlight,
					  FooCanvasItem *translation_item, FooCanvasItem *item,
					  gboolean cds_only, ZMapSequenceType coords_type,
					  int region_start, int region_end) ;
static FooCanvasItem *getTranslationItemFromItemFrame(ZMapWindow window,
						      ZMapFeatureBlock block, char *required_col, ZMapFrame frame) ;
static FooCanvasItem *translation_from_block_frame(ZMapWindow window, char *column_name,
							gboolean require_visible,
							ZMapFeatureBlock block, ZMapFrame frame) ;




/*
 *                        External functions.
 */



FooCanvasItem *zmapWindowItemGetDNAParentItem(ZMapWindow window, FooCanvasItem *item)
{
  ZMapFeature feature;
  ZMapFeatureBlock block = NULL;
  FooCanvasItem *dna_item = NULL;
  GQuark feature_set_unique = 0, dna_id = 0;
  char *feature_name = NULL;

  feature_set_unique = zMapStyleCreateID(ZMAP_FIXED_STYLE_DNA_NAME);

  if ((feature = zmapWindowItemGetFeature(item)))
    {
      if((block = (ZMapFeatureBlock)(zMapFeatureGetParentGroup((ZMapFeatureAny)feature, ZMAPFEATURE_STRUCT_BLOCK))) &&
         (feature_name = zMapFeatureDNAFeatureName(block)))
        {
          dna_id = zMapFeatureCreateID(ZMAPSTYLE_MODE_SEQUENCE,
                                       feature_name,
                                       ZMAPSTRAND_FORWARD, /* ALWAYS FORWARD */
                                       block->block_to_sequence.block.x1,
                                       block->block_to_sequence.block.x2,
                                       0,0);
          g_free(feature_name);
        }

      if((dna_item = zmapWindowFToIFindItemFull(window,window->context_to_item,
						block->parent->unique_id,
						block->unique_id,
						feature_set_unique,
						ZMAPSTRAND_FORWARD, /* STILL ALWAYS FORWARD */
						ZMAPFRAME_NONE,/* NO STRAND */
						dna_id)))
	{
	  if(!(FOO_CANVAS_ITEM(dna_item)->object.flags & FOO_CANVAS_ITEM_VISIBLE))
	    dna_item = NULL;
	}
    }
  else
    {
      zMapAssertNotReached();
    }

  return dna_item;
}

FooCanvasItem *zmapWindowItemGetDNATextItem(ZMapWindow window, FooCanvasItem *item)
{
  FooCanvasItem *dna_item = NULL ;
  ZMapFeatureAny feature_any = NULL ;
  ZMapFeatureBlock block = NULL ;

  if ((feature_any = zmapWindowItemGetFeatureAny(item))
      && (block = (ZMapFeatureBlock)zMapFeatureGetParentGroup(feature_any, ZMAPFEATURE_STRUCT_BLOCK)))
    {
      GQuark dna_set_id ;
      GQuark dna_id ;

      dna_set_id = zMapFeatureSetCreateID(ZMAP_FIXED_STYLE_DNA_NAME);

      dna_id = zMapFeatureDNAFeatureID(block);


      if ((dna_item = zmapWindowFToIFindItemFull(window,window->context_to_item,
						 block->parent->unique_id,
						 block->unique_id,
						 dna_set_id,
						 ZMAPSTRAND_FORWARD, /* STILL ALWAYS FORWARD */
						 ZMAPFRAME_NONE, /* NO FRAME */
						 dna_id)))
	{

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	  /* this ought to work but it doesn't, I think this may because the parent (column) gets
	   * unmapped but not the dna text item, so it's not visible any more but it is mapped ? */
	  if(!(FOO_CANVAS_ITEM(dna_item)->object.flags & FOO_CANVAS_ITEM_VISIBLE))
	    dna_item = NULL;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	  if (!(dna_item->object.flags & FOO_CANVAS_ITEM_MAPPED))
	    dna_item = NULL;
	}
    }

  return dna_item ;
}




/*
 * Sequence highlighting functions, these feel like they should be in the
 * sequence item class objects.
 */

void zmapWindowHighlightSequenceItem(ZMapWindow window, FooCanvasItem *item)
{
  ZMapFeatureAny feature_any ;

  if ((feature_any = zmapWindowItemGetFeatureAny(FOO_CANVAS_ITEM(item))))
    {
      ZMapFeatureBlock block ;

      block = (ZMapFeatureBlock)(zMapFeatureGetParentGroup((ZMapFeatureAny)(feature_any), ZMAPFEATURE_STRUCT_BLOCK));
      zMapAssert(block);

      highlightSequenceItems(window, block, item, ZMAPSEQUENCE_NONE, ZMAPFRAME_NONE, 0, 0, FALSE) ;
    }

  return ;
}







void zmapWindowHighlightSequenceRegion(ZMapWindow window, ZMapFeatureBlock block,
				       ZMapSequenceType seq_type, ZMapFrame frame, int start, int end,
				       gboolean centre_on_region)
{
  highlightSequenceItems(window, block, NULL, seq_type, frame, start, end, centre_on_region) ;

  return ;
}



/* highlights the dna given any foocanvasitem (with a feature) and a start and end
 * This _only_ highlights in the current window! */
void zmapWindowItemHighlightDNARegion(ZMapWindow window, gboolean item_highlight,
				      FooCanvasItem *item, ZMapFrame required_frame,
				      ZMapSequenceType coords_type, int region_start, int region_end)
{
  handleHightlightDNA(TRUE, item_highlight, window, item, required_frame,
		      coords_type, region_start, region_end) ;

  return ;
}


void zmapWindowItemUnHighlightDNA(ZMapWindow window, FooCanvasItem *item)
{
  handleHightlightDNA(FALSE, FALSE, window, item, ZMAPFRAME_NONE, ZMAPSEQUENCE_NONE, 0, 0) ;

  return ;
}



/* This function highlights the peptide translation columns from region_start to region_end
 * (in dna or pep coords). Note any existing highlight is removed. If required_frame
 * is set to ZMAPFRAME_NONE then highlighting is in all 3 cols otherwise only in the
 * frame column given. */
void zmapWindowItemHighlightTranslationRegions(ZMapWindow window, gboolean item_highlight,
					       FooCanvasItem *item,
					       ZMapFrame required_frame,
					       ZMapSequenceType coords_type,
					       int region_start, int region_end)
{
  ZMapFrame frame ;

  for (frame = ZMAPFRAME_0 ; frame < ZMAPFRAME_2 + 1 ; frame++)
    {
      gboolean highlight ;

      if (required_frame == ZMAPFRAME_NONE || frame == required_frame)
	highlight = TRUE ;
      else
	highlight = FALSE ;

      highlightTranslationRegion(window,
				 highlight, item_highlight, item, 
				 ZMAP_FIXED_STYLE_3FT_NAME, frame,
				 coords_type, region_start, region_end) ;
    }

  return ;
}


void zmapWindowItemUnHighlightTranslations(ZMapWindow window, FooCanvasItem *item)
{
  ZMapFrame frame ;

  for (frame = ZMAPFRAME_0 ; frame < ZMAPFRAME_2 + 1 ; frame++)
    {
      unHighlightTranslation(window, item, ZMAP_FIXED_STYLE_3FT_NAME, frame) ;
    }

  return ;
}


/* This function highlights the translation for a feature from region_start to region_end
 * (in dna or pep coords). Note any existing highlight is removed. */
void zmapWindowItemHighlightShowTranslationRegion(ZMapWindow window, gboolean item_highlight,
						  FooCanvasItem *item,
						  ZMapFrame required_frame,
						  ZMapSequenceType coords_type,
						  int region_start, int region_end)
{
  highlightTranslationRegion(window, TRUE, item_highlight,
			     item, ZMAP_FIXED_STYLE_SHOWTRANSLATION_NAME, required_frame,
			     coords_type, region_start, region_end) ;
  return ;
}


void zmapWindowItemUnHighlightShowTranslations(ZMapWindow window, FooCanvasItem *item)
{
  unHighlightTranslation(window, item, ZMAP_FIXED_STYLE_SHOWTRANSLATION_NAME, ZMAPFRAME_NONE) ;

  return ;
}





FooCanvasItem *zmapWindowItemGetShowTranslationColumn(ZMapWindow window, FooCanvasItem *item)
{
  FooCanvasItem *translation = NULL;
  ZMapFeature feature;
  ZMapFeatureBlock block;

  if ((feature = zmapWindowItemGetFeature(item)))
    {
      ZMapFeatureSet feature_set;
      ZMapFeatureTypeStyle style;

      /* First go up to block... */
      block = (ZMapFeatureBlock)(zMapFeatureGetParentGroup((ZMapFeatureAny)(feature), ZMAPFEATURE_STRUCT_BLOCK));
      zMapAssert(block);

      /* Get the frame for the item... and its translation feature (ITEM_FEATURE_PARENT!) */
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      if ((style = zMapFeatureContextFindStyle((ZMapFeatureContext)(block->parent->parent),
					       ZMAP_FIXED_STYLE_SHOWTRANSLATION_NAME))
	  && !(feature_set = zMapFeatureBlockGetSetByID(block,
							zMapStyleCreateID(ZMAP_FIXED_STYLE_SHOWTRANSLATION_NAME))))
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
	if ((style = zMapFindStyle(window->context_map->styles, zMapStyleCreateID(ZMAP_FIXED_STYLE_SHOWTRANSLATION_NAME)))
	    && !(feature_set = zMapFeatureBlockGetSetByID(block,
							  zMapStyleCreateID(ZMAP_FIXED_STYLE_SHOWTRANSLATION_NAME))))

	{
	  /* Feature set doesn't exist, so create. */
	  feature_set = zMapFeatureSetCreate(ZMAP_FIXED_STYLE_SHOWTRANSLATION_NAME, NULL);
	  //feature_set->style = style;
	  zMapFeatureBlockAddFeatureSet(block, feature_set);
	}

      if (feature_set)
	{
	  ZMapWindowContainerGroup parent_container;
	  ZMapWindowContainerStrand forward_container;
	  ZMapWindowContainerFeatures forward_features;
	  FooCanvasGroup *tmp_forward, *tmp_reverse;

#ifdef SIMPLIER
	  FooCanvasGroup *forward_group, *parent_group, *tmp_forward, *tmp_reverse ;
	  /* Get the FeatureSet Level Container */
	  parent_group = zmapWindowContainerCanvasItemGetContainer(item);
	  /* Get the Strand Level Container (Could be Forward OR Reverse) */
	  parent_group = zmapWindowContainerGetSuperGroup(parent_group);
	  /* Get the Block Level Container... */
	  parent_group = zmapWindowContainerGetSuperGroup(parent_group);
#endif /* SIMPLIER */

	  parent_container = zmapWindowContainerUtilsItemGetParentLevel(item, ZMAPCONTAINER_LEVEL_BLOCK);

	  /* Get the Forward Group Parent Container... */
	  forward_container = zmapWindowContainerBlockGetContainerStrand((ZMapWindowContainerBlock)parent_container, ZMAPSTRAND_FORWARD);
	  /* zmapWindowCreateSetColumns needs the Features not the Parent. */
	  forward_features  = zmapWindowContainerGetFeatures((ZMapWindowContainerGroup)forward_container);

	  /* make the column... */
	  if (zmapWindowCreateSetColumns(window,
					 forward_features,
					 NULL,
					 block,
					 feature_set,
					 window->context_map->styles,
					 ZMAPFRAME_NONE,
					 &tmp_forward, &tmp_reverse, NULL))
	    {
	      translation = FOO_CANVAS_ITEM(tmp_forward);
	    }
	}
      else
	zMapLogWarning("Failed to find Feature Set for '%s'", ZMAP_FIXED_STYLE_SHOWTRANSLATION_NAME);
    }

  return translation ;
}





/*
 *                  Internal routines.
 */



static void highlightSequenceItems(ZMapWindow window, ZMapFeatureBlock block,
				   FooCanvasItem *focus_item,
				   ZMapSequenceType seq_type, ZMapFrame required_frame, int start, int end,
				   gboolean centre_on_region)
{
  FooCanvasItem *item ;
  GQuark set_id ;
  ZMapFrame tmp_frame ;
  ZMapStrand tmp_strand ;
  gboolean done_centring = FALSE ;


  /* If there is a dna column then highlight match in that. */
  tmp_strand = ZMAPSTRAND_NONE ;
  tmp_frame = ZMAPFRAME_NONE ;
  set_id = zMapStyleCreateID(ZMAP_FIXED_STYLE_DNA_NAME) ;

  if ((item = zmapWindowFToIFindItemFull(window,window->context_to_item,
					 block->parent->unique_id, block->unique_id,
					 set_id, tmp_strand, tmp_frame, 0)))
    {
      int dna_start, dna_end ;

      dna_start = start ;
      dna_end = end ;

      zmapWindowItemHighlightDNARegion(window, FALSE, item, required_frame, seq_type, dna_start, dna_end) ;

      if (centre_on_region)
	{
	  /* Need to convert sequence coords to block for this call. */
	  zMapFeature2BlockCoords(block, &dna_start, &dna_end) ;


	  zmapWindowItemCentreOnItemSubPart(window, item, FALSE, 0.0, dna_start, dna_end) ;
	  done_centring = TRUE ;
	}
    }


  /* If there are peptide columns then highlight match in those. */
  tmp_strand = ZMAPSTRAND_NONE ;
  tmp_frame = ZMAPFRAME_NONE ;
  set_id = zMapStyleCreateID(ZMAP_FIXED_STYLE_3FT_NAME) ;

  if ((item = zmapWindowFToIFindItemFull(window,window->context_to_item,
					 block->parent->unique_id, block->unique_id,
					 set_id, tmp_strand, tmp_frame, 0)))
    {
      int frame_num, pep_start, pep_end ;



      for (frame_num = ZMAPFRAME_0 ; frame_num <= ZMAPFRAME_2 ; frame_num++)
	{
	  pep_start = start ;
	  pep_end = end ;

	  if (seq_type == ZMAPSEQUENCE_DNA)
	    {
	      highlightTranslationRegion(window, TRUE, FALSE,
					 item, ZMAP_FIXED_STYLE_3FT_NAME, frame_num, seq_type, pep_start, pep_end) ;
	    }
	  else
	    {
	      if (frame_num == required_frame)
		highlightTranslationRegion(window, TRUE, FALSE, item,
					   ZMAP_FIXED_STYLE_3FT_NAME, frame_num, seq_type, pep_start, pep_end) ;
	      else
		unHighlightTranslation(window, item, ZMAP_FIXED_STYLE_3FT_NAME, frame_num) ;
	    }
	}

      if (centre_on_region && !done_centring)
	zmapWindowItemCentreOnItemSubPart(window, item, FALSE, 0.0, start, end) ;
    }


  return ;
}








static void handleHightlightDNA(gboolean on, gboolean item_highlight,
				ZMapWindow window, FooCanvasItem *item, ZMapFrame required_frame,
				ZMapSequenceType coords_type, int region_start, int region_end)
{
  FooCanvasItem *dna_item ;

  if ((dna_item = zmapWindowItemGetDNATextItem(window, item))
      && ZMAP_IS_WINDOW_SEQUENCE_FEATURE(dna_item) && item != dna_item)
    {
      ZMapWindowSequenceFeature sequence_feature ;
      ZMapFeature feature ;

      sequence_feature = (ZMapWindowSequenceFeature)dna_item ;

      /* highlight specially for transcripts (i.e. exons). */
      if (on)
	{
	  if (item_highlight && (feature = zmapWindowItemGetFeature(item)))
	    {
	      zMapWindowSequenceFeatureSelectByFeature(sequence_feature, item, feature, FALSE) ;
	    }
	  else
	    {
	      zMapWindowSequenceFeatureSelectByRegion(sequence_feature, coords_type, region_start, region_end) ;
	    }
	}
      else
	{
	  zMapWindowSequenceDeSelect(sequence_feature) ;
	}
    }

  return ;
}


/* highlights the translation given any foocanvasitem (with a
 * feature), frame and a start and end (protein seq coords) */
/* This _only_ highlights in the current window! */
static void highlightTranslationRegion(ZMapWindow window,
				       gboolean highlight, gboolean item_highlight, FooCanvasItem *item,
				       char *required_col, ZMapFrame required_frame,
				       ZMapSequenceType coords_type, int region_start, int region_end)
{
  handleHighlightTranslation(highlight, item_highlight, window, item,
			     required_col, required_frame, coords_type, region_start, region_end) ;

  return ;
}


static void unHighlightTranslation(ZMapWindow window, FooCanvasItem *item,
				   char *required_col, ZMapFrame required_frame)
{
  handleHighlightTranslation(FALSE, FALSE, window, item, required_col, required_frame, ZMAPSEQUENCE_NONE, 0, 0) ;

  return ;
}


static void handleHighlightTranslation(gboolean highlight, gboolean item_highlight,
				       ZMapWindow window, FooCanvasItem *item,
				       char *required_col, ZMapFrame required_frame,
				       ZMapSequenceType coords_type, int region_start, int region_end)
{
  FooCanvasItem *translation_item = NULL;
  ZMapFeatureAny feature_any ;
  ZMapFeatureBlock block ;

  if ((feature_any = zmapWindowItemGetFeatureAny(item)))
    {
      block = (ZMapFeatureBlock)(zMapFeatureGetParentGroup((ZMapFeatureAny)(feature_any), ZMAPFEATURE_STRUCT_BLOCK)) ;

      if ((translation_item = getTranslationItemFromItemFrame(window, block, required_col, required_frame)))
	{
	  gboolean cds_only = FALSE ;

	  if (g_ascii_strcasecmp(ZMAP_FIXED_STYLE_SHOWTRANSLATION_NAME, required_col) == 0)
	    cds_only = TRUE ;

	  handleHighlightTranslationSeq(highlight, item_highlight,
					translation_item, item,
					cds_only, coords_type, region_start, region_end) ;
	}
    }

  return ;
}


static void handleHighlightTranslationSeq(gboolean highlight, gboolean item_highlight,
					  FooCanvasItem *translation_item, FooCanvasItem *item,
					  gboolean cds_only, ZMapSequenceType coords_type,
					  int region_start, int region_end)
{
  ZMapWindowSequenceFeature sequence_feature ;
  ZMapFeature feature ;

  sequence_feature = (ZMapWindowSequenceFeature)translation_item ;

  if (highlight)
    {
      if (item_highlight && (feature = zmapWindowItemGetFeature(item)))
	{
	  zMapWindowSequenceFeatureSelectByFeature(sequence_feature, item, feature, cds_only) ;
	}
      else
	{
	  zMapWindowSequenceFeatureSelectByRegion(sequence_feature, coords_type, region_start, region_end) ;
	}
    }
  else
    {
      zMapWindowSequenceDeSelect(sequence_feature) ;
    }

  return ;
}


static FooCanvasItem *getTranslationItemFromItemFrame(ZMapWindow window,
						      ZMapFeatureBlock block,
						      char *req_col_name, ZMapFrame req_frame)
{
  FooCanvasItem *translation = NULL;

  /* Get the frame for the item... and its translation feature (ITEM_FEATURE_PARENT!) */
  translation = translation_from_block_frame(window, req_col_name, TRUE, block, req_frame) ;

  return translation ;
}


static FooCanvasItem *translation_from_block_frame(ZMapWindow window, char *column_name,
						   gboolean require_visible,
						   ZMapFeatureBlock block, ZMapFrame frame)
{
  FooCanvasItem *translation = NULL;
  GQuark feature_set_id, feature_id;
  ZMapFeatureSet feature_set;
  ZMapStrand strand = ZMAPSTRAND_FORWARD;

  feature_set_id = zMapStyleCreateID(column_name);
  /* and look up the translation feature set with ^^^ */

  if ((feature_set = zMapFeatureBlockGetSetByID(block, feature_set_id)))
    {
      char *feature_name;

      /* Get the name of the framed feature...and its quark id */
      if (g_ascii_strcasecmp(ZMAP_FIXED_STYLE_SHOWTRANSLATION_NAME, column_name) == 0)
	{
	  GList *trans_set ;
	  ID2Canvas trans_id2c ;

	  feature_id = zMapStyleCreateID("*") ;

	  trans_set = zmapWindowFToIFindItemSetFull(window, window->context_to_item,
						    block->parent->unique_id, block->unique_id, feature_set_id,
						    feature_set_id, "+", ".",
						    feature_id,
						    NULL, NULL) ;

	  trans_id2c = (ID2Canvas)(trans_set->data) ;

	  translation = trans_id2c->item ;
	}
      else
	{
	  feature_name = zMapFeature3FrameTranslationFeatureName(feature_set, frame) ;
	  feature_id = g_quark_from_string(feature_name) ;

	  translation = zmapWindowFToIFindItemFull(window,window->context_to_item,
						   block->parent->unique_id,
						   block->unique_id,
						   feature_set_id,
						   strand, /* STILL ALWAYS FORWARD */
						   frame,
						   feature_id) ;

	  g_free(feature_name) ;
	}

      if (translation && require_visible && !(FOO_CANVAS_ITEM(translation)->object.flags & FOO_CANVAS_ITEM_VISIBLE))
	translation = NULL ;
    }

  return translation;
}


