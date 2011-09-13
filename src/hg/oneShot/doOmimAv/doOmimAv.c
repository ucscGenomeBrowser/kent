/* doOmimAv - This is a one shot program used in the omimAvSnp build pipeline */
#include "common.h"
#include "hCommon.h"
#include "dnautil.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "doOmimAv - This program is part of the omimAvSnp build pipeline.\n"
  "usage:\n"
  "   doOmimAv db outFileName errFileName\n"
  "      db is the database name\n"
  "      outFileName is the filename of the output file\n"
  "      errFileName is the filename of the error output file\n"
  "example: doOmimAv hg19 omimAvRepl.tab j.err\n");
}

char *avId, *omimId;
FILE   *outf;
FILE   *errf;

struct aminoAcidTable *aaTable;
char aaAbbrev[21][4];
char aaChar[21];

struct aaReplacement
/* structure return information about AA replacement */
{
char firstAa;
char secondAa;
char dbSnpId[255];
int  aaPosition;
};

void aaInit()
/* initialize aaAbbrev and aaChar arrays */
{
int i,j;
for (i=0; i<21; i++)
    {
    char *chp;

    chp = aaAbbr(i);
    for (j=0; j<3; j++)
    	{
	aaAbbrev[i][j] = toupper(*chp);
	chp++;
	aaChar[i] = aaLetter(i);
	}
    aaAbbrev[i][3] = '\0';
    }
}

int processRepl(char *avId, char *omimId, char *dbSnpId, char *replIn, 
                struct aaReplacement *aaRepl, char *description)
/* process replacement string */
{
int result;
int i, j;
char *part2=NULL;
char *chp;
char repl[1024];

strncpy(repl, replIn, sizeof(repl));

result = 0;

for (i=0; i<21; i++)
    {
    chp = strstr(repl, aaAbbrev[i]);
    if (chp != NULL)
	{
	if (chp == repl)
	    {
	    aaRepl->firstAa = aaChar[i];
	    chp++;chp++;chp++;
	    part2 = chp;
	    for (j=0; j<21; j++)
	        {
		chp = strstr(part2, aaAbbrev[j]);
	    	if (chp != NULL)
	    	    {
		    char *moreSnp;

		    aaRepl->secondAa = aaChar[j];
		    *chp = '\0';

		    aaRepl->aaPosition = atoi(part2);
		    
		    if (sameWord(dbSnpId,""))
		    	{
			strcpy(aaRepl->dbSnpId, "-");
			}
		    strncpy(aaRepl->dbSnpId, dbSnpId, (size_t)255);
		   
		    /* check to see if 2 dbSnpIds specified */
		    moreSnp = strstr(aaRepl->dbSnpId, ",");
		    if (moreSnp == NULL)
		    	{
		    	fprintf(outf, 
		                "%s\t%s\t%c\t%3d\t%c\t%s\t%s\t%s\n", avId, omimId, aaRepl->firstAa, aaRepl->aaPosition, aaRepl->secondAa,  
		    	        aaRepl->dbSnpId, replIn, description);
			}
		    else
		    	{
			*moreSnp = '\0';
		    	fprintf(outf, "%s\t%s\t%c\t%3d\t%c\t%s\t%s\t%s\n", 
				avId, omimId, aaRepl->firstAa, aaRepl->aaPosition, aaRepl->secondAa, 
		    	        aaRepl->dbSnpId, replIn, description);
		        // disable the processing of subsequent non-single dbSnpID case for now, until OMIM really fix their data
			/*
			while (moreSnp != NULL)
			    {
			    char *nextSnp;
			    moreSnp ++;
			    nextSnp = moreSnp;
			    moreSnp = strstr(nextSnp, ",");
		    	    if (moreSnp != NULL) *moreSnp = '\0';
			    fprintf(outf, 
		                    "%s\t%s\t%c\t%3d\t%c\t%s\t%s\t\%s\n", 
				    avId, omimId, aaRepl->firstAa, aaRepl->aaPosition, aaRepl->secondAa, 
		    	            nextSnp, replIn, description);
			    }
			*/
			}
		    
		    result = 1;		    
		    fflush(stdout);
		    }
		}
	    }
	
	}
    }
return(result);
}

int main(int argc, char *argv[])
{
char *database;
char *outFn;
char *errFn;

struct sqlConnection *conn2;
char query2[256];
struct sqlResult *sr2;
char **row2;

struct aaReplacement aaRepl;

char *repl2;
char *chp;
char *dbSnpId;
char *desc;

boolean successful;

if (argc != 4) usage();

aaInit();

database = argv[1];
conn2= hAllocConn(database);

outFn   = argv[2];
errFn   = argv[3];
outf    = mustOpen(outFn, "w");
errf    = mustOpen(errFn, "w");

sprintf(query2,"select avId, omimId, dbSnpId, repl2, description from omimAv");
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    avId    = row2[0];
    omimId  = row2[1];
    dbSnpId = row2[2];
    repl2   = row2[3];
    desc    = row2[4];

    if (repl2 != NULL) 
    	{
	if (!sameWord(repl2, "-")) 
		{
		chp = strstr(repl2, " AND ");
		if (chp != NULL) *chp = '\0';

		successful = processRepl(avId, omimId, dbSnpId, repl2, &aaRepl, desc);
		if (!successful) fprintf(errf, "!!! %s\t%s\n", avId, repl2);
	
		// disable the processing of subsequent non-single replacement AA string case for now, until OMIM really fix their data
		/*
		if (chp != NULL)
		   {
		   chp = chp + strlen(" AND ");
		   processRepl(avId, omimId, dbSnpId, chp, &aaRepl, desc);
		   if (!successful) fprintf(errf, "!!! %s\t%s\n", avId, chp);
		   }
		*/
		}
	}
    
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

fclose(outf);
fclose(errf);
hFreeConn(&conn2);
return(0);
}

