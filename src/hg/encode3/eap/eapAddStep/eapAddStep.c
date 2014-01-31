/* eapAddStep - Add a step to eapStep and related tables.  This is just a small shortcut for doing it in SQL.  You can only add steps defined in C code.. */
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
    char *software;	/* Comma separated list of software names. First one gives name to step. */
    char *inputTypes;
    char *inputFormats;	/* Comma separated list of input formats */
    char *outputTypes;
    char *outputFormats;/* Comma separated list of formats */
    char *outputNamesInTempDir;  /* Comma separated list of file names in temp dir */
    };

struct stepInit steps[] =  
{
    {
    "bwa_single_end", 2,		// name and CPUs
    "eap_run_bwa_se,bwa,samtools",	// software
    "reads", "fastq",			// input names and formats
    "alignments", "bam", "out.bam",	// output names, formats, and file names
    },

    {
    "bwa_paired_end", 2, 
    "eap_run_bwa_pe,bwa,samtools",
    "reads1,reads2", "fastq,fastq",
    "alignments", "bam", "out.bam",
    },

    {
    "macs2_dnase_se", 1,
    "eap_run_macs2_dnase_se,macs2,bedToBigBed,bedGraphToBigWig",
    "alignments", "bam",
    "macs2_dnase_peaks,macs2_dnase_signal", "narrowPeak,bigWig", "out.narrowPeak.bigBed,out.bigWig",
    },

    {
    "macs2_dnase_pe", 1,
    "eap_run_macs2_dnase_pe,macs2,bedToBigBed,bedGraphToBigWig",
    "alignments", "bam",
    "macs2_dnase_peaks,macs2_dnase_signal", "narrowPeak,bigWig", "out.narrowPeak.bigBed,out.bigWig",
    },

    {
    "hotspot", 1,
    "eap_run_hotspot,hotspot.py,starch,unstarch,hotspot,bedtools,eap_broadPeak_to_bigBed,eap_narrowPeak_to_bigBed,bedToBigBed,bedGraphToBigWig,bedmap,bedGraphPack",
    "alignments", "bam",
    "hotspot_broad_peaks,hotspot_narrow_peaks,hotspot_signal",
    "broadPeak,narrowPeak,bigWig",
    "out.broadPeak.bigBed,out.narrowPeak.bigBed,out.bigWig"
    },

    {
    "macs2_chip_se", 1,
    "eap_run_macs2_chip_se,macs2,bedToBigBed,bedGraphToBigWig",
    "chipBam,controlBam", "bam,bam",
    "macs2_chip_peaks,macs2_chip_signal", "narrowPeak,bigWig", "out.narrowPeak.bigBed,out.bigWig",
    },

    {
    "macs2_chip_pe", 1,
    "eap_run_macs2_chip_pe,macs2,bedToBigBed,bedGraphToBigWig",
    "chipBam,controlBam", "bam,bam",
    "macs2_chip_peaks,macs2_chip_signal", "narrowPeak,bigWig", "out.narrowPeak.bigBed,out.bigWig",
    },

    {
    "sum_bigWig", 1,
    "eap_sum_bigWig,bigWigMerge,bedGraphPack,bedGraphToBigWig",
    "signal", "bigWig",
    "pooled_signal", "bigWig", "out.bigWig",
    }

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
int outCount = commaSepCount(init->outputTypes);
matchCount = commaSepCount(init->outputFormats);
if (outCount != matchCount)
    errAbort("outputTypes has %d elements but outputFormats has %d in step %s", 
	    outCount, matchCount, init->name);
matchCount = commaSepCount(init->outputNamesInTempDir);
if (outCount != matchCount)
    errAbort("outputTypes has %d elements but outputNamesInTempDir has %d in step %s", 
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
	"insert eapStep (name,cpusRequested,"
        " inCount,inputTypes,inputFormats,"
	" outCount,outputNamesInTempDir,outputTypes,outputFormats)"
	" values (");
dyStringPrintf(query, "'%s',", init->name);
dyStringPrintf(query, "%d,", init->cpusRequested);
dyStringPrintf(query, "%d,", inCount);
dyStringPrintf(query, "'%s',", init->inputTypes);
dyStringPrintf(query, "'%s',", init->inputFormats);
dyStringPrintf(query, "%d,", outCount);
dyStringPrintf(query, "'%s',", init->outputNamesInTempDir);
dyStringPrintf(query, "'%s',", init->outputTypes);
dyStringPrintf(query, "'%s'", init->outputFormats);
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
uglyf("About to force  eapCurrentStepVersion on %s\n", init->name);
eapCurrentStepVersion(conn, init->name);
uglyf("Past eapCurrentStepVersion on %s\n", init->name);

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
