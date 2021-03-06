/*  File: zmapFeatureLoadDisplay.h
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
 * Description: Interface for loading, manipulating, displaying
 *              sets of features.
 *
 *-------------------------------------------------------------------
 */
#ifndef ZMAP_FEATURE_LOAD_DISPLAY_H
#define ZMAP_FEATURE_LOAD_DISPLAY_H

#include <map>
#include <list>
#include <string>

#include <gbtools/gbtools.hpp>

#include <ZMap/zmapStyle.hpp>
#include <ZMap/zmapStyleTree.hpp>
#include <ZMap/zmapConfigIni.hpp>
#include <ZMap/zmapConfigStanzaStructs.hpp>
#include <ZMap/zmapConfigStrings.hpp>


class ZMapConfigSourceStruct ;
struct ZMapFeatureAnyStructType ;



/* Overview:
 * 
 * A "Featureset" is a set of one type of features (e.g. EST alignments from mouse),
 * this set is derived from a single data source. The meta data for the featureset
 * and the source is held in a  ZMapFeatureSourceStruct.
 * 
 * Featuresets are displayed in columns and there can be more than one featureset
 * in any one column. Data about which column a feature set is held in is held in
 * a ZMapFeatureSetDescStruct (bad name for it).
 * 
 * Columns hold one-to-many featuresets and information about columns is held in
 * a ZMapFeatureColumnStruct.
 * 
 * All these structs need to associated with each other and this information is
 * held in a ZMapFeatureContextMapStruct (again, not a good name).
 * 
 */


typedef enum
  {
    ZMAPFLAG_REVCOMPED_FEATURES,         /* True if the user has done a revcomp */
    ZMAPFLAG_HIGHLIGHT_FILTERED_COLUMNS, /* True if filtered columns should be highlighted */

    ZMAPFLAG_SAVE_FEATURES,              /* True if there are new features that have not been saved */
    ZMAPFLAG_SAVE_SCRATCH,               /* True if changes have been made in the scratch column
                                          * that have not been "saved" to a real featureset */
    ZMAPFLAG_SAVE_COLUMN_GROUPS,         /* True if there are unsaved changes to column groups */
    ZMAPFLAG_SAVE_STYLES,                /* True if there are unsaved changes to styles */
    ZMAPFLAG_SAVE_COLUMNS,               /* True if there are unsaved changes to the columns order */
    ZMAPFLAG_SAVE_SOURCES,               /* True if there are unsaved changes to the sources */
    ZMAPFLAG_SAVE_FEATURESET_STYLE,      /* True if featureset-style relationships have changed */

    ZMAPFLAG_ENABLE_ANNOTATION,          /* True if we should enable editing via the annotation column */
    ZMAPFLAG_ENABLE_ANNOTATION_INIT,     /* False until the enable-annotation flag has been initialised */

    ZMAPFLAG_NUM_FLAGS                   /* Must be last in list */
  } ZMapFlag;


/* Struct holding information about sets of features. Can be used to look up the
 * style for a feature plus other stuff. */
typedef struct ZMapFeatureSourceStructType
{
  GQuark source_id ;    /* The source name. From ACE this is the key used to ref this struct */
                        /* but we can config an alternate name (requested by graham) */

  GQuark source_text ;  /* Description. */

  GQuark style_id ;     /* The style for processing the source. */

  GQuark related_column;	/* eg real data from coverage */

  GQuark maps_to;			/* composite featureset many->one
  					 * composite does not exist but all are displayed as one
  					 * requires ZMapWindowGraphDensityItem
  					 * only relevant to coverage data
  					 */

  gboolean is_seq;		/* true for coverage and real seq-data */

} ZMapFeatureSourceStruct, *ZMapFeatureSource ;


/* Struct for "feature set" information. Used to look up "meta" information for each feature set. */
typedef struct ZMapFeatureSetDescStructType
{
  GQuark column_id ;           /* The set name. (the display column) as a key value*/
  GQuark column_ID ;           /* The set name. (the display column) as display text*/

  GQuark feature_src_ID;            // the name of the source featureset (with upper case)
                                    // struct is keyed with normalised name
  const char *feature_set_text;           // renamed so we can search for this

} ZMapFeatureSetDescStruct, *ZMapFeatureSetDesc ;


/* All the info about a display column, note these are "logical" columns,
 * real display columns get munged with strand and frame */
typedef struct ZMapFeatureColumnStructType
{
  /* Column name, description, ordering. */
  GQuark unique_id ;					    /* For searching. */
  GQuark column_id ;					    /* For display. */
  char *column_desc ;
  int order ;

  /* column specific style data may be config'd explicitly or derived from contained featuresets */
  ZMapFeatureTypeStyle style ;
  GQuark style_id;					    /* can be set before we get the style itself */

  /* all the styles needed by the column */
  GList *style_table ;

  /* list of those configured these get filled in when servers request featureset-names
   * for pipe servers we could do this during server config but for ACE
   * (and possibly DAS) we have to wait till they provide data. */
  GList *featuresets_names ;

  /* we need both user style and unique id's both are filled in by lazy evaluation
   * if not configured explicitly (featuresets is set by the [columns] config)
   * NOTE we now have virtual featuresets for BAM coverage that do not exist
   * servers that provide a mapping must delete these lists */
  GList *featuresets_unique_ids ;

} ZMapFeatureColumnStruct, *ZMapFeatureColumn ;





