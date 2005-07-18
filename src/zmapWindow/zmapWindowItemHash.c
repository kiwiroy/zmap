/*  File: zmapWindowItemHash.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) Sanger Institute, 2005
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
 *      Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk,
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk
 *
 * Description: Functions to go from an alignment, block, column or feature
 *              in a feature context to the corresponding foocanvas group or item.
 *              This is fundamental in linking our feature context hierachy
 *              to the foocanvas item hierachy.
 *
 * Exported functions: See zMapWindow_P.h
 * HISTORY:
 * Last edited: Jul 17 10:12 2005 (edgrif)
 * Created: Mon Jun 13 10:06:49 2005 (edgrif)
 * CVS info:   $Id: zmapWindowItemHash.c,v 1.5 2005-07-18 09:31:23 edgrif Exp $
 *-------------------------------------------------------------------
 */

#include <ZMap/zmapUtils.h>
#include <zmapWindow_P.h>


/* The plan is to expand the functions here to provide a kind of querying service so that
 * we can look for all features in a column for instance. */


/* Used to hold coord information + return a result the child search callback function. */
typedef struct
{
  int child_start, child_end ;
  FooCanvasItem *child_item ;
} ChildSearchStruct, *ChildSearch ;




/* We store ids with the group or item that represents them in the canvas.
 * May want to consider more efficient way of storing these than malloc... */
typedef struct
{
  FooCanvasItem *item ;					    /* could be group or item. */

  GHashTable *hash_table ;
} ID2CanvasStruct, *ID2Canvas ;



typedef struct
{
  GList *search ;					    /* List of quarks specifying search. */

  GList **results ;					    /* result of search. */

} ItemListSearchStruct, *ItemListSearch ;






/* SOMETHING I NEED TO ADD HERE IS FORWARD/REVERSE COLUMN STUFF..... */



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
static gboolean equalIDHash(gconstpointer a, gconstpointer b) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

static void destroyIDHash(gpointer data) ;


static void doHashSet(GHashTable *hash_table, GList *search, GList **result) ;
static void searchItem(gpointer key, gpointer value, gpointer user_data) ;
static void getItem(gpointer key, gpointer value, gpointer user_data) ;



static void childSearchCB(gpointer data, gpointer user_data) ;


/* Create the table that hashes feature set ids to hash tables of features.
 * NOTE that the glib hash stuff does not store anything except the pointers to the
 * keys and values which is a pain if you are only hashing on ints....as I am.
 * I've therefore decided to try using their "direct hash" stuff to only work on
 * the pointers... */
GHashTable *zmapWindowFToICreate(void)
{
  GHashTable *feature_to_context ;

  feature_to_context = g_hash_table_new_full(NULL,	    /* Uses direct hash of keys. */
					     NULL,	    /* Uses direct comparison of key values. */
					     NULL,	    /* Nothing to destroy for hash key. */
					     destroyIDHash) ;

  return feature_to_context ;
}


/* We need to make special ids for the forward/reverse column groups as these don't exist
 * in the features.... */
GQuark zmapWindowFToIMakeSetID(GQuark set_id, ZMapStrand strand)
{
  GQuark strand_set_id = 0 ;
  char *set_strand ;
  char *strand_str ;
  
  if (strand == ZMAPSTRAND_NONE || strand == ZMAPSTRAND_FORWARD)
    set_strand= "FORWARD" ;
  else
    set_strand= "REVERSE" ;

  strand_str = g_strdup_printf("%s_%s", g_quark_to_string(set_id), set_strand) ;

  strand_set_id = g_quark_from_string(strand_str) ;

  g_free(strand_str) ;

  return strand_set_id ;
}





/* Add a hash for the given alignment. The alignment may already exist if we are merging
 * more data from another server, if so then no action is taken otherwise we would lose
 * all our existing feature hashes !
 * NOTE: always returns TRUE as either the align is already there or we add it. */
