/* genFedDef - generate MySQL table definitions from regular tables to FEDERATED tables */
#include "common.h"

char line_in[10000];
char line[10000];

void usage()
/* Explain usage and exit. */
{   
errAbort(
  "genFedDef - generate MySQL table definitions from regular tables to FEDERATED tables\n"
  "usage:\n"
  "   genFedDef hostName dbName, userId, password, inFn, outFn\n"
  "example:\n"
  "   genFedDef hgwdev.csc.ucsc.edu hg18 hguser xxxpwd hg18.sql hg18Fed.sql\n");
}
    
int main(int argc, char *argv[])
{
char *hostName, *databaseName, *userId, *password;
char *tableName = NULL;
char *inFileName, *outFileName;
char *chp;

FILE *inf;
FILE *outf;
  
if (argc != 7) usage();
    
hostName  	= argv[1];
databaseName  	= argv[2];
userId   	= argv[3];
password 	= argv[4];
inFileName      = argv[5];
outFileName     = argv[6];
  
inf  = mustOpen(inFileName, "r");
outf = fopen(outFileName, "w");

while (fgets(line_in, 10000, inf) != NULL)
    {
    strcpy(line, line_in);

    chp = strstr(line_in, "CREATE TABLE ");
    if (chp != NULL)
    	{
	chp = chp + strlen("CREATE TABLE ");
	tableName = strdup(chp);
	chp = tableName;
	while (*chp != ' ') chp++;
	*chp = 0;
	}
    
    chp = strstr(line_in, "MyISAM");
    if (chp == NULL)
    	{
    	fprintf(outf, "%s", line);
	}
    else
    	{
	fprintf(outf, ") ENGINE=FEDERATED\n");
	fprintf(outf, "CONNECTION=\'mysql://%s:%s@%s/%s/%s\';\n", userId, password, hostName, databaseName, tableName);
	}
    }
    
return(0);
}
