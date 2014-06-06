/* txGeneColor - Figure out color to draw gene in.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "spDb.h"
#include "cdsPick.h"
#include "txInfo.h"
#include "memgfx.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneColor - Figure out color to draw gene in.\n"
  "usage:\n"
  "   txGeneColor uniProtDb tx.info tx.pick kgColor.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct rgbColor black = {0,0,0};
struct rgbColor trueBlue = {12,12,120};
struct rgbColor mediumBlue = {80, 80, 160};
struct rgbColor lightBlue = {130, 130, 210};

void txGeneColor(char *uniProtDb, char *infoFile, char *pickFile, char *outFile)
/* txGeneColor - Figure out color to draw gene in.. */
{
/* Load picks into hash.  We don't use cdsPicksLoadAll because empty fields
 * cause that autoSql-generated routine problems. */
struct hash *pickHash = newHash(18);
struct cdsPick *pick;
struct lineFile *lf = lineFileOpen(pickFile, TRUE);
char *row[CDSPICK_NUM_COLS];
while (lineFileRowTab(lf, row))
    {
    pick = cdsPickLoad(row);
    hashAdd(pickHash, pick->name, pick);
    }

/* Open uniprot database connection. */
struct sqlConnection *uConn = sqlConnect(uniProtDb);

#ifdef OLD
/* Figure out our light and medium colors. */
mediumBlue.r = (6*trueBlue.r + 4*255)/10;
mediumBlue.g = (6*trueBlue.g + 4*255)/10;
mediumBlue.b = (6*trueBlue.b + 4*255)/10;
lightBlue.r = (1*trueBlue.r + 2*255)/3;
lightBlue.g = (1*trueBlue.g + 2*255)/3;
lightBlue.b = (1*trueBlue.b + 2*255)/3;
#endif /* OLD */

/* Read in info file, and loop through it to make out file. */
struct txInfo *info, *infoList = txInfoLoadAll(infoFile);
FILE *f = mustOpen(outFile, "w");
for (info = infoList; info != NULL; info = info->next)
    {
    struct rgbColor *col;
    pick = hashFindVal(pickHash, info->name);
    if (pick != NULL)
        {
	char *source = pick->source;
	if (sameString(source, "RefPepValidated"))
	    col = &trueBlue;
	else if (sameString(source, "ccds"))
	    col = &trueBlue;
	else if (sameString(source, "RefPepReviewed"))
	    col = &trueBlue;
	else if (sameString(source, "RefSeqValidated"))
	    col = &trueBlue;
	else if (sameString(source, "RefSeqReviewed"))
	    col = &trueBlue;
	else if (sameString(source, "swissProt"))
	    col = &trueBlue;
	else if (startsWith("Ref", source))
	    col = &mediumBlue;
	else
	    col = &lightBlue;
	if (pick->swissProt[0] != 0)
	    {
	    char *acc = spLookupPrimaryAcc(uConn, pick->swissProt);
	    struct slName *pdbList = spPdbAccs(uConn, acc);
	    if (pdbList != NULL)
	        col = &black;
	    slFreeList(&pdbList);
	    }
	}
    else
        col = &lightBlue;
    fprintf(f, "%s\t%d\t%d\t%d\n", info->name, col->r, col->g, col->b);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
txGeneColor(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
