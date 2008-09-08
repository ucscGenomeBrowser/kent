/* gsidMsaToTab - create .tab files from MSA sequence data to DNA and protein .tab files*/

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "hdb.h"
#include "options.h"
#include "localmem.h"
#include "dystring.h"
#include "portable.h"
#include "obscure.h"

static char const rcsid[] = "$Id: gsidMsaToTab.c,v 1.2 2008/09/08 17:20:03 markd Exp $";

char *seq, *id;
char newSeq[100000];
char *database;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gsidMsaToTab - create .tab files from MSA sequence data to DNA and protein .tab files\n"
  "usage:\n"
  "   gsidMsaToTab db seqset\n"
  "      db is the database\n"
  "      seqDataSet is the sequence data set\n"
  "example: gsidMsaToTab hiv1 vax004\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2;
 
char query2[256];
struct sqlResult *sr2;
char **row2;
FILE *outf, *outf2;

char *chp, ch, *chp2;
char outFileNameDna[255];
char outFileNameAa[255];
char *seqDataSet;
int  pos;

if (argc != 3) usage();
database   = argv[1];
seqDataSet = argv[2];

conn2= hAllocConn(database);

safef(outFileNameDna, sizeof(outFileNameDna), "%sDnaSeq.tab", seqDataSet);
safef(outFileNameAa,  sizeof(outFileNameAa),  "%sAaSeq.tmp",  seqDataSet);

outf = mustOpen(outFileNameDna, "w");
outf2= mustOpen(outFileNameAa,  "w");

/* read in all MSA sequences */
sprintf(query2,"select id,seq from hiv1.%sMsa", seqDataSet);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    id  = strdup(row2[0]);
    seq = strdup(row2[1]);
    fprintf(outf2, "%s\t", id);
    chp  = seq;
    chp2 = newSeq;

    while (*chp != '\0')
	{
	/* skip "-" */
	if (*chp != '-')
	    {
	    *chp2 = *chp;
	    chp2++;
	    }
	chp++;
	}
    *chp2 = '\0';
    
    fprintf(outf, "%s\t%s\n", id, newSeq);
    chp = newSeq;
    pos = 0;
    while (*chp != '\0')
	{
	ch = lookupCodon(chp);
	if (ch == 'X')
	    {
	    *(chp+3L)= '\0';
	    fprintf(stderr, "Invalid codon %s encountered in sequence %s at position %d.\n", chp, id, pos);
	    }
	if (ch == 0)
	    {
	    *(chp+3L)= '\0';
	    fprintf(stderr, "Stop codon %s encountered in sequence %s at position %d.\n", chp, id, pos);
	    }
	fprintf(outf2, "%c", ch);fflush(stdout);
	chp++;
	chp++;
	chp++;
	pos = pos+3;
	}	
    fprintf(outf2, "\n");
    row2 = sqlNextRow(sr2);
    }

fclose(outf);
fclose(outf2);
return(0);
}
