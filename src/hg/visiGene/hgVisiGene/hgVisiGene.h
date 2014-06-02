/* hgVisiGene - Gene Picture Browser. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef HGVISIGENE_H
#define HGVISIGENE_H

/* Names of CGI variables. */
#define hgpPrefix "hgp_"
#define hgpDoPrefix "hgp_do"
#define hgpDoThumbnails "hgp_doThumbnails"
#define hgpDoControls "hgp_doControls"
#define hgpDoImage "hgp_doImage"
#define hgpDoId "hgp_doId"
#define hgpDoSearch "hgp_doSearch"
#define hgpDoProbe "hgp_doProbe"
#define hgpDoConfig "hgp_doConfig"
#define hgpDoDownload "hgp_doDownload"

#define hgpId "hgp_id"	/* ID of image in big frame */
#define hgpMatchFile "hgp_matchFile"	/* Name of files containing search matches */
#define hgpListSpec "hgp_listSpec"	/* Contents of search */
#define hgpStartAt "hgp_startAt"	/* Where in match list to start */

#define hgpIncludeMutants "hgp_includeMutants"  /* Include mutant images. */

#define hgpSs "hgp_ss"	/* Submission Source ID */

/* Global variables (all read-only outside of hgVisiGene). */
extern struct cart *cart;		/* Current CGI values */

char *hgVisiGeneShortName();
/* Return short descriptive name (not cgi-executable name)
 * of program */

char *hgVisiGeneCgiName();
/* Return name of executable. */

char *shortOrgName(char *binomial);
/* Return short name for taxon - scientific or common whatever is
 * shorter */

char *makeCommaSpacedList(struct slName *list);
/* Turn linked list of strings into a single string with
 * elements separated by a comma and a space.  You can
 * freeMem the result when done. */

struct sqlConnection *vAllocConn();
/* Get a connection from connection cache */

void vFreeConn(struct sqlConnection **pConn);
/* Free up connection from connection cache. */

#endif /* HGVISIGENE_H */
