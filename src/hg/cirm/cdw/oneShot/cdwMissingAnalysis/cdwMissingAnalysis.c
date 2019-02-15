/* cdwMissingAnalysis - Find fastq files that seem to be missing analysis.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdwLib.h"
#include "portable.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwMissingAnalysis - Find fastq files that seem to be missing analysis.\n"
  "usage:\n"
  "   cdwMissingAnalysis samples.tsv datasets.info detailsDir\n"
  "where the detailsDir will be created and filled with one tsv file per dataset.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct dataset
    {
    struct dataset *next;
    char *name;
    struct assay *assayList;
    struct hash *assayHash;
    };

struct assay
    {
    struct assay *next;
    char *name;
    struct sample *sampleList;
    struct hash *sampleHash;
    };

struct sample
    {
    struct sample *next;
    char *name;
    char *species;
    struct format *formatList;
    struct hash *formatHash;
    };

struct format
    {
    struct format *next;
    char *name;
    struct slName *fileList;
    int fileCount;	// Matches length of fileList
    };

boolean allInHash(struct hash *hash,  char *keys[], int keyCount)
/* Return true if all keys in array of length keyCount are found in hash. */
{
int i;
for (i=0; i<keyCount; ++i)
    if (hashLookup(hash, keys[i]) == NULL)
        return FALSE;
return TRUE;
}

void outputFileDetails(struct dataset *dataset, struct assay *assay, struct sample *sample,
    struct format *format,  FILE *f)
/* Write out what we hope analysis centers want to know about the files in format->fileList
 * which as of this writing is all fastqs. */
{
struct slName *file;
for (file = format->fileList; file != NULL; file = file->next)
    {
    fprintf(f, "%s\t%s\t%s\t%s\t%s\n", assay->name, dataset->name, sample->name, sample->species, file->name);
    }
}

void checkSamplesHaveFormats(struct dataset *dataset, struct assay *assay, char *keyFormat,
    char *formats[], int formatCount, FILE *f)
/* Print out info about missing files of given format */
{
struct sample *sample;
for (sample = assay->sampleList; sample != NULL; sample = sample->next)
    {
    struct format *fastqFormat = hashFindVal(sample->formatHash, keyFormat);
    if (fastqFormat != NULL)
	{
	if (!allInHash(sample->formatHash, formats, formatCount))
	    {
	    outputFileDetails(dataset, assay, sample, fastqFormat, f);
	    }
	}
    }
}

void checkMissingFilesForAssays(char *fileName, struct dataset *dataset)
/* Create file of given name that describes missing files for a dataset */
{
struct assay *assay;
FILE *f = mustOpen(fileName, "w");
fprintf(f, "#assay\tdataset\tsample\tspecies\tfile\n");
for (assay = dataset->assayList; assay != NULL; assay = assay->next)
    {
    char *assayName = assay->name;
    if (sameWord(assayName, "long-RNA-seq") || sameWord(assayName, "sc-RNA-seq"))
        {
	char *neededFormats[] = {"bam", };
	checkSamplesHaveFormats(dataset, assay, "fastq", neededFormats, ArraySize(neededFormats), f);
	}
    else if (sameWord(assayName, "WGS"))
        {
	char *neededFormats[] = {"bam", "vcf"};
	checkSamplesHaveFormats(dataset, assay, "fastq", neededFormats, ArraySize(neededFormats), f);
	}

    }
carefulClose(&f);
}

