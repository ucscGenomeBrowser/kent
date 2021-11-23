/* This program remove lines which has duplicate column 1, 
   column 1 is not included in the output.

        Usage: rmDupCol1 infile
*/

/* Copyright (C) 2010 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"

int main(argc, argv)
	int argc;
	char **argv;
{
char **p;
char *arg0, *arg1;

char line[2000];
char oldPos[20] = {""};
FILE *inf;
char *chp;
if (argc==2) 
    {
    p=argv;
    arg0=*argv;

    p++;
    arg1=*p;

    if ((inf = fopen(arg1, "r")) == NULL)
	    {		
	    fprintf(stderr, "Can't open file %s.\n", arg1);
	    exit(8);
	    }

    while (fgets(line, 1000, inf) != NULL)
	    {
	    chp = line;
	    while (*chp != '\t') chp++;
	    *chp = '\0';
	    if (strcmp(line, oldPos) != 0)
	    	{
		chp++;
		printf("%s", chp);
		safef(oldPos, sizeof(oldPos), "%s", line);
		}
	    }
    }
else
    {
    fprintf(stderr, "Usage: rmDupCol1 infile\n");
    }
return 0;
}
