/* mgcClick - click handling for MGC-related tracks */
#ifndef MGCCLICK_H
#define MGCCLICK_H

char *mgcDbName();
/* get just the MGC collection name for the current ucsc database */

void printMgcUrl(int imageId);
/* print out an URL to link to the MGC site for a full-length MGC clone */

void printMgcDetailsUrl(char *acc, int start);
/* print out an URL to link to MGC details pages from another details page in
 * the browser.*/

void doMgcGenes(struct trackDb *tdb, char *acc);
/* Process click on a mgcGenes track. */

void doOrfeomeGenes(struct trackDb *tdb, char *acc);
/* Process click on a orfeomeGenes track. */

#endif

