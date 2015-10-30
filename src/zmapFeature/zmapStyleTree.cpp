/*  File: zmapStyleTree.c
 *  Author: Gemma Guest (gb10@sanger.ac.uk)
 *  Copyright (c) 2015: Genome Research Ltd.
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
 * Description: Implements the zmapStyle GObject
 *
 * Exported functions: See ZMap/zmapStyle.h
 *
 *-------------------------------------------------------------------
 */

//#include <ZMap/zmap.hpp>
//
//#include <string.h>
//
//#include <ZMap/zmapUtils.hpp>
//#include <ZMap/zmapGLibUtils.hpp>
//#include <ZMap/zmapConfigIni.hpp>   // for zMapConfigLegacyStyles() only can remove when system has moved on
#include <zmapStyle_P.hpp>
#include <ZMap/zmapStyleTree.hpp>
#include <ZMap/zmapUtilsMesg.hpp>



/* Utility struct used to merge a styles hash table into a tree */
typedef struct MergeDataStruct_
{
  ZMapStyleTree *styles_tree ;
  GHashTable *styles_hash ;
} MergeDataStruct, *MergeData ;



ZMapStyleTree::~ZMapStyleTree()
{
  for (std::vector<ZMapStyleTree*>::iterator iter = m_children.begin(); iter != m_children.end(); ++iter)
    {
      delete *iter ;
    }
}


/* Return true if this tree node represents the given style */
gboolean ZMapStyleTree::is_style(ZMapFeatureTypeStyle style)
{
  gboolean result = FALSE ;

  if (m_style == style)
    result = TRUE ;

  return result ;
}


/* Return true if this tree node represents the given style id */
gboolean ZMapStyleTree::is_style(const GQuark style_id)
{
  gboolean result = FALSE ;

  if (m_style && m_style->unique_id == style_id)
    result = TRUE ;

  return result ;
}


/* Find the tree node for the given style. Return the tree node or null if not found. */
ZMapStyleTree* ZMapStyleTree::find(ZMapFeatureTypeStyle style)
{
  ZMapStyleTree *result = NULL ;

  if (is_style(style))
    {
      result = this ;
    }
  else
    {
      for (std::vector<ZMapStyleTree*>::iterator iter = m_children.begin(); !result && iter != m_children.end(); ++iter)
        {
          ZMapStyleTree *child = *iter ;
          result = child->find(style) ;
        }
    }

  return result ;
}


/* Find the tree node for the given style id. Return the tree node or null if not found. */
ZMapStyleTree* ZMapStyleTree::find(const GQuark style_id)
{
  ZMapStyleTree *result = NULL ;

  if (is_style(style_id))
    {
      result = this ;
    }
  else
    {
      for (std::vector<ZMapStyleTree*>::iterator iter = m_children.begin(); !result && iter != m_children.end(); ++iter)
        {
          ZMapStyleTree *child = *iter ;
          result = child->find(style_id) ;
        }
    }

  return result ;
}


/* Find the style struct for the given style id. Return the struct pointer or null if not found. */
ZMapFeatureTypeStyle ZMapStyleTree::find_style(const GQuark style_id)
{
  ZMapFeatureTypeStyle result = NULL ;
  ZMapStyleTree *node = find(style_id) ;

  if (node)
    result = node->get_style() ;

  return result ;
}


/* Find the parent node of the given style. Return the tree node or null if not found. */
ZMapStyleTree* ZMapStyleTree::find_parent(ZMapFeatureTypeStyle style)
{
  ZMapStyleTree *result = NULL ;

  /* Check if any of our child nodes are the given style */
  for (std::vector<ZMapStyleTree*>::iterator iter = m_children.begin(); !result && iter != m_children.end(); ++iter)
    {
      ZMapStyleTree *child = *iter ;
      
      if (child->is_style(style))
        result = this ;
    }

  /* if not found, recurse */
  for (std::vector<ZMapStyleTree*>::iterator iter = m_children.begin(); !result && iter != m_children.end(); ++iter)
    {
      ZMapStyleTree *child = *iter ;
      result = child->find_parent(style) ;
    }
  

  return result ;
}



