/* parsOmimDisorder - This is a one shot program used in the OMIM related subtracks build pipeline */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "parsOmimDisorder - This program is part of the OMIM related subtracks build pipeline.\n"
  "             It parses the uniprot_id  col of the gene records of the raw RGD gene data file, GENES_RAT,\n"
  "             and create a xref table between RGD gene ID and UniProt ID.\n"
  "usage:\n"
  "   parsOmimDisorder db outFileName\n"
  "      db is the database name\n"
  "      outFileName is the filename of the output file\n"
  "example: parsOmimDisorder hg19 omimDisorderPhenotype.tab\n");
}

char notAvailable[10] = {"-1"};

char *omimId;
char *geneSymbols, *location;
boolean questionable;
boolean hasBracket;
boolean hasBrace;

FILE   *outf;

int parseDisorderText(char *disorderText)
{
char *chp1;
char *chp8;
char *chp9;
char *phenotype, phenotypeClass;
char inText[1024];

strcpy(inText, disorderText);

chp1 = disorderText;
chp9 = disorderText+ strlen(disorderText);

while (*chp9 != ')') chp9 --;
chp9 --;

if ((*chp9 == '1') || (*chp9 == '2') ||(*chp9 == '3') || (*chp9 == '4') )
    {
    phenotypeClass = *chp9;
    }
else
    {
    fprintf(stderr, "error: %s|%s\n", omimId, inText);
    exit(1);
    }

chp9 --;
while (*chp9 != '(')
    {
    chp9--;
    if (chp9 == disorderText)
    	{
    	fprintf(stderr, "error: %s|%s\n", omimId, disorderText);
    	exit(1);
    	}
    }
*chp9 = '\0';
chp9--;

while (*chp9 == ' ')chp9--;
if (*chp9 == ',')   chp9--;

/* search for a phenotype number */

phenotype = NULL;

if ((*chp9 == ']') || (*chp9 == '}') || (*chp9 == ',') || (chp9 == disorderText))
	{
    	phenotype = notAvailable;
	if  (chp9 != disorderText)
	    {
	    *chp9 = '\0';
	    }
	goto skipped;
	}

if (!isdigit(*chp9)) 
    {
    phenotype = notAvailable;
    chp9++;
    *chp9 = '\0';
    goto skipped;
    }

while (isdigit(*chp9)) chp9--;
chp9++;

// sometimes a number (shorter than 5 digits) shows up 
// at the end of disorder description, it is not a phenotype ID
if (strlen(chp9) < 5)
    {
    //printf("skipping %s\n", chp9);fflush(stdout);
    phenotype = notAvailable;
    chp9++;
    *chp9 = '\0';
    goto skipped;
    }

phenotype = chp9;
chp9--;

// added extra logic here to check for the situation where there is no ',' in front of a
// potential phenotype number

chp8 = chp9;

checkComma:
chp8--;
if (*chp8 != ',')
    {
    // sometimes there are two blanks
    if (*chp8 == ' ')
    	{
	goto checkComma;
	}
    else
    	{
    	//printf("***--%c-- %s\n", *chp8, phenotype);fflush(stdout);
    	// if there is no ',' infront of a phenotype number, then it is not a true phenotype ID
	phenotype = notAvailable;
    	chp9++;
    	*chp9 = '\0';
    	goto skipped;
	}
    }

*chp9 = '\0';
skipped:
chp9--;
while (*chp9 == ' ')
    {
    *chp9 = '\0';
    chp9--;
    }
while (*chp9 == ',')
    {
    *chp9 = '\0';
    chp9--;
    }

if (*chp9 == '}') *chp9 = '\0';
if (*chp9 == ']') *chp9 = '\0';

fprintf(outf, "%s\t%c\t%d\t%d\t%d\t%s\t%s\n", 
omimId, phenotypeClass, questionable, hasBracket, hasBrace, phenotype, disorderText);fflush(stdout);
return(0);
}

int main(int argc, char *argv[])
{
char *database;
char *outFn;

struct sqlConnection *conn2;
char query2[256];
struct sqlResult *sr2;
char **row2;

char *chp1, *chp2, *chp9;
char *disorderText;

if (argc != 3) usage();

database = argv[1];
conn2= hAllocConn(database);

outFn   = argv[2];
outf    = mustOpen(outFn, "w");

sprintf(query2,
"select omimId, disorder, geneSymbols, location, questionable from omimDisorderMap");
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    omimId = row2[0];
    disorderText = row2[1];
    geneSymbols  = row2[2];
    location     = row2[3];
    questionable  = atoi(row2[4]);

    chp1 = disorderText;
    chp9 = disorderText + strlen(disorderText);

    hasBracket = FALSE;
    if (*chp1 == '[') 
    	{
	hasBracket = TRUE;
	chp1++;
	chp2 = chp1 + strlen(chp1);
	while (*chp2 != '[') chp2 --;
	*chp2 = '\0';
	}

    hasBrace = FALSE;
    if (*chp1 == '{') 
    	{
	hasBrace = TRUE;
	chp1++;
	chp2 = chp1 + strlen(chp1);
	while (*chp2 != '{') chp2 --;
	*chp2 = '\0';
	}
    if (*chp1 == '?') 
    	{
	questionable = TRUE;
	chp1++;
	}

    parseDisorderText(chp1);

    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

fclose(outf);
hFreeConn(&conn2);
return(0);
}
