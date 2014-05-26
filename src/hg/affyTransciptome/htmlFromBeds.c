/* htmlFromBeds - htmlFromBeds - This program creates a web page with a series of hyperlinks to the beds in the bed track. This is useful when a bed track is sparse and you want to just zoom to items instead of scrolling around.\n. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "htmlFromBeds - htmlFromBeds - This program creates a web page with a\n"
  "series of hyperlinks to the beds in the bed track. This is useful\n"
  "when a bed track is sparse and you want to just zoom to items instead\n"
  "of scrolling around.\n"
  "usage:\n"
  "   htmlFromBeds bedFile htmlFile db url\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct bed *bedLoadAll(char *fileName)
/* Load all bed's in file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct bed *bedList = NULL, *bed;
char *row[12];
while(lineFileRow(lf,row))
    {
    bed = bedLoad12(row);
    slAddHead(&bedList, bed);
    }
slReverse(&bedList);
lineFileClose(&lf);
return bedList;
}

void htmlFromBeds(char *bedFile, char *htmlFile, char *db, char *url)
/* htmlFromBeds - htmlFromBeds - This program creates a web page with a series of hyperlinks to the beds in the bed track. This is useful when a bed track is sparse and you want to just zoom to items instead of scrolling around.\n. */
{
struct bed *bed = NULL;
struct bed *bedList = bedLoadAll(bedFile);
FILE *htmlOut = mustOpen(htmlFile, "w");
slSort(&bedList, bedCmp);
fprintf(htmlOut, "<html><body>\n<ul>\n");
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    fprintf(htmlOut, 
	    "<li><a href=\"http://genome.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s:%d-%d&hgt.customText=%s/%s\">%s:%d-%d</a></li>\n",
	    db,
	    bed->chrom, bed->chromStart, bed->chromEnd,
	    url, bedFile,
	    bed->chrom, bed->chromStart, bed->chromEnd);
    }
fprintf(htmlOut, "</ul></body></html>\n");
carefulClose(&htmlOut);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
htmlFromBeds(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
