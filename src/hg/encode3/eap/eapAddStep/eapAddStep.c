/* eapAddStep - Add a step to eapStep and related tables.  This is just a small shortcut for doing it in SQL.  You can only add steps defined in C code.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "jksql.h"
#include "eapDb.h"
#include "eapLib.h"
#include "intValTree.h"

boolean clList = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "eapAddStep - Add a step to eapStep and related tables.  This is just a small shortcut for doing it in SQL.  You can only add steps defined in C code.\n"
  "usage:\n"
  "   eapAddStep stepName\n"
  "The step name can be a quoted pattern as well as in\n"
  "   eapAddStep '*'\n"
  "to get them all.\n"
  "options:\n"
  "   -list - Just list steps matching stepName pattern\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"list", OPTION_BOOLEAN},
   {NULL, 0},
};

struct stepInit
/* Just stuff to help initialize a step */
    {
    char *name;     /* Name of step, should be same ase first software name */
    int cpusRequested;	/* # of cpus requested from job control system */
    char *description;  /* Sentence long description of step */
    char *software;	/* Comma separated list of software names. First one gives name to step. */
    char *inputTypes;
    char *inputFormats;	/* Comma separated list of input formats */
    char *inputDescriptions; /* Comma separated list of input descriptions. */
    char *outputTypes;
    char *outputFormats;/* Comma separated list of formats */
    char *outputNamesInTempDir;  /* Comma separated list of file names in temp dir */
    char *outputDescriptions; /* Comma separated list of input descriptions. */
    };

struct stepInit steps[] =  
{
    {
    "bwa_single_end", 2,		// name and CPUs
    "Align single ended fastq with bwa to produce BAM sorted by genome position",  // Description
    "eap_run_bwa_se,bwa,samtools",	// software
    "reads", "fastq",			// input names and formats
    "Reads in Sanger fastq format",     // input description
    "alignments", "bam", "out.bam",	// output names, formats, and file names
    "Alignments in BAM format sorted by genomic position including random pick of multi-aligners"
    },

    {
    "bwa_paired_end", 2, 
    "Align paired end fastqs with bwa to produce BAM sorted by genome position",  // Description
    "eap_run_bwa_pe,bwa,samtools",
    "reads1,reads2", "fastq,fastq",
    "Forward direction reads in Sanger fastq format,"     // input description
    "Reverse direction reads in Sanger fastq format",
    "alignments", "bam", "out.bam",
    "Alignments in BAM format sorted by genomic position including random pick of multi-aligners"
    },

    {
    "macs2_dnase_se", 1,
    "Call peaks and generate signal plot from a single ended DNAse bam file using Macs2",
    "eap_run_macs2_dnase_se,macs2,bedToBigBed,bedGraphToBigWig",
    "alignments", "bam",
    "Alignments of single end reads in bam format",
    "macs2_dnase_peaks,macs2_dnase_signal", "narrowPeak,bigWig", "out.narrowPeak.bigBed,out.bigWig",
    "Narrow peak calls from Macs2,Base-by-base signal graph from macs2"
    },

    {
    "macs2_dnase_pe", 1,
    "Call peaks and generate signal plot from a paired end DNAse bam file using Macs2",
    "eap_run_macs2_dnase_pe,macs2,bedToBigBed,bedGraphToBigWig",
    "Alignments of paired end reads in bam format",
    "alignments", "bam",
    "macs2_dnase_peaks,macs2_dnase_signal", "narrowPeak,bigWig", "out.narrowPeak.bigBed,out.bigWig",
    "Narrow peak calls from Macs2,Base-by-base signal graph from macs2"
    },

    {
    "hotspot", 1,
    "Call hotspots, peaks, and generate a signal plot from DNAse bam file using hotspot",
    "eap_run_hotspot,hotspot.py,edwBamFilter,starch,unstarch,hotspot,bedtools,eap_broadPeak_to_bigBed,eap_narrowPeak_to_bigBed,bedToBigBed,bedGraphToBigWig,bedmap,bedGraphPack",
    "alignments", "bam",
    "Alignments of reads with cuts on 5-prime ends in bam format",
    "hotspot_broad_peaks,hotspot_narrow_peaks,hotspot_signal",
    "broadPeak,narrowPeak,bigWig",
    "out.broadPeak.bigBed,out.narrowPeak.bigBed,out.bigWig",
    "Hotspot calls,Peak calls from hotspot,Base-by-base signal graph from hotspot"
    },

