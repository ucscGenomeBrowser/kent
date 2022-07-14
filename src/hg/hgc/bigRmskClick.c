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


struct bigRmskAlignRecord
    {
    char *chrom;           /*Reference sequence chromosome or scaffold*/
    unsigned chromStart;   /*Start position of alignment on chromosome*/
    unsigned chromEnd;     /*End position of alignment on chromosome*/
    unsigned chromRemain;  /*Remaining bp in the chromosome or scaffold*/
    float score;           /*alignment score (sw, bits or evalue)*/
    float percSubst;       /*Base substitution percentage*/
    float percDel;         /*Base deletion percentage*/
    float percIns;         /*Bases insertion percentage*/
    char strand[2];        /*Strand - either + or -*/
    char *repName;         /*Name of repeat*/
    char *repType;         /*Type of repeat*/
    char *repSubtype;      /*Subtype of repeat*/
    unsigned repStart;     /*Start in repeat sequence*/
    unsigned repEnd;       /*End in repeat sequence*/
    unsigned repRemain;    /*Remaining unaligned bp in the repeat sequence*/
    unsigned id;           /*The ID of the hit. Used to link related fragments*/
    char *calignData;      /*The alignment data stored as a single string*/
    };


void printAlignmentBR (char strand, char * genoName, uint genoStart, uint genoEnd, 
                       char *repName, uint repStart, uint repEnd,
                       char *calignData )
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
uint qStart = genoStart;
uint sStart = repStart;
if (strand == '-')
    sStart = repEnd;

int maxNameLen =
    (strlen(genoName) >
     strlen(repName) ? strlen(genoName) : strlen(repName));