void cdwMissingAnalysis(char *sampleLevelFile, char *datasetLevelFile, char *detailsDir)
/* cdwMissingAnalysis - Find fastq files that seem to be missing analysis.. */
{
struct sqlConnection *conn = cdwConnect();
char query[512];


/* Look at formats for all files in data set */
verbose(1, "Reading cdwFileTags table\n");
struct hash *datasetHash = hashNew(0);
struct hash *speciesHash = hashNew(0);
struct dataset *datasetList = NULL;
sqlSafef(query, sizeof(query), 
    "select data_set_id,assay,meta,format,submit_file_name,species from cdwFileTags");
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *datasetName = naForNull(row[0]);
    char *assayName = naForNull(row[1]);
    char *sampleName = naForNull(row[2]);
    char *formatName = naForNull(row[3]);
    char *fileName = row[4];   // Better to crash if it's NULL to be honest.
    char *species = hashStoreName(speciesHash, naForNull(row[5]));

    struct dataset *dataset = hashFindVal(datasetHash,  datasetName);
    if (dataset == NULL)
        {
	AllocVar(dataset);
	hashAddSaveName(datasetHash, datasetName, dataset, &dataset->name);
	slAddHead(&datasetList, dataset);
	dataset->assayHash = hashNew(0);
	}

    struct assay *assay = hashFindVal(dataset->assayHash, assayName);
    if (assay == NULL)
        {
	AllocVar(assay);
	hashAddSaveName(dataset->assayHash, assayName, assay, &assay->name);
	slAddHead(&dataset->assayList, assay);
	assay->sampleHash = hashNew(0);
	}

    struct sample *sample = hashFindVal(assay->sampleHash, sampleName);
    if (sample == NULL)
        {
	AllocVar(sample);
	hashAddSaveName(assay->sampleHash,  sampleName, sample, &sample->name);
	slAddHead(&assay->sampleList, sample);
	sample->species = species;
	sample->formatHash = hashNew(4);
	}

    struct format *format = hashFindVal(sample->formatHash, formatName);
    if (format == NULL)
        {
	AllocVar(format);
	hashAddSaveName(sample->formatHash, formatName, format, &format->name);
	slAddHead(&sample->formatList, format);
	}
    assert(fileName != NULL);
    slNameAddHead(&format->fileList, fileName);
    format->fileCount += 1;
    }

verbose(1, "Outputting sample level format counts in %s\n", sampleLevelFile);
FILE *f = mustOpen(sampleLevelFile, "w");
slSort(&datasetList, slPairCmp);
struct dataset *dataset;
for (dataset = datasetList; dataset != NULL; dataset = dataset->next)
    {
    slSort(&dataset->assayList, slPairCmp);
    struct assay *assay;
    for (assay = dataset->assayList; assay != NULL; assay = assay->next)
	{
	slSort(&assay->sampleList, slPairCmp);
	struct sample *sample;
	for (sample = assay->sampleList; sample != NULL; sample = sample->next)
	    {
	    slSort(&sample->formatList, slPairCmp);
	    struct format *format;
	    for (format = sample->formatList; format != NULL; format = format->next)
		{
		fprintf(f, "%s\t%s\t%s\t%s\t%d\n", dataset->name, assay->name,
		    sample->name, format->name, slCount(format->fileList));
		}
	    }
	}
    }
carefulClose(&f);

verbose(1, "Outputting dataset level format counts in %s\n", datasetLevelFile);
f = mustOpen(datasetLevelFile, "w");
for (dataset = datasetList; dataset != NULL; dataset = dataset->next)
    {
    fprintf(f, "%s with %d assay\n", dataset->name, slCount(dataset->assayList));
    struct assay *assay;
    for (assay = dataset->assayList; assay != NULL; assay = assay->next)
	{
	fprintf(f, "\t%s with %d samples\n", assay->name, slCount(assay->sampleList));
	struct hash *formatHash = hashNew(0);
	struct format *formatList = NULL;
	struct sample *sample;
	for (sample = assay->sampleList; sample != NULL; sample = sample->next)
	    {
	    struct format *format;
	    for (format = sample->formatList; format != NULL; format = format->next)
		{
		struct format *assayFormat = hashFindVal(formatHash, format->name);
		if (assayFormat == NULL)
		    {
		    AllocVar(assayFormat);
		    hashAddSaveName(formatHash, format->name, assayFormat, &assayFormat->name);
		    slAddHead(&formatList, assayFormat);
		    }
		assayFormat->fileCount += format->fileCount;
		slSort(&format->fileList, slNameCmp);
		}
	    }
	slSort(&formatList, slPairCmp);
	struct format *format;
	for (format = formatList; format != NULL; format = format->next)
	    fprintf(f, "\t\t%s\t%d\n", format->name, format->fileCount);
	hashFree(&formatHash);
	}
    fprintf(f, "\n");
    }
carefulClose(&f);

verbose(1, "Outputting sample fastq files that are missing analysis in %s.\n", detailsDir);
makeDirsOnPath(detailsDir);
for (dataset = datasetList; dataset != NULL; dataset = dataset->next)
    {
    if (!sameWord(dataset->name, "n/a"))
	{
	char path[PATH_LEN];
	safef(path, sizeof(path), "%s/%s", detailsDir, dataset->name);
	checkMissingFilesForAssays(path, dataset);
	}
    }


sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
cdwMissingAnalysis(argv[1], argv[2], argv[3]);
return 0;
}
