/* hgExp - help browse expression data. */

/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef HGEXP_H
#define HGEXP_H

void hgExpColorDropDown(struct cart *cart, char *varName);
/* Make color drop-down. */

boolean hgExpRatioUseBlue(struct cart *cart, char *varName);
/* Return TRUE if should use blue instead of red
 * in the expression ratios. */

char **hgExpGetNames(
	char *database,	/* Database, commonly "hgFixed" */
	char *table, 	/* Table, such as "affyRatioExps" */
	int expCount, 	/* Number of experiments we want to see */
	int *expIds, 	/* Id's of each experiment.  -1's separate groups. */
	int skipSize);	/* Skip this many letters of experiment name */
/* Create array filled with experiment names. */

void hgExpLabelPrint(
        char *database,         /* database we're loading */
	char *colName, 		/* hgNear column name */
	char *subName, 		/* all/median/selected */
	int skipName,		/* number of characters in name to skip */
	char *url, 		/* URL of hyperlink, may be NULL */
	int representativeCount,/* Number of representative experiments */ 
	int *representatives,   /* ID's of each rep. -1's separate groups */
	char *expTable,	/* Table (in hgFixed) where names stored. */
	int gifStart);		/* If split deal with this. */
/* Print out labels of various experiments - as cells filled
 * with gifs in a table. */

boolean hgExpLoadVals(struct sqlConnection *lookupConn,
	struct sqlConnection *dataConn,
	char *lookupTable, char *name, char *dataTable,
	int *retValCount, float **retVals);
/* Load up and return expression bed record.  Return NULL
 * if none of given name exist. */

void hgExpCellPrint(char *colName, char *geneId, 
	struct sqlConnection *lookupConn, char *lookupTable,
	struct sqlConnection *dataConn, char *dataTable,
	int representativeCount, int *representatives,
	boolean useBlue, boolean useGrays, boolean logGrays, float scale);
/* Print out html for expression cell in table. */

#endif /* HGEXP_H */

