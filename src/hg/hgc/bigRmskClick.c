/* bigRmskClick - Click handler for bigRmskTrack.  This is based
 *                loosely on the original rmsk click handler inside
 *                of hgc.c.
 *
 *  Written by Robert Hubley 10/2021
 */

/* Copyright (C) 2021 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hgc.h"
#include "rmskAlign.h"
#include "hCommon.h"
#include "chromAlias.h"


void printAlignmentBR (struct rmskAlign *ra)
/*
 * Print RepeatMasker alignment data stored in RM's cAlign format.
 * The format is basically a lightly compressed diff format where
 * the query and subject are merged into one squence line. The
 * runs of exact matching sequences are interrupted by either
 * single base substitutions annotated as queryBase "/" subjectBase,
 * insertions in the subject annotated as "+ACTAT+", or deletions
 * in the query annotated as "-ACTTTG-".
 */
{
int alignLength = 80;
char querySeq[alignLength + 1];
char diffSeq[alignLength + 1];
char subjSeq[alignLength + 1];
int inSub = 0;
int inDel = 0;
int inIns = 0;

int aIdx = 0;
int sIdx = 0;
int qCnt = 0;
int sCnt = 0;
int qStart = ra->genoStart;
int sStart = ra->repStart;
if (ra->strand[0] == '-')
    sStart = ra->repEnd;

int maxNameLen =
    (strlen (ra->genoName) >
     strlen (ra->repName) ? strlen (ra->genoName) : strlen (ra->repName));

while (ra->alignment[aIdx] != '\0')
    {
    if (ra->alignment[aIdx] == '/')
        inSub = 1;
    else if (ra->alignment[aIdx] == '-')
        inDel ^= 1;
    else if (ra->alignment[aIdx] == '+')
        inIns ^= 1;
    else
	{
	if (inSub)
	    {
	    subjSeq[sIdx - 1] = ra->alignment[aIdx];
	    if ((querySeq[sIdx - 1] == 'C' &&
		 subjSeq[sIdx - 1] == 'T') ||
		(querySeq[sIdx - 1] == 'T' &&
		 subjSeq[sIdx - 1] == 'C') ||
		(querySeq[sIdx - 1] == 'A' &&
		 subjSeq[sIdx - 1] == 'G') ||
		(querySeq[sIdx - 1] == 'G' && subjSeq[sIdx - 1] == 'A'))
	    diffSeq[sIdx - 1] = 'i';
	    else if ((index ("BDHVRYKMSWNX", querySeq[sIdx - 1]) != NULL) ||
		     (index ("BDHVRYKMSWNX", subjSeq[sIdx - 1]) != NULL))
	        diffSeq[sIdx - 1] = '?';
	    else
	        diffSeq[sIdx - 1] = 'v';
	    inSub = 0;
	    }
	else if (inDel)
	    {
	    querySeq[sIdx] = ra->alignment[aIdx];
	    subjSeq[sIdx] = '-';
	    diffSeq[sIdx] = '-';
	    qCnt++;
	    sIdx++;
	    }
	else if (inIns)
	    {
	    querySeq[sIdx] = '-';
	    subjSeq[sIdx] = ra->alignment[aIdx];
	    diffSeq[sIdx] = '-';
	    sCnt++;
	    sIdx++;
	    }
	else
	    {
	    diffSeq[sIdx] = ' ';
	    querySeq[sIdx] = ra->alignment[aIdx];
	    subjSeq[sIdx] = ra->alignment[aIdx];
	    sCnt++;
	    qCnt++;
	    sIdx++;
	    }
	if (sIdx == alignLength)
	    {
	    querySeq[sIdx] = '\0';
	    diffSeq[sIdx] = '\0';
	    subjSeq[sIdx] = '\0';
	    printf ("%*s %10d %s %d\n", maxNameLen, ra->genoName, qStart,
		    querySeq, (qStart + qCnt - 1));
	    printf ("%*s            %s\n", maxNameLen, " ", diffSeq);
	    if (ra->strand[0] == '+')
	        printf ("%*s %10d %s %d\n", maxNameLen, ra->repName, sStart,
		    subjSeq, (sStart + sCnt - 1));
	    else
	        printf ("%*s %10d %s %d\n", maxNameLen, ra->repName, sStart,
		    subjSeq, (sStart - sCnt + 1));
	    printf ("\n");
	    qStart += qCnt;
	    if (ra->strand[0] == '+')
	        sStart += sCnt;
	    else
	        sStart -= sCnt;
	    qCnt = 0;
	    sCnt = 0;
	    sIdx = 0;
	    }
	}
    aIdx++;
    }
if (sIdx)
    {
    querySeq[sIdx] = '\0';
    diffSeq[sIdx] = '\0';
    subjSeq[sIdx] = '\0';
    printf ("%*s %10d %s %d\n", maxNameLen, ra->genoName, qStart, querySeq,
	    (qStart + qCnt - 1));
    printf ("%*s            %s\n", maxNameLen, " ", diffSeq);
    if (ra->strand[0] == '+')
        printf ("%*s %10d %s %d\n", maxNameLen, ra->repName, sStart, subjSeq,
	    (sStart + sCnt - 1));
    else
        printf ("%*s %10d %s %d\n", maxNameLen, ra->repName, sStart, subjSeq,
	    (sStart - sCnt + 1));
    ;
    }
}

