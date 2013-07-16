/* fixAgp - fix those extra hap agp files */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "fixAgp - fix agp file\n"
  "usage:\n"
  "   fixAgp db inFile outFile\n"
  "      db      is the genome under construction, e.g.: hg18\n"
  "      inFile  is the input file name\n"
  "      outFile is the output file name\n"
  "example:\n"
  "   fixAgp hg18 sav/in.agp out.agp\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2;
char condStr[500];

FILE *inf;
FILE   *outf;

char line[1000];

char *chrStart;

char *inFileName, *outFileName;
char contig[100], start[100], end[100];
char num[100], code[100], id[100], oStart[100], oEnd[100], strnd[100];
char *database;

char *oldContig;
int  oldNum = 0;

int lastNum = 0;
int lastEnd = 0;

if (argc != 4)usage();

database    = argv[1];
inFileName  = argv[2];
outFileName = argv[3];

hSetDb(database);

outf = fopen(outFileName, "w");
conn2= hAllocConn();

inf   = mustOpen(inFileName, "r");

oldContig = strdup("");
while (fgets(line, 1000, inf) != NULL)
    {
    sscanf(line, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
    	   contig, start, end, num, code, id, oStart, oEnd, strnd);
    sqlSafefFrag(condStr, sizeof condStr, "ctg_acc='%s'", contig);
    chrStart =  sqlGetField(database, "seq_contig", "chr_start", condStr);
    if (!sameWord(oldContig, contig)) 
    	{
    	if (!sameWord(oldContig, ""))
    	    {
	    lastNum++;
	    fprintf(outf, "%s\t%d\t%d\t", 
    	    	    oldContig, lastEnd+1, atoi(chrStart)+atoi(start)-2);
    	    fprintf(outf, "%d\t%s\t%d\t%s\t%s\n", 
    	    	    lastNum, "N", atoi(chrStart)+atoi(start)-2 - (lastEnd+1) +1, "contig", "no");
	    }
	oldContig = strdup(contig);
	oldNum = lastNum;
	}
    lastNum = atoi(num) + oldNum;    
    fprintf(outf, "%s\t%d\t%d\t", 
    	    contig, atoi(chrStart)+atoi(start)-1, atoi(chrStart)+atoi(end)-1);
    fprintf(outf, "%d\t%s\t%s\t%s\t%s\t%s\n", 
    	    lastNum, code, id, oStart, oEnd, strnd);
    lastEnd = atoi(chrStart)+atoi(end)-1;
    }
hFreeConn(&conn2);
	
fclose(outf);
return(0);
}
