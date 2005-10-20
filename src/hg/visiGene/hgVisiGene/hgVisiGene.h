/* hgVisiGene - Gene Picture Browser. */

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

#define hgpId "hgp_id"	/* ID of image in big frame */
#define hgpMatchFile "hgp_matchFile"	/* Name of files containing search matches */
#define hgpListSpec "hgp_listSpec"	/* Contents of search */
#define hgpStartAt "hgp_startAt"	/* Where in match list to start */

char *hgVisiGeneShortName();
/* Return short descriptive name (not cgi-executable name)
 * of program */

char *hgVisiGeneCgiName();
/* Return name of executable. */

char *shortOrgName(char *binomial);
/* Return short name for taxon - scientific or common whatever is
 * shorter */

#endif /* HGVISIGENE_H */