    {
    "macs2_chip_se", 1,
    "Generate peaks and signal for single ended ChIP-seq BAM files from IP and control using Macs2",
    "eap_run_macs2_chip_se,macs2,bedToBigBed,bedGraphToBigWig",
    "chipBam,controlBam", "bam,bam",
    "Alignments of single end reads from IP,Alignments of single end reads from control",
    "macs2_chip_peaks,macs2_chip_signal", "narrowPeak,bigWig", "out.narrowPeak.bigBed,out.bigWig",
    "Narrow peak calls from Macs2,Base-by-base signal graph from macs2"
    },

    {
    "macs2_chip_pe", 1,
    "Generate peaks and signal for paired end ChIP-seq BAM files from IP and control using Macs2",
    "eap_run_macs2_chip_pe,macs2,bedToBigBed,bedGraphToBigWig",
    "chipBam,controlBam", "bam,bam",
    "Alignments of paired end reads from IP,Alignments of paired end reads from control",
    "macs2_chip_peaks,macs2_chip_signal", "narrowPeak,bigWig", "out.narrowPeak.bigBed,out.bigWig",
    "Narrow peak calls from Macs2,Base-by-base signal graph from macs2"
    },

    {
    "spp_chip_se", 1,
    "Generate peaks single end ChIP-seq BAM files from IP and control using SPP",
    "eap_run_spp_chip_se,Rscript,bedToBigBed",
    "chipBam,controlBam", "bam,bam",
    "Alignments of single end reads from IP,Alignments of single end reads from control",
    "spp_chip_peaks", "narrowPeak", "out.narrowPeak.bigBed",
    "Narrow peak calls from SPP"
    },

    {
    "sum_bigWig", 1,
    "Add together signals from multiple bigWigs producing a bigWig for the sum",
    "eap_pool_big_wig,bigWigMerge,bedGraphPack,bedGraphToBigWig",
    "signal", "bigWig",
    "List of bigWig files",
    "pooled_signal", "bigWig", "out.bigWig",
    "Sum of inputs signals"
    },

    {
    "replicated_hotspot", 1,
    "Pool together two replicates and run hotspot on them",
    "eap_pool_hotspot,eap_run_hotspot,edwBamFilter,hotspot.py,starch,unstarch,hotspot,bedtools,eap_broadPeak_to_bigBed,eap_narrowPeak_to_bigBed,bedToBigBed,bedGraphToBigWig,bedmap,bedGraphPack",
    "alignments", "bam",
    "Alignments in bam format",
    "hotspot_broad_peaks,hotspot_narrow_peaks,hotspot_signal",
    "broadPeak,narrowPeak,bigWig",
    "out.broadPeak.bigBed,out.narrowPeak.bigBed,out.bigWig",
    "Hotspot calls,Peak calls from hotspot,Base-by-base signal graph from hotspot"
    },
    
    {
    "phantom_peak_stats", 1,
    "Run the phantom peaks stats tools to calculate RSC and NSC among other things.",
    "eap_run_phantom_peak_spp,Rscript",
    "alignments", "bam",
    "Alignments from some sort of peaky data set in BAM format",
    "", "", "",
    "",
    },

    {
    "dnase_stats", 1,
    "Subsample bam file to 5M mapped reads, run hotspot, and collect a bunch of statistics.",
    "eap_dnase_stats,edwBamStats,bigBedToBed,edwBamFilter,bigWigAverageOverBed,eap_run_phantom_peak_spp,Rscript,eap_run_hotspot,hotspot.py,starch,unstarch,hotspot,bedtools,eap_broadPeak_to_bigBed,eap_narrowPeak_to_bigBed,bedToBigBed,bedGraphToBigWig,bedmap,bedGraphPack",
    "alignments", "bam",
    "Alignments from a DNAse hypersensitivity assay in BAM format",
    "", "", "",
    "",
    },

};

int commaSepCount(char *s)
/* Count number of comma-separated items assuming there can be a terminal non-separating comma
 * or not. */
{
if (isEmpty(s))
    return 0;
int commaCount = countChars(s, ',');
int sepCount = commaCount;
char lastC = lastChar(s);
if (lastC == ',')
    --sepCount;
return sepCount + 1;
}


