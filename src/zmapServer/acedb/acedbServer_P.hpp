/*  File: acedbServer_P.h
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2006-2017: Genome Research Ltd.
 *-------------------------------------------------------------------
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------
 * This file is part of the ZMap genome database package
 * originally written by:
 * 
 *      Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk
 *        Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk
 *   Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *       Gemma Guest (Sanger Institute, UK) gb10@sanger.ac.uk
 *      Steve Miller (Sanger Institute, UK) sm23@sanger.ac.uk
 *  
 * Description:
 *-------------------------------------------------------------------
 */
#ifndef ACEDB_SERVER_P_H
#define ACEDB_SERVER_P_H


/* This code and the acedb server must stay in step as the two are co-developed so
 * we set a minimum acedb version that the code requires to work properly. */
#define ACEDB_SERVER_MIN_VERSION "4.9.48"

#define ACEDB_SERVER_DEFAULT_TIMEOUT 0


#define ACEDB_PROTOCOL_STR "Acedb"			    /* For error messages. */


/* For an acedb server we can use ?Style or ?Method class objects, default is now styles. */
#define ACEDB_USE_METHODS       "use_methods"


/* Some tag labels... */
#define COL_PARENT "Column_parent"
#define COL_CHILD  "Column_child"
#define STYLE      "Style"


/* Acedb handling of widths is quite complex: some methods do not have a width and there is
 * more than one default width (!). To complicate matters further, acedb screen units are
 * larger than zmap (foocanvas) units. We copy the default width most commonly used by
 * acedb and apply a magnification factor to make columns look similar in width. */
#define ACEDB_DEFAULT_WIDTH 2.0
#define ACEDB_MAG_FACTOR 8.0



/* Holds all the state we need to manage the acedb connection. */
typedef struct _AcedbServerStruct
{
  ZMapConfigSource source ;
  char *config_file ;

  /* Connection details. */
  AceConnection connection ;
  char *host ;
  int port ;

  /* Needed so we can return err msgs for aceconn errors. */
  AceConnStatus last_err_status ;
  char *last_err_msg ;

  char *version_str ;					    /* For checking server is at right level. */

  gboolean fetch_gene_finder_features ;			    /* Need to send additional requests to
							       server to get these. */
  ZMapFeatureContext req_context ;

  GList *all_methods ;					    /* List of all methods to be used in
							       seqget/seqfeatures calls. */

  gboolean has_new_tags ;				    /* TRUE => use new column tags/zmap style objects. */

  gboolean stylename_from_methodname ;			    /* TRUE => match each method with a
							       style of the same name without
							       looking for a style tag in the method. */

  GHashTable *method_2_data ;				    /* Records data for each method
							       (NULL if acedb_styles == FALSE). */

  GHashTable *method_2_feature_set ;			    /* Records the feature set for each method
							       (NULL if acedb_styles == FALSE). */

  ZMapFeatureContext current_context ;

  gint zmap_start, zmap_end ;				    /* request coordinates for our one block */


  gint gff_version ;                                        /* Which version of gff to request from acedb. */


} AcedbServerStruct, *AcedbServer ;




#endif /* !ACEDB_SERVER_P_H */
