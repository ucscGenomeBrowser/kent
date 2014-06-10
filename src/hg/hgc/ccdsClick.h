/* ccdsClick - click handling for CCDS track and related functions  */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef CCDSCLICK_H
#define CCDSCLICK_H

void printCcdsExtUrl(char *ccdsId);
/* Print out URL to link to CCDS database at NCBI */

void printCcdsForSrcDb(struct sqlConnection *conn, char *acc);
/* Print out CCDS hc link for a refseq, ensembl, or vega gene, if it
 * exists.  */

struct ccdsGeneMap *getCcdsGenesForMappedGene(struct sqlConnection *conn, char *acc,
                                              char *mapTable);
/* get a list of ccds genes associated with a current and window from a
 * mapping table, or NULL */

void printCcdsUrl(struct sqlConnection *conn, char *ccdsId);
/* Print out CCDS hgc URL for a gene  */

void doCcdsGene(struct trackDb *tdb, char *ccdsId);
/* Process click on a CCDS gene. */


#endif
