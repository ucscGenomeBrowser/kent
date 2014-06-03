/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "cheapcgi.h"



/* # script to automate the long process of creating affymetrix's  */
/* # transcriptome track from their lift files. */

void usage()
{
    errAbort("trackFromLifts.pl - generate a track from the lift files provided by\n"
	     "Simon Cawley at Affymetrix. Assumes that you are already in the directory.\n"
	     "usage:\n\t"
	     "trackFromLifts.pl <dbFreeze> <optional: directory containing sql files for tables.> <optional -noCleanup>\n");
}

int main(int argc, char *argv[])
{
char *freeze = NULL;
char *sqlFileDir = "/projects/compbio/data/microarray/kapranov/maps/transcriptome/sci_21_22_paper/chr21_22/sql";
char buff[2048];
if(argc == 1)
    usage();

freeze = cloneString(argv[1]);
if(argc == 3)
    sqlFileDir = cloneString(argv[2]);

warn("\nFreeze is %s", freeze);

warn("\ngunzipping things, may take a while.");
system("gunzip *.gz");

warn("\nCatting files together.");
if(system("catTriplets go") != 0) errAbort("Couldn't run catTriplets");

warn("\nConverting to sample format (can take a while)");
if(system("affyTransLiftedToSample 1000 *.s.*.tab") != 0)  errAbort("system call failed.\n");

warn("\nAveraging experiments");
if(system("avgTranscriptomeExps go -doAll suffix=tab.sample") != 0)  errAbort("system call failed.\n");

warn("\nFinding Maximum vals");
if(system("maxTranscriptomeExps max.s.1.tab.sample Transcriptome *.s.*.avg") != 0)  errAbort("system call failed.\n");
//if(system("maxTranscriptomeExps max.b.1.tab.sample Transcriptome *.b.*.avg") != 0)  errAbort("system call failed.\n");
//if(system("maxTranscriptomeExps max.c.1.tab.sample Transcriptome *.c.*.avg") != 0)  errAbort("system call failed.\n");

warn("\nMerging averages");
if(system("cat *.sample.avg | egrep \"chr21|chr22\" > all.chr2X.tab.samples") != 0)  errAbort("system call failed.\n");

warn("\nMerging max vals into rest.");
if(system("cat max.*.sample | egrep \"chr21|chr22\" >> all.chr2X.tab.samples") != 0)  errAbort("system call failed.\n");

warn("\nCalculating zoom averages.");
snprintf(buff, sizeof(buff), "averageZoomLevels 8 8 %s all.chr2X.tab.samples", freeze);
if(system(buff) != 0)  errAbort("system call failed.\n");
if(system("groupSamples 1000 all.chr2X.tab.samples.zoom_8 all.chr2X.tab.samples.zoom_8.group") != 0)  errAbort("system call failed.\n");
if(system("groupSamples 1000 all.chr2X.tab.samples.zoom_1 all.chr2X.tab.samples.zoom_1.group") != 0)  errAbort("system call failed.\n");

warn("\nScaling values between 1 and 1000 after multiplying by .3");
if(system("scaleSampleFiles 1 1000 .3 all.chr2X.tab.samples") != 0)  errAbort("system call failed.\n");
if(system("scaleSampleFiles 1 1000 1 all.chr2X.tab.samples.zoom*") != 0)  errAbort("system call failed.\n");

warn("\nLoading into database");
snprintf(buff, sizeof(buff), "hgLoadBed %s affyTranscriptome -sqlTable=%s/affyTranscriptome.sql all.chr2X.tab.samples.scale", freeze, sqlFileDir);
if(system(buff) != 0) errAbort( "system call failed.\n");
snprintf(buff, sizeof(buff), "hgLoadBed %s zoom1_affyTranscriptome -sqlTable=%s/zoom1_affyTranscriptome.sql all.chr2X.tab.samples.zoom_1.group.scale", freeze, sqlFileDir);
if(system(buff) != 0) errAbort( "system call failed.\n");
snprintf(buff, sizeof(buff), "hgLoadBed %s zoom2_affyTranscriptome -sqlTable=%s/zoom2_affyTranscriptome.sql all.chr2X.tab.samples.zoom_8.group.scale", freeze, sqlFileDir);
if(system(buff) != 0) errAbort( "system call failed.\n");
cgiSpoof(&argc, argv);
if(!cgiBoolean("noCleanup"))
    {
    warn("\nCleaning up.");
    if(system("rm *sample* bed.tab") != 0) errAbort( "system call failed.\n");
    warn("\ngzipping up files. Will take a while...");
    if(system("gzip *.tab") != 0) errAbort("system call failed.\n");
    }
else 
    {
    warn("\nSkipping cleanup");
    }
warn ("Done.");
return 0;
}
