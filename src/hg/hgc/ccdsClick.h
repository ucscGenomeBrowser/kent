/* ccdsClick - click handling for CCDS track and related functions  */
#ifndef CCDSCLICK_H
#define CCDSCLICK_H

struct ccdsInfo *getCcdsUrlForSrcDb(struct sqlConnection *conn, char *acc);
/* Get a ccdsInfo object for a RefSeq, ensembl, or vega gene, if it
 * exists, otherwise return NULL */

void printCcdsUrlForSrcDb(struct sqlConnection *conn, struct ccdsInfo *ccdsInfo);
/* Print out CCDS URL for a refseq, ensembl, or vega gene, if it
 * exists.  */

void printCcdsForSrcDb(struct sqlConnection *conn, char *acc);
/* Print out CCDS link for a refseq, ensembl, or vega gene, if it
 * exists.  */

struct ccdsGeneMap *getCcdsGenesForMappedGene(struct sqlConnection *conn, char *acc,
                                              char *mapTable);
/* get a list of ccds genes associated with a current and window from a
 * mapping table, or NULL */

void printCcdsUrlForMappedGene(struct sqlConnection *conn, struct ccdsGeneMap *gene);
/* Print out CCDS url for a gene mapped via a cddsGeneMap table  */

void doCcdsGene(struct trackDb *tdb, char *ccdsId);
/* Process click on a CCDS gene. */


#endif
