/* edwMakePairedEndQa - Do alignments of paired-end fastq files and calculate distrubution of 
 * insert size. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "portable.h"
#include "obscure.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "raToStruct.h"
#include "ra.h"

int maxInsert = 1000;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwMakePairedEndQa - Do alignments of paired-end fastq files and calculate distrubution of \n"
  "insert size.\n"
  "usage:\n"
  "   edwMakePairedEndQa startId endId\n"
  "options:\n"
  "   -maxInsert=N - maximum allowed insert size, default %d\n"
  , maxInsert
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"maxInsert", OPTION_INT},
   {NULL, 0},
};

/**** - Start raToStructGen generated code - ****/

struct raToStructReader *edwQaPairedEndFastqRaReader()
/* Make a raToStructReader for edwQaPairedEndFastq */
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
return raToStructReaderNew("edwQaPairedEndFastq", ArraySize(fields), fields, ArraySize(requiredFields), requiredFields);
}


struct edwQaPairedEndFastq *edwQaPairedEndFastqFromNextRa(struct lineFile *lf, struct raToStructReader *reader)
/* Return next stanza put into an edwQaPairedEndFastq. */
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

struct edwQaPairedEndFastq *el;
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


struct edwQaPairedEndFastq *edwQaPairedEndFastqLoadRa(char *fileName)
/* Return list of all edwQaPairedEndFastq in ra file. */
{
struct raToStructReader *reader = edwQaPairedEndFastqRaReader();
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct edwQaPairedEndFastq *el, *list = NULL;
while ((el = edwQaPairedEndFastqFromNextRa(lf, reader)) != NULL)
    slAddHead(&list, el);
slReverse(&list);
lineFileClose(&lf);
raToStructReaderFree(&reader);
return list;
}

struct edwQaPairedEndFastq *edwQaPairedEndFastqOneFromRa(char *fileName)
/* Return edwQaPairedEndFastq in file and insist there be exactly one record. */
{
struct edwQaPairedEndFastq *one = edwQaPairedEndFastqLoadRa(fileName);
if (one == NULL)
    errAbort("No data in %s", fileName);
if (one->next != NULL)
    errAbort("Multiple records in %s", fileName);
return one;
}

/**** - End raToStructGen generated code - ****/

char *oppositeEnd(char *end)
/* Return "1" for "2" and vice versa */
{
if (sameString(end, "1"))
    return "2";
else if (sameString(end, "2"))
    return "1";
else
    {
    errAbort("Expecting 1 or 2, got %s in oppositeEnd", end);
    return NULL;
    }
}

#define FASTQ_SAMPLE_SIZE 10000

void makeTmpSai(struct sqlConnection *conn, struct edwValidFile *vf, char *genoFile, 
    char **retSampleFile, char **retSaiFile)
/* Given a fastq file, make a subsample of it 100k reads long and align it with
 * bwa producing a sai file of given name. */
{
/* Get fastq record */
long long fileId = vf->fileId;
struct edwFastqFile *fqf = edwFastqFileFromFileId(conn, fileId);
if (fqf == NULL)
    errAbort("No edwFastqFile record for file id %lld", fileId);

/* Create downsampled fastq in temp directory - downsampled more than default even. */
char sampleFastqName[PATH_LEN];
edwMakeTempFastqSample(fqf->sampleFileName, FASTQ_SAMPLE_SIZE, sampleFastqName);
verbose(1, "downsampled %s into %s\n", vf->licensePlate, sampleFastqName);

/* Do alignment */
char cmd[3*PATH_LEN];
char *saiName = cloneString(rTempName(edwTempDir(), "edwPairSample", ".sai"));
safef(cmd, sizeof(cmd), "bwa aln -t 3 %s %s > %s", genoFile, sampleFastqName, saiName);
mustSystem(cmd);

/* Save return variables, clean up,  and go home. */
*retSampleFile = cloneString(sampleFastqName);
*retSaiFile = saiName;
edwFastqFileFree(&fqf);
}

