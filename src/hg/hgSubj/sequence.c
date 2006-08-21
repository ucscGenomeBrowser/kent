/* sequence - do Sequence section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "spDb.h"
#include "hgSubj.h"
#include "hdb.h"
#include "net.h"

static char const rcsid[] = "$Id: sequence.c,v 1.1 2006/08/21 22:06:40 fanhsu Exp $";

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

/* !!! to be updated later */

char *getSeqId(char *subjId)
{
char *chp;
chp = strstr(subjId, "GSID") +5;
return(strdup(chp));
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

seqId = getSeqId(subjId);
printf("<B>DNA Sequences</B><BR>");
safef(query, sizeof(query), "select * from dnaSeq where id like '%c%s%c' order by id", 
     '%',  seqId, '%');
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL) printf("<BR>Not available.<BR><BR>");

while (row != NULL) 
    {
    seqId    = row[0];
    seq	     = row[1];

    l =strlen(seq);
    hPrintf("<pre>\n");
    hPrintf("%c%s", '>', seqId);
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

seqId = getSeqId(subjId);
printf("<B>Protein Sequences</B><BR>");
safef(query, sizeof(query), "select * from aaSeq where id like '%c%s%c' order by id", '%', seqId, '%');
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL) printf("<BR>Not available.<BR>");
    
while (row != NULL) 
    {
    seqId    = row[0];
    seq	     = row[1];

    l =strlen(seq);
    hPrintf("<pre>\n");
    hPrintf("%c%s", '>', seqId);
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