while (calignData[aIdx] != '\0')
    {
    if (calignData[aIdx] == '/')
        inSub = 1;
    else if (calignData[aIdx] == '-')
        inDel ^= 1;
    else if (calignData[aIdx] == '+')
        inIns ^= 1;
    else
	{
	if (inSub)
	    {
	    subjSeq[sIdx - 1] = calignData[aIdx];
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
	    querySeq[sIdx] = calignData[aIdx];
	    subjSeq[sIdx] = '-';
	    diffSeq[sIdx] = '-';
	    qCnt++;
	    sIdx++;
	    }
	else if (inIns)
	    {
	    querySeq[sIdx] = '-';
	    subjSeq[sIdx] = calignData[aIdx];
	    diffSeq[sIdx] = '-';
	    sCnt++;
	    sIdx++;
	    }
	else
	    {
	    diffSeq[sIdx] = ' ';
	    querySeq[sIdx] = calignData[aIdx];
	    subjSeq[sIdx] = calignData[aIdx];
	    sCnt++;
	    qCnt++;
	    sIdx++;
	    }
	if (sIdx == alignLength)
	    {
	    querySeq[sIdx] = '\0';
	    diffSeq[sIdx] = '\0';
	    subjSeq[sIdx] = '\0';
	    printf ("%*s %10d %s %d\n", maxNameLen, genoName, qStart,
		    querySeq, (qStart + qCnt - 1));
	    printf ("%*s            %s\n", maxNameLen, " ", diffSeq);
	    if (strand == '+')
	        printf ("%*s %10d %s %d\n", maxNameLen, repName, sStart,
		    subjSeq, (sStart + sCnt - 1));
	    else
	        printf ("%*s %10d %s %d\n", maxNameLen, repName, sStart,
		    subjSeq, (sStart - sCnt + 1));
	    printf ("\n");
	    qStart += qCnt;
	    if (strand == '+')
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
    printf ("%*s %10d %s %d\n", maxNameLen, genoName, qStart, querySeq,
	    (qStart + qCnt - 1));
    printf ("%*s            %s\n", maxNameLen, " ", diffSeq);
    if (strand == '+')
        printf ("%*s %10d %s %d\n", maxNameLen, repName, sStart, subjSeq,
	    (sStart + sCnt - 1));
    else
        printf ("%*s %10d %s %d\n", maxNameLen, repName, sStart, subjSeq,
	    (sStart - sCnt + 1));
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


/* Open bigBed file and get interval list. */
char *fileName = trackDbSetting(tdb, "bigDataUrl");
struct bbiFile *bbi = bigBedFileOpenAlias(fileName,chromAliasFindAliases);
struct lm *lm = lmInit(0);
struct bigBedInterval *bbList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);

/* Find particular item in list - matching start, and item if possible. */
boolean found = FALSE;
struct bigBedInterval *bb;
const char *data_style = "style=\"padding:0px 6px;\"";
int bedSize = bbi->fieldCount;
char *fields[bedSize];
char startBuf[17], endBuf[17];
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
char *xrefDataUrl = trackDbSetting(tdb, "xrefDataUrl");
if ( xrefDataUrl ) 
    {
    //struct bbiFile *abbi = bigBedFileOpen(xrefDataUrl);
    struct bbiFile *abbi = bigBedFileOpenAlias(xrefDataUrl,chromAliasFindAliases);
    struct lm *alm = lmInit(0);
    struct bigBedInterval *abbList = bigBedIntervalQuery(abbi, chrom, start, end, 0, alm);

    printf ("<h4>RepeatMasker Alignments:</h4>\n");
    printf ("The raw alignment data used by RepeatMasker to generate "
	     "the final annotation call for this element. NOTE: The "
	     "aligned sequence names and consensus positions may differ "
	     "from the final annotation.<p>\n");
    printf ("<table>\n");

    struct bigRmskAlignRecord *arec = NULL;
    AllocVar(arec);
    for (bb = abbList; bb != NULL; bb = bb->next)
        {
        char *afields[abbi->fieldCount];
        bigBedIntervalToRow(bb, chrom, startBuf, endBuf, afields,
                            abbi->fieldCount);
        if ( sameOk(afields[15], item) )
            {
            arec->chrom = afields[0];
            arec->chromStart = sqlUnsigned(afields[1]);
            arec->chromEnd = sqlUnsigned(afields[2]);
            arec->chromRemain = sqlUnsigned(afields[3]);
            arec->score = sqlFloat(afields[4]);
            arec->percSubst = sqlFloat(afields[5]);
            arec->percDel = sqlFloat(afields[6]);
            arec->percIns = sqlFloat(afields[7]);
            safef(arec->strand, sizeof(arec->strand), "%s", afields[8]);
            arec->repName = afields[9];
            arec->repType = afields[10];
            arec->repSubtype = afields[11];
            arec->repStart = sqlUnsigned(afields[12]);
            arec->repEnd = sqlUnsigned(afields[13]);
            arec->repRemain = sqlUnsigned(afields[14]);
            arec->id = sqlUnsigned(afields[15]);
            arec->calignData = afields[16];

	    printf ("  <tr>\n");
	    printf ("    <td>%.2f</td>\n", arec->score);
	    printf ("    <td>%3.2f</td>\n", arec->percSubst);
	    printf ("    <td>%3.2f</td>\n", arec->percDel);
	    printf ("    <td>%3.2f</td>\n", arec->percIns);
	    printf ("    <td>%s</td>\n", arec->chrom);
	    printf ("    <td>%d</td>\n", arec->chromStart + 1);
	    printf ("    <td>%d</td>\n", arec->chromEnd);
	    printf ("    <td>(%d)</td>\n", arec->chromRemain);
	    printf ("    <td>%s</td>\n", arec->strand);
	    printf ("    <td>%s</td>\n", arec->repName);
	    printf ("    <td>%s/%s</td>\n", arec->repType, arec->repSubtype);
	    if (arec->strand[0] == '-')
		{
		printf ("    <td>(%d)</td>\n", arec->repRemain);
		printf ("    <td>%d</td>\n", arec->repEnd);
		printf ("    <td>%d</td>\n", arec->repStart);
		}
	    else
		{
		printf ("    <td>%d</td>\n", arec->repStart);
		printf ("    <td>%d</td>\n", arec->repEnd);
		printf ("    <td>(%d)</td>\n", arec->repRemain);
		}
	    printf ("    <td>%d</td>\n", arec->id);
	    printf ("    </tr><tr><td colspan=\"15\"><pre>\n");
            printAlignmentBR ( 
                       arec->strand[0], arec->chrom, arec->chromStart, arec->chromEnd, 
                       arec->repName, arec->repStart, arec->repEnd,
                       arec->calignData );
	    printf ("    </pre><td></tr>\n");
	    }
        } //for(bb = abblist....
    printf ("</table>\n");
    lmCleanup(&alm);
    bbiFileClose(&abbi);
    } // if ( xref...
printTrackHtml(tdb);
}