void printOutTableHeaderBR (char strand)
/* Print the appropriate HTML table header
 * given the strand of the element.
 */
{
const char *hd1_style =
    "style=\"background-color:#C2C9E0; font-size:14px; padding:4px 14px;\"";
const char *hd1span_style =
    "style=\"background-color:#6678B1; font-size:14px; padding:4px 14px;\"";
const char *hd2_style =
    "style=\"background-color:#C2C9E0; font-size:14px; padding:4px 14px; "
    "border-bottom: 2px solid #6678B1;\"";
printf ("  <thead>\n");
printf ("    <tr>\n");
printf ("      <th %s>bit/sw</th>\n", hd1_style);
printf ("      <th %s>perc</th>\n", hd1_style);
printf ("      <th %s>perc</th>\n", hd1_style);
printf ("      <th %s>perc</th>\n", hd1_style);
printf ("      <th colspan=\"4\" %s>query</th>\n", hd1span_style);
printf ("      <th %s></th>\n", hd1_style);
printf ("      <th colspan=\"5\" %s>matching repeat</th>\n", hd1span_style);
printf ("      <th %s></th>\n", hd1_style);
printf ("    </tr>\n");
printf ("    <tr>\n");
printf ("      <th %s>score</th>\n", hd2_style);
printf ("      <th %s>div.</th>\n", hd2_style);
printf ("      <th %s>del.</th>\n", hd2_style);
printf ("      <th %s>ins.</th>\n", hd2_style);
printf ("      <th %s>sequence</th>\n", hd2_style);
printf ("      <th %s>begin</th>\n", hd2_style);
printf ("      <th %s>end</th>\n", hd2_style);
printf ("      <th %s>remaining</th>\n", hd2_style);
printf ("      <th %s>orient.</th>\n", hd2_style);
printf ("      <th %s>name</th>\n", hd2_style);
printf ("      <th %s>class/family</th>\n", hd2_style);
if (strand == 'C')
    {
    printf ("      <th %s>remaining</th>\n", hd2_style);
    printf ("      <th %s>end</th>\n", hd2_style);
    printf ("      <th %s>begin</th>\n", hd2_style);
    }
else
    {
    printf ("      <th %s>begin</th>\n", hd2_style);
    printf ("      <th %s>end</th>\n", hd2_style);
    printf ("      <th %s>remaining</th>\n", hd2_style);
    }
printf ("      <th %s>id</th>\n", hd2_style);
printf ("    </tr>\n");
printf ("  </thead>\n");
}


void doBigRmskRepeat (struct trackDb *tdb, char *item)
/* Main entry point */
{
cartWebStart (cart, database, "%s", tdb->longLabel);
char *chrom = cartString(cart, "c");
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
char *fileName = trackDbSetting(tdb, "bigDataUrl");

/* Open BigWig file and get interval list. */
struct bbiFile *bbi =  bigBedFileOpenAlias(fileName, chromAliasFindAliases);
struct lm *lm = lmInit(0);
struct bigBedInterval *bbList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);

/* Find particular item in list - matching start, and item if possible. */
boolean found = FALSE;
struct bigBedInterval *bb;
const char *data_style = "style=\"padding:0px 6px;\"";
int bedSize = bbi->fieldCount;
char *fields[bedSize];
char startBuf[16], endBuf[16];
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, chrom, startBuf, endBuf, fields,
                                           bedSize);
    if ( sameOk(fields[12], item) )
        {
        found = TRUE;
        break;
        }
    }

