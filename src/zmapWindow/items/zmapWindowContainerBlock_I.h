/*  File: zmapWindowContainerBlock_I.h
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
 * Last edited: May 20 11:58 2009 (rds)
 * Created: Fri Feb  6 11:49:03 2009 (rds)
 * CVS info:   $Id: zmapWindowContainerBlock_I.h,v 1.1 2009-06-02 11:20:23 rds Exp $
 *-------------------------------------------------------------------
 */

#ifndef __ZMAP_WINDOW_CONTAINER_BLOCK_I_H__
#define __ZMAP_WINDOW_CONTAINER_BLOCK_I_H__

#include <zmapWindowContainerBlock.h>
#include <zmapWindowContainerUtils_P.h>

#define ZMAP_BIN_MAX_VALUE(BITMAP) ((1 << BITMAP->bin_depth) - 1)

typedef struct _zmapWindowContainerBlockStruct
{
  zmapWindowContainerGroup __parent__;

  ZMapWindow  window;

  GList      *compressed_cols, *bumped_cols ;
  
  GHashTable *loaded_region_hash;

} zmapWindowContainerBlockStruct;


typedef struct _zmapWindowContainerBlockClassStruct
{
  zmapWindowContainerGroupClass __parent__;

} zmapWindowContainerBlockClassStruct;



#endif /* __ZMAP_WINDOW_CONTAINER_BLOCK_I_H__ */
