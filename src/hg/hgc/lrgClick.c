/* lrgClick.c - Locus Reference Genomic (LRG) sequences mapped to genome assembly */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hgc.h"
#include "bigBed.h"
#include "fa.h"
#include "genbank.h"
#include "hdb.h"
#include "trackHub.h"
#include "lrg.h"
#include "hui.h"

INLINE void printStartAndMaybeEnd(uint start, uint end)
{
if (end == start+1)
    printf("%d", end);
else
    printf("%d-%d", start+1, end);
}

static void printLrgDiffs(struct lrg *lrg, struct lrgDiff *diffList)
/* Diffstr is a comma-separated list of colon-separated blkStart, lrgStart, assemblySeq, lrgSeq.
 * Make it more readable as a table with links to position ranges. */
{
printf("<TABLE class='stdTbl'>\n"
       "<TR><TH>%s<BR>location</TH><TH>%s<BR>sequence</TH>"
       "<TH>%s<BR>location</TH><TH>%s<BR>sequence",
       lrg->name, lrg->name, database, database);
if (lrg->strand[0] == '-')
    printf("<BR>(on - strand)");
printf("</TH></TR>");
struct lrgDiff *diff;
for (diff = diffList;  diff != NULL;  diff = diff->next)
    {
    printf("<TR><TD>");
    printStartAndMaybeEnd(diff->lrgStart, diff->lrgEnd);
    printf("</TD><TD>%s</TD><TD>", diff->lrgSeq);
    // Make position links that are zoomed out a tiny bit from the actual positions:
    const int rangeFudge = 10;
    uint rangeStart = (diff->chromStart <= rangeFudge) ? 0 : diff->chromStart - rangeFudge;
    uint rangeEnd = diff->chromEnd + rangeFudge;
    printf("<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">%s:",
	   hgTracksPathAndSettings(), database, lrg->chrom, rangeStart+1, rangeEnd, lrg->chrom);
    printStartAndMaybeEnd(diff->chromStart, diff->chromEnd);
    printf("</A></TD><TD>%s</TD></TR>\n", diff->chromSeq);
    }
printf("</TABLE>\n");
slNameFreeList(&diffList);
}

void doLrg(struct trackDb *tdb, char *item)
/* Locus Reference Genomic (LRG) track info. */
{
char *chrom = cartString(cart, "c");
int start = cgiInt("o");
int end = cgiInt("t");
struct sqlConnection *conn = NULL ;
if (!trackHubDatabase(database))
    conn = hAllocConnTrack(database, tdb);
char *fileName = bbiNameFromSettingOrTable(tdb, conn, tdb->table);
hFreeConn(&conn);
if (isEmpty(fileName))
    {
    errAbort("doLrg: missing bigBed fileName for track '%s'", tdb->track);
    }

genericHeader(tdb, item);

// Get column urls from trackDb:
char *urlsStr = trackDbSetting(tdb, "urls");
struct hash *columnUrls = hashFromString(urlsStr);

// Open BigWig file and get interval list.
struct bbiFile *bbi = bigBedFileOpen(fileName);
int bedSize = bbi->definedFieldCount;
int fieldCount = bbi->fieldCount;
struct lm *lm = lmInit(0);
struct bigBedInterval *bbList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
boolean found = FALSE;
struct bigBedInterval *bb;
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    if (!(bb->start == start && bb->end == end))
	continue;
    char *fields[fieldCount];
    char startBuf[16], endBuf[16];
    int bbFieldCount = bigBedIntervalToRow(bb, chrom, startBuf, endBuf, fields, fieldCount);
    if (bbFieldCount != fieldCount)
	errAbort("doLrg: inconsistent fieldCount (bbi has %d, row has %d)",
		 fieldCount, bbFieldCount);
    struct lrg *lrg = lrgLoad(fields);
    if (differentString(lrg->name, item))
	continue;
    found = TRUE;
    printCustomUrl(tdb, lrg->name, TRUE);
    bedPrintPos((struct bed *)lrg, bedSize, tdb);
    char *url = hashFindVal(columnUrls, "hgncId");
    printf("<B>HGNC Gene Symbol:</B> ");
    if (isNotEmpty(url))
	{
	char hgncIdStr[32];
	safef(hgncIdStr, sizeof(hgncIdStr), "%d", lrg->hgncId);
        char *idUrl = replaceInUrl(url, hgncIdStr, cart, database, seqName, winStart, 
                    winEnd, tdb->track, TRUE, NULL);
	printf("<A HREF='%s' TARGET=_BLANK>%s</A><BR>\n", idUrl, lrg->hgncSymbol);
	}
    else
	printf("%s<BR>\n", lrg->hgncSymbol);
    printf("<B>Source of LRG sequence:</B> "
	   "<A HREF='%s' TARGET=_BLANK>%s</A><BR>\n", lrg->lrgSourceUrl, lrg->lrgSource);
    printf("<B>LRG Sequence at NCBI:</B> ");
    url = hashFindVal(columnUrls, "ncbiAcc");
    if (isNotEmpty(url))
	{
        char *idUrl = replaceInUrl(url, lrg->ncbiAcc, cart, database, seqName, winStart, 
                    winEnd, tdb->track, TRUE, NULL);
	printf("<A HREF='%s' TARGET=_BLANK>%s</A><BR>\n", idUrl, lrg->ncbiAcc);
	}
    else
	printf("%s<BR>\n", lrg->ncbiAcc);
    printf("<B>Creation Date:</B> %s<BR>\n", lrg->creationDate);
    char *assemblyDate = hFreezeDate(database);
    if (isNotEmpty(lrg->mismatches))
	{
	printf("<BR><B>Mismatches between %s and %s assembly sequence:</B><BR>\n",
	       lrg->name, assemblyDate);
	struct lrgDiff *mismatches = lrgParseMismatches(lrg);
	printLrgDiffs(lrg, mismatches);
	}
    if (isNotEmpty(lrg->indels))
	{
	printf("<BR><B>Insertions/deletions between %s and %s assembly sequence:</B><BR>\n",
	       lrg->name, assemblyDate);
	struct lrgDiff *indels = lrgParseIndels(lrg);
	printLrgDiffs(lrg, indels);
	}
    }
