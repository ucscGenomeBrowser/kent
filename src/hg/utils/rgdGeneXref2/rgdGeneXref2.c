/* rgdGeneXref2 - from all mRNAs in a genome (e.g. rn3) referenced by SWISS-PROT 
          generate a list of proteins and a list of protein/mRNA pairs */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rgdGeneXref2 - parse out Dbxref field of the raw RGD Gene GFF file to generate data for the rgdGeneXref22 table\n"
  "       generate a list of proteins and a list of protein/mRNA pairs.\n"
  "usage:\n"
  "   rgdGeneXref2 databas outfile\n"
  "      database is the genome database\n"
  "      outfile  is the output file name\n"
  "example: rgdGeneXref2 rn4 rgdGeneXref2.tab\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn;
 
char query[512];
struct sqlResult *sr;
char **row;

char *dataBase;
char *chp;
char *chp9;
char *feature, *xrefStr;
char *Dbxref;
char *DbxrefEnd = NULL;
char *rgdGeneId;
char *rest = NULL;
FILE *outf;
char *outfileName;
boolean more;

if (argc != 3) usage();
dataBase    = argv[1];
outfileName = argv[2];

outf = mustOpen(outfileName, "w");

conn= hAllocConn(dataBase);
	
sqlSafef(query, sizeof query,"select feature, rgdId from rgdGeneRaw0 where feature = 'gene'");
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
while (row != NULL)
    {
    feature 	= row[0];
    xrefStr     = row[1];

    Dbxref = row[1];
    chp9 = strstr(xrefStr, ";");
    if (chp9 != NULL) 
    	{
	*chp9 = '\0';
	DbxrefEnd = chp9;
	}

    chp = Dbxref;

    /* get start of "RGD:" */
    chp = strstr(chp, ",");
    chp ++;
    rgdGeneId = chp;
    
    /* check if there are other references beside the RGD: entry */
    more = FALSE;
    chp = strstr(rgdGeneId, ",");
    if (chp != NULL) 
    	{
	more = TRUE;
	*chp = '\0';
        chp++;
	rest = chp;
	}

    if (more)
    	{
	chp9 = strstr(rest, ",");
	while (chp9 != NULL)
	    {
	    *chp9 = '\0';
	    fprintf(outf, "%s\t%s\n", rgdGeneId, chp); fflush(stdout);
	    if (DbxrefEnd == chp9)
	    	{
		/* if end is reached, break */
		break;
		}
	    else
	    	{
		/* keep looking for next entry */
		chp9++;
		chp = chp9;
		chp9 = strstr(chp, ",");
		}
	    }
	
	/* print last entry */
	fprintf(outf, "%s\t%s\n", rgdGeneId, chp); fflush(stdout);
	}
    
    row = sqlNextRow(sr);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
fclose(outf);

return(0);
}