gboolean zmapWindowFToIAddAlign(GHashTable *feature_to_context_hash,
				GQuark align_id,
				FooCanvasGroup *align_group)
{
  gboolean result = TRUE ;

  if (!(g_hash_table_lookup(feature_to_context_hash, GUINT_TO_POINTER(align_id))))
    {
      ID2Canvas align ;

      align = g_new0(ID2CanvasStruct, 1) ;
      align->item = FOO_CANVAS_ITEM(align_group) ;
      align->hash_table = g_hash_table_new_full(NULL, NULL, NULL, destroyIDHash) ;

      g_hash_table_insert(feature_to_context_hash, GUINT_TO_POINTER(align_id), align) ;
    }

  return result ;
}


/* Add a hash for the given block within an alignment. The block may already exist if we are merging
 * more data from another server, if so then no action is taken otherwise we would lose
 * all our existing feature hashes !
 * Returns FALSE if the align_id cannot be found, otherwise it returns TRUE as the block_id hash
 * already exists or we add it. */
gboolean zmapWindowFToIAddBlock(GHashTable *feature_to_context_hash,
				GQuark align_id, GQuark block_id,
				FooCanvasGroup *block_group)
{
  gboolean result = FALSE ;
  ID2Canvas align ;

  if ((align = (ID2Canvas)g_hash_table_lookup(feature_to_context_hash,
					      GUINT_TO_POINTER(align_id))))
    {
      if (!(g_hash_table_lookup(align->hash_table, GUINT_TO_POINTER(block_id))))
	{
	  ID2Canvas block ;

	  block = g_new0(ID2CanvasStruct, 1) ;
	  block->item = FOO_CANVAS_ITEM(block_group) ;
	  block->hash_table = g_hash_table_new_full(NULL, NULL, NULL, destroyIDHash) ;

	  g_hash_table_insert(align->hash_table, GUINT_TO_POINTER(block_id), block) ;
	}

      result = TRUE ;
    }

  return result ;
}



/* Add a hash for the given feature set within a block within an alignment.
 * The set may already exist if we are merging more data from another server,
 * if so then no action is taken otherwise we would lose all our existing feature hashes !
 * Returns FALSE if the align_id or block_id cannot be found, otherwise it returns TRUE
 * as the block_id hash already exists or we add it. */
gboolean zmapWindowFToIAddSet(GHashTable *feature_to_context_hash,
			      GQuark align_id, GQuark block_id, GQuark set_id,
			      FooCanvasGroup *set_group)
{
  gboolean result = FALSE ;
  ID2Canvas align ;
  ID2Canvas block ;

  if ((align = (ID2Canvas)g_hash_table_lookup(feature_to_context_hash,
					      GUINT_TO_POINTER(align_id)))
      && (block = (ID2Canvas)g_hash_table_lookup(align->hash_table,
						 GUINT_TO_POINTER(block_id))))
    {
      if (!(g_hash_table_lookup(block->hash_table, GUINT_TO_POINTER(set_id))))
	{
	  ID2Canvas set ;

	  set = g_new0(ID2CanvasStruct, 1) ;
	  set->item = FOO_CANVAS_ITEM(set_group) ;
	  set->hash_table = g_hash_table_new_full(NULL, NULL, NULL, destroyIDHash) ;

	  g_hash_table_insert(block->hash_table, GUINT_TO_POINTER(set_id), set) ;
	}

      result = TRUE ;
    }

  return result ;
}


/* Note that this routine is different in that we just insert the canvas item itself, there
 * is no need for a hash table at this level.
 * Returns FALSE if the align_id, block_id or set_id cannot be found, otherwise it returns TRUE
 * as the feature_id hash already exists or we add it. */