if ( found )
    {
    char class[32];
    class[0] = '\0';
    char subclass[32];
    subclass[0] = '\0';
    char *poundPtr = index (fields[3], '#');
    if (poundPtr)
        {
        // Terminate name string properly
        safecpy (class, sizeof (class), poundPtr + 1);
        *poundPtr = '\0';
        char *slashPtr = index (class, '/');
        if (slashPtr)
            {
            // Terminate class string properly
            safecpy (subclass, sizeof (subclass), slashPtr + 1);
            *slashPtr = '\0';
            }
        }
    printf ("<b>Repeat:</b> %s<br>\n", fields[3]);
    printf ("<b>Class:</b> %s<br>\n", class);
    printf ("<b>Subclass:</b> %s<br>\n", subclass);
    printf ("<b>Orientation:</b> %s<br>\n", fields[5]);
    //printf ("<b>Joined Element Genomic Range:</b> %s:%d-%d<br>\n",
    //rmJoin->chrom, rmJoin->alignStart+1, rmJoin->alignEnd);
    printf ("<br><br>\n");

    printf ("<h4>RepeatMasker Joined Annotations:</h4>\n");
    printf
           ("The RepeatMasker annotation line(s) for this element. "
            "If the element is fragmented the output will contain one "
            "line per joined fragment.<p>\n");

    printf ("<table cellspacing=\"0\">\n");
    char *description = fields[13];
    printOutTableHeaderBR(*fields[5]);
    char *outLine;
    char *outLineTok = description;
    while ((outLine = strsep(&outLineTok,",")) != NULL)
        {
        char *outField;
        char *outFieldTok = outLine;
        printf ("  <tr>\n");
        while ((outField = strsep(&outFieldTok," ")) != NULL)
            {
            printf ("    <td %s>%s</td>\n", data_style, outField);
            }
        printf ("  <td>\n");
        printf ("<A HREF=\"%s&db=%s&position=%s%%3A%d-%s\">Browser</A>",
                hgTracksPathAndSettings (), database, fields[0],
                atoi(fields[6]) + 1, fields[7]);
        char *tbl = cgiUsualString ("table", cgiString ("g"));
        printf (" - <A HREF=\"%s&o=%s&g=getDna&i=%s&c=%s&l=%s&r=%s&"
                "strand=%s&table=%s\">"
                "DNA</A>\n", hgcPathAndSettings (), fields[6],
                (fields[3] != NULL ? cgiEncode (fields[3]) : ""),
                fields[0], fields[6], fields[7], cgiEncode (fields[5]),
                tbl);
        printf ("  </td>\n");
        printf ("  </tr>\n");
        }
    printf("</table>\n");
    }
else
    {
    printf("No item %s starting at %d\n", emptyForNull(item), start);
    }
lmCleanup(&lm);
bbiFileClose(&bbi);

    /*
     * Locate *.align data for this element
     */
/* 
 * DISABLED FOR NOW, this might be part of a future enhancement if we create a auxilary bigBed file to
 * contain this data
 *
    if (hTableExists (database, alignTable))
	{
	struct rmskAlign *ro;
	if (!hFindSplitTable (database, seqName, alignTable, qTable, sizeof(qTable), &hasBin))
	    errAbort("track %s not found", alignTable);
	sqlSafef (query, sizeof (query),
	       "select * from %s where genoName = '%s' and genoStart >= %d"
	       " and id = %s", qTable, seqName, start-1, repeat);
	sr2 = sqlGetResult (conn2, query);
	printf ("<h4>RepeatMasker Alignments:</h4>\n");
	printf
	    ("The raw alignment data used by RepeatMasker to generate "
	     "the final annotation call for this element. NOTE: The "
	     "aligned sequence names and consensus positions may differ "
	     "from the final annotation.<p>\n");
	printf ("<table>\n");
	while ((row = sqlNextRow (sr2)) != NULL)
	    {
	    ro = rmskAlignLoad (row + hasBin);
	    printf ("  <tr>\n");
	    printf ("    <td>%d</td>\n", ro->swScore);
	    printf ("    <td>%3.2f</td>\n",
		    (double) ro->milliDiv * (double) 0.01);
	    printf ("    <td>%3.2f</td>\n",
		    (double) ro->milliDel * (double) 0.01);
	    printf ("    <td>%3.2f</td>\n",
		    (double) ro->milliIns * (double) 0.01);
	    printf ("    <td>%s</td>\n", ro->genoName);
	    printf ("    <td>%d</td>\n", ro->genoStart + 1);
	    printf ("    <td>%d</td>\n", ro->genoEnd);
	    printf ("    <td>(%d)</td>\n", ro->genoLeft);
	    printf ("    <td>%s</td>\n", ro->strand);
	    printf ("    <td>%s</td>\n", ro->repName);
	    printf ("    <td>%s/%s</td>\n", ro->repClass, ro->repFamily);
	    if (ro->strand[0] == '-')
		{
		printf ("    <td>(%d)</td>\n", ro->repLeft);
		printf ("    <td>%d</td>\n", ro->repEnd);
		printf ("    <td>%d</td>\n", ro->repStart);
		}
	    else
		{
		printf ("    <td>%d</td>\n", ro->repStart);
		printf ("    <td>%d</td>\n", ro->repEnd);
		printf ("    <td>(%d)</td>\n", ro->repLeft);
		}
	    printf ("    <td>%d</td>\n", ro->id);
	    printf ("    </tr><tr><td colspan=\"15\"><pre>\n");
	    printAlignment (ro);
	    printf ("    </pre><td></tr>\n");
	    }
	sqlFreeResult (&sr2);
	printf ("</table>\n");
	}
    hFreeConn (&conn2);
    }
    */
printTrackHtml (tdb);
}
