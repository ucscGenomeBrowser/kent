/* das - Distributed Annotation System server. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "psl.h"
#include "jksql.h"
#include "hdb.h"
#include "chromInfo.h"
#include <regex.h>

char *database = "hg7";
char *version = "7.00";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "das - Distributed Annotation System server.\n"
  "Needs to be called from a Web server.\n"
  );
}

void dasAbout()
/* Print a little info when they just hit cgi-bin/das. */
{
puts("Content-Type:text/plain\n");
puts("UCSC Human Genome DAS Server.\n");
puts("See http://www.biodas.org for more info on DAS.");
exit(0);
}

void dasHeader(int err)
/* Write out very start of DAS header */
{
puts("Content-Type:text/plain");
puts("X-DAS-Version: DAS/0.95");
printf("X-DAS-Status: %d\n", err);
printf("\n");
if (err != 200)
    {
    exit(-1);
    }
puts("<?xml version=\"1.0\" standalone=\"no\"?>");
}

void normalHeader()
/* Write normal (non-error) header. */
{
dasHeader(200);
}

void earlyError(int errCode)
/* Return error in early processing (before writing header) */
{
dasHeader(errCode);
}

char *currentUrl()
/* Query environment to get current URL. */
{
static char url[512];
sprintf(url, "http://%s%s%s", getenv("SERVER_NAME"), getenv("SCRIPT_NAME"), getenv("PATH_INFO"));
return url;
}

struct segment
/* A subset of a sequence. */
    {
    struct segment *next;	/* Next in list. */
    char *seq;		/* Name of sequence, often a chromosome. */
    int start;		/* Zero based start. */
    int end;		/* Base after end. */
    boolean wholeThing;	/* TRUE if user requested whole sequence. */
    };

struct segment *segmentNew(char *seq, int start, int end, boolean wholeThing)
/* Make a new segment. */
{
struct segment *segment;
AllocVar(segment);
segment->seq = cloneString(seq);
segment->start = start;
segment->end = end;
segment->wholeThing = wholeThing;
return segment;
}

struct segment *dasSegmentList(boolean mustExist)
/* Get a DAS segment list from either segment or ref parameters. 
 * Call this before you write out header so it can return errors
 * properly if parameters are malformed. */
{
struct slName *segList, *seg;
struct segment *segmentList = NULL, *segment;
char *seq;
int start=0, end=0;
boolean wholeThing;

segList = cgiStringList("segment");
if (segList != NULL)
    {
    /* Handle new format (post 0.995 spec) lists. */
    for (seg = segList; seg != NULL; seg = seg->next)
	{
	char *parts[3];
	int partCount;

	wholeThing = FALSE;
	partCount = chopString(seg->name, ":,", parts, ArraySize(parts));
	seq = parts[0];
	if (partCount == 1)
	    {
	    start = 0;
	    end = hChromSize(seq);
	    wholeThing = TRUE;
	    }
	else if (partCount == 3)
	    {
	    if (!isdigit(parts[1][0]) || !isdigit(parts[2][0]))
		earlyError(402);
	    start = atoi(parts[1])-1;
	    end = atoi(parts[2]);
	    if (start > end)
	        earlyError(405);
	    }
	else
	    {
	    earlyError(402);
	    }
	segment = segmentNew(seq, start, end, wholeThing);
	slAddHead(&segmentList, segment);
	}
    slReverse(&segmentList);
    }
else
    {
    /* Handle old format (pre 0.995 spec) lists. */
    seq = cgiOptionalString("ref");
    wholeThing = TRUE;
    if (seq == NULL)
	{
	if (mustExist)
	    earlyError(402);
	else
	    {
	    return NULL;
	    }
	}
    if (cgiVarExists("start"))
	{
        start = cgiInt("start")-1;
	wholeThing = FALSE;
	}
    else
        start = 0;
    if (cgiVarExists("stop"))
	{
        end = cgiInt("stop");
	wholeThing = FALSE;
	}
    else
        end = hChromSize(seq);
    if (start > end)
	earlyError(405);
    segmentList = segmentNew(seq, start, end, wholeThing);
    }
/* Check all segments are chromosomes. */
for (segment = segmentList; segment != NULL; segment = segment->next)
    {
    if (!startsWith("chr", segment->seq))
        earlyError(403);
    }
return segmentList;
}