/* All the featureset/featureset data/column/style data - used by view and window */
typedef struct ZMapFeatureContextMapStructType
{
  /* All the styles known to the view or window.
   * Maps the style's unique_id (GQuark) to ZMapFeatureTypeStyle. */
  ZMapStyleTree styles ;

  /* All the columns that ZMap will display.
   * These may contain several featuresets each, They are in display order left to right.
   * Maps the column's unique_id (GQuark) to ZMapFeatureColumn */
  std::map<GQuark, ZMapFeatureColumn> *columns ;


  /* Mapping of a feature source to a column using ZMapFeatureSetDesc
   * NB: this contains data from ZMap config sections [columns] [featureset_description] _and_
   * ACEDB */
  /* gb10: Note that this relationship is cached in reverse in the ZMapFeatureColumn, but the data
   * originates from this hash table. It's something to do with acedb servers returning this
   * relationship after the column has been created so we don't know it up front. Perhaps that's
   * something we could revisit and maybe get rid of this hash table? */
  GHashTable *featureset_2_column ;

  /* Mapping of each column to all the styles it requires. 
   * NB: this stores data from ZMap config sections [featureset_styles] _and_ [column_styles] 
   * _and_ ACEDB collisions are merged Columns treated as fake featuresets so as to have a style.
   * Maps the column's unique_id (GQuark) to GList of style unique_ids (GQuark). */
  /* gb10: It looks like this is duplicate information because each ZMapFeatureColumn has a
   * style_table listing all of the styles that column requires. Also, the column has a list of
   * featuresets and each featureset has a pointer to the style. Perhaps we can get rid of this
   * hash table and use the column's style_table instead? Or get rid of both of those and just
   * access the styles directly from the featuresets. */
  GHashTable *column_2_styles ;

  /* The source data for a featureset. 
   * This consists of style id and description and source id
   * NB: the GFFSource.source  (quark) is the GFF_source name the hash table
   * Maps the featureset unique_id (GQuark) to ZMapFeatureSource. */
  /* gb10: Could we just get the ZMapFeatureSet to point directly to this data and get rid 
   * of this hash table? */
  GHashTable *source_2_sourcedata ;

  
  /* This maps virtual featuresets to their lists of real featuresets. The reverse mapping is
   * provided by the maps_to field in the ZMapFeatureSource. 
   * Maps the virtual featureset unique_id (GQuark) to a GList of real featureset unique_ids
   * (GQuarks) */
  /* gb10: This is currently only used in one place (zmapWindowFetchData) to get the list of
   * real featureset IDs from the virtual featureset ID. Perhaps we can use the info from maps_to
   * to do that instead (if that's not too slow)? */
  GHashTable *virtual_featuresets ;

  
  /* This allows columns to be grouped together on a user-defined/configured basis, e.g. for
   * running blixem on a related set of columns. A column can be in multiple groups. 
   * Maps the group unique_id (GQuark) to a GList of column unique ids (GQuark) */
  GHashTable *column_groups ;


  gboolean isCoverageColumn(GQuark column_id) ;
  gboolean isSeqColumn(GQuark column_id) ;
  gboolean isSeqFeatureSet(GQuark fset_id) ;

  ZMapFeatureColumn getSetColumn(GQuark set_id) ;
  GList *getColumnFeatureSets(GQuark column_id, gboolean unique_id) ;
  std::list<GQuark> getOrderedColumnsListIDs(const bool unique = true) ;
  std::list<ZMapFeatureColumn> getOrderedColumnsList() ;
  GList* getOrderedColumnsGListIDs() ;
  GQuark getRelatedColumnID(const GQuark fset_id) ;

  bool updateContextColumns(_ZMapConfigIniContextStruct *context, ZMapConfigIniFileType file_type) ;
  bool updateContextColumnGroups(_ZMapConfigIniContextStruct *context, ZMapConfigIniFileType file_type) ;

  ZMapFeatureSource getSource(GQuark fset_id) ;
  ZMapFeatureSource createSource(const GQuark fset_id, 
                                 const GQuark source_id, const GQuark source_text, const GQuark style_id,
                                 const GQuark related_column, const GQuark maps_to, const bool is_seq) ;

private:
  void setSource(GQuark fset_id, ZMapFeatureSource src) ;

} ZMapFeatureContextMapStruct, *ZMapFeatureContextMap ;



/* This cached info about GFF parsing that is in progress. We need to cache this info if we need
 * to parse the input GFF file(s) on startup to populate the ZMapFeatureSequenceMap before we're
 * ready to read the features in themselves - then when we do come read the features, this cached
 * info lets us use the same parser to continue reading the file where we left off.
 * (Note that we can't rewind the input stream and start again because we want to support stdin.) */