void initStep(struct sqlConnection *conn, struct stepInit *init)
/* Create step based on initializer */
{
/* Do a little validation on while counting up inputs and outputs */
int inCount = commaSepCount(init->inputTypes);
int matchCount = commaSepCount(init->inputFormats);
if (inCount != matchCount)
    errAbort("inputTypes has %d elements but inputFormats has %d in step %s", 
	    inCount, matchCount, init->name);
matchCount = commaSepCount(init->inputDescriptions);
if (inCount != matchCount)
    errAbort("inputTypes has %d elements but inputDescriptions has %d in step %s",
	    inCount, matchCount, init->name);
int outCount = commaSepCount(init->outputTypes);
matchCount = commaSepCount(init->outputFormats);
if (outCount != matchCount)
    errAbort("outputTypes has %d elements but outputFormats has %d in step %s", 
	    outCount, matchCount, init->name);
matchCount = commaSepCount(init->outputNamesInTempDir);
if (outCount != matchCount)
    errAbort("outputTypes has %d elements but outputNamesInTempDir has %d in step %s", 
	    outCount, matchCount, init->name);
matchCount = commaSepCount(init->outputDescriptions);
if (outCount != matchCount)
    errAbort("outputTypes has %d elements but outputDescriptions has %d in step %s", 
	    outCount, matchCount, init->name);

struct dyString *query = dyStringNew(0);
dyStringPrintf(query, "select count(*) from eapStep where name='%s'", init->name);
int existingCount = sqlQuickNum(conn, query->string);
if (existingCount > 0)
    {
    warn("%s already exists in eapStep", init->name);
    dyStringFree(&query);
    return;
    }

/* Parse out software part and make sure that all pieces are there. */
char **softwareArray;
int softwareCount;
sqlStringDynamicArray(init->software, &softwareArray, &softwareCount);
unsigned softwareIds[softwareCount];
int i;
for (i=0; i<softwareCount; ++i)
    {
    char *name = softwareArray[i];
    dyStringClear(query);
    dyStringPrintf(query, "select id from eapSoftware where name='%s'", name);
    unsigned softwareId = sqlQuickNum(conn, query->string);
    if (softwareId == 0)
        errAbort("Software %s doesn't exist by that name in eapSoftware", name);
    softwareIds[i] = softwareId;
    }

/* Make step record. */
dyStringClear(query);
dyStringAppend(query,
	"insert eapStep (name,cpusRequested,description,"
        " inCount,inputTypes,inputFormats,inputDescriptions,"
	" outCount,outputNamesInTempDir,outputTypes,outputFormats,outputDescriptions)"
	" values (");
dyStringPrintf(query, "'%s',", init->name);
dyStringPrintf(query, "%d,", init->cpusRequested);
dyStringPrintf(query, "\"%s\",", init->description);
dyStringPrintf(query, "%d,", inCount);
dyStringPrintf(query, "'%s',", init->inputTypes);
dyStringPrintf(query, "'%s',", init->inputFormats);
dyStringPrintf(query, "\"%s\",", init->inputDescriptions);
dyStringPrintf(query, "%d,", outCount);
dyStringPrintf(query, "'%s',", init->outputNamesInTempDir);
dyStringPrintf(query, "'%s',", init->outputTypes);
dyStringPrintf(query, "'%s',", init->outputFormats);
dyStringPrintf(query, "\"%s\"", init->outputDescriptions);
dyStringPrintf(query, ")");
sqlUpdate(conn, query->string);

/* Make software/step associations. */
for (i=0; i<softwareCount; ++i)
    {
    dyStringClear(query);
    dyStringPrintf(query, "insert eapStepSoftware (step,software) values ('%s','%s')",
	    init->name, softwareArray[i]);
    sqlUpdate(conn, query->string);
    }


/* Force step version stuff to be made right away */
eapCurrentStepVersion(conn, init->name);

/* Clean up. */
dyStringFree(&query);
freez(&softwareArray[0]);
freez(&softwareArray);
}

void eapAddStep(char *pattern)
/* eapAddStep - Add a step to eapStep and related tables.  This is just a small shortcut for doing 
 * it in SQL.  You can only add steps defined in C code.. */
{
struct sqlConnection *conn = eapConnectReadWrite();
int i;
for (i=0; i<ArraySize(steps); ++i)
    {
    struct stepInit *init = &steps[i];
    if (wildMatch(pattern, init->name))
        {
	if (clList)
	    puts(init->name);
	else
	    initStep(conn, init);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
clList = optionExists("list");
eapAddStep(argv[1]);
return 0;
}
