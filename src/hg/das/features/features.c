/* features - DAS Annotation Feature Server. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "hdb.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "features - DAS Annotation Feature Server\n"
  );
}

void dasHeader(int err)
/* Write out very start of DAS header */
{
puts("Content-Type:text/plain");
puts("X-DAS-Version: DAS/0.95");
printf("X-DAS-Status: %d\n", err);
puts("\n");
if (err != 200)
    exit(-1);
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
    printf("  <TYPE id=\"splicedEsts\" category=\"transcription\" reference=\"no\">spliced EST</TYPE>\n");
    printf("  <METHOD id=\"id\">BLAT</METHOD>\n");
    printf("  <START>%d</START>\n", start);
    printf("  <END>%d</END>\n", end);
    printf("  <ORIENTATION>%c</ORIENTATION>\n", psl->strand[0]);
    printf("  <GROUP id=\"%s.%s.%d\">\n", psl->qName, psl->tName, psl->tStart);
    printf("	<LINK href=\"http://genome.ucsc.edu/cgi-bin/hgTracks?position=%s\">Link to UCSC Browser</LINK>\n", psl->qName);
    printf("  </GROUP>\n");
    printf("</FEATURE>\n");
    }
}

void features()
/* features - DAS Annotation Feature Server. */
{
char *segment = cgiOptionalString("segment");
char *type = cgiOptionalString("type");
char *category = cgiOptionalString("category");
char *parts[3];
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
struct dyString *table = newDyString(0);
struct dyString *query = newDyString(0);
int start, end;
char *seq;

/* Parse segment parameter and make sure it's ok.  Write
 * out error or normal header. */
if (segment == NULL)
    dasHeader(402);
if (chopString(segment, ":,", parts, ArraySize(parts)) < 3)
    dasHeader(402);
if (!isdigit(parts[1][0]) || !isdigit(parts[2][0]))
    dasHeader(402);
seq = parts[0];
start = atoi(parts[1])-1;
end = atoi(parts[2]);
dasHeader(200);

/* Write out DAS Header. */
printf(
"<?xml version=\"1.0\" standalone=\"no\"?>\n"
"     <!DOCTYPE DASGFF SYSTEM \"dasgff.dtd\">\n"
"     <DASGFF>\n"
"       <GFF version=\"1.0\" href=\"%s\">\n", getenv("REQUEST_URL"));
printf(
"       <SEGMENT id=\"splicedEsts\" start=\"%d\" stop=\"%d\" version=\"hg6\" label=\"%s\">\n",
       start+1, end, seq);


/* Query database and output features. */
hSetDb("hg6");
conn = hAllocConn();
dyStringPrintf(table, "%s_intronEst", seq);
dyStringPrintf(query, "select * from %s where tStart<%u and tEnd>%u",
   table->string, end, start);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct psl *psl = pslLoad(row);
    dasOutPsl(psl);
    }

/* Write out DAS footer. */
printf("</SEGMENT></GFF></DASGFF>\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
features();
return 0;
}
