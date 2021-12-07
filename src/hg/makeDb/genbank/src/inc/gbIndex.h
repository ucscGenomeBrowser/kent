/* Copyright (C) 2003 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

/*
 * Object used to load flat-file indices of contents of the genbank/refseq
 * release heirarchies.  Only select sections (processed, aligned) of the
 * index will be loaded by explicted request. 
 *
 * Object heirarchy:
 *    - gbIndex - root, holds other objects
 *      - gbGenome - genome assembly the release is aligned against,
 *         defines database and orgnamism.  Only one genome maybe loaded.
 *      - gbRelease - one object per genbank/refseq release directory
 *        - gbUpdate - update information for the release
 *        - gbEntry - entry for one accession
 *          - gbProcessed - Info about a processed accessions
 *          - gbAligned - Info about aligned accessions
 *
 */
#ifndef GBINDEX_H
#define GBINDEX_H

#include "common.h"
#include "gbDefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct gbIndex
/* Root object of index. */
{
    char *gbRoot;             /* path to the genbank root directory */
    struct gbGenome* genome;  /* genome the index is loaded for, or NULL */
    struct gbRelease* rels[GB_NUM_SRC_DB]; /* Newest-first rels of genbank,
                                            * refseq, indexed by gbSrcDbIdx()
                                            */
};

unsigned gbTypeFromName(char* fileName, boolean checkSpecies);
/* Determine the type flags for a filename based on the naming conventions.
 * Returns set of GB_MRNA or  GB_EST), and (GB_NATIVE or GB_XENO) if species
 * is checked.  */

struct gbIndex* gbIndexNew(char *database, char* gbRoot);
/* Construct a new gbIndex object, finding all of the release directories.  If
 * aligned objects are to be accessed database should be specified. If gbRoot
 * is not null, it should be the path to the root directory.  Otherwise
 * the current directory is assumed.*/

void gbIndexFree(struct gbIndex** indexPtr);
/* Free a gbIndex object */

struct gbRelease* gbIndexMustFindRelease(struct gbIndex* index, char* relName);
/* Find a release */

struct gbRelease* gbIndexNewestRelease(struct gbIndex* index,
                                       unsigned state, unsigned srcDb,
                                       unsigned types, char* limitAccPrefix);
/* find the newest release that is either processed or aligned (based on
 * state) for the specified srcDb */

struct gbSelect* gbIndexGetPartitions(struct gbIndex* index,
                                      unsigned state,
                                      unsigned srcDbs,
                                      char *limitRelName,
                                      unsigned types,
                                      unsigned orgCats,
                                      char *limitAccPrefix);
/* Generate a list of gbSelect objects for partitions of the data that have
 * been processed or aligned based on various filters.  state is GB_PROCESSED
 * or GB_ALIGNED. srcDbs selects source database.  If limitRelName is not
 * null, only this release is used, otherwise the latest release is used.
 * types select mRNAs and/or ESTs.  If limitAccPrefix is not null, only that
 * partition is returned.  List can be freed with slFree.  Entries will be
 * grouped by release.
 */

void gbIndexDump(struct gbIndex* index, FILE* out);
/* print a gbIndex object for debugging */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
