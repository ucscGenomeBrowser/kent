/* kgAliasP read and parse the SWISS-PROT data file to generate 
   the protein part of the gene alias list for Known Genes track */

#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgAliasP - create gene alias (protein part) .tab files "
  "usage:\n"
  "   kgAliasM xxxx yyyy zzzz\n"
  "            xxxx is genome  database name\n"
  "            yyyy is protein database name\n"
  "            zzzz is .tab output file name\n"
  "Example: kgAliasP hg15 /cluster/store5/fan/data/swissprot/0405/trembl_new.dat new.tab\n");
}

int main(int argc, char *argv[])
    {
    struct sqlConnection *conn, *conn2, *conn5;
    char query[256], query2[256], query5[256];
    struct sqlResult *sr, *sr2, *sr5;
    char **row, **row2, **row5;
    char *r1, *r2, *r5;
    
    int  i;
    FILE *inf;

    FILE *o1;

    char cond_str[256];
    char *database;
    char *proteinFileName;
    char *outputFileName;
    char *answer;
    char *symbol, *alias, *aliases;

    char *id;
    char *chp0, *chp;

    char *kgID;
    char line[2000];

    if (argc != 4) usage();
    
    database         = cloneString(argv[1]);
    proteinFileName  = cloneString(argv[2]);
    outputFileName   = cloneString(argv[3]);

    conn = hAllocConn();
    conn2= hAllocConn();
	
    o1 = fopen(outputFileName, "w");
    
    if ((inf = fopen(proteinFileName, "r")) == NULL)
	{		
	fprintf(stderr, "Can't open file %s.\n", proteinFileName);
	exit(8);
	}
	
    while (fgets(line, 1000, inf) != NULL)
		{
		chp = strstr(line, "ID   ");
		if (chp != line)
			{
			fprintf(stderr, "expected ID line, but got: %s\n", line);
			exit(1);
			} 
		chp = chp + strlen("ID   ");
		id = chp;
		chp = strstr(id, " ");
		*chp = '\0';
		id = strdup(id);
        
		sprintf(cond_str, "proteinID = '%s'", id);
        	answer = sqlGetField(conn, database, "knownGene", "name", cond_str);
		kgID = NULL;
		if (answer != NULL)
			{
			kgID = strdup(answer);
			}

	again:
		if (fgets(line, 1000, inf) == NULL) break;

		/* "//" signal end of a record */		
		if ((line[0] == '/') && (line[1] == '/')) goto one_done;

		// work on GN (Gene Name) line only
		chp = strstr(line, "GN   ");
		if (chp != NULL)
			{
			chp = line + strlen(line) -2;
			if (*chp == '.') 
				{
				*chp = '\0';
				}
			else
				{
				chp++;
				*chp = '\0';
				}
			
			chp0 = line + 5;
			while (chp0 != NULL)
			    {
                    	    while (*chp0 == ' ') chp0++;
                    	    chp = strstr(chp0, " OR ");
                    	    if (chp == NULL)
                        	{
                        	alias = strdup(chp0);
                        	chp0 = NULL;
                        	}
                    	    else 
                        	{
                        	*chp = '\0';
                        	alias = strdup(chp0);
                        	chp0 = chp+4;
                        	}
 			   
			    if (kgID != NULL)
				{
			    	fprintf(o1, "%s\t%s\n", kgID, alias);
				}
			    }
			}
		goto again;
	        
	        one_done:
		}
    fclose(o1);
    }