void pairedEndQa(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf)
/* Look for other end,  do a pairwise alignment, and save results in database. */
{
/* Get other end, return if not found. */
char *otherEnd = oppositeEnd(vf->pairedEnd);
char query[1024];
sqlSafef(query, sizeof(query), 
    "select * from edwValidFile where experiment='%s' and outputType='%s' and replicate='%s' "
    "and technicalReplicate='%s' and pairedEnd='%s'"
    , vf->experiment, vf->outputType, vf->replicate, vf->technicalReplicate, otherEnd);
struct edwValidFile *otherVf = edwValidFileLoadByQuery(conn, query);
if (otherVf == NULL)
    return;
if (otherVf->next != NULL)
    errAbort("Multiple results from pairedEnd query %s", query);

/* Sort the two ends. */
struct edwValidFile *vf1, *vf2;
if (sameString(vf->pairedEnd, "1"))
    {
    vf1 = vf;
    vf2 = otherVf;
    }
else
    {
    vf1 = otherVf;
    vf2 = vf;
    }

/* See if we already have a record for these two. */
sqlSafef(query, sizeof(query), 
    "select * from edwQaPairedEndFastq where fileId1=%u and fileId2=%u",
    vf1->fileId, vf2->fileId);
struct edwQaPairedEndFastq *pair = edwQaPairedEndFastqLoadByQuery(conn, query);
if (pair != NULL)
    {
    edwValidFileFree(&otherVf);
    return;
    }

/* Make placeholder record to help avoid race condition with other member of pair. */
sqlSafef(query, sizeof(query),
    "insert into edwQaPairedEndFastq (fileId1,fileId2) values (%u,%u)"
    , vf1->fileId, vf2->fileId);
sqlUpdate(conn, query);
unsigned id = sqlLastAutoId(conn);

/* Get target assembly and figure out path for BWA index. */
struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, vf->ucscDb);
assert(assembly != NULL);
char genoFile[PATH_LEN];
safef(genoFile, sizeof(genoFile), "%s%s/bwaData/%s.fa", 
    edwValDataDir, assembly->ucscDb, assembly->ucscDb);

/* Make alignments of subsamples. */
char *sample1 = NULL, *sample2 = NULL, *sai1 = NULL, *sai2 = NULL;
makeTmpSai(conn, vf1, genoFile, &sample1, &sai1);
makeTmpSai(conn, vf2, genoFile, &sample2, &sai2);

/* Make paired end alignment */
char *tmpSam = cloneString(rTempName(edwTempDir(), "edwPairSample", ".sam"));
char command[6*PATH_LEN];
safef(command, sizeof(command),
   "bwa sampe -n 1 -N 1 -f %s %s %s %s %s %s"
   , tmpSam, genoFile, sai1, sai2, sample1, sample2);
mustSystem(command);

/* Make ra file with pairing statistics */
char *tmpRa = cloneString(rTempName(edwTempDir(), "edwPairSample", ".ra"));
safef(command, sizeof(command), 
    "edwSamPairedEndStats -maxInsert=%d %s %s", maxInsert, tmpSam, tmpRa);
mustSystem(command);

/* Read RA file into variables. */
struct edwQaPairedEndFastq *pe = edwQaPairedEndFastqOneFromRa(tmpRa);

/* Update database to complete record. */
/* The alignment may have taken a while so let's get a new connection. */
struct sqlConnection *freshConn = edwConnectReadWrite();
sqlSafef(query, sizeof(query),
    "update edwQaPairedEndFastq set "
    " concordance=%g, distanceMean=%g, distanceStd=%g, distanceMin=%g, distanceMax=%g, "
    " recordComplete=1 where id=%u"
    , pe->concordance, pe->distanceMean, pe->distanceStd, pe->distanceMin, pe->distanceMax, id);
sqlUpdate(freshConn, query);
sqlDisconnect(&freshConn);

/* Clean up and go home. */
edwValidFileFree(&otherVf);
remove(sample1);
remove(sample2);
remove(sai1);
remove(sai2);
remove(tmpSam);
remove(tmpRa);
#ifdef SOON
#endif /* SOON */
freez(&sample1);
freez(&sample2);
freez(&sai1);
freez(&sai2);
freez(&tmpSam);
freez(&tmpRa);
edwQaPairedEndFastqFree(&pe);
edwValidFileFree(&otherVf);
}

void edwMakePairedEndQa(unsigned startId, unsigned endId)
/* edwMakePairedEndQa - Do alignments of paired-end fastq files and calculate distrubution of 
 * insert size. */
{
struct sqlConnection *conn = edwConnectReadWrite();
struct edwFile *ef, *efList = edwFileAllIntactBetween(conn, startId, endId);
for (ef = efList; ef != NULL; ef = ef->next)
    {
    struct edwValidFile *vf = edwValidFileFromFileId(conn, ef->id);
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
edwMakePairedEndQa(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
