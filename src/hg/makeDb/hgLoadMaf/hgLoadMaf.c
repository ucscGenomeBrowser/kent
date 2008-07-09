/* hgLoadMaf - Load a maf file index into the database. */
#include "common.h"
#include "cheapcgi.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "hgRelate.h"
#include "portable.h"
#include "maf.h"
#include "scoredRef.h"
#include "dystring.h"
#include "errCatch.h"

static char const rcsid[] = "$Id: hgLoadMaf.c,v 1.24 2008/07/09 18:56:43 braney Exp $";

/* Command line options */

/* multiz currently will generate some alignments that have a missing score
 * and/or contain only 1 or 2 sequences.  Webb Miller recommends these
 * be ignored, and he intends to remove them from multiz output.
 * When/if this happens, the -warn option should not be used.
 */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"warn", OPTION_BOOLEAN},
    {"WARN", OPTION_BOOLEAN},
    {"test", OPTION_STRING},
    {"pathPrefix", OPTION_STRING},
    {"loadFile", OPTION_STRING},
    {"tmpDir", OPTION_STRING},
    {"refDb", OPTION_STRING},
    {"maxNameLen", OPTION_INT},
    {"defPos", OPTION_STRING},
    {"custom", OPTION_BOOLEAN},
    {NULL, 0}
};

boolean warnOption = FALSE;
boolean warnVerboseOption = FALSE;  /* print warning detail */
char *testFile = NULL;              /* maf filename if testing */
char *pathPrefix = NULL;            /* extFile directory */
static char *tmpDir = NULL;	    /* location to create a temporary file */
char *loadFile;	                    /* file to read maf file from */
char *refDb;	                    /* reference db (for custom tracks) */
int maxNameLen = 0;	            /* maximum chrom name length */
char *defPosFile;                   /* file to put default pos in */
boolean isCustom;                   /* we're loading a custom track */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadMaf - Load a maf file index into the database\n"
  "usage:\n"
  "   hgLoadMaf database table\n"
  "options:\n"
  "   -warn            warn instead of error upon empty/incomplete alignments\n"
  "   -WARN            warn instead of error, with detail for the warning\n" 
  "   -test=infile     use infile as input, and suppress loading\n"
  "                    the database. Just create .tab file in current dir.\n" 
  "   -pathPrefix=dir  load files from specified directory \n"
  "                    (default /gbdb/database/table.\n"
  "   -tmpDir=<path>   path to directory for creation of temporary .tab file\n"
  "                    which will be removed after loading\n"
  "   -loadFile=file   use file as input\n"
  "   -maxNameLen=N    specify max chromosome name length to avoid\n"
  "                    reference to chromInfo table\n"
  "   -defPos=file     file to put default position in\n"
  "                    default position is first block\n"
  "   -custom          loading a custom track, don't use history\n"
  "                    or extFile tables\n"
  "\n"
  "NOTE: The maf files need to be in chromosome coordinates,\n"
  "      the reference species must be the first component, and \n"
  "      the blocks must be correctly ordered and be on the\n"
  "      '+' strand\n"
  );
}

struct mafComp *findComponent(struct mafAli *maf, char *prefix)
/* Find maf component with the given prefix. */
{
int preLen = strlen(prefix);
struct mafComp *mc;
for (mc = maf->components; mc != NULL; mc = mc->next)
    {
    if (memcmp(mc->src, prefix, preLen) == 0 && mc->src[preLen] == '.')
        return mc;
    }
return NULL;
}

