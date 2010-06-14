/*  File: zmapWindowAssemblyFeature.h
 *  Author: Roy Storey (rds@sanger.ac.uk)
 *  Copyright (c) 2006-2010: Genome Research Ltd.
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
 *     Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *
 * Description:
 *
 * Exported functions: See XXXXXXXXXXXXX.h
 * HISTORY:
 * Last edited: Feb 16 10:16 2010 (edgrif)
 * Created: Wed Dec  3 08:44:06 2008 (rds)
 * CVS info:   $Id: zmapWindowAssemblyFeature.h,v 1.6 2010-06-14 15:40:17 mh17 Exp $
 *-------------------------------------------------------------------
 */
#ifndef ZMAP_WINDOW_ASSEMBLY_FEATURE_H
#define ZMAP_WINDOW_ASSEMBLY_FEATURE_H

#include <glib-object.h>


#define ZMAP_WINDOW_ASSEMBLY_FEATURE_NAME "ZMapWindowAssemblyFeature"

#define ZMAP_TYPE_WINDOW_ASSEMBLY_FEATURE           (zMapWindowAssemblyFeatureGetType())

#if GOBJ_CAST
#define ZMAP_WINDOW_ASSEMBLY_FEATURE(obj)     ((ZMapWindowAssemblyFeature) obj)
#define ZMAP_WINDOW_ASSEMBLY_FEATURE_CONST(obj)     ((ZMapWindowAssemblyFeature const) obj)
#else
#define ZMAP_WINDOW_ASSEMBLY_FEATURE(obj)	    (G_TYPE_CHECK_INSTANCE_CAST((obj), ZMAP_TYPE_WINDOW_ASSEMBLY_FEATURE, zmapWindowAssemblyFeature))
#define ZMAP_WINDOW_ASSEMBLY_FEATURE_CONST(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), ZMAP_TYPE_WINDOW_ASSEMBLY_FEATURE, zmapWindowAssemblyFeature const))
#endif

#define ZMAP_WINDOW_ASSEMBLY_FEATURE_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),  ZMAP_TYPE_WINDOW_ASSEMBLY_FEATURE, zmapWindowAssemblyFeatureClass))
#define ZMAP_IS_WINDOW_ASSEMBLY_FEATURE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZMAP_TYPE_WINDOW_ASSEMBLY_FEATURE))
#define ZMAP_WINDOW_ASSEMBLY_FEATURE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj),  ZMAP_TYPE_WINDOW_ASSEMBLY_FEATURE, zmapWindowAssemblyFeatureClass))


/* Instance */
typedef struct _zmapWindowAssemblyFeatureStruct  zmapWindowAssemblyFeature, *ZMapWindowAssemblyFeature ;


/* Class */
typedef struct _zmapWindowAssemblyFeatureClassStruct  zmapWindowAssemblyFeatureClass, *ZMapWindowAssemblyFeatureClass ;


/* Public funcs */
GType zMapWindowAssemblyFeatureGetType(void);

#endif /* ZMAP_WINDOW_ASSEMBLY_FEATURE_H */
