/* samHit reads the SAM output file, XXXX.t2k.best-scores.rdb and produce .tab data for the protHomolog table */

/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "samHit - reads the SAM output .rdb file and produce .tab data for the protHomolog table. "
  "usage:\n"
  "      samHit proteinId rdbFN\n"
  "      proteinId is the protein ID\n"
  "      rdbFN is the .rdb file name\n"
  "      ro_db is the target genome to read data from\n"
  "example:\n"
  "   samHit YNL100W /projects/compbio/experiments/protein-predict/yeast/YNL1/YNL100W.t2k.best-scores.rdb\n");
}

char *word[10];
char *previousWord[10];
char *proteinId;

char getChain(char *id)
/* parse out the last letter (if it is in upper case) as the Chain  */
    {
    int l;
    char ch, *chp;
    l=strlen(id);
    chp = id+l-1;
    if (isupper(*chp))
    	{
	ch = *chp;
	*chp = '\0';
	return(ch);
	}
    else
        {
	return('-');
	}
    }

void processOneLine(char *line)
/* process one line */
{
int i;
char *chp;
char ch;

chp = line;
for (i=0; i<7; i++)
    {
    word[i] = chp;
    chp = strstr(chp, "\t");
    if (chp != NULL) 
    	{
	*chp = '\0';
    	chp++;
	}
    }

for (i=0; i<7; i++)
    {
    if (sameWord(word[i],""))
    	{
	/* for fields that are empty, use previous ones */
	word[i] = previousWord[i];
	}
    previousWord[i] = strdup(word[i]);
    }

ch = getChain(word[0]);

if (ch == '-')
    {
    printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", proteinId, word[0], "", word[1], word[2], word[3],
	   word[4], word[5], word[6]);
    }
else
    {
    printf("%s\t%s\t%c\t%s\t%s\t%s\t%s\t%s\t%s\n", proteinId, word[0], ch, word[1], word[2], word[3],
	   word[4], word[5], word[6]);
    }

}

int main(int argc, char *argv[])
{
FILE *inf;
char line[1000];

int i;
char *infileName;

if (argc != 3) usage();

proteinId  = argv[1];
infileName = argv[2];

for (i=0; i<8; i++) previousWord[i] = strdup("n/a");
    
inf   = mustOpen(infileName, "r");

/* skip initial 2 header lines in .rdb format file */
mustGetLine(inf, line, sizeof(line));
mustGetLine(inf, line, sizeof(line));

/* read and process all lines one by one */
while (fgets(line, sizeof(line), inf) != NULL)
    {
    *(line + strlen(line) - 1) = '\0';
    processOneLine(line);
    }

return(0);
}

