/* encode2Manifest - Create a encode3 manifest file for encode2 files. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "jksql.h"
#include "encode/encodeExp.h"
#include "encode3/encode3Valid.h"
#include "mdb.h"

char *metaDbs[] = {"hg19", "mm9"};
char *metaTable = "metaDb";

/* Command line variables. */
char *fileRootDir = NULL;   /* If set we look at files a bit. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encode2Manifest - Create a encode3 manifest file for encode2 files by looking\n"
  "at the metaDb tables in hg19 and mm9.\n"
  "usage:\n"
  "   encode2Manifest manifest.tab\n"
  "options:\n"
  "   -checkExists=/root/dir/for/encode/downloads\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"checkExists", OPTION_STRING},
   {NULL, 0},
};

struct mdbObj *getMdbList(char *database)
/* Get list of metaDb objects for a database. */
{
struct sqlConnection *conn = sqlConnect(database);
struct mdbObj *list = mdbObjsQueryAll(conn, metaTable);
sqlDisconnect(&conn);
return list;
}

int unknownFormatCount = 0;
int totalFileCount = 0;
int missingFileCount = 0;
int foundFileCount = 0;

boolean isHistoneModAntibody(char *antibody)
/* Returns TRUE if it looks something like H3K4Me3 */
{
return antibody[0] == 'H' && isdigit(antibody[1]);
}

char *guessEnrichedIn(char *composite, char *dataType, char *antibody)
/* Figure out what it should be enriched in by looking at composite and dataType and antibody */
{
if (composite == NULL || dataType == NULL)
    errAbort("Got composite=%s, dataType=%s, can't guess", composite, dataType);
char *guess = "unknown";
if (sameString(dataType, "AffyExonArray")) guess = "exon";
else if (sameString(dataType, "Cage")) guess = "exon";
else if (sameString(dataType, "ChipSeq")) guess = "open";
else if (sameString(dataType, "DnaseDgf")) guess = "promoter";
else if (sameString(dataType, "DnaseSeq")) guess = "promoter";
else if (sameString(dataType, "FaireSeq")) guess = "open";
else if (sameString(dataType, "Gencode")) guess = "exon";
else if (sameString(dataType, "MethylArray")) guess = "promoter";
else if (sameString(dataType, "MethylRrbs")) guess = "promoter";
else if (sameString(dataType, "MethylSeq")) guess = "promoter";
else if (sameString(dataType, "Proteogenomics")) guess = "coding";
else if (sameString(dataType, "RipGeneSt")) guess = "exon";
else if (sameString(dataType, "RipSeq")) guess = "exon";
else if (sameString(dataType, "RipTiling")) guess = "exon";
else if (sameString(dataType, "RnaChip")) guess = "exon";
else if (sameString(dataType, "RnaPet")) guess = "exon";
else if (sameString(dataType, "RnaSeq")) guess = "exon";
else if (sameString(dataType, "Switchtear")) guess = "open";
else if (sameString(dataType, "TfbsValid")) guess = "open";

/* Do some fine tuning within Chip-seq to treat histone mods differently. */
if (sameString(dataType, "ChipSeq"))
    {
    if (antibody == NULL)
       errAbort("Missing antibody in ChipSeq");
    if (isHistoneModAntibody(antibody))
        {
	if (startsWith("H3K36me3", antibody))
	   guess = "exon";
	else if (startsWith("H3K79me2", antibody))
	   guess = "exon";
	}
    }
return guess;
}

char *guessFormatFromFileName(char *fileName)
/* Figure out validateFile format from fileName */
{
char *name = cloneString(fileName);  /* Our own copy to parse. */
if (endsWith(name, ".gz"))
    chopSuffix(name);
char *suffix = strrchr(name, '.');
char *result = "unknown";
if (suffix != NULL)
    {
    suffix += 1;    // Skip over .
    if (sameString(suffix, "bam"))
	result = "bam";
    else if (sameString(suffix, "bed"))
	result = "bed";
    else if (sameString(suffix, "bed9"))
	result = "bed9";
    else if (sameString(suffix, "bigBed"))
	result = "bigBed";
    else if (endsWith(name, "bigWig"))
	result = "bigWig";
    else if (sameString(suffix, "broadPeak"))
	result = "broadPeak";
    else if (sameString(suffix, "fastq"))
        result = "fastq";
    else if (sameString(suffix, "gtf"))
        result = "gtf";
    else if (sameString(suffix, "narrowPeak"))
        result = "narrowPeak";
    else
        ++unknownFormatCount;
    ++totalFileCount;
    }
freeMem(name);
return result;
}

void addGenomeToManifest(char *genome, struct mdbObj *mdbList,
    char *rootDir, FILE *f)
