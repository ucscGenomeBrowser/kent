/* spOrganism - Produce a .tab file of SWISS-PROT display ID/NCBI taxonomy ID pairs */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "spOrganism - Extract taxonomy data from SWISS-PROT data file and \n"
  " produce a .tab file of SWISS-PROT display ID/NCBI taxonomy ID pairs.\n" 
  "usage:\n"
  "   spOrganism xxxx\n"
  "      xxxx is the input  file name\n"
  "      yyyy is the output file name\n"
  "example: spOrganism /cluster/store5/swissprot/070403/sprot.dat oxSP.tab\n");
}

int main(int argc, char *argv[])
{
char *id;
char *ox;
char *chp;
char *infName, *outfName;

char line[2000];
FILE *inf, *outf;

if (argc!=3)
   {
   usage();
   }

infName  = argv[1];
outfName = argv[2];

if ((inf = fopen(infName, "r")) == NULL)
    {		
    fprintf(stderr, "Can't open file %s.\n", infName);
    exit(8);
    }

outf = fopen(outfName, "w");

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

    again:
    if (fgets(line, 1000, inf) == NULL) break;

    /* "//" is the end of record line */	
    if ((line[0] == '/') && (line[1] == '/')) goto one_done;

    chp = strstr(line, "OX   ");
    if (chp != NULL)
	{
	chp = strstr(line, "NCBI_TaxID=");
	ox  = chp + strlen("NCBI_TaxID=");

	again1:
	chp = strstr(ox, ",");
	if (chp != NULL)
	    {
	    *chp='\0';
	    while (*ox == ' ') ox++;
	    fprintf(outf, "%s\t%s\n", id, ox);
 	    chp++;
	    ox = chp;	
	    if (*ox == '\n') 
		{
		fgets(line, 1000, inf);
		chp = strstr(line, "OX   ");
		if (chp == NULL)
		    {
		    fprintf(stderr, "no OX line after OX continuation line!\n");
		    exit(1);
	 	    }
		ox  = line + strlen("OX   ");
		goto again1;
	    	}	
	    }
        else
	    {
	    chp = strstr(ox, ";");
	    if (chp != NULL)
	    	{
	    	*chp = '\0';
	    	while (*ox == ' ') ox++;
	    	fprintf(outf, "%s\t%s\n", id, ox);
	    	ox = NULL;
	    	}	
		}
    	if (ox != NULL) goto again1;
    	}
    goto again;
    one_done: id = id;
    }
fclose(outf);
return 0;
}