/* Add a new child tree node with this style to our list of children */
void ZMapStyleTree::add_child_style(ZMapFeatureTypeStyle style)
{
  ZMapStyleTree *node = new ZMapStyleTree(style) ;

  m_children.push_back(node) ;
}


ZMapFeatureTypeStyle ZMapStyleTree::get_style() const
{
  return m_style ;
}


std::vector<ZMapStyleTree*> ZMapStyleTree::get_children() const
{
  return m_children ;
}


/* Add the given style into the style hierarchy tree in the appropriate place according to its
 * parent. Recursively add the style's parent(s) if not already in the tree. Does nothing if the
 * style is already in the tree. */
void ZMapStyleTree::add_style(ZMapFeatureTypeStyle style, GHashTable *styles)
{
  if (!find(style))
    {
      ZMapFeatureTypeStyle parent = (ZMapFeatureTypeStyle)g_hash_table_lookup(styles, GINT_TO_POINTER(style->parent_id)) ;

      if (parent)
        {
          /* If the parent doesn't exist, recursively create it */
          add_style(parent, styles) ;

          ZMapStyleTree *parent_node = find(parent) ;

          /* Add the child to the parent node */
          if (parent_node)
            parent_node->add_child_style(style) ;
          else
            zMapWarning("%s", "Error adding style to tree") ;
        }
      else
        {
          /* This style has no parent, so add it to the root style in the tree */
          add_child_style(style) ;
        }
      
    }
}


/* Remove the given style from the style hierarchy tree */
void ZMapStyleTree::remove_style(ZMapFeatureTypeStyle style)
{
  ZMapStyleTree *parent = find_parent(style) ;
  ZMapStyleTree *node = find(style) ;

  if (parent && node)
    {
      std::vector<ZMapStyleTree*>::iterator iter = std::find(m_children.begin(), m_children.end(), node) ;

      if (iter != m_children.end())
        m_children.erase(iter) ;
    }
}


/* Predicate for sorting styles alphabetically */
inline bool styleLessThan(const ZMapStyleTree *node1, const ZMapStyleTree *node2)
{
  bool result = FALSE ;

  ZMapFeatureTypeStyle style1 = node1->get_style() ;
  ZMapFeatureTypeStyle style2 = node2->get_style() ;

  if (style1 && style1->original_id && style2 && style2->original_id)
    {
      result = (strcmp(g_quark_to_string(style1->original_id), 
                       g_quark_to_string(style2->original_id)) < 0);
    }
  
  return result ;
}


/* Sort all list of nodes alphabetically by style name */
void ZMapStyleTree::sort()
{
  /* Sort our own list of children */
  std::sort(m_children.begin(), m_children.end(), styleLessThan);
  
  /* Recurse */
  for (std::vector<ZMapStyleTree*>::iterator iter = m_children.begin(); iter != m_children.end(); ++iter)
    {
      ZMapStyleTree *child = *iter ;
      child->sort() ;
    }
}


/* Called for each style in a hash table. Add the style to the appropriate node 
 * in the tree */
static void mergeStyleCB(gpointer key, gpointer value, gpointer user_data)
{
  ZMapFeatureTypeStyle style = (ZMapFeatureTypeStyle)value ;
  MergeData merge_data = (MergeData)user_data ;

  merge_data->styles_tree->add_style(style, merge_data->styles_hash) ;
}


void ZMapStyleTree::merge(GHashTable *styles_hash)
{
  if (styles_hash)
    {
      MergeDataStruct merge_data = {this, styles_hash} ;
      
      g_hash_table_foreach(styles_hash, mergeStyleCB, &merge_data) ;
    }
}
