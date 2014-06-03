/* intronSize - a column that shows sizes of introns for your gene. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "cheapcgi.h"
#include "localmem.h"
#include "dystring.h"
#include "obscure.h"
#include "jksql.h"
#include "hdb.h"
#include "hgNear.h"
#include "sqlList.h"

boolean intronSizeExists(struct column *col, struct sqlConnection *conn)
/* This returns true if all tables this depends on exists. */
{
return sqlTableExists(conn, col->table);
}

int intronSizeForRow(struct column *col, char **row)
/* Get size of intron (max/min according to user desires)
 * given row[0] is comma separated list of exon starts, 
 * and row[1] is comma separated list of exon ends. */
{
unsigned *starts, *ends;
int startCount, endCount;
int intronSize = 0;

/* Convert to arrays of numbers instead of comma separated strings. */
sqlUnsignedDynamicArray(row[0], &starts, &startCount);
sqlUnsignedDynamicArray(row[1], &ends, &endCount);
if (startCount != endCount)
    errAbort("mismatch count");

if (startCount >= 2)  // any introns?
    {
    intronSize = starts[1] - ends[0];	// First intron.
    if (startCount >= 3)
	{
	int intronIx;
	if (sameString(col->whichIntron, "max"))
	    {
	    for (intronIx = 2; intronIx < startCount; ++intronIx)
		{
		int oneSize = starts[intronIx] - ends[intronIx-1];
		if (oneSize > intronSize)
		    intronSize = oneSize;
		}
	    }
	else if (sameString(col->whichIntron, "min"))
	    {
	    for (intronIx = 2; intronIx < startCount; ++intronIx)
		{
		int oneSize = starts[intronIx] - ends[intronIx-1];
		if (oneSize < intronSize)
		    intronSize = oneSize;
		}
	    }
	}
    }
freez(&starts);
freez(&ends);
return intronSize;
}

char *intronSizeCellVal(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Make comma separated list of matches to association table. */
{
int intronSize = 0;

/* Set up query */
char query[1000];
sqlSafef(query, sizeof(query), "select exonStarts, exonEnds from %s where name='%s'",
	col->table, gp->name);

/* Get row we need. */
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
if ((row = sqlNextRow(sr)) != NULL)
    intronSize = intronSizeForRow(col, row);
sqlFreeResult(&sr);

/* Convert numerical result to string in dynamic memory. */
if (intronSize == 0)
    return NULL;
char buf[16];
safef(buf, sizeof(buf), "%d", intronSize);
return cloneString(buf);
}

#define defaultWhichIntron "max"

static void intronSizeConfigControls(struct column *col)
/* Print out configuration column */
{
hPrintf("<TD>");
char *varName = configVarName(col, "whichIntron");
char *val = cartUsualString(cart, varName, defaultWhichIntron);
cgiMakeRadioButton(varName, "max", sameString(val, "max"));
hPrintf(" max ");
cgiMakeRadioButton(varName, "min", sameString(val, "min"));
hPrintf(" min ");
hPrintf("</TD>");
}

static struct genePos *intronSizeAdvFilter(struct column *col, 
	struct sqlConnection *conn, struct genePos *list)
/* Do advanced filter on intronSize. */
{
char *minString = advFilterVal(col, "min");
char *maxString = advFilterVal(col, "max");
if (minString != NULL || maxString != NULL)
    {
    int minVal = 0, maxVal = BIGNUM;
    if (minString != NULL)
        minVal = atoi(minString);
    if (maxString != NULL)
        maxVal = atoi(maxString);

    /* Construct a hash of all genes that pass filter. */
    struct hash *passHash = newHash(17);
    char query[1000];
    char **row;
    struct sqlResult *sr;
    sqlSafef(query, sizeof(query), "select name,exonStarts,exonEnds from %s", 
        col->table);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	int size = intronSizeForRow(col, row+1);
	if (minVal <= size && size <= maxVal)
	    hashAdd(passHash, row[0], NULL);
	}
    sqlFreeResult(&sr);

    /* Remove non-passing genes. */
    list = weedUnlessInHash(list, passHash);
    hashFree(&passHash);
    }
return list;
}

void setupColumnIntronSize(struct column *col, char *parameters)
/* Set up a intronSize type column. */
{
char *varName = configVarName(col, "whichIntron");
col->whichIntron = cartUsualString(cart, varName, defaultWhichIntron);
col->table = genomeSetting("geneTable");
col->exists = intronSizeExists;	   
col->cellVal = intronSizeCellVal;
col->configControls = intronSizeConfigControls;
col->filterControls = minMaxAdvFilterControls;
col->advFilter = intronSizeAdvFilter;
}
