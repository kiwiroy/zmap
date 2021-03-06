/*  File: zmapGFFAttribute.h
 *  Author: Steve Miller (sm23@sanger.ac.uk)
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
 * Description: Header file for attribute data structure and
 * associated functions. This is used for representing GFF v3
 * (or v2 if required) attributes, and contains associated
 * parsing functions.
 *
 *-------------------------------------------------------------------
 */




#ifndef ZMAP_GFFATTRIBUTE_H
#define ZMAP_GFFATTRIBUTE_H

#include <string.h>
#include <ctype.h>
#include <ZMap/zmapFeature.hpp>
#include <ZMap/zmapGFF.hpp>
#include <zmapGFF3_P.hpp>

/*
 * These macro definitions create the attribute names in the following form:
 *
 *     DEFINE_ATTRIBUTE(a, b)    =>    const char * a_b = "b" ;
 *
 * and are used to generate names such as
 *
 *   const char * sAttributeName_ensembl_variation = "ensembl_variation" ;
 *
 * and so on. I prefer not to (ab)use macros in this way, but this saves some
 * typing and reduces the possibility of errors. Also gives a consistent naming
 * convention.
 *
 */
#ifdef ATTRIBUTE_DEFINITIONS
#define DEFINE_ATTRIBUTE(a, b) const char * a##_##b = #b ;
#else
#define DEFINE_ATTRIBUTE(a, b) extern const char * a##_##b ;
#endif

DEFINE_ATTRIBUTE(sAttributeName, Class)
DEFINE_ATTRIBUTE(sAttributeName, percentID)
DEFINE_ATTRIBUTE(sAttributeName, Align)
DEFINE_ATTRIBUTE(sAttributeName, start_not_found)
DEFINE_ATTRIBUTE(sAttributeName, end_not_found)
DEFINE_ATTRIBUTE(sAttributeName, length)
DEFINE_ATTRIBUTE(sAttributeName, Name)
DEFINE_ATTRIBUTE(sAttributeName, Alias)
DEFINE_ATTRIBUTE(sAttributeName, Target)
DEFINE_ATTRIBUTE(sAttributeName, Dbxref)
DEFINE_ATTRIBUTE(sAttributeName, Ontology_term)
DEFINE_ATTRIBUTE(sAttributeName, Is_circular)
DEFINE_ATTRIBUTE(sAttributeName, Parent)
DEFINE_ATTRIBUTE(sAttributeName, url)
DEFINE_ATTRIBUTE(sAttributeName, ensembl_variation)
DEFINE_ATTRIBUTE(sAttributeName, allele_string)
DEFINE_ATTRIBUTE(sAttributeName, Note)
DEFINE_ATTRIBUTE(sAttributeName, Derives_from)
DEFINE_ATTRIBUTE(sAttributeName, ID)
DEFINE_ATTRIBUTE(sAttributeName, sequence)
DEFINE_ATTRIBUTE(sAttributeName, Source)
DEFINE_ATTRIBUTE(sAttributeName, Assembly_source)
DEFINE_ATTRIBUTE(sAttributeName, locus)
DEFINE_ATTRIBUTE(sAttributeName, gaps)
DEFINE_ATTRIBUTE(sAttributeName, Gap)
DEFINE_ATTRIBUTE(sAttributeName, cigar_exonerate)
DEFINE_ATTRIBUTE(sAttributeName, cigar_ensembl)
DEFINE_ATTRIBUTE(sAttributeName, cigar_bam)
DEFINE_ATTRIBUTE(sAttributeName, vulgar_exonerate)
DEFINE_ATTRIBUTE(sAttributeName, Known_name)
DEFINE_ATTRIBUTE(sAttributeName, assembly_path)
DEFINE_ATTRIBUTE(sAttributeName, read_pair_id)

#undef DEFINE_ATTRIBUTE

/*
 * Structure for attribute data. Data are:
 *
 * sName                string of the name as found in the data file
 * sTemp                data string found with attribute
 *
 */
typedef struct ZMapGFFAttributeStruct_
  {
    char *sName ;
    char *sTemp ;
  } ZMapGFFAttributeStruct ;


ZMapGFFAttribute zMapGFFCreateAttribute() ;
gboolean zMapGFFDestroyAttribute(ZMapGFFAttribute) ;

char * zMapGFFAttributeGetNamestring(ZMapGFFAttribute ) ;
char * zMapGFFAttributeGetTempstring(ZMapGFFAttribute) ;

