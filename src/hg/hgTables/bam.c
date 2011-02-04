/* bam - stuff to handle BAM stuff in table browser. */

#ifdef USE_BAM

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
#include "hubConnect.h"
#include "hgTables.h"
#include "bamFile.h"
#if (defined USE_BAM && defined KNETFILE_HOOKS)
#include "knetUdc.h"
#include "udc.h"
#endif//def USE_BAM && KNETFILE_HOOKS


boolean isBamTable(char *table)
/* Return TRUE if table corresponds to a BAM file. */
{
if (isHubTrack(table))
    {
    struct trackDb *tdb = hashFindVal(fullTrackAndSubtrackHash, table);
    return startsWithWord("bam", tdb->type);
    }
else
    return trackIsType(database, table, curTrack, "bam", ctLookupName);
}

char *bamFileName(char *table, struct sqlConnection *conn)
/* Return file name associated with BAM.  This handles differences whether it's
 * a custom or built-in track.  Do a freeMem on returned string when done. */
{
/* Implementation is same as bigWig. */
return bigWigFileName(table, conn);
}

char *bamAsDef = 
"";

void showSchemaBam(char *table)
/* Show schema on bam. */
{
/* Get description of columns, making it up from BED records if need be. */
struct asObject *as = bigBedAs(bbi);
if (as == NULL)
    as = asParseText(bedAsDef(bbi->definedFieldCount, bbi->fieldCount));

hPrintf("<B>Database:</B> %s", database);
hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<B>Primary Table:</B> %s<br>", table);
hPrintf("<B>Big Bed File:</B> %s", fileName);
if (bbi->version >= 2)
    {
    hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<B>Item Count:</B> ");
    printLongWithCommas(stdout, bigBedItemCount(bbi));
    }
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
#endif /* USE_BAM */

