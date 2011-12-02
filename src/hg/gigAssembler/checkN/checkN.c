/* This program check if a FASTA file contains pure Ns.

        Usage: checkN filenam
	
   The program echos the file name if it contains all Ns.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(argc, argv)
	int argc;
	char **argv;
{
char **p;
char *arg0, *arg1;

int i,j;

char line[2000];
FILE *inf;
int is_pure_N = 1;

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

    // skip first line of FASTA
    fgets(line, 1000, inf);

    while (fgets(line, 1000, inf) != NULL)
	    {
	    j = strlen(line) - 1;
	    for (i=0; i<j; i++)
		    {
		    if (line[i] != 'N') 
			    {
			    is_pure_N = 0;
			    exit(1);
			    }
		    }
	    }

    if (is_pure_N) 
	    {
	    printf("%s\n", arg1);
	    exit(0);
	    }
    }
else
    {
    fprintf(stderr, "Usage: checkN filename\n\nWill echo the file name if it contains all Ns.\n\n");
    }
return 0;
}
