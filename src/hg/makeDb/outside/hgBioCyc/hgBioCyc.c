/* hgBioCyc - Creates bioCycPathway.tab for Known Genes to link to SRI BioCyc pathways */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

FILE *inf;
char line_in[10000];
char line[10000];
char *infileName;

int aaLen;
int cdsLen;
char cond_str[256];
char *aaStr;

void usage()
/* Explain usage and exit. */
{   
errAbort(
  "bioCyc - Creates bioCycPathway.tab for Known Genes to link to SRI BioCyc pathways\n"
  "usage:\n"
  "   hgBoCyc xxxx yyyy\n"
  "      xxxx is the input file name\n"
  "      yyyy is the genome under construction\n"
  "example:\n"
  "   hgBioCyc gene-pathway.dat hg16\n");
}
    
int main(int argc, char *argv[])
{
struct sqlConnection *conn;
FILE *o1;
char *chp0, *chp;
char *genomeDBname;
char refseqID[40], mapID[40];
char *kgID, *geneSymbol;
 
if (argc != 3) usage();
  
infileName      = argv[1];
genomeDBname    = argv[2];

conn= hAllocConn(genomeDBname);
o1 = fopen("j.dat", "w");

inf  = mustOpen(infileName, "r");
while (fgets(line_in, 1000, inf) != NULL)
    {
    strcpy(line, line_in);
    chp = strstr(line, "\t");
    *chp = '\0';
    strcpy(refseqID, line);

again:
    chp ++;
    chp0 = chp;
    chp = strtok(chp, "\r\t\n");	

    if (chp == NULL) continue;

    sprintf(cond_str, "alias='%s'", refseqID);
    kgID=sqlGetField(genomeDBname, "kgAlias", "kgID", cond_str);

    // check with refLink if not found in kgAlias
    if (kgID == NULL)
	{
    	sprintf(cond_str, "mrnaAcc='%s'", refseqID);
    	geneSymbol=sqlGetField(genomeDBname, "refLink", "name", cond_str);
    	sprintf(cond_str, "alias='%s'", geneSymbol);
    	kgID=sqlGetField(genomeDBname, "kgAlias", "kgID", cond_str);
	}

    strcpy(mapID, chp);
   
    if (kgID != NULL)
	{ 
    	fprintf(o1, "%s\t%s\t%s\n", kgID, refseqID, mapID);fflush(stdout);
    	}
    else
	{
	printf("%s not found in kgAlias nor in refLink\n", refseqID);
	}
    chp = chp + strlen(mapID);
    
    // process remaing refeqID(s)
    goto again;
    }

fclose(o1);
system("cat j.dat|sort|uniq >bioCycPathway.tab");
system("rm j.dat");
return 0;
}

