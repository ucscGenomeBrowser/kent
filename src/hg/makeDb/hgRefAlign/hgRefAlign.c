/*
 * hgRefAlign format table alignfile ...
 *
 * Load a reference alignment into a table. 
 *
 * o format - Specifies the format of the input.  Currently
 *   support values:
 *   o webb - Format of alignment's from Webb Miller.
 */
#include "common.h"
#include "errabort.h"
#include "jksql.h"
#include "hgRefAlignWebb.h"
#include "refAlign.h"

static char* TMP_TAB_FILE = "refAlign.tab";

static int countInserts(char* insertSeq, char* otherSeq, int* numInsert,
                        int* baseInsert)
/* Count a contiguous insert in insertSeq, that is, number of `-' in
 * otherSeq return amount to increment pointers by to place them at
 * the end of the inserts.*/
{
int cnt = 0;
assert(*otherSeq == '-');

while (*otherSeq++ == '-')
    cnt++;

(*numInsert)++;
(*baseInsert) += cnt;
return cnt-1;  /* want to move to end of block, not past */
}

static void calcScore(struct refAlign* refAlign)
/* Calculate and set score from the per-base stats */
{
unsigned numInserts = refAlign->aNumInsert + refAlign->hNumInsert;
unsigned baseInserts = refAlign->aNumInsert + refAlign->hNumInsert;
refAlign->score = 1000*(refAlign->matches
                        /(refAlign->matches+refAlign->misMatches+numInserts
                          +log((double)baseInserts)));
}

static void calcRecStats(struct refAlign* refAlign)
/* Calculate and fill in the statistics and score for a refAlign*/
{
char* hp = refAlign->humanSeq;
char* ap = refAlign->alignSeq;

/* Calculate per-base stats */
refAlign->matches = 0;
refAlign->misMatches = 0;
refAlign->aNumInsert = 0;
refAlign->aBaseInsert = 0;
refAlign->hNumInsert = 0;
refAlign->hBaseInsert = 0;

for (; *hp != '\0'; hp++, ap++)
    {
    if (*hp == '-')
        {
        int incr = countInserts(ap, hp, &refAlign->aNumInsert,
                                &refAlign->aBaseInsert);
        hp += incr;
        ap += incr;
        }
    else if (*ap == '-')
        {
        int incr = countInserts(hp, ap, &refAlign->hNumInsert,
                                &refAlign->hBaseInsert);
        hp += incr;
        ap += incr;
        }
    else if (*hp == *ap)
        refAlign->matches++;
    else
        refAlign->misMatches++;
    }

calcScore(refAlign);
}

static void calcStats(struct refAlign* refAlignList)
/* Calculate and fill in the statistics and scores for a list of refAlign
 * objects */
{
struct refAlign* refAlign = refAlignList;
while (refAlign != NULL)
    {
    calcRecStats(refAlign);
    refAlign = refAlign->next;
    }
}

#if 0
static void loadDatabase(char* database,
                         char* table,
                         struct traceInfo* traceInfoList)
/* Load the trace information into the database. */
{
struct sqlConnection *conn = NULL;
FILE *tmpFh = NULL; 
struct traceInfo* trace = NULL;
char query[512];

/* Write it as tab-delimited file. */
printf("Writing tab-delimited file %s\n", TMP_TAB_FILE);
tmpFh = mustOpen(TMP_TAB_FILE, "w");

for (trace = traceInfoList; trace != NULL; trace = trace->next)
    {
    traceInfoTabOut(trace, tmpFh);
    }
carefulClose(&tmpFh);

/* Import into database. */
printf("Importing into %d rows into %s\n",
       slCount(traceInfoList), database);
conn = sqlConnect(database);
if (!sqlTableExists(conn, table))
    errAbort("You need to create the table first (with %s.sql)", table);

sprintf(query, "delete from %s", table);
sqlUpdate(conn, query);
sprintf(query, "load data local infile '%s' into table %s", TMP_TAB_FILE,
        table);
sqlUpdate(conn, query);
sqlDisconnect(&conn);
unlink(TMP_TAB_FILE);
printf("Import complete\n");
}
#endif

static void usage()
/* Explain usage and exit. */
{
errAbort(
"hgRefAlign - Load a reference alignment into a table.\n"
  "usage:\n"
  "   hgRefAlign format table alignfile ...\n"
  "the only currently support format is `webb'");
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct refAlign* refAlignList = NULL;
setlinebuf(stdout); 
setlinebuf(stderr); 

if (argc < 4)
    usage();

if (strcmp(argv[1], "webb") == 0)
    refAlignList = parseWebbFiles(argc-3, argv+3);
else
    errAbort("unknown alignment file format \"%s\"", argv[1]);

calcStats(refAlignList);

#if 0
loadDatabase(argv[1], argv[2], traceInfoList);
#endif
slFreeList(&refAlignList);
return 0;
}
