/*  File: zmapString.h
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2006-2015: Genome Research Ltd.
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
 * Description: General string functions for ZMap.
 *
 *-------------------------------------------------------------------
 */
#ifndef ZMAP_STRING_H
#define ZMAP_STRING_H

#include <glib.h>

// Defaults for abbreviating strings.
#define STR_ABBREV_CHARS   "[...]"
#define STR_ABBREV_MAX_LEN 20




const char *zMapStringAbbrevStr(const char *orig_str,
                                const char *optional_abbrev_chars = STR_ABBREV_CHARS,
                                const unsigned int optional_max_orig_len = STR_ABBREV_MAX_LEN) ;
int zMapStringFindMatch(char *target, char *query) ;
int zMapStringFindMatchCase(char *target, char *query, gboolean caseSensitive) ;
gboolean zMapStringBlank(char *string_arg) ;
char *zMapStringCompress(char *txt) ;
char *zMapStringFlatten(char *string_with_spaces) ;

#endif /* ZMAP_STRING_H */