/* cdwMakePairedEndQa - Do alignments of paired-end fastq files and calculate distrubution of 
 * insert size. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "portable.h"
#include "obscure.h"
#include "cdw.h"
#include "cdwLib.h"
#include "raToStruct.h"
#include "ra.h"
#include "cdwLib.h"

int maxInsert = 1000;
bool keepTemp = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwMakePairedEndQa - Do alignments of paired-end fastq files and calculate distribution of \n"
  "insert size.\n"
  "usage:\n"
  "   cdwMakePairedEndQa startId endId\n"
  "options:\n"
  "   -maxInsert=N - maximum allowed insert size, default %d\n"
  "   -keep - do not delete temp files\n"
  , maxInsert
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"maxInsert", OPTION_INT},
   {NULL, 0},
};

/**** - Start raToStructGen generated code - ****/

struct raToStructReader *cdwQaPairedEndFastqRaReader()
/* Make a raToStructReader for cdwQaPairedEndFastq */
{
static char *fields[] = {
    "fileId1",
    "concordance",
    "distanceMean",
    "distanceStd",
    "distanceMin",
    "distanceMax",
    };
static char *requiredFields[] = {
    "concordance",
    "distanceMean",
    "distanceStd",
    "distanceMin",
    "distanceMax",
    };
return raToStructReaderNew("cdwQaPairedEndFastq", ArraySize(fields), fields, ArraySize(requiredFields), requiredFields);
}


struct cdwQaPairedEndFastq *cdwQaPairedEndFastqFromNextRa(struct lineFile *lf, struct raToStructReader *reader)
/* Return next stanza put into an cdwQaPairedEndFastq. */
{
enum fields
    {
    fileId1Field,
    concordanceField,
    distanceMeanField,
    distanceStdField,
    distanceMinField,
    distanceMaxField,
    };
if (!raSkipLeadingEmptyLines(lf, NULL))
    return NULL;

struct cdwQaPairedEndFastq *el;
AllocVar(el);

bool *fieldsObserved = reader->fieldsObserved;
bzero(fieldsObserved, reader->fieldCount);

char *tag, *val;
while (raNextTagVal(lf, &tag, &val, NULL))
    {
    struct hashEl *hel = hashLookup(reader->fieldIds, tag);
    if (hel != NULL)
        {
	int id = ptToInt(hel->val);
	if (fieldsObserved[id])
	     errAbort("Duplicate tag %s line %d of %s\n", tag, lf->lineIx, lf->fileName);
	fieldsObserved[id] = TRUE;
	switch (id)
	    {
	    case fileId1Field:
	        {
	        el->fileId1 = sqlUnsigned(val);
		break;
	        }
	    case concordanceField:
	        {
	        el->concordance = sqlDouble(val);
		break;
	        }
	    case distanceMeanField:
	        {
	        el->distanceMean = sqlDouble(val);
		break;
	        }
	    case distanceStdField:
	        {
	        el->distanceStd = sqlDouble(val);
		break;
	        }
	    case distanceMinField:
	        {
	        el->distanceMin = sqlDouble(val);
		break;
	        }
	    case distanceMaxField:
	        {
	        el->distanceMax = sqlDouble(val);
		break;
	        }
	    default:
	        internalErr();
		break;
	    }
	}
    }

raToStructReaderCheckRequiredFields(reader, lf);
return el;
}


struct cdwQaPairedEndFastq *cdwQaPairedEndFastqLoadRa(char *fileName)
/* Return list of all cdwQaPairedEndFastq in ra file. */
{
struct raToStructReader *reader = cdwQaPairedEndFastqRaReader();
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct cdwQaPairedEndFastq *el, *list = NULL;
while ((el = cdwQaPairedEndFastqFromNextRa(lf, reader)) != NULL)
    slAddHead(&list, el);
slReverse(&list);
lineFileClose(&lf);
raToStructReaderFree(&reader);
return list;
}

struct cdwQaPairedEndFastq *cdwQaPairedEndFastqOneFromRa(char *fileName)
/* Return cdwQaPairedEndFastq in file and insist there be exactly one record. */
{
struct cdwQaPairedEndFastq *one = cdwQaPairedEndFastqLoadRa(fileName);
if (one == NULL)
    errAbort("No data in %s", fileName);
if (one->next != NULL)
    errAbort("Multiple records in %s", fileName);
return one;
}

/**** - End raToStructGen generated code - ****/

#define FASTQ_SAMPLE_SIZE 10000

void sampleTrimAndAlign(struct sqlConnection *conn, struct cdwValidFile *vf1, struct cdwValidFile *vf2, 
        char *genoFile, char *assay, char *tmpSam)
/* Given a fastq file, make a subsample of it 10k reads long and align it with
 * bwa mem producing a sam file of given name. */
{
/* Get fastq record 1 */
long long fileId = vf1->fileId;
struct cdwFastqFile *fqf1 = cdwFastqFileFromFileId(conn, fileId);
if (fqf1 == NULL)
    errAbort("No cdwFastqFile record for file id %lld", fileId);

/* Get fastq record 2 */
fileId = vf2->fileId;
struct cdwFastqFile *fqf2 = cdwFastqFileFromFileId(conn, fileId);
if (fqf2 == NULL)
    errAbort("No cdwFastqFile record for file id %lld", fileId);

char sampleFastqName1[PATH_LEN], sampleFastqName2[PATH_LEN];
char trimmedPath1[PATH_LEN], trimmedPath2[PATH_LEN];

/* Create downsampled fastq1 in temp directory - downsampled more than default even. */
cdwMakeTempFastqSample(fqf1->sampleFileName, FASTQ_SAMPLE_SIZE, sampleFastqName1);
verbose(1, "downsampled %s into %s\n", fqf1->sampleFileName, sampleFastqName1);

/* Create downsampled fastq2 in temp directory - downsampled more than default even. */
cdwMakeTempFastqSample(fqf2->sampleFileName, FASTQ_SAMPLE_SIZE, sampleFastqName2);
verbose(1, "downsampled %s into %s\n", fqf2->sampleFileName, sampleFastqName2);

/* Trim long-RNA-seq files to ensure no poly-A tails remain*/
cdwTrimReadsForAssay(sampleFastqName1, trimmedPath1, assay); 
cdwTrimReadsForAssay(sampleFastqName2, trimmedPath2, assay); 

/* Do alignment */
char cmd[3*PATH_LEN];
//also see comments in cdwLib on bowtie on background about the bwa -> bowtie change
//safef(cmd, sizeof(cmd), "bwa mem -t 3 %s %s %s > %s", genoFile, trimmedPath1, trimmedPath2, tmpSam);
safef(cmd, sizeof(cmd), "bowtie -l 40 -n 1 %s --threads 3 --mm -1 %s -2 %s -S > %s", genoFile, trimmedPath1, trimmedPath2, tmpSam);
verbose(1, "running %s\n", cmd);
mustSystem(cmd);

/* Save return variables, clean up,  and go home. */
cdwCleanupTrimReads(sampleFastqName1, trimmedPath1); 
cdwCleanupTrimReads(sampleFastqName2, trimmedPath2); 
if (!keepTemp) {
    remove(sampleFastqName1);
    remove(sampleFastqName2);
}
cdwFastqFileFree(&fqf1);
cdwFastqFileFree(&fqf2);
}

void pairedEndQa(struct sqlConnection *conn, struct cdwFile *ef, struct cdwValidFile *vf)
/* Look for other end,  do a pairwise alignment, and save results in database. */
{
verbose(2, "pairedEndQa on %u %s %s\n", ef->id, ef->cdwFileName, ef->submitFileName);
/* Get other end, return if not found. */
struct cdwValidFile *otherVf = cdwOppositePairedEnd(conn, ef, vf);
if (otherVf == NULL) {
    verbose(1, "no opposite paired end file found");
    return;
    }

if (otherVf->fileId > vf->fileId) {
    verbose(1, "this is not the first file of the pair");
    return;
    }

struct cdwValidFile *vf1, *vf2;
struct cdwQaPairedEndFastq *pair = cdwQaPairedEndFastqFromVfs(conn, vf, otherVf, &vf1, &vf2);
if (pair != NULL)
    {
    verbose(1, "pair has already been processed\n");
    cdwValidFileFree(&otherVf);
    return;
    }
/* Get target assembly and figure out path for BWA index. */
struct cdwAssembly *assembly = cdwAssemblyForUcscDb(conn, vf->ucscDb);
assert(assembly != NULL);
char genoFile[PATH_LEN];
cdwIndexPath(assembly, genoFile);

verbose(1, "aligning subsamples on %u vs. %u paired reads\n", vf1->fileId, vf2->fileId);

/* Make alignments of subsamples. */
struct cgiParsedVars *tags = cdwMetaVarsList(conn, ef);
char *assay = cdwLookupTag(tags, "assay");

char *tmpSam = cloneString(rTempName(cdwTempDir(), "cdwPairSample", ".sam"));
sampleTrimAndAlign(conn, vf1, vf2, genoFile, assay, tmpSam);

/* Make ra file with pairing statistics */
char command[6*PATH_LEN];
char *tmpRa = cloneString(rTempName(cdwTempDir(), "cdwPairSample", ".ra"));
safef(command, sizeof(command), 
    "edwSamPairedEndStats -maxInsert=%d %s %s", maxInsert, tmpSam, tmpRa);
mustSystem(command);

/* Read RA file into variables. */
struct cdwQaPairedEndFastq *pe = cdwQaPairedEndFastqOneFromRa(tmpRa);

/* Update database with record. */
struct sqlConnection *freshConn = cdwConnectReadWrite();
char query[256];
sqlSafef(query, sizeof(query),
    "insert into cdwQaPairedEndFastq "
    "(fileId1,fileId2,concordance,distanceMean,distanceStd,distanceMin,distanceMax,recordComplete) "
    " values (%u,%u,%g,%g,%g,%g,%g,1)"
    , vf1->fileId, vf2->fileId, pe->concordance, pe->distanceMean
    , pe->distanceStd, pe->distanceMin, pe->distanceMax);
sqlUpdate(conn, query);
sqlDisconnect(&freshConn);

/* Clean up and go home. */
cdwValidFileFree(&otherVf);
remove(tmpSam);
remove(tmpRa);

freez(&tmpSam);
freez(&tmpRa);
cdwQaPairedEndFastqFree(&pe);
cdwValidFileFree(&otherVf);
}

void cdwMakePairedEndQa(unsigned startId, unsigned endId)
/* cdwMakePairedEndQa - Do alignments of paired-end fastq files and calculate distrubution of 
 * insert size. */
{
struct sqlConnection *conn = cdwConnectReadWrite();
struct cdwFile *ef, *efList = cdwFileAllIntactBetween(conn, startId, endId);
for (ef = efList; ef != NULL; ef = ef->next)
    {
    struct cdwValidFile *vf = cdwValidFileFromFileId(conn, ef->id);
    if (vf != NULL)
	{
	if (sameString(vf->format, "fastq") && !isEmpty(vf->pairedEnd))
	    pairedEndQa(conn, ef, vf);
	}
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
maxInsert = optionInt("maxInsert", maxInsert);
keepTemp = optionExists("keep");
cdwMakePairedEndQa(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