gboolean zmapWindowFToIAddFeature(GHashTable *feature_to_context_hash,
				  GQuark align_id, GQuark block_id, GQuark set_id,
				  GQuark feature_id, FooCanvasItem *feature_item)
{
  gboolean result = FALSE ;
  ID2Canvas align ;
  ID2Canvas block ;
  ID2Canvas set ;

  if ((align = (ID2Canvas)g_hash_table_lookup(feature_to_context_hash,
					      GUINT_TO_POINTER(align_id)))
      && (block = (ID2Canvas)g_hash_table_lookup(align->hash_table,
						 GUINT_TO_POINTER(block_id)))
      && (set = (ID2Canvas)g_hash_table_lookup(block->hash_table,
					       GUINT_TO_POINTER(set_id))))
    {
      if (!(g_hash_table_lookup(set->hash_table, GUINT_TO_POINTER(feature_id))))
	{
	  ID2Canvas feature ;

	  feature = g_new0(ID2CanvasStruct, 1) ;
	  feature->item = feature_item ;
	  feature->hash_table = g_hash_table_new_full(NULL, NULL, NULL, destroyIDHash) ;

	  g_hash_table_insert(set->hash_table, GUINT_TO_POINTER(feature_id), feature) ;
	}

      result = TRUE ;
    }

  return result ;
}


/* RENAME TO FINDFEATUREITEM.... */
FooCanvasItem *zmapWindowFToIFindFeatureItem(GHashTable *feature_to_context_hash,
                                             ZMapFeature feature)
{
  FooCanvasItem *item = NULL ;
  GQuark column_id ;

  if (feature->strand == ZMAPSTRAND_FORWARD || feature->strand == ZMAPSTRAND_NONE)
    column_id = zmapWindowFToIMakeSetID(feature->parent_set->unique_id, ZMAPSTRAND_FORWARD) ;
  else
    column_id = zmapWindowFToIMakeSetID(feature->parent_set->unique_id, ZMAPSTRAND_REVERSE) ;


  item = zmapWindowFToIFindItemFull(feature_to_context_hash,
				    feature->parent_set->parent_block->parent_alignment->unique_id,
				    feature->parent_set->parent_block->unique_id,
				    column_id,
				    feature->unique_id) ;

  return item ;
}


FooCanvasItem *zmapWindowFToIFindSetItem(GHashTable *feature_to_context_hash,
					 ZMapFeatureSet feature_set, ZMapStrand strand)
{
  FooCanvasItem *item = NULL ;
  GQuark column_id ;

  if (strand == ZMAPSTRAND_FORWARD || strand == ZMAPSTRAND_NONE)
    column_id = zmapWindowFToIMakeSetID(feature_set->unique_id, ZMAPSTRAND_FORWARD) ;
  else
    column_id = zmapWindowFToIMakeSetID(feature_set->unique_id, ZMAPSTRAND_REVERSE) ;


  item = zmapWindowFToIFindItemFull(feature_to_context_hash,
				    feature_set->parent_block->parent_alignment->unique_id,
				    feature_set->parent_block->unique_id,
				    column_id,
				    0) ;

  return item ;
}




/* Returns the Foo canvas item/group corresponding to the supplied ids or NULL
 * if the id(s) could not be found.
 * 
 * Which hash tables are searched is decided by the ids supplied:
 * 
 *   align     block      set     feature
 * 
 *     id        0         < not read >         returns the alignment item
 * 
 *     id        id        0     <not read>     returns the block item within the alignment
 * 
 * etc.
 * 
 * Warning, may return null so result MUST BE TESTED by caller.
 *  */
FooCanvasItem *zmapWindowFToIFindItemFull(GHashTable *feature_to_context_hash,
					  GQuark align_id, GQuark block_id, GQuark set_id,
					  GQuark feature_id)
{
  FooCanvasItem *item = NULL ;
  ID2Canvas align ;
  ID2Canvas block ;
  ID2Canvas set ;
  ID2Canvas feature ;

  /* Required for minimum query. */
  zMapAssert(feature_to_context_hash && align_id) ;

  /* Cascade down through the hashes until we reach the point the caller wants to stop at. */
  if ((align = (ID2Canvas)g_hash_table_lookup(feature_to_context_hash,
					      GUINT_TO_POINTER(align_id))))
    {
      if (!block_id)
	item = FOO_CANVAS_ITEM(align->item) ;
      else if ((block = (ID2Canvas)g_hash_table_lookup(align->hash_table,
						       GUINT_TO_POINTER(block_id))))
	{
	  if (!set_id)
	    item = FOO_CANVAS_ITEM(block->item) ;
	  else if ((set = (ID2Canvas)g_hash_table_lookup(block->hash_table,
							 GUINT_TO_POINTER(set_id))))
	    {
	      if (!feature_id)
		item = FOO_CANVAS_ITEM(set->item) ;
	      else
		{
		  feature = (ID2Canvas)g_hash_table_lookup(set->hash_table,
							   GUINT_TO_POINTER(feature_id)) ;
		  item = feature->item ;
		}
	    }
	}
    }

  return item ;
}



