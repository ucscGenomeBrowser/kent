/* transMapStuff - common definitions and functions for supporting transMap
 * tracks in the browser CGIs */

/* Copyright (C) 2008 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef transMapStuff_h
#define transMapStuff_h
struct trackDb;

/* 
 * transMap table names setting names in trackDb. To get tables shared between
 * genomes, use names "hgFixed.transMapSrc"
 */
#define transMapInfoTblSetting    "transMapInfo"
#define transMapSrcTblSetting     "transMapSrc"
#define transMapGeneTblSetting    "transMapGene"

char* transMapSkipGenomeDbPrefix(char *id);
/* Skip the source genome db prefix (e.g. hg19:) in a TransMap identifier.
 * Return the full id if no db prefix is found for compatibility with older
 * version of transmap. */

char *transMapIdToSeqId(char *id);
/* remove all unique suffixes (starting with last `-') from any TransMap 
 * id, leaving the database prefix in place.  WARNING: static return */

char *transMapIdToAcc(char *id);
/* remove database prefix and all unique suffixes (starting with last `-')
 * from any TransMap id.  WARNING: static return */
#endif
