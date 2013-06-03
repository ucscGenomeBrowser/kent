/* omimParseAv - parse the AV fields of OMIM records */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "hdb.h"
#include "options.h"
#include "localmem.h"
#include "dystring.h"
#include "portable.h"
#include "obscure.h"


boolean avDebug = FALSE;

char buffer[80*5000];

char *fieldType;
FILE *omimAvPos;

char empty[1] = {""};

struct omimSubField
/* An OMIM sub field. */
    {
    char *omimId;       /* OMIM ID */
    char *fieldType;    /* field type */
    char *subFieldId;	/* sub-field ID */
    int  startPos;	/* offset of starting position */
    int  subFieldLen;	/* length of text of the sub-field */
    char *text;		/* text of the sub-field */
    };

char aaCode3[20][4] = {
"ALA",
"VAL",
"LEU",
"ILE",
"PRO",
"PHE",
"TRP",
"MET",
"GLY",
"SER",
"THR",
"CYS",
"TYR",
"ASN",
"GLN",
"ASP",
"GLU",
"LYS",
"ARG",
"HIS"
};

char aaCode1[20][2] = {
"A",
"V",
"L",
"I",
"P",
"F",
"W",
"M",
"G",
"S",
"T",
"C",
"Y",
"N",
"Q",
"D",
"E",
"K",
"R",
"H"
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "omimParseAv - parse the AV fields of OMIM records \n"
  "usage:\n"
  "   omimParseAv omimDb omimTextFile avSubOutFile avPosOutFile\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

boolean findAaSub(char *lineIn, struct omimSubField *subFd, FILE *avPosFh)
/* check the input line to see if it contains a patern of AA substitution (AAAnnAAA) 
   and print out a record if it does. */
{
char *words[300];
char inStr[300];
int  wcnt;

char *chp, *chpa, *chp1, *chp2;
char ch;
char aa1 = ' ';
char aa2 = ' ';

int avPos;
int aaCnt;
int i, j;

boolean posValid;
boolean result;

result = FALSE;

/* chop the line into words */
safef(inStr, sizeof(inStr), "%s", lineIn);
wcnt = chopString(inStr, " ,", words, 300);

/* check each word */
for (j=0; j<wcnt; j++)
    {
    aaCnt = 0;
    chp1 = chp2 = NULL;
    
    /* check each AA */
    for (i=0; i<20; i++)
	{
	chp = words[j];
	chpa = strstr(words[j], aaCode3[i]);
	if (chpa != NULL) 
	    {
	    aaCnt++;
	    /* remember first AA */
	    if (aaCnt == 1) 
	    	{
		aa1  = *aaCode1[i];
		chp1 = chpa;
		}
	    /* remember 2nd AA */	
	    if (aaCnt == 2) 
	    	{
		aa2  = *aaCode1[i];
		chp2 = chpa;
		}
	    }
	}

    /* if the word contains 2 AAs, do further checking */
    if (aaCnt == 2) 
    	{
	/* switch chp1 and chp2 if chp1 goes after chp2 */
	if (chp1 > chp2) 
	    {
	    ch   = aa1;
	    chp  = chp1;
	    
	    chp1 = chp2;
	    chp2 = chp;
	    
	    aa1  = aa2;
	    aa2  = ch;
	    }
	    
	/* check if the characters between two AAs are all digits */    
	chp = chp1 + 3;
	posValid = TRUE;
	while (chp < chp2)
	    {
	    if (!isdigit(*chp))
	    	{
		//fprintf(stderr, 
		//"%s in '%s' is not a valid AA substution string.\n", words[j], lineIn);
		posValid = FALSE;
		}
	    chp ++;
	    }
	    
	ch = *chp2;
	*chp2 = '\0';
	avPos = atoi(chp1+3);
	*chp2 = ch;
	if (posValid) 
	    {
	    fprintf(avPosFh, 
	    	    "%s\t%s\t%s\t%d\t%c\t%c\n", 
		    subFd->omimId, subFd->subFieldId, words[j], avPos, aa1, aa2);
	    fflush(avPosFh);
	    result = TRUE;
	    return(result);
	    }
	}
    }
    
return(result);
}
 
struct omimSubField *newSubFd(struct lm *lm, char *omimId, char *fieldType)
/* allocat and initialize a new omimSubField object */
{
struct omimSubField *subFd;

lmAllocVar(lm, subFd);
subFd->omimId    = omimId;
subFd->fieldType = fieldType;
return(subFd);
}

void printSubField(struct omimSubField *subFd, FILE *fh2, FILE *avSubFh)
/* print out the sub-field data */
{
int bytesRead;

fseek(fh2, (long)(subFd->startPos), SEEK_SET);
if (subFd->subFieldLen >= sizeof(buffer))
    {
    fprintf(stderr, "Subfield %s %s length %d exceeds the size of buffer %d.\n",
             subFd->omimId, subFd->subFieldId, subFd->subFieldLen, (int)sizeof(buffer));
    exit(1);
    }
bytesRead = (long)fread(buffer, (size_t)1, (size_t)(subFd->subFieldLen), fh2);
*(buffer + subFd->subFieldLen) = '\0';

fprintf(avSubFh, "%s\t%s\t%s\t%d\t%d\n", 
       subFd->omimId, subFd->fieldType, subFd->subFieldId, subFd->startPos, subFd->subFieldLen);

if (avDebug) 
    {
    printf("%s\t%s--->\n%s<---\n", subFd->omimId, subFd->subFieldId, buffer);fflush(stdout);
    }
}

void omimParseAv(char *omimDb, char *omimTxtFileName, FILE *fh2, FILE *avSubFh, FILE *outFh)
{
struct sqlConnection *conn2, *conn3, *conn4;
 
char query2[256], query3[256];
struct sqlResult *sr2, *sr3;
char **row2, **row3;

char *omimId, *fieldType;
char *startPos, *fieldLength;
long bytesRead;

char *line;
int lineSize;
char *chp, *chp2;
boolean avPosFound = FALSE;
size_t iStart, fieldLen;
int subLen = 0;
boolean entryProcessed;

/* sub-field start and end position */
int  subFdStartPos = 0;
int  subFdEndPos   = 0;	

char *subFieldText;
char *subAvNum;
struct lm *lm;

struct omimSubField *subFd;

struct lineFile *lf = lineFileOpen(omimTxtFileName, TRUE);

conn2= hAllocConn();
	
/* loop thru all recordd in the omimField table */
sqlSafef(query2, sizeof query2, "select * from %s.omimField where fieldType='AV'", omimDb);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    omimId 	= row2[0];
    fieldType   = row2[1];
    startPos    = row2[2];
    fieldLength	= row2[3];

    lm = lmInit(8*1024);
    lmAllocVar(lm, subFd);
    
    iStart  	= (size_t)atoi(startPos);
    fieldLen	= (size_t)atoi(fieldLength);
    
    lineFileSeek(lf, iStart, SEEK_SET);
    entryProcessed = FALSE;

    subAvNum = NULL;
    while (!entryProcessed)
    	{
        if (!lineFileNext(lf, &line, &lineSize)) 
	    {
	    fprintf(stderr, "Premature end of file reached.\n");
	    exit(1);
	    }
        
	/* check if the line is the starting of a sub AV entry */
        if (*line == '.') 
    	    {
	    if (subAvNum != NULL)
	    	{
	    	subFdEndPos = lf->lineStart + lf->bufOffsetInFile - 1;
		subLen = subFdEndPos - subFdStartPos;
		
		subFd->subFieldLen = subLen;
		printSubField(subFd, fh2, avSubFh);
		}
	    subAvNum = strdup(line+1);
    	    subFd = newSubFd(lm, omimId, fieldType);
	
	    subFd->subFieldId = subAvNum;
	    subFdStartPos = lf->lineEnd + lf->bufOffsetInFile;
	    subFd->startPos = subFdStartPos;
	    
	    /* reset the boolean flag, once the ".0000xx" sub AV line found */ 
	    avPosFound = FALSE;
	    }

    	/* search for the pattern, only if it is not found for a sub entry yet */
    	if (!avPosFound)
    	    {
	    avPosFound = findAaSub(line, subFd, outFh);
	    }
    	
	if ((lf->lineStart + lf->bufOffsetInFile) >= (iStart + fieldLen - 1))
	    {
	    entryProcessed = TRUE;
	    subFdEndPos = lf->lineStart + lf->bufOffsetInFile - 1;
	    subLen = subFdEndPos - subFdStartPos;
	    
	    subFd->subFieldLen = subLen;
	    printSubField(subFd, fh2, avSubFh);
	    }
	}
    lmCleanup(&lm);
    
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);
hFreeConn(&conn2);
}

int main(int argc, char *argv[])
/* Process command line. */
{
FILE *omimAvSubFh, *omimAvPosFh, *fh2;

optionInit(&argc, argv, options);
if (argc != 5)
    usage();

fh2 = mustOpen(argv[2], "r");
omimAvSubFh = mustOpen(argv[3], "w");
omimAvPosFh = mustOpen(argv[4], "w");

omimParseAv(argv[1], argv[2], fh2, omimAvSubFh, omimAvPosFh);

fclose(omimAvPosFh);

return 0;
}