/* Find the child item that matches the supplied start/end, use for finding feature items
 * that are part of a compound feature, e.g. exons/introns in a transcript.
 * Warning, may return null so result MUST BE TESTED by caller. */
FooCanvasItem *zmapWindowFToIFindItemChild(GHashTable *feature_to_context_hash,
					   ZMapFeature feature,
					   int child_start, int child_end)
{
  FooCanvasItem *item = NULL ;

  /* If the returned item is not a compound item then return NULL, otherwise look for the correct
   * child using the start/end. */
  if ((item = zmapWindowFToIFindFeatureItem(feature_to_context_hash, feature))
      && FOO_IS_CANVAS_GROUP(item))
    {
      FooCanvasGroup *group = FOO_CANVAS_GROUP(item) ;
      ChildSearchStruct child_search = {0} ;

      child_search.child_start = child_start ;
      child_search.child_end = child_end ;

      g_list_foreach(group->item_list, childSearchCB, (void *)&child_search) ;

      if (child_search.child_item)
	item = child_search.child_item ;
    }

  return item ;
}




/* Returns the Foo canvas item/groups corresponding to the supplied ids or NULL
 * if the id(s) could not be found.
 * 
 * Which hash tables are searched is decided by the ids supplied, "*" acts as the
 * the wild card and 0 acts as search limiter.
 * 
 *   align     block      set     feature
 * 
 *     "*"       <    not  read       >       returns all the alignment items
 * 
 *     id       "*"        < not read >       returns all block items within the alignment
 * 
 *     "*"      id        "*"       0         returns all sets in the block "id" in all alignments.
 * 
 * 
 * 
 * etc.
 * 
 * Warning, may return null so result MUST BE TESTED by caller.
 *  */

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
GList *zmapWindowFToIFindItemSetFull(GHashTable *feature_to_context_hash,
				     GQuark align_id, GQuark block_id, GQuark set_id,
				     GQuark feature_id)
{
  GList *item_list = NULL ;
  GQuark wild_card ;
  ID2Canvas align ;
  ID2Canvas block ;
  ID2Canvas set ;


  /* Required for minimum query. */
  zMapAssert(feature_to_context_hash && align_id) ;


  wild_card = g_quark_from_string("*") ;


  /* Cascade down through the hashes until we reach the point the caller wants to stop at. */
  if (align_id == wild_card)
    {
      /* return list of alignments */
      g_hash_table_foreach(feature_to_context_hash, getItem,
			   &item_list) ;
    }
  else if ((align = (ID2Canvas)g_hash_table_lookup(feature_to_context_hash,
						   GUINT_TO_POINTER(align_id))))
    {
      if (block_id == wild_card)
	{
	  /* return list of blocks */
	  g_hash_table_foreach(align->hash_table, getItem,
			       &item_list) ;
	}
      else if ((block = (ID2Canvas)g_hash_table_lookup(align->hash_table,
						       GUINT_TO_POINTER(block_id))))
	{
	  if (set_id == wild_card)
	    {

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	      item = FOO_CANVAS_ITEM(block->item) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
	    }
	  else if ((set = (ID2Canvas)g_hash_table_lookup(block->hash_table,
							 GUINT_TO_POINTER(set_id))))
	    {
	      FooCanvasItem *item = NULL ;

	      if (feature_id == wild_card)
		{

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
		  item = FOO_CANVAS_ITEM(set->item) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

		}
	      else if ((item = (FooCanvasItem *)g_hash_table_lookup(set->hash_table,
								    GUINT_TO_POINTER(feature_id))))
		{
		  /* Return the single item as list. */
		  item_list = g_list_append(item_list, item) ;
		}
	    }
	}
    }

  return item_list ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



GList *zmapWindowFToIFindItemSetFull(GHashTable *feature_to_context_hash,
				     GQuark align_id, GQuark block_id, GQuark set_id,
				     GQuark feature_id)
{
  GList *item_list = NULL ;
  GQuark wild_card ;
  GList *search = NULL, *result = NULL ;


  /* Required for minimum query. */
  zMapAssert(feature_to_context_hash && align_id) ;


  /* build the search list. */
  search = g_list_append(search, GUINT_TO_POINTER(align_id)) ;
  search = g_list_append(search, GUINT_TO_POINTER(block_id)) ;
  search = g_list_append(search, GUINT_TO_POINTER(set_id)) ;
  search = g_list_append(search, GUINT_TO_POINTER(feature_id)) ;
  search = g_list_append(search, GUINT_TO_POINTER(0)) ; /* Terminal stop. */


  /* Now do the search */
  doHashSet(feature_to_context_hash, search, &result) ;



  return result ;
}


static void doHashSet(GHashTable *hash_table, GList *search, GList **results_out)
{
  GQuark curr_search, next_search ;
  GQuark wild_card ;
  GQuark stop = 0 ;
  GList *results = *results_out ;

  wild_card = g_quark_from_string("*") ;

  curr_search = GPOINTER_TO_UINT(search->data) ; 
  next_search = GPOINTER_TO_UINT(search->next->data) ;


  if (curr_search == stop)
    {
      /* I'm not sure if we should get here.... */
      printf("stopped\n") ;
    }
  else
    {
      if (next_search == stop)
	{
	  if (curr_search == wild_card)
	    {
	      g_hash_table_foreach(hash_table, getItem, (gpointer)&results) ;
	    }
	  else 
	    {
	      ID2Canvas item_id ;
	      
	      if ((item_id = (ID2Canvas)g_hash_table_lookup(hash_table,
							    GUINT_TO_POINTER(curr_search))))
		{
		  results = g_list_append(results, item_id->item) ;
		}
	    }	      
	}
      else
	{
	  ItemListSearchStruct search_data ;

	  /* bump on to next search pattern. */
	  search = g_list_next(search) ;

	  search_data.search = search ;
	  search_data.results = &results ;

	  if (curr_search == wild_card)
	    {
	      g_hash_table_foreach(hash_table, searchItem, (gpointer)&search_data) ;

	      /* need to return results list here.... */

	    }
	  else 
	    {
	      ID2Canvas item_id ;
	      
	      if ((item_id = (ID2Canvas)g_hash_table_lookup(hash_table,
							    GUINT_TO_POINTER(curr_search))))
		{

		  g_hash_table_foreach(item_id->hash_table, searchItem, (gpointer)&search_data) ;
		}
	      
	    }
	}

    }

  *results_out = results ;

  return ;
}





/* A GHFunc() callback to add the foo canvas item stored in each hash in the table
 * to a list of such items. */
static void getItem(gpointer key, gpointer value, gpointer user_data)
{
  ID2Canvas hash_item = (ID2Canvas)value ;
  GList **results = (GList **)user_data ;

  *results = g_list_append(*results, hash_item->item) ;

  return ;
}


/* A GHFunc() callback to add the foo canvas item stored in each hash in the table
 * to a list of such items. */
static void searchItem(gpointer key, gpointer value, gpointer user_data)
{
  ID2Canvas hash_item = (ID2Canvas)value ;
  ItemListSearch search = (ItemListSearch)user_data ;

  doHashSet(hash_item->hash_table, search->search, search->results) ;

  return ;
}






















/* Destroy the feature context hash, this will cause all the hash tables/datastructs that it
 * references to be destroyed as well via the hash table destroy function. */
void zmapWindowFToIDestroy(GHashTable *feature_to_context_hash)
{
  g_hash_table_destroy(feature_to_context_hash) ;
  
  return ;
}


FooCanvasItem *zMapWindowFindFeatureItemByQuery(ZMapWindow window, ZMapWindowFeatureQuery ftq)
{
  FooCanvasItem *item  = NULL;
  GQuark feature_id ;
  GQuark column_id ;

  /* Do a (hash, '.', '.', 'style', 'feature') lookup */
  /* Check we won't fail any Asserts!!!!
   * type >= 0 (!= ZMAPFEATURE_INVALID)
   * feature name != NULL
   * start >= 1
   * start <= end
   */
  if(ftq->ft_name && ftq->type != ZMAPFEATURE_INVALID
     && ftq->strand_set  && ftq->start >= 1
     && ftq->start <= ftq->end
     )
    {
      column_id  = zmapWindowFToIMakeSetID(g_quark_from_string(ftq->style), ftq->strand);

      feature_id = zMapFeatureCreateID(ftq->type, ftq->ft_name, ftq->strand, 
                                       ftq->start, ftq->end,
                                       ftq->query_start, ftq->query_end
                                       );
      if(!ftq->alignment)
        ftq->alignment = g_strdup_printf("%s_master", window->sequence);

      if(!ftq->block)
        {
          int start, end;
          start = window->seq_start;
          end   = (window->seq_end == window->seqLength ? 0 : window->seq_end);
          /* This next bit is backward in coming forward... */
          ftq->block = g_strdup(g_quark_to_string(zMapFeatureBlockCreateID(start, end, ZMAPSTRAND_FORWARD,
                                                                           start, end, ZMAPSTRAND_FORWARD)));
        }

      item = zmapWindowFToIFindItemFull(window->context_to_item,
                                        g_quark_from_string(ftq->alignment),
                                        g_quark_from_string(ftq->block),
                                        column_id,
                                        feature_id);
      /* MUST CHECK item != NULL users send rubbish! */
    }

  return item;
}


/* 
 *                      Internal routines.
 */



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE

/* RUBBISH, NOT NEEDED..... */

/* Compare function, called any time a new item is to be inserted into a table or to be
 * found in a table. */
static gboolean equalIDHash(gconstpointer a, gconstpointer b)
{
  gboolean result = FALSE ;
  ID2Canvas id_a = (ID2Canvas)a, id_b = (ID2Canvas)b ;

  if (id_a.id == id_b.id)
    result = TRUE ;

  return result ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */




/* Is called for all entries in the hash table of feature set ids -> hash table of features. For
 * each one we need to destroy the feature hash table and then free the struct itself. */
static void destroyIDHash(gpointer data)
{
  ID2Canvas id = (ID2Canvas)data ;

  g_hash_table_destroy(id->hash_table) ;

  g_free(id) ;

  return ;
}



/* This is a g_datalist callback function. */
static void childSearchCB(gpointer data, gpointer user_data)
{
  FooCanvasItem *item = (FooCanvasItem *)data ;
  ChildSearch child_search = (ChildSearch)user_data ;
  ZMapWindowItemFeatureType item_feature_type ;

  /* We take the first match we find so this function does nothing if we have already
   * found matching child item. */
  if (!(child_search->child_item))
    {
      item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item),
							    "item_feature_type")) ;
      if (item_feature_type == ITEM_FEATURE_CHILD)
	{
	  ZMapWindowItemFeature item_subfeature_data ;

	  item_subfeature_data = (ZMapWindowItemFeature)g_object_get_data(G_OBJECT(item),
									  "item_subfeature_data") ;

	  if (item_subfeature_data->start == child_search->child_start
	      && item_subfeature_data->end == child_search->child_end)
	    child_search->child_item = item ;
	}
    }

  return ;
}