if (!found)
    {
    printf("No item '%s' starting at %d\n", emptyForNull(item), start);
    }
lmCleanup(&lm);
printTrackHtml(tdb);
}

static struct genbankCds *getLrgCds(struct sqlConnection *conn, struct trackDb *tdb,
				    struct psl *psl)
{
char *cdsTable = trackDbSetting(tdb, "cdsTable");
if (isEmpty(cdsTable) || !hTableExists(database, cdsTable))
    return NULL;
struct dyString *dy = sqlDyStringCreate("select cds from %s where id = '%s'",
					cdsTable, psl->qName);
char *cdsString = sqlQuickString(conn, dy->string);
if (cdsString != NULL)
    {
    struct genbankCds *cds;
    AllocVar(cds);
    // Trash cdsString while parsing out start..end:
    char *startString = cdsString, *endString = strchr(cdsString, '.');
    if (endString == NULL || endString[1] != '.' || !isdigit(endString[2]))
	errAbort("getLrgCds: can't parse cdsString '%s' for %s", cdsString, psl->qName);
    *endString++ = '\0';
    endString++;
    cds->start = sqlUnsigned(startString) - 1;
    cds->end = sqlUnsigned(endString);
    cds->startComplete = TRUE;
    cds->endComplete = TRUE;
    cds->complement = (psl->strand[0] == '-');
    return cds;
    }
return NULL;
}

void htcLrgCdna(char *item)
/* Serve up LRG transcript cdna seq */
{
cartHtmlStart("LRG Transcript Sequence");
char *pslTable = cartString(cart, "table");
struct trackDb *tdb = hashFindVal(trackHash, pslTable);
char *cdnaTable = trackDbSetting(tdb, "cdnaTable");
if (isEmpty(cdnaTable) || !hTableExists(database, cdnaTable))
    errAbort("htcLrgCdna: cdnaTable not found for '%s'", pslTable);

struct sqlConnection *conn = hAllocConn(database);
struct psl *psl = getAlignments(conn, pslTable, item);
struct genbankCds *cds = getLrgCds(conn, tdb, psl);
hFreeConn(&conn);

struct dnaSeq *seq = hGenBankGetMrna(database, item, cdnaTable);
if (seq == NULL)
    errAbort("htcLrgCdna: no sequence for '%s' in cdnaTable '%s'", item, cdnaTable);
tolowers(seq->dna);
if (cds != NULL)
    toUpperN(seq->dna + cds->start, cds->end - cds->start);

printf("<PRE><TT>");
printf(">%s\n", item);
faWriteNext(stdout, NULL, seq->dna, seq->size);
printf("</TT></PRE>");

hFreeConn(&conn);
}

void doLrgTranscriptPsl(struct trackDb *tdb, char *item)
/* Locus Reference Genomic (LRG) transcript mapping and sequences. */
{
struct sqlConnection *conn = hAllocConn(database);
struct psl *psl = getAlignments(conn, tdb->table, item);
struct genbankCds *cds = getLrgCds(conn, tdb, psl);
hFreeConn(&conn);

genericHeader(tdb, item);
char *url = trackDbSetting(tdb, "url");
if (isNotEmpty(url))
    {
    char *lrgName = cloneString(item);
    // Truncate the "t1" part to get the LRG ID for link:
    char *p = strchr(lrgName, 't');
    if (p)
	*p = '\0';
    char *urlLabel = trackDbSettingOrDefault(tdb, "urlLabel", "LRG Transcript link");
    printf("<B>%s</B> ", urlLabel);
    //char *lrgTUrl = replaceInUrl(tdb, url, lrgName, TRUE);
    char *lrgTUrl = replaceInUrl(url, lrgName, cart, database, seqName, winStart, 
                    winEnd, tdb->track, TRUE, NULL);
    printf("<A HREF='%s' TARGET=_BLANK>%s</A><BR>\n", lrgTUrl, item);
    }

struct genePred *gp = genePredFromPsl3(psl, cds, genePredAllFlds, genePredPslCdsMod3, -1, -1);
printPos(gp->chrom, gp->txStart, gp->txEnd, gp->strand, FALSE, NULL);
printf("<H3>Links to %s sequence:</H3>\n", item);
printf("<UL>\n");
char *pepTable = trackDbSetting(tdb, "pepTable");
if ((pepTable != NULL) && hGenBankHaveSeq(database, item, pepTable))
    {
    puts("<LI>\n");
    hgcAnchorSomewhere("htcTranslatedProtein", item, pepTable, seqName);
    printf("Translated Protein</A> \n");
    puts("</LI>\n");
    }
puts("<LI>\n");
hgcAnchorSomewhere("htcLrgCdna", item, tdb->table, seqName);
printf("Transcript mRNA</A> (may be different from the genomic sequence)\n");
puts("</LI>\n");
printf("</UL>\n");

printf("<H3>Alignment of %s to genome assembly:</H3>", item);
int start = cartInt(cart, "o");
printAlignments(psl, start, "htcCdnaAli", tdb->table, item);

printTrackHtml(tdb);
}
