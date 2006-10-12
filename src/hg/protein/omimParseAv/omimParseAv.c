/* omimParseAv - parse the AV fields of OMIM records */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "dystring.h"
#include "portable.h"
#include "obscure.h"

static char const rcsid[] = "$Id: omimParseAv.c,v 1.1 2006/10/09 21:21:59 fanhsu Exp $";

char *omimID;
char *avNum;

char *fieldType;
FILE *omimAvPos;

char empty[1] = {""};

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
  "   omimParseAv avInFile avPosOutFile\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

/* check the input line to see if it contains a patern of AA substitution (AAAnnAAA) */

boolean findAaSub(char *lineIn, FILE *avPosFh)
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
		    omimID, avNum, words[j], avPos, aa1, aa2);
	    result = TRUE;
	    return(result);
	    }
	}
    }
    
return(result);
}

/* ------------- Start main program ------------------  */

void omimParseAv(char *infName, FILE *outFh)
{
FILE *inf;
char *line;
int lineSize;
char *chp, *chp2;
boolean avPosFound = FALSE;

struct lineFile *lf = lineFileOpen(infName, TRUE);

for (;;)
    {
    if (!lineFileNext(lf, &line, &lineSize)) break;

    /* check if it is the starting of a new record */
    if (*line == '*')
    	{
	avPosFound = FALSE;
    	chp = strstr(line, "***");
    	if ((chp == line) && (chp != NULL))
    	    {
	    chp = strstr(line, "\t");
	    chp++;
	    chp2 = strstr(chp, "\t");
	    *chp2 = '\0';
	    omimID = strdup(chp);
	    }
	}
	
    /* check if the line is the starting of a sub AV entry */
    if (*line == '.') 
    	{
	avNum = strdup(line+1);
	
	/* reset the boolean flag, once the ".xxxxx" sub AV line found */ 
	avPosFound = FALSE;
	}

    /* search for the pattern, only if it is not found for a sub entry yet */
    if (!avPosFound)
    	{
	avPosFound = findAaSub(line, outFh);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
int i;
FILE *omimAvPosFh;

optionInit(&argc, argv, options);
if (argc != 3)
    usage();

omimAvPosFh = mustOpen(argv[2], "w");

omimParseAv(argv[1], omimAvPosFh);

fclose(omimAvPosFh);

return 0;
}
