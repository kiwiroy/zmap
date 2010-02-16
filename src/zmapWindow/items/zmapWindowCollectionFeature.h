/*  File: zmapWindowCollectionFeature.h
 *  Author: Roy Storey (rds@sanger.ac.uk)
 *  Copyright (c) 2008: Genome Research Ltd.
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
 * Last edited: Feb 16 09:54 2010 (edgrif)
 * Created: Wed Dec  3 08:44:06 2008 (rds)
 * CVS info:   $Id: zmapWindowCollectionFeature.h,v 1.4 2010-02-16 10:24:04 edgrif Exp $
 *-------------------------------------------------------------------
 */
#ifndef ZMAP_WINDOW_COLLECTION_FEATURE_H
#define ZMAP_WINDOW_COLLECTION_FEATURE_H

#include <glib-object.h>
#include <zmapWindow_P.h>
#include <zmapWindowCanvasItem.h>

#define ZMAP_WINDOW_COLLECTION_FEATURE_NAME "ZMapWindowCollectionFeature"


#define ZMAP_TYPE_WINDOW_COLLECTION_FEATURE           (zMapWindowCollectionFeatureGetType())
#define ZMAP_WINDOW_COLLECTION_FEATURE(obj)	      (G_TYPE_CHECK_INSTANCE_CAST((obj), ZMAP_TYPE_WINDOW_COLLECTION_FEATURE, zmapWindowCollectionFeature))
#define ZMAP_WINDOW_COLLECTION_FEATURE_CONST(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), ZMAP_TYPE_WINDOW_COLLECTION_FEATURE, zmapWindowCollectionFeature const))
#define ZMAP_WINDOW_COLLECTION_FEATURE_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),  ZMAP_TYPE_WINDOW_COLLECTION_FEATURE, zmapWindowCollectionFeatureClass))
#define ZMAP_IS_WINDOW_COLLECTION_FEATURE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZMAP_TYPE_WINDOW_COLLECTION_FEATURE))
#define ZMAP_WINDOW_COLLECTION_FEATURE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj),  ZMAP_TYPE_WINDOW_COLLECTION_FEATURE, zmapWindowCollectionFeatureClass))

typedef enum
  {
    ZMAP_WINDOW_COLLECTION_0 = 0, 	/* invalid */
    ZMAP_WINDOW_COLLECTION_BOX,
    ZMAP_WINDOW_COLLECTION_GLYPH
  } ZMapWindowCollectionFeatureType ;

/* Instance */
typedef struct _zmapWindowCollectionFeatureStruct  zmapWindowCollectionFeature, *ZMapWindowCollectionFeature ;


/* Class */
typedef struct _zmapWindowCollectionFeatureClassStruct  zmapWindowCollectionFeatureClass, *ZMapWindowCollectionFeatureClass ;


typedef ColinearityType (*ZMapFeatureCompareFunc)(ZMapFeature feature_a, ZMapFeature feature_b, gpointer user_data);


/* Collection Features hold a set of ZMapWindowCanvasItems and are
 * themselves ZMapWindowCanvasItems */


/* Public funcs */
GType zMapWindowCollectionFeatureGetType(void);

ZMapWindowCanvasItem zMapWindowCollectionFeatureCreate(FooCanvasGroup *parent);
void zMapWindowCollectionFeatureRemoveSubFeatures(ZMapWindowCanvasItem collection, 
						  gboolean keep_in_place_x, 
						  gboolean keep_in_place_y);
void zMapWindowCollectionFeatureStaticReparent(ZMapWindowCanvasItem reparentee, 
					       ZMapWindowCanvasItem new_parent);
void zMapWindowCollectionFeatureAddColinearMarkers(ZMapWindowCanvasItem   collection,
						   ZMapFeatureCompareFunc compare_func,
						   gpointer               compare_data);
void zMapWindowCollectionFeatureAddIncompleteMarkers(ZMapWindowCanvasItem collection,
						     gboolean revcomped_features);
void zMapWindowCollectionFeatureAddSpliceMarkers(ZMapWindowCanvasItem collection);


#endif /* ZMAP_WINDOW_COLLECTION_FEATURE_H */
