/* pdbSP - create xref file for PDB and SWISS-PROT IDs */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pdbSP - create xref file for PDB and SWISS-PROT IDs\n"
  "usage:\n"
  "   pdbSP xxxx\n"
  "            xxxx is protein database name \n"
  "example: pdbSP proteins072003\n");
}

int main(int argc, char *argv[])
{
FILE *inf;
FILE *outf;
struct sqlConnection *conn;
char cond_str[256];
char *proteinDB;
char *displayID;
char *chp0, *chp, *chp1, *chp9;
char line[2000];
int  len;
char *pdb = NULL;
char *date;
char *spID;
char *endPos = NULL;
char *answer;

if ((inf = fopen("pdbtosp.htm", "r")) == NULL)
    {		
    fprintf(stderr, "Can't open input file pdbtosp.htm.\n");
    exit(8);
    }
    
if (argc != 2) usage();
proteinDB = cloneString(argv[1]);
conn = hAllocConn();

/* First skip to the line contains "Number of Swiss-Prot entries" */
while (fgets(line, 1000, inf) != NULL)
    {
    if (strstr(line, "Number of Swiss-Prot entries") != NULL) break;
    }

/* Then skip to the line contains "________________________" */
while (fgets(line, 1000, inf) != NULL)
    {
    if (strstr(line, "________________________") != NULL) break;
    }

outf = fopen("pdbSP.tab", "w");	
while (fgets(line, 1000, inf) != NULL)
    {
    /* an empty line signals end of data */
    if (strlen(line) < 3) break;
		
    if (isdigit(line[0]))
	{
	endPos = line + strlen(line) - 1;

	/* get pdb ID */
	chp1 = line;
	chp9 = strstr(line, " ");
	*chp9 = '\0';
	pdb = strdup(chp1);

	/* get date */
	/* New pdbtosp.txt does not have the date field any more, so skip the following lines */
	/* 
	chp = chp9 + 1;
	chp1 = strstr(chp, "(");
	chp1++;
	chp9 = strstr(chp1, ")");
	*chp9 = '\0';
	date = strdup(chp1);
	*/
	
	chp = chp9 + 1;

	more:
	chp1 = strstr(chp, "?");
	if (chp1 != NULL)
	    {
	    /* get SWISS-PROT accesion */
	    chp1++;
	    chp9 = strstr(chp1, "\"");
	    *chp9 = '\0';
	    spID = strdup(chp1);
	
	    /* get corresponding display ID from spXref3 table	*/
	    sqlSafefFrag(cond_str, "accession = '%s'", spID);
            answer = sqlGetField(conn, proteinDB, "spXref3", "displayID", cond_str);
            if (answer != NULL)
                {
                displayID = strdup(answer);
                }
	    else
		{
		displayID = NULL;
		}

	    fprintf(outf, "%s\t%s\n", pdb, displayID);fflush(stdout);

	    chp9 ++;
	    chp = strstr(chp9, ",");
	
	    /* if more, loop back */		
	    if (chp != NULL)
		{	
		len = (int)(endPos - chp);		
		    {
		    if (len > 2) goto more;
		    }
		}
	    }
	}
    else
	{
	/* process the case of spill over lines */
	
	/* get accession number */
	chp = line;
	more2:
	chp1 = strstr(chp, "?");
	chp1++;
	chp9 = strstr(chp1, "\"");
	*chp9 = '\0';
	spID = strdup(chp1);
			
	/* get display ID from spXref3 table */
	sqlSafefFrag(cond_str, "accession = '%s'", spID);
        answer = sqlGetField(conn, proteinDB, "spXref3", "displayID", cond_str);
        if (answer != NULL)
            {
            displayID = strdup(answer);
            }
	else
	    {
	    displayID = NULL;
	    }
	fprintf(outf, "t%s\t%s\n", pdb,  displayID);fflush(stdout);

	/* loop back if more */
	chp9 ++;
	chp = strstr(chp9, ",");
        if (chp != NULL)
            {
            len = (int)(endPos - chp);
            	{
                if (len > 2) goto more2;
                }
            }
	}	
   }

fclose(outf);
return(0);
}

