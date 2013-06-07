/* sequence - do Sequence section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "spDb.h"
#include "gisaidSample.h"
#include "hdb.h"
#include "net.h"
#include "hPrint.h"


static boolean sequenceExists(struct section *section, 
	struct sqlConnection *conn, char *sampleId)
/* Return TRUE if sequenceAll table exists and it has an entry with the gene symbol */
{
if (sqlTableExists(conn, "dnaSeq") == TRUE)
    {
    return(TRUE);
    }
return(FALSE);
}

static void sequencePrint(struct section *section, 
	struct sqlConnection *conn, char *sampleId)
/* Print out Sequence section. */
{
char query[256];
struct sqlResult *sr;
char **row;
char *seq, *seqId;

int i, l;
char *chp;
char *subjId, *gene;

printf("<B>DNA Sequences</B><BR>");
sqlSafef(query, sizeof(query), 
      "select seqId, seq, strain, geneSymbol from h1n1SeqXref, dnaSeq where islId = '%s' and id = seqId order by seqId", sampleId);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL) printf("<BR>Not available.<BR><BR>");

while (row != NULL) 
    {
    seqId    = row[0];
    seq	     = row[1];
    subjId   = row[2];
    gene     = row[3];

    l =strlen(seq);
    hPrintf("<A NAME=\"%s\">\n", seqId);
    hPrintf("<pre>\n");
    hPrintf("%c%s", '>', seqId);
    hPrintf("%s%s", " | ", sampleId);
    hPrintf("%s%s", " | ", subjId);
    hPrintf("%s%s", " | ", gene);
    chp = seq;
    for (i=0; i<l; i++)
	{
	if ((i%50) == 0) hPrintf("\n");
	hPrintf("%c", *chp);
	chp++;
	}
    hPrintf("</pre>");
    fflush(stdout);
    
    row = sqlNextRow(sr);
    }
sqlFreeResult(&sr);

printf("<B>Protein Sequences</B><BR>");
sqlSafef(query, sizeof(query), 
      "select aaSeqId, seq, strain from h1n1SeqXref i, gisaidXref, aaSeq where islId = '%s' and i.seqId=dnaSeqId and aaSeqId = id order by aaSeqId", sampleId);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL) printf("<BR>Not available.<BR>");
    
while (row != NULL) 
    {
    seqId    = row[0];
    seq	     = row[1];
    subjId   = row[2];

    l =strlen(seq);
    hPrintf("<A NAME=\"%s\">\n", seqId);

    hPrintf("<pre>\n");
    hPrintf("%c%s", '>', seqId);
    hPrintf("%s%s", " | ", sampleId);
    hPrintf("%s%s", " | ", subjId);
    //hPrintf("<A NAME=\"%s\">\n", seqId);
    //hPrintf("><A HREF=\"../cgi-bin/pbGsid?proteinID=%s\"", seqId);
    //hPrintf("%c%s", '>', seqId);
    //hPrintf("</A>");
    //hPrintf("%c%s", ':', sampleId);
    chp = seq;
    for (i=0; i<l; i++)
	{
	if ((i%50) == 0) hPrintf("\n");
	hPrintf("%c", *chp);
	chp++;
	}
    hPrintf("</pre>");
    fflush(stdout);
    
    row = sqlNextRow(sr);
    }
sqlFreeResult(&sr);
return;
}

struct section *sequenceSection(struct sqlConnection *conn, 
	struct hash *sectionRa)
/* Create sequence section. */
{
struct section *section = sectionNew(sectionRa, "sequence");
section->exists = sequenceExists;
section->print = sequencePrint;
return section;
}

