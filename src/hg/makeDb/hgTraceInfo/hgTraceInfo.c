/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/*
 * Program to parse mouse read FASTA files and created a table of trace id
 * (ti) to template id.  This is a subset of the trace ancillary information.
 * If more of this is required, then it probably makes sense to get either
 * that tab seperate or XML files and make this a more complete subset of this
 * information.
 *
 * Loads trace files obtained from:
 *   ftp://ftp.ncbi.nlm.nih.gov/pub/TraceDB/
 * See doc at:
 * http://www.ncbi.nlm.nih.gov/Traces/Docs/TraceDoc.html
 * http://www.ncbi.nlm.nih.gov/Traces/TraceArchiveRFC.html
 *
 *
 */
#include "common.h"
#include "errAbort.h"
#include "jksql.h"
#include "dnautil.h"
#include "fa.h"
#include "traceInfo.h"


static char* TMP_TAB_FILE = "traceInfo.tab";
static char* TRACE_ID_PREFIX = "gnl|";

static char* parseTraceId(char* seqId, char* fastaName)
/* Convert the sequence id from a mouse trace fasta file to a trace id.
 * Returns a dynamic string. */
{
if (strncmp(seqId, TRACE_ID_PREFIX, strlen(TRACE_ID_PREFIX)) == 0)
    return cloneString(seqId+4);
else
    errAbort("sequence name does not start with \"%s\", got \"%s\" in %s",
             TRACE_ID_PREFIX, seqId, fastaName);
return NULL;
}

static char* parseTemplateId(char* comment, char* fastaName)
/* Parse the template id out of the FASTA comment.  The comment contains 
 * the trace name, which is the template name with `.something' appended.
  Returns a dynamic string. */
{
char *words[2];
int numWords;
char* templateId;
char* chPtr;

/* parse, e.g. `gnl|ti|1 G10P69425RC6.T0' */
numWords = chopByWhite(comment, words, 2);
if (numWords < 2)
    errAbort("Can't parse trace id for \"%s\": %s", comment, fastaName);
templateId = words[1];

/* remove suffix, e.g. `.T0' */
chPtr = strchr(templateId, '.');
if (chPtr != NULL)
    *chPtr = '\0';

return cloneString(words[1]);
}

static struct traceInfo* parseFastaRecord(FILE* fh, char* fastaName)
/* read the next fasta record akd create a traceInfo object.  This
 * parses the sequence id and comment for the read and clone name. */
{
struct dnaSeq* dna;
char* comment;
struct traceInfo* traceInfo;

if (!faReadNext(fh, NULL, 0, &comment, &dna))
    return NULL; /* EOF */

AllocVar(traceInfo);
traceInfo->ti = parseTraceId(dna->name, fastaName);
traceInfo->size = dna->size;
traceInfo->templateId = parseTemplateId(comment, fastaName);

freeMem(comment);
freeDnaSeq(&dna);
return traceInfo;
}
                            
static void parseFasta(char *fastaName,
                       struct traceInfo** traceInfoList)
/* Read a mouse FASTA and parse read and clone ids */
{
FILE* fh;
static struct traceInfo* traceInfo;

printf("Parsing %s\n", fastaName);

fh = mustOpen(fastaName, "r");

while ((traceInfo = parseFastaRecord(fh, fastaName)) != NULL)
    slAddHead(traceInfoList, traceInfo);

carefulClose(&fh);
}

static struct traceInfo* readFastaFiles(int numFiles, char **fastaFiles)
/* read data for all fasta files */
{
int i;
struct traceInfo* traceInfoList = NULL;
for (i = 0; i < numFiles; i++)
    {
    parseFasta(fastaFiles[i], &traceInfoList);
    }

return traceInfoList;
}

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

sqlSafef(query, sizeof query, "delete from %s", table);
sqlUpdate(conn, query);
sqlSafef(query, sizeof query, "load data local infile '%s' into table %s", TMP_TAB_FILE,
        table);
sqlUpdate(conn, query);
sqlDisconnect(&conn);
unlink(TMP_TAB_FILE);
printf("Import complete\n");
}

static void usage()
/* Explain usage and exit. */
{
errAbort(
"hgTraceInfo - import subset of mouse trace ancillary information\n"
"parsed from FASTA files\n"
  "usage:\n"
  "   hgTraceInfo database table fa1 [...]\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct traceInfo* traceInfoList;
setlinebuf(stdout); 
setlinebuf(stderr); 

if (argc < 4)
    usage();

traceInfoList = readFastaFiles(argc-3, argv+3);
loadDatabase(argv[1], argv[2], traceInfoList);
slFreeList(&traceInfoList);
return 0;
}