void doDsn()
/* dsn - DSN Server for DAS. */
{
normalHeader();
printf(
" <!DOCTYPE DASDSN SYSTEM \"http://www.biodas.org/dtd/dasdsn.dtd\">\n"
" <DASDSN>\n");
printf(
"   <DSN>\n"
"     <SOURCE id=\"%s\" version=\"%s\">Reference April 2001 Human Genome at UCSC</SOURCE>\n"
"     <MAPMASTER>http://genome-test.cse.ucsc.edu:80/cgi-bin/das/%s</MAPMASTER>\n"
"     <DESCRIPTION>Reference April 2001 Human Genome at UCSC</DESCRIPTION>\n"
"   </DSN>\n", database, version, database);
printf(" </DASDSN>\n");
}

void dasOutPsl(struct psl *psl)
/* Write out DAS info on a psl alignment. */
{
int i;
for (i=0; i<psl->blockCount; ++i)
    {
    int s = psl->tStarts[i];
    int e = s + psl->blockSizes[i];
    int start,end;
    if (psl->strand[1] == '-')
        {
	start = psl->tSize - e;
	end = psl->tSize - s;
	}
    else
        {
	start = s;
	end = e;
	}
    printf(
    "<FEATURE id=\"%s.%s.%d\" label=\"%s\">\n", psl->qName, psl->tName, psl->tStart, psl->qName);
    printf(" <TYPE id=\"intronEst\" category=\"transcription\" reference=\"no\">spliced ESTs</TYPE>\n");
    printf(" <METHOD id=\"id\">BLAT</METHOD>\n");
    printf(" <START>%d</START>\n", start);
    printf(" <END>%d</END>\n", end);
    printf(" <ORIENTATION>%c</ORIENTATION>\n", psl->strand[0]);
    printf(" <GROUP id=\"%s.%s.%d\">\n", psl->qName, psl->tName, psl->tStart);
    printf("  <LINK href=\"http://genome.ucsc.edu/cgi-bin/hgTracks?position=%s\">Link to UCSC Browser</LINK>\n", psl->qName);
    printf(" </GROUP>\n");
    printf("</FEATURE>\n");
    }
}

void doFeatures()
/* features - DAS Annotation Feature Server. */
{
struct segment *segmentList = dasSegmentList(TRUE), *segment;
struct slName *typeList = cgiStringList("type"), *typeEl;
char *type = cgiOptionalString("type");
char *category = cgiOptionalString("category");
char *parts[3];
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
int start, end;
char *seq;

/* Write out DAS features header. */
normalHeader();
printf(
"<!DOCTYPE DASGFF SYSTEM \"http://www.biodas.org/dtd/dasgff.dtd\">\n"
"<DASGFF>\n"
"<GFF version=\"1.0\" href=\"%s\">\n", currentUrl());

for (segment = segmentList; segment != NULL; segment = segment->next)
    {
    struct dyString *table = newDyString(0);
    struct dyString *query = newDyString(0);

    /* Print segment header. */
    seq = segment->seq;
    start = segment->start;
    end = segment->end;
    printf(
    "<SEGMENT id=\"seq\" start=\"%d\" stop=\"%d\" version=\"%s\" label=\"%s\">\n",
	   start+1, end, version, seq);

    /* Query database and output features. */
    conn = hAllocConn();
    dyStringPrintf(table, "%s_intronEst", seq);
    dyStringPrintf(query, "select * from %s where tStart<%u and tEnd>%u",
       table->string, end, start);
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	struct psl *psl = pslLoad(row);
	dasOutPsl(psl);
	pslFree(&psl);
	}
    printf("</SEGMENT>\n");
    sqlFreeResult(&sr);
    freeDyString(&table);
    freeDyString(&query);
    }

/* Write out DAS footer. */
printf("</GFF></DASGFF>\n");
}

void doEntryPoints()
/* Handle entry points request. */
{
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
struct chromInfo *ci;

normalHeader();
conn = hAllocConn();
printf("<!DOCTYPE DASEP SYSTEM \"http://www.biodas.org/dtd/dasep.dtd\">\n");
printf("<DASEP>\n");
printf("<ENTRY_POINTS href=\"%s\" version=\"7.00\">\n",
	currentUrl());

sr = sqlGetResult(conn, "select * from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    ci = chromInfoLoad(row);
    printf(" <SEGMENT id=\"%s\" start=\"%d\" stop=\"%d\" orientation=\"+\" subparts=\"no\">%s</SEGMENT>\n", ci->chrom, 1, ci->size, ci->chrom);
    chromInfoFree(&ci);
    }
printf("</ENTRY_POINTS>\n");
printf("</DASEP>\n");
}