typedef struct ZMapFeatureParserCacheStructType
{
  gpointer parser ;
  GString *line ;     /* the last line that was parsed */
  GIOChannel *pipe ;
  GIOStatus pipe_status ;
} ZMapFeatureParserCacheStruct, *ZMapFeatureParserCache ;



/* Holds data about a sequence to be fetched.
 * 
 * Used for the 'default-sequence' from the config file or main window, or for a sequence loaded
 * later via a peer program, e.g. otterlace. */
typedef struct ZMapFeatureSequenceMapStructType
{
  char *config_file ;
  char *stylesfile ;    /* path to styles file given on command line or in config dir */

  GHashTable *cached_parsers ; /* filenames (as GQuarks) mapped to cached info about GFF parsing that
                                * is in progress (ZMapFeatureParserCache) if parsing has already been started */

  std::map<std::string, ZMapConfigSourceStruct*> *sources ; /* map source name to struct */

  char *dataset ;                                           /* e.g. human */
  char *sequence ;                                          /* e.g. chr6-18 */
  int start, end ;                                          /* chromosome coordinates */

  gboolean flags[ZMAPFLAG_NUM_FLAGS] ;


  ZMapFeatureSequenceMapStructType() ;

  ZMapFeatureSequenceMapStructType* copy() ;

  gboolean getFlag(ZMapFlag flag) ;
  void setFlag(ZMapFlag flag, const gboolean value) ;

  ZMapConfigSource getSource(const std::string &source_name) ;
  ZMapConfigSource getSource(const std::string &source_name, const std::string &url) ;
  const char* getSourceName(ZMapConfigSource source) ;
  char* getSourceURL(const std::string &source_name) ;
  GList* getSources(const bool include_children = true) ;
  void getSourceChildren(ZMapConfigSource source, GList **result) ;
  void addSourcesFromConfig(const char *filename, const char *config_str, char **stylesfile) ;
  void addSourcesFromConfig(const char *config_str, char **stylesfile) ;
  bool updateContext(_ZMapConfigIniContextStruct *context, ZMapConfigIniFileType file_type) ;

  ZMapConfigSource createSource(const char *source_name, const std::string &url, 
                                const char *featuresets, const char *biotypes, 
                                const bool is_child = false, const bool allow_duplicate = true,
                                GError **error = NULL) ;
  ZMapConfigSource createSource(const char *source_name, const std::string &url, 
                                const char *featuresets, const char *biotypes, 
                                const std::string &file_type, const int num_fields,
                                const bool is_child = false, const bool allow_duplicate = true,
                                GError **error = NULL) ;
  void updateSource(const char *source_name, const char *url, 
                    const char *featuresets, const char *biotypes, 
                    const std::string &file_type, const int num_fields,
                    GError **error) ;
  void updateSource(const char *source_name, const std::string &url, 
                    const char *featuresets, const char *biotypes, 
                    const std::string &file_type, const int num_fields,
                    GError **error) ;
  ZMapConfigSource createFileSource(const char *source_name, const char *file) ;
  ZMapConfigSource createPipeSource(const char *source_name, const char *file, const char *script, const char *args) ;
  void removeSource(const char *source_name_cstr, GError **error) ;
  void countSources(unsigned int &num_total, unsigned int &num_with_data, unsigned int &num_to_load, const bool recent = false) ;

  bool runningUnderOtter() ;

  gboolean getConfigBoolean(const char *key_name, const char *stanza_name = ZMAPSTANZA_APP_CONFIG, 
                            const char *stanza_type = ZMAPSTANZA_APP_CONFIG) ;
  char* getConfigString(const char *key_name, const char *stanza_name = ZMAPSTANZA_APP_CONFIG, 
                        const char *stanza_type = ZMAPSTANZA_APP_CONFIG) ;
  int getConfigInt(const char *key_name, const char *stanza_name = ZMAPSTANZA_APP_CONFIG, 
                   const char *stanza_type = ZMAPSTANZA_APP_CONFIG) ;

  gbtools::trackhub::Registry& getTrackhubRegistry() ;

private:
  void addSource(const std::string &source_name, ZMapConfigSourceStruct *source, GError **error) ;
  void createSourceChildren(ZMapConfigSource source, GError **error = NULL) ;
  void createTrackhubSourceChild(ZMapConfigSource parent_source, const gbtools::trackhub::Track &track) ;

} ZMapFeatureSequenceMapStruct, *ZMapFeatureSequenceMap ;


void zMapFeatureUpdateContext(ZMapFeatureContextMap context_map,
                              ZMapFeatureSequenceMap sequence_map,
                              ZMapFeatureAnyStructType *feature_any, 
                              ZMapConfigIniContext context, 
                              ZMapConfigIniFileType file_type) ;




#endif /* ZMAP_FEATURE_LOAD_DISPLAY_H */