ZMapGFFAttribute zMapGFFAttributeParse(ZMapGFFParser, const char * const, gboolean) ;
gboolean zMapGFFAttributeRemoveQuotes(ZMapGFFAttribute) ;
ZMapGFFAttribute* zMapGFFAttributeParseList(ZMapGFFParser, const char * const, unsigned int * const, gboolean ) ;
gboolean zMapGFFAttributeDestroyList(ZMapGFFAttribute * , unsigned int) ;

ZMapGFFAttribute zMapGFFAttributeListContains(ZMapGFFAttribute * , unsigned int, const char * const) ;

/*
 * Attribute parsing functions. Similar to previous implementation, but broken up
 * into simpler components.
 */
gboolean zMapAttParseAlias(ZMapGFFAttribute, char ** const ) ;
gboolean zMapAttParseAlign(ZMapGFFAttribute, int * const, int * const, ZMapStrand * const);
gboolean zMapAttParseAssemblySource(ZMapGFFAttribute, char ** const, char ** const) ;
gboolean zMapAttParseAssemblyPath(ZMapGFFAttribute, char ** const, ZMapStrand * const , int * const, GArray ** const, char*) ;
gboolean zMapAttParseAnyTwoStrings(ZMapGFFAttribute, char ** const, char ** const) ;
gboolean zMapAttParseClass(ZMapGFFAttribute, ZMapHomolType *const );
gboolean zMapAttParseGap(ZMapGFFAttribute , GArray ** const , ZMapStrand , int, int, ZMapStrand, int, int) ;
gboolean zMapAttParseCigarExonerate(ZMapGFFAttribute , GArray ** const , ZMapStrand , int, int, ZMapStrand, int, int) ;
gboolean zMapAttParseCigarEnsembl(ZMapGFFAttribute , GArray ** const , ZMapStrand , int, int, ZMapStrand, int, int) ;
gboolean zMapAttParseCigarBam(ZMapGFFAttribute , GArray ** const , ZMapStrand , int, int, ZMapStrand, int, int) ;
gboolean zMapAttParseCDSStartNotFound(ZMapGFFAttribute, int * const ) ;
gboolean zMapAttParseCDSEndNotFound(ZMapGFFAttribute) ;
gboolean zMapAttParseDerives_from(ZMapGFFAttribute, char ** const ) ;
gboolean zMapAttParseDbxref(ZMapGFFAttribute, char ** const, char ** const ) ;
gboolean zMapAttParseGaps(ZMapGFFAttribute, GArray ** const, ZMapStrand, ZMapStrand ) ;
gboolean zMapAttParseID(ZMapGFFAttribute, GQuark * const ) ;
gboolean zMapAttParseIs_circular(ZMapGFFAttribute, gboolean * const ) ;
gboolean zMapAttParseKnownName(ZMapGFFAttribute, char ** const ) ;
gboolean zMapAttParseLocus(ZMapGFFAttribute, GQuark * const ) ;
gboolean zMapAttParseLength(ZMapGFFAttribute, int* const) ;
gboolean zMapAttParseName(ZMapGFFAttribute, char ** const) ;
gboolean zMapAttParseNameV2(ZMapGFFAttribute, GQuark *const, char ** const, char ** const) ;
gboolean zMapAttParseNote(ZMapGFFAttribute, char ** const ) ;
gboolean zMapAttParseOntology_term(ZMapGFFAttribute, char ** const, char ** const ) ;
gboolean zMapAttParseParent(ZMapGFFAttribute, char ** const ) ;
gboolean zMapAttParsePID(ZMapGFFAttribute, double *const);
gboolean zMapAttParseSequence(ZMapGFFAttribute, char ** const ) ;
gboolean zMapAttParseSource(ZMapGFFAttribute, char ** const) ;
gboolean zMapAttParseTargetV2(ZMapGFFAttribute, char ** const, char ** const ) ;
gboolean zMapAttParseTarget(ZMapGFFAttribute, GQuark * const, int * const , int * const , ZMapStrand * const ) ;
gboolean zMapAttParseURL(ZMapGFFAttribute, char** const ) ;
gboolean zMapAttParseVulgarExonerate(ZMapGFFAttribute ,
                                     ZMapStrand , int, int, ZMapStrand, int, int,
                                     GArray ** const exons_out, GArray ** const introns_out,
                                     GArray ** const exon_aligns_out) ;
gboolean zMapAttParseEnsemblVariation(ZMapGFFAttribute, char ** const ) ;
gboolean zMapAttParseAlleleString(ZMapGFFAttribute, char ** const) ;
gboolean zMapAttParseReadPairID(ZMapGFFAttribute, GQuark * const) ;

#endif