void doDna()
/* Handle request for DNA. */
{
struct segment *segmentList = dasSegmentList(TRUE), *segment;
int size = 0;
struct dnaSeq *seq;
int i, oneSize, lineSize = 50;

/* Write header. */
normalHeader();
printf("<!DOCTYPE DASDNA SYSTEM \"http://www.biodas.org/dtd/dasdna.dtd\">\n");
printf("<DASDNA>\n");

/* Write each sequence. */
for (segment = segmentList; segment != NULL; segment = segment->next)
    {
    printf("<SEQUENCE id=\"%s\" start=\"%d\" stop=\"%d\" version=\"%s\">\n",
	    segment->seq, segment->start+1, segment->end, version);
    printf("<DNA length=\"%d\">\n", segment->end - segment->start);

    /* Write out DNA. */
    seq = hDnaFromSeq(segment->seq, segment->start, segment->end, dnaLower);
    if (seq == NULL)
        errAbort("Couldn't fetch %s\n", seq);
    size = seq->size;
    for (i=0; i<size; i += oneSize)
	{
	oneSize = size - i;
	if (oneSize > lineSize) oneSize = lineSize;
	mustWrite(stdout, seq->dna+i, oneSize);
	fputc('\n', stdout);
	}
    printf("</DNA>\n");
    printf("</SEQUENCE>\n");
    }

/* Write footer. */
printf("</DASDNA>\n");
}

int countFeatures(char *table, struct segment *segmentList)
/* Count all the features in a given segment. */
{
struct segment *segment;
int acc = 0;
struct sqlConnection *conn = hAllocConn();
char chrTable[256];
char query[512];
for (segment = segmentList; segment != NULL; segment = segment->next)
    {
    if (segment->wholeThing)
        {
	sprintf(chrTable, "%s_%s", segment->seq, table);
	if (sqlTableExists(conn, chrTable))
	    {
	    sprintf(query, "select count(*) from %s", chrTable);
	    acc += sqlQuickNum(conn, query);
	    }
	}
    }
hFreeConn(&conn);
return acc;
}

void doTypes()
/* Handle a types request. */
{
struct segment *segmentList = dasSegmentList(FALSE);
int count = countFeatures("intronEst", segmentList);
normalHeader();
printf("<!DOCTYPE DASTYPES SYSTEM \"http://www.biodas.org/dtd/dastypes.dtd\">\n");
printf("<DASTYPES>\n");
printf("<GFF version=\"1.2\" summary=\"yes\" href=\"%s\">\n", currentUrl());
printf("<SEGMENT version=\"%s\">\n", version);
printf("<TYPE id=\"intronEst\" category=\"transcription\" method=\"BLAT\" >");
if (count > 0)
    printf("%d", count);
printf("</TYPE>\n");
printf("</SEGMENT>\n");
printf("</GFF>\n");
printf("</DASTYPES>\n");
}

void dispatch(char *dataSource, char *command)
/* Dispatch a dase command. */
{
if (sameString(dataSource, "ensembl110"))
    dataSource = "hg7";
if (sameString(dataSource, "dsn"))
    doDsn();
else if (sameString(database, dataSource))
    {
    hSetDb(dataSource);
    if (sameString(command, "entry_points"))
        doEntryPoints();
    else if (sameString(command, "dna"))
        doDna();
    else if (sameString(command, "types"))
        doTypes();
    else if (sameString(command, "features"))
        doFeatures();
    else
        earlyError(501);
    }
else 
    earlyError(401);
}

void das(char *pathInfo)
/* das - Das Server. */
{
static char *parts[3];
int partCount;
if (pathInfo == NULL)
    dasAbout();
pathInfo = cloneString(pathInfo);
partCount = chopString(pathInfo, "/", parts, ArraySize(parts));
if (partCount < 1)
    dasAbout();
dispatch(parts[0], parts[1]);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *path = getenv("PATH_INFO");
das(path);
return 0;
}
