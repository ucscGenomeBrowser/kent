/* das - Das Server. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "psl.h"
#include "jksql.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "das - Das Server\n"
  "usage:\n"
  "   das XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
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
    printf("Error code %d\n", err);
    exit(-1);
    }
puts("<?xml version=\"1.0\" standalone=\"no\"?>\n");
}

void doDsn()
/* dsn - DSN Server for DAS. */
{
dasHeader(200);
puts(
" <!DOCTYPE DASDSN SYSTEM \"dasdsn.dtd\">\n"
" <DASDSN>\n"
"   <DSN>\n"
"     <SOURCE id=\"splicedEsts\" version=\"hg7\">UCSC Spliced ESTs</SOURCE>\n"
"     <MAPMASTER>http://servlet.sanger.ac.uk:8080/das/ensembl110</MAPMASTER>\n"
"     <DESCRIPTION>UCSC Spliced ESTs on April 2001</DESCRIPTION>\n"
"   </DSN>\n"
" </DASDSN>\n");
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

void doFeatures()
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

void doEntryPoints()
/* Handle entry points request. */
{
}

void dispatch(char *table, char *command, char *sub)
/* Dispatch a dase command. */
{
if (sameString(table, "dsn"))
    doDsn();
else if (sameString(command, "DSN"))
    {
    if (sub == NULL)
        dasHeader(400);
    if (sameString(sub, "features"))
	doFeatures();
    else if (sameString(sub, "entry_points"))
        doEntryPoints();
    else
        dasHeader(500);
    }
else 
    dasHeader(500);
}

void das(char *pathInfo)
/* das - Das Server. */
{
static char *parts[3];
int partCount;
if (pathInfo == NULL)
    errAbort("Sorry, no path info");
pathInfo = cloneString(pathInfo);
partCount = chopString(pathInfo, "/", parts, ArraySize(parts));
if (partCount < 1)
    errAbort("No parts");
dispatch(parts[0], parts[1], parts[2]);
}

int main(int argc, char *argv[])
/* Process command line. */
{
/* Print DAS header. */
das(getenv("PATH_INFO"));
return 0;
}
