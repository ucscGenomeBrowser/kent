/* bedToFrames.c Quick program to create a frame based version
   for browsing custom tracks. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "bed.h"
#include "options.h"

char *browserName = NULL; /* Name of browser for example: hgwdev-sugnet.cse */
void usage()
{
errAbort("bedToFrames - Makes html files for browsing custom bed track\n"
	 "using frames. Use -pad for padding\n"
	 "usage:\n"
	 "   bedToFrames db bedFileIn.bed htmlTableFile.html htmlFrameFile.html url\n"
	 "-browserName=browserName (i.e. genome-test.cse)"
	 );
}

void writeOutFrames(FILE *htmlFrame, char *fileName, char *db, char *url, struct bed *bed)
/** Write out frame for htmls. */
{
fprintf(htmlFrame, "<html><head><title>Bed Custom Track</title></head>\n"
	"<frameset cols=\"18%%,82%%\">\n"
	"<frame name=\"_list\" src=\"./%s\">\n"
	"<frame name=\"browser\" src=\"http://%s.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s:%d-%d&"
	"hgt.customText=%s\">\n"
	"</frameset>\n"
	"</html>\n", fileName, browserName, db, bed->chrom, bed->chromStart, bed->chromEnd, url) ;
}

void bedToFrames(char *db, char *bedFile, char *html, char *frame, char *url)
/* Load the beds and print the custom file. */
{
struct bed *bedList=NULL, *bed = NULL;
FILE *htmlFrame = NULL, *htmlOut=NULL;
int padding = optionInt("pad", 100);
boolean score = optionExists("score");
warn("Padding is: %d", padding);
browserName = optionVal("browserName", "genome");
bedList = bedLoadAll(bedFile);
if(score) 
    {
    slSort(&bedList, bedCmpScore);
    slReverse(&bedList);
    }
else
    {
    slSort(&bedList, bedCmp);
    }
htmlFrame = mustOpen(frame, "w");

writeOutFrames(htmlFrame, html, db, url, bedList);
carefulClose(&htmlFrame);

htmlOut = mustOpen(html, "w");
fprintf(htmlOut, "<html><body><ol>\n");
for(bed=bedList; bed != NULL; bed = bed->next)
    {
    fprintf(htmlOut, "<li><a target=\"browser\" "
	    "href=\"http://%s.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s:%d-%d\"> "
	    "%s %d</a></li>\n", 
	    browserName,db, bed->chrom, bed->chromStart-padding, bed->chromEnd+padding, bed->name, score ? bed->score : 0);
    }
fprintf(htmlOut, "</body></html>\n");
carefulClose(&htmlOut);
bedFreeList(&bedList);
}

int main(int argc, char *argv[])
{
optionHash(&argc, argv);
if(argc != 6)
    usage();
bedToFrames(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
