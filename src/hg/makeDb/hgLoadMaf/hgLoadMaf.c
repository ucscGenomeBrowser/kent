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

static char const rcsid[] = "$Id: hgLoadMaf.c,v 1.8 2003/07/28 22:56:20 kate Exp $";

/* Command line options */

/* multiz currently will generate some alignments that have a missing score
 * and/or contain only 1 or 2 sequences.  Webb Miller recommends these
 * be ignored, and he intends to remove them from multiz output.
 * When/if this happens, the -warn option should not be used.
 */
boolean warnOption = FALSE;
boolean warnVerboseOption = FALSE;     /* print warning detail */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadMaf - Load a maf file index into the database\n"
  "usage:\n"
  "   hgLoadMaf database table\n"
  "options:\n"
    "   -warn   warn instead of error if empty/incomplete alignments\n"
    "           are found found.\n"
    "   -WARN   warn instead of error, with detail for the warning\n" 
  "The maf files need to already exist in chromosome coordinates\n"
  "in the directory /gbdb/database/table.\n"
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
int i;
char extFileDir[512];
struct fileInfo *fileList = NULL, *fileEl;
struct sqlConnection *conn;
long mafCount = 0;
FILE *f = hgCreateTabFile(".", table);

hSetDb(database);
safef(extFileDir, sizeof(extFileDir), "/gbdb/%s/%s", database, table);
fileList = listDirX(extFileDir, "*.maf", TRUE);
conn = hgStartUpdate();
scoredRefTableCreate(conn, table);
if (fileList == NULL)
    errAbort("%s doesn't exist or doesn't have any maf files", extFileDir);
for (fileEl = fileList; fileEl != NULL; fileEl = fileEl->next)
    {
    char *fileName = fileEl->name;
    struct mafFile *mf;
    struct mafComp *mc;
    struct scoredRef mr;
    struct mafAli *maf;
    off_t offset;
    int warnCount = 0;
    HGID extId = hgAddToExtFile(fileName, conn);
    int dbNameLen = strlen(database);

    mf = mafOpen(fileName);
    printf("Indexing and tabulating %s\n", fileName);
    while ((maf = mafNextWithPos(mf, &offset)) != NULL)
        {
	double maxScore, minScore;
	mafColMinMaxScore(maf, &minScore, &maxScore);
	++mafCount;
	mc = findComponent(maf, database);
	if (mc == NULL) 
            {
            if (warnOption || warnVerboseOption) 
                {
                warnCount++;
                if (warnVerboseOption)
                    printf("Couldn't find %s. sequence line %d of %s\n", 
	    	                database, mf->lf->lineIx, fileName);
                mafAliFree(&maf);
                continue;
                }
            else 
                errAbort("Couldn't find %s. sequence line %d of %s\n", 
	    	                database, mf->lf->lineIx, fileName);
            }
	ZeroVar(&mr);
	mr.chrom = mc->src + dbNameLen + 1;
	mr.chromStart = mc->start;
	mr.chromEnd = mc->start + mc->size;
	if (mc->strand == '-')
	    reverseIntRange(&mr.chromStart, &mr.chromEnd, mc->srcSize);
	mr.extFile = extId;
	mr.offset = offset;
	mr.score = maf->score/mc->size;
	mr.score = (mr.score-minScore)/(maxScore-minScore);
	if (mr.score > 1.0) mr.score = 1.0;
	if (mr.score < 0.0) mr.score = 0.0;
	fprintf(f, "%u\t", hFindBin(mr.chromStart, mr.chromEnd));
	scoredRefTabOut(&mr, f);
	mafAliFree(&maf);
	}
    mafFileFree(&mf);
    if (warnCount)
        printf("%d warnings\n", warnCount);
    }
printf("Loading %s into database\n", table);
hgLoadTabFile(conn, ".", table, &f);
hgEndUpdate(&conn, "Add %ld mafs in %d files from %s", mafCount, slCount(fileList), extFileDir);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
warnOption = cgiBoolean("warn");
warnVerboseOption = cgiBoolean("WARN");
hgLoadMaf(argv[1], argv[2]);
return 0;
}
