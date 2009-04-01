/*  File: zmapWindowItemFeatureSet_I.h
 *  Author: Roy Storey (rds@sanger.ac.uk)
 *  Copyright (c) 2009: Genome Research Ltd.
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
 * Description: 
 *
 * Exported functions: See XXXXXXXXXXXXX.h
 * HISTORY:
 * Last edited: Mar 17 10:04 2009 (rds)
 * Created: Fri Feb  6 11:49:03 2009 (rds)
 * CVS info:   $Id: zmapWindowItemFeatureSet_I.h,v 1.3 2009-04-01 15:55:54 rds Exp $
 *-------------------------------------------------------------------
 */

#ifndef __ZMAP_WINDOW_ITEM_FEATURE_SET_I_H__
#define __ZMAP_WINDOW_ITEM_FEATURE_SET_I_H__

#include <zmapWindow_P.h>
#include <zmapWindowItemFeatureSet.h>

#define ZMAP_PARAM_STATIC (G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)
#define ZMAP_PARAM_STATIC_RW (ZMAP_PARAM_STATIC | G_PARAM_READWRITE)
#define ZMAP_PARAM_STATIC_RO (ZMAP_PARAM_STATIC | G_PARAM_READABLE)



typedef struct _zmapWindowItemFeatureSetDataStruct
{
  GObject __parent__;

  ZMapWindow  window;
  ZMapStrand  strand ;
  ZMapFrame   frame ;
  GHashTable *style_table ;

  FooCanvasGroup *column_container;

  GQuark      style_id, unique_id;

  /* We keep the features sorted by position and size so we can cursor through them... */
  gboolean    sorted ;

  /* Features hidden by user, should stay hidden. */
  GQueue     *user_hidden_stack ;

  /* These fields are used for some of the more exotic column bumping. */
  gboolean    hidden_bump_features ; /* Features were hidden because they
				      * are out of the marked range. */ 
  GList      *extra_items ;	/* Match backgrounds etc. */

  GList      *gaps_added_items ; /* List of features where gap data was added. */
  
  struct
  {
    unsigned int frame_mode      : 1;
    unsigned int frame_specific  : 1;
    unsigned int display_state   : 1;
    unsigned int show_when_empty : 1;
  }lazy_loaded;

  struct
  {
    gboolean                    has_feature_set;
    gboolean                    has_stats;
    gboolean                    show_when_empty;
    gboolean                    frame_specific;
    ZMapStyle3FrameMode         frame_mode;
    ZMapStyleColumnDisplayState display_state;
    ZMapStyleOverlapMode        overlap_mode;
    ZMapStyleOverlapMode        default_overlap_mode;
  }settings;

} zmapWindowItemFeatureSetDataStruct;


typedef struct _zmapWindowItemFeatureSetDataClassStruct
{
  GObjectClass __parent__;

} zmapWindowItemFeatureSetDataClassStruct;



#endif /* __ZMAP_WINDOW_ITEM_FEATURE_SET_I_H__ */