void hgLoadMaf(char *database, char *table)
/* hgLoadMaf - Load a maf file index into the database. */
{
struct fileInfo *fileList = NULL, *fileEl;
struct sqlConnection *conn;
long mafCount = 0;
FILE *f = NULL;
char extFileDir[512];
char ext[10];
char file[100];
int indexLen;

if (refDb == NULL)
    refDb = database;

if (tmpDir == NULL)
    f = hgCreateTabFile(".", table);
else
    f = hgCreateTabFile(tmpDir, table);


if (testFile != NULL)
    {
    if (!fileExists(testFile))
        errAbort("Test file %s doesn't exist\n", testFile);
    splitPath(testFile, extFileDir, file, ext);
    strcat(file, ext);
    fileList = listDirX(extFileDir, file, TRUE);
    pathPrefix = extFileDir;
    }
else
    {
    hSetDb(database);
    if (maxNameLen)
	indexLen = maxNameLen;
    else
	indexLen = hGetMinIndexLength();

    if (loadFile != NULL)
	{
	if (!fileExists(loadFile))
	    errAbort("Load file %s doesn't exist\n", loadFile);
	splitPath(loadFile, extFileDir, file, ext);
	strcat(file, ext);
	pathPrefix = extFileDir;
	fileList = listDirX(pathPrefix, file, TRUE);
	if (fileList == NULL)
	    fileList = listDirX(".", file, TRUE);
	}
    else
	{
	if (pathPrefix == NULL)
	    {
	    safef(extFileDir, sizeof(extFileDir), 
		"/gbdb/%s/%s", database, table);
	    pathPrefix = extFileDir;
	    }
	fileList = listDirX(pathPrefix, "*.maf", TRUE);
	}

    if (isCustom)
	conn = sqlConnect(database);
    else
	conn = hgStartUpdate();

    scoredRefTableCreate(conn, table, indexLen);
    }
if (fileList == NULL)
    errAbort("%s doesn't exist or doesn't have any maf files", pathPrefix);
for (fileEl = fileList; fileEl != NULL; fileEl = fileEl->next)
    {
    char *fileName = fileEl->name;
    struct mafFile *mf;
    struct mafComp *mc;
    struct scoredRef mr;
    struct mafAli *maf;
    off_t offset;
    int warnCount = 0;
    int dbNameLen = strlen(refDb);
    HGID extId;
    if ((testFile != NULL) || isCustom)
        extId = 0;
    else
        extId = hgAddToExtFile(fileName, conn);

    struct errCatch *errCatch = errCatchNew();
    char *errMsg = NULL;
    if (errCatchStart(errCatch))
	{
	mf = mafOpen(fileName);
	verbose(1,"Indexing and tabulating %s\n", fileName);
	while ((maf = mafNextWithPos(mf, &offset)) != NULL)
	    {
	    double maxScore, minScore;
	    mc = findComponent(maf, refDb);
	    if (mc == NULL) 
		{
		char msg[256];
		safef(msg, sizeof(msg),
				"Couldn't find %s. sequence line %d of %s\n", 
				    refDb, mf->lf->lineIx, fileName);
		if (warnOption || warnVerboseOption) 
		    {
		    warnCount++;
		    if (warnVerboseOption)
			verbose(1, msg);
		    mafAliFree(&maf);
		    continue;
		    }
		else 
		    errAbort(msg);
		}

	    ZeroVar(&mr);
	    mr.chrom = mc->src + dbNameLen + 1;
	    mr.chromStart = mc->start;
	    mr.chromEnd = mc->start + mc->size;
	    if (mc->strand == '-')
		reverseUnsignedRange(&mr.chromStart, &mr.chromEnd, mc->srcSize);
	    mr.extFile = extId;
	    mr.offset = offset;

	    if (defPosFile != NULL)
		{
		FILE *f = mustOpen(defPosFile, "w");

		fprintf(f, "%s:%d-%d\n",mr.chrom, mr.chromStart+1, mr.chromEnd);
		fclose(f);

		defPosFile = NULL;
		}

	    /* The maf scores are the sum of all pairwise
	     * alignment scores.  If you have A,B,C,D
	     * species in the alignments it's the sum of
	     * the AB, AC, AD, BC, BD, CD pairwise scores.
	     * If you just had two species it would just
	     * be a single pairwise score.  Since we don't
	     * want to penalize things for missing species,
	     * we basically divide by the number of possible
	     * pairings.  To get it from the -100 to +100 (more or
	     * less) per base blastz scoring scheme to something
	     * that goes from 0 to 1, we also also divide by 200.
	     * JK */

	    /* scale using alignment size to get per-base score */
	    mr.score = maf->score/mc->size;

	    /* get scoring range -- depends on #species in alignment */
	    mafColMinMaxScore(maf, &minScore, &maxScore);
	    if (maxScore == minScore) 
		/* protect against degenerate case -- 1 species, produces
		 * max & min score of 0 */
		mr.score = 0.0;
	    else
		/* translate to get zero-based score, 
		 * then scale by blastz scoring range */
		mr.score = (mr.score-minScore)/(maxScore-minScore);
	    if (mr.score <= 0.0) 
		{
		char msg[256];
		safef(msg, sizeof(msg),
			"Score too small (raw %.1f scaled %.1f #species %d),"
			       " line %d of %s\n", 
			    maf->score, mr.score, slCount(maf->components),
				    mf->lf->lineIx, fileName);
		if (warnOption || warnVerboseOption) 
		    {
		    warnCount++;
		    if (warnVerboseOption)
			verbose(1, msg);
		    }
		mr.score = 0.001;
		}
	    if (mr.score > 1.0) mr.score = 1.0;
	    mafCount++;
	    fprintf(f, "%u\t", hFindBin(mr.chromStart, mr.chromEnd));
	    scoredRefTabOut(&mr, f);
	    mafAliFree(&maf);
	    }
	mafFileFree(&mf);
	}
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	{
	errMsg = cloneString(errCatch->message->string);
	}
    errCatchFree(&errCatch);

    if (errMsg)
	{
	if (!isCustom)
	    hgPurgeExtFile(extId,  conn);
	errAbort(errMsg);
	}

    if (warnCount)
        verbose(1, "%d warnings\n", warnCount);
    }
if (testFile != NULL)
    return;
verbose(1, "Loading %s into database\n", table);
if (tmpDir == NULL)
    hgLoadTabFile(conn, ".", table, &f);
else
    hgLoadTabFile(conn, tmpDir, table, &f);
verbose(1, "Loaded %ld mafs in %d files from %s\n", mafCount, slCount(fileList), pathPrefix);
if (isCustom)
    sqlDisconnect(&conn);
else
    hgEndUpdate(&conn, "Add %ld mafs in %d files from %s\n", mafCount, slCount(fileList), pathPrefix);

/*	if temp dir specified, unlink file to make it disappear */
if ((char *)NULL != tmpDir)
    hgUnlinkTabFile(tmpDir, table);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
warnOption = optionExists("warn");
isCustom = optionExists("custom");
warnVerboseOption = optionExists("WARN");
testFile = optionVal("test", NULL);
pathPrefix = optionVal("pathPrefix", NULL);
tmpDir = optionVal("tmpDir", NULL);
loadFile = optionVal("loadFile", loadFile);
refDb = optionVal("refDb", refDb);
maxNameLen = optionInt("maxNameLen", maxNameLen);
defPosFile = optionVal("defPos", defPosFile);

if (argc != 3)
    usage();

hgLoadMaf(argv[1], argv[2]);
return 0;
}
