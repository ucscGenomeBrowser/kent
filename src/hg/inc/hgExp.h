/* hgExp - help browse expression data. */

char **hgExpGetNames(
	char *database,	/* Database, commonly "hgFixed" */
	char *table, 	/* Table, such as "affyRatioExps" */
	int expCount, 	/* Number of experiments we want to see */
	int *expIds, 	/* Id's of each experiment.  -1's separate groups. */
	int skipSize);	/* Skip this many letters of experiment name */
/* Create array filled with experiment names. */

void hgExpLabelPrint(
	char *colName, 		/* hgNear column name */
	char *subName, 		/* all/median/selected */
	int skipName,		/* number of characters in name to skip */
	char *url, 		/* URL of hyperlink, may be NULL */
	int representativeCount,/* Number of representative experiments */ 
	int *representatives,   /* ID's of each rep. -1's separate groups */
	char *expTable);	/* Table (in hgFixed) where names stored. */
/* Print out labels of various experiments - as cells filled
 * with gifs in a table. */
