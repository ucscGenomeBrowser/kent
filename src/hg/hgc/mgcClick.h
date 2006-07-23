/* mgcClick - click handling for MGC-related tracks */
#ifndef MGCCLICK_H
#define MGCCLICK_H

char *mgcDbName();
/* get just the MGC collection name for the current ucsc database */

void printMgcUrl(int imageId);
/* print out an URL to link to the MGC site for a full-length MGC clone */

void printMgcDetailsUrl(char *acc);
/* print out an URL to link to MGC details pages from another details page in
 * the browser.*/

void printMgcRnaSpecs(struct trackDb *tdb, char *acc, int imageId);
/* print status information for MGC mRNA or EST; must have imageId */

void doMgcGenes(struct trackDb *tdb, char *acc);
/* Process click on a mgcGenes track. */
#endif

