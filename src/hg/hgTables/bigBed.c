/* bigBed - stuff to handle bigBed in the Table Browser. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "localmem.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "cart.h"
#include "web.h"
#include "bed.h"
#include "hdb.h"
#include "trackDb.h"
#include "obscure.h"
#include "hmmstats.h"
#include "correlate.h"
#include "asParse.h"
#include "bbiFile.h"
#include "bigBed.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: bigBed.c,v 1.4 2009/03/17 04:28:38 kent Exp $";

boolean isBigBed(char *table)
/* Return TRUE if table corresponds to a bigBed file. */
{
return trackIsType(table, "bigBed");
}

char *bigBedFileName(char *table, struct sqlConnection *conn)
/* Return file name associated with bigBed.  This handles differences whether it's
 * a custom or built-in track.  Do a freeMem on returned string when done. */
{
/* Implementation is same as bigWig. */
return bigWigFileName(table, conn);
}

struct hash *asColumnHash(struct asObject *as)
/* Return a hash full of the object's columns, keyed by colum name */
{
struct hash *hash = hashNew(6);
struct asColumn *col;
for (col = as->columnList; col != NULL; col = col->next)
    hashAdd(hash, col->name, col);
return hash;
}

static void fillField(struct hash *colHash, char *key, char output[HDB_MAX_FIELD_STRING])
/* If key is in colHash, then copy key to output. */
{
if (hashLookup(colHash, key))
    strncpy(output, key, HDB_MAX_FIELD_STRING-1);
}

struct hTableInfo *bigBedToHti(char *table, struct sqlConnection *conn)
/* Get fields of bigBed into hti structure. */
{
char *fileName = bigBedFileName(table, conn);
/* Get columns in asObject format, from file or failing that as bed-standard. */
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct asObject *as = bigBedAs(bbi);
if (as == NULL) 
    as = asParseText(bedAsDef(bbi->definedFieldCount, bbi->fieldCount));

/* Allocate hTableInfo structure and fill in info about bed fields. */
struct hash *colHash = asColumnHash(as);
struct hTableInfo *hti;
AllocVar(hti);
hti->rootName = cloneString(table);
hti->isPos= TRUE;
fillField(colHash, "chrom", hti->chromField);
fillField(colHash, "chromStart", hti->startField);
fillField(colHash, "chromEnd", hti->endField);
fillField(colHash, "name", hti->nameField);
fillField(colHash, "score", hti->scoreField);
fillField(colHash, "strand", hti->strandField);
fillField(colHash, "thickStart", hti->cdsStartField);
fillField(colHash, "thickEnd", hti->cdsEndField);
fillField(colHash, "blockCount", hti->countField);
fillField(colHash, "chromStarts", hti->startsField);
fillField(colHash, "blockSizes", hti->endsSizesField);
hti->hasCDS = (bbi->definedFieldCount >= 8);
hti->hasBlocks = (bbi->definedFieldCount >= 12);
char type[256];
safef(type, sizeof(type), "bed %d %c", bbi->definedFieldCount,
	(bbi->definedFieldCount == bbi->fieldCount ? '.' : '+'));
hti->type = cloneString(type);

hashFree(&colHash);
bbiFileClose(&bbi);
return hti;
}


void showSchemaBigBed(char *table)
/* Show schema on bigBed. */
{
/* Figure out bigBed file name and open it.  Get contents for first chromosome as an example. */
struct sqlConnection *conn = hAllocConn(database);
char *fileName = bigBedFileName(table, conn);
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct bbiChromInfo *chromList = bbiChromList(bbi);
struct lm *lm = lmInit(0);
struct bigBedInterval *ivList = bigBedIntervalQuery(bbi, chromList->name, 0, 
					 	    chromList->size, 10, lm);

/* Get description of columns, making it up from BED records if need be. */
struct asObject *as = bigBedAs(bbi);
if (as == NULL)
    as = asParseText(bedAsDef(bbi->definedFieldCount, bbi->fieldCount));

hPrintf("<B>Big Bed File:</B> %s", fileName);
hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<B>Item Count:</B> ");
printLongWithCommas(stdout, bigBedItemCount(bbi));
hPrintf("<BR>\n");
hPrintf("<B>Format description:</B> %s<BR>", as->comment);

/* Put up table that describes fields. */
hTableStart();
hPrintf("<TR><TH>field</TH>");
hPrintf("<TH>example</TH>");
hPrintf("<TH>description</TH> ");
puts("</TR>\n");
struct asColumn *col;
int colCount = 0;
char *row[bbi->fieldCount];
char startBuf[16], endBuf[16];
char *dupeRest = lmCloneString(lm, ivList->rest);	/* Manage rest-stomping side-effect */
bigBedIntervalToRow(ivList, chromList->name, startBuf, endBuf, row, bbi->fieldCount);
ivList->rest = dupeRest;
for (col = as->columnList; col != NULL; col = col->next)
    {
    hPrintf("<TR><TD><TT>%s</TT></TD>", col->name);
    hPrintf("<TD>%s</TD>", row[colCount]);
    hPrintf("<TD>%s</TD></TR>", col->comment);
    ++colCount;
    }

/* If more fields than descriptions put up minimally helpful info (at least has example). */
for ( ; colCount < bbi->fieldCount; ++colCount)
    {
    hPrintf("<TR><TD><TT>column%d</TT></TD>", colCount+1);
    hPrintf("<TD>%s</TD>", row[colCount]);
    hPrintf("<TD>n/a</TD></TR>\n");
    }
hTableEnd();


/* Put up another section with sample rows. */
webNewSection("Sample Rows");
hTableStart();

/* Print field names as column headers for example */
hPrintf("<TR>");
int colIx = 0;
for (col = as->columnList; col != NULL; col = col->next)
    {
    hPrintf("<TH>%s</TH>", col->name);
    ++colIx;
    }
for (; colIx < colCount; ++colIx)
    hPrintf("<TH>column%d</TH>", colIx+1);
hPrintf("</TR>\n");

/* Print sample lines. */
struct bigBedInterval *iv;
for (iv=ivList; iv != NULL; iv = iv->next)
    {
    bigBedIntervalToRow(iv, chromList->name, startBuf, endBuf, row, bbi->fieldCount);
    hPrintf("<TR>");
    for (colIx=0; colIx<colCount; ++colIx)
        {
	writeHtmlCell(row[colIx]);
	}
    hPrintf("</TR>\n");
    }
hTableEnd();

/* Clean up and go home. */
lmCleanup(&lm);
bbiFileClose(&bbi);
freeMem(fileName);
hFreeConn(&conn);
}
