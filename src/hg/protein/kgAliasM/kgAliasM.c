/* kgAliasM - generate alias list table (the mRNA part, there is another protein part) */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgAliasM - create gene alias (mRNA part) .tab files "
  "usage:\n"
  "   kgAliasM xxxx yyyy\n"
  "            xxxx is genome  database name\n"
  "            yyyy is protein database name \n"
  "example: kgAliasM hg15 proteins0405\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2;

char query2[256];
struct sqlResult *sr2;
char **row2;
    
char *chp0, *chp;
char *kgID;
FILE *o1, *o2;
char cond_str[256];
char *database;
char *proteinDB;
boolean doingAlias, bothDone;

char *answer;
char *symbol, *alias, *aliases;

if (argc != 3) usage();
database  = cloneString(argv[1]);
proteinDB = cloneString(argv[2]);

conn = hAllocConn(database);
conn2= hAllocConn(database);
o1 = fopen("j.dat", "w");
o2 = fopen("jj.dat", "w");

doingAlias = TRUE;
bothDone   = FALSE;

while (!bothDone)
    {
    if (doingAlias)
	{
    	sprintf(query2,"select symbol, aliases from %s.hgnc;", proteinDB);
	}
    else
	{
        sprintf(query2,"select symbol, prvSymbols from %s.hgnc;", proteinDB);
    	}
    
    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    while (row2 != NULL)
	{
	symbol		= row2[0];
	aliases		= row2[1];

	if ( (symbol  != NULL) && (strlen(symbol) != 0) )
	    {
            sprintf(cond_str, "geneSymbol = '%s'", symbol);
            answer = sqlGetField(database, "kgXref", "kgID", cond_str);
	    if (answer != NULL)
		{
		kgID = strdup(answer);
		fprintf(o2, "%s\t%s\n", kgID, symbol);
		}
	    if ( (aliases  != NULL) && (strlen(aliases) != 0) && (answer != NULL) )
		{
		kgID = strdup(answer);
    
		chp0 = aliases; 
	    	while (chp0 != NULL)
		    {
		    while (*chp0 == ' ') chp0++;
		    chp = strstr(chp0, ",");
		    if (chp == NULL)
			{
			alias = strdup(chp0);
			
			/* get rid of quote character in some aliases */
			if (*alias == '"') 
			    {
			    *(alias + strlen(alias) - 1) = '\0';
			    alias++;
			    printf("%s\n", alias);fflush(stdout);
			    }
			chp0 = NULL;
			}
		    else
			{
			*chp = '\0';
			
			/* get rid of quote character in some aliases */
			if (*chp0 == '"') 
			    {
			    *(chp0 + strlen(chp0) - 1) = '\0';
			    chp0++;
			    printf("%s\n", chp0);fflush(stdout);
			    }
			alias = strdup(chp0);
			chp0 = chp+1;
			}
		    if (kgID != NULL)
			{
			fprintf(o1, "%s\t%s\t%s\n", kgID, symbol, alias);
			fprintf(o2, "%s\t%s\n", kgID, alias);
			}
		    }
		}
	    }
	row2 = sqlNextRow(sr2);
	}
    sqlFreeResult(&sr2);

    if (doingAlias) 
	{
	doingAlias = FALSE;
	}
    else
	{
	bothDone = TRUE;
	}
    }
fclose(o1);
fclose(o2);

/* geneAlias.tab has 3 columns, the 2nd is HUGO.symbol 
   and 3rd contains aliases and withdraws */

system("cat  j.dat|sort|uniq  >geneAlias.tab");

/*  kgAliasM.tab has 2 columns, all entries from HUGO.symbol, HUGO.aliass, 
    and HUGO.withdraws are listed in the 2nd column. */
system("cat jj.dat|sort|uniq  >kgAliasM.tab");
system("rm j.dat");
system("rm jj.dat");
    
return(0);
}