/* Get metaDb table for genome and write out relevant bits of it to f */
{
verbose(1, "processing %s\n", genome);
struct mdbObj *mdb;
for (mdb = mdbList; mdb != NULL; mdb = mdb->next)
    {
    /* Grab the fields we want out of the variable list. */
    char *fileName = NULL;
    char *dccAccession = NULL;
    char *replicate = NULL;
    char *composite = NULL;
    char *dataType = NULL;
    char *objType = NULL;
    char *antibody = NULL;
    char *md5sum = NULL;
    char *view = NULL;
    struct mdbVar *v;
    for (v = mdb->vars; v != NULL; v = v->next)
         {
	 char *var = v->var, *val = v->val;
	 if (sameString("fileName", var))
	     fileName = val;
	 else if (sameString("dccAccession", var))
	     dccAccession = val;
	 else if (sameString("replicate", var))
	     replicate = val;
	 else if (sameString("composite", var))
	     composite = val;
	 else if (sameString("dataType", var))
	     dataType = val;
	 else if (sameString("objType", var))
	     objType = val;
	 else if (sameString("antibody", var))
	     antibody = val;
	 else if (sameString("md5sum", var))
	     md5sum = val;
	 else if (sameString("view", var))
	     view = val;
	 }

    /* If we have the fields we need,  fake the rest if need be and output. */
    if (fileName != NULL && dccAccession != NULL && objType != NULL)
        {
	if (sameString(objType, "file") || sameString(objType, "table"))
	    {
	    /* Just take first fileName in comma separated list. */
	    char *comma = strchr(fileName, ',');
	    if (comma != NULL) 
		*comma = 0;

	    if (composite == NULL)
	        errAbort("No composite for %s %s", dccAccession, fileName);

	    if (md5sum != NULL)
		{
		char *comma = strchr(md5sum, ',');
		if (comma != NULL) 
		    *comma = 0;
		}

	    /* Check file size (which also checks if file exists */
	    char *validationKey = "n/a";
	    off_t size = 0;
	    time_t updateTime = 0;
	    if (rootDir)
		{
		char path[PATH_LEN];
		safef(path, sizeof(path), "%s/%s/%s/%s", rootDir, genome, composite, fileName);
		size = fileSize(path);
		if (size == -1)
		    {
		    verbose(2, "%s doesn't exist\n", path);
		    ++missingFileCount;
		    continue;
		    }
		++foundFileCount;
		updateTime = fileModTime(path); 
		if (md5sum != NULL)
		    validationKey = encode3CalcValidationKey(md5sum, size);
		}

	    /* Output each field. */ 
	    fprintf(f, "%s/%s/%s\t", genome, composite, fileName);
	    fprintf(f, "%s\t", guessFormatFromFileName(fileName));
	    fprintf(f, "%s\t", naForNull(view));
	    fprintf(f, "%s\t", dccAccession);
	    fprintf(f, "%s\t", naForNull(replicate));
	    fprintf(f, "%s", guessEnrichedIn(composite, dataType, antibody));
	    if (rootDir)
		{
		fprintf(f, "\t%s", naForNull(md5sum));
		fprintf(f, "\t%lld", (long long)size);
		fprintf(f, "\t%ld", (long)updateTime);
		fprintf(f, "\t%s", validationKey);
		}
	    fprintf(f, "\n");
	    }
	}
    }
}

void encode2Manifest(char *outFile)
/* encode2Manifest - Create a encode3 manifest file for encode2 files. */
{
int i;
struct mdbObj *mdbLists[ArraySize(metaDbs)];
for (i=0; i<ArraySize(metaDbs); ++i)
    {
    char *db = metaDbs[i];
    mdbLists[i] = getMdbList(db);
    verbose(1, "%d meta objects in %s\n", slCount(mdbLists[i]), db);
    }

/* Open output and write header */
FILE *f = mustOpen(outFile, "w");
fputs("#file_name\tformat\toutput_type\texperiment\treplicate\tenriched_in", f);
if (fileRootDir)
    fputs("\tmd5_sum\tsize\tmodified\tvalid_key\n", f);
else
    fputs("\n", f);

for (i=0; i<ArraySize(metaDbs); ++i)
   {
   addGenomeToManifest(metaDbs[i], mdbLists[i], fileRootDir, f);
   }
verbose(1, "%d total files including %d of unknown format\n", totalFileCount, unknownFormatCount);
if (fileRootDir)
    {
    verbose(1, "%d files in metaDb not found in %s,  run with verbose=2 to see list\n", 
	missingFileCount, fileRootDir);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
fileRootDir = optionVal("checkExists", fileRootDir);
encode2Manifest(argv[1]);
return 0;
}
