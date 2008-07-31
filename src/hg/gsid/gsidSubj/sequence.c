/* sequence - do Sequence section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "spDb.h"
#include "gsidSubj.h"
#include "hdb.h"
#include "net.h"
#include "hPrint.h"

static char const rcsid[] = "$Id: sequence.c,v 1.7 2008/07/31 21:59:22 fanhsu Exp $";

static boolean sequenceExists(struct section *section, 
	struct sqlConnection *conn, char *subjId)
/* Return TRUE if sequenceAll table exists and it has an entry with the gene symbol */
{
if (sqlTableExists(conn, "dnaSeq") == TRUE)
    {
    return(TRUE);
    }
return(FALSE);
}

static void sequencePrint(struct section *section, 
	struct sqlConnection *conn, char *subjId)
/* Print out Sequence section. */
{
char query[256];
struct sqlResult *sr;
char **row;
char *seq, *seqId;

int i, l;
char *chp;

printf("<B>gp120 DNA Sequences</B><BR>");
safef(query, sizeof(query), 
      "select dnaSeqId, seq from gsIdXref, dnaSeq where subjId = '%s' and id = dnaSeqId order by dnaSeqId", subjId);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL) printf("<BR>Not available.<BR><BR>");

while (row != NULL) 
    {
    seqId    = row[0];
    seq	     = row[1];

    l =strlen(seq);
    hPrintf("<A NAME=\"%s\">\n", seqId);
    hPrintf("<pre>\n");
    hPrintf("%c%s", '>', seqId);
    hPrintf("%s%s", ":", subjId);
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

printf("<B>gp120 Protein Sequences</B><BR>");
safef(query, sizeof(query), 
      "select aaSeqId, seq from gsIdXref, aaSeq where subjId = '%s' and aaSeqId = id order by aaSeqId", subjId);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL) printf("<BR>Not available.<BR>");
    
while (row != NULL) 
    {
    seqId    = row[0];
    seq	     = row[1];

    l =strlen(seq);
    hPrintf("<A NAME=\"%s\">\n", seqId);
    hPrintf("<pre>\n");
    hPrintf("%c%s", '>', seqId);
    hPrintf("%c%s", '|', subjId);
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
hFreeConn(&conn);
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

