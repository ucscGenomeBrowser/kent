/* hgLoadMaf - Load a maf file index into the database. */
#include "common.h"
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

static char const rcsid[] = "$Id: hgLoadMaf.c,v 1.7 2003/05/16 15:26:48 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadMaf - Load a maf file index into the database\n"
  "usage:\n"
  "   hgLoadMaf database table\n"
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
	    errAbort("Couldn't find %s. sequence line %d of %s\n", 
	    	database, mf->lf->lineIx, fileName);
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
    }
printf("Loading %s into database\n", table);
hgLoadTabFile(conn, ".", table, &f);
hgEndUpdate(&conn, "Add %ld mafs in %d files from %s", mafCount, slCount(fileList), extFileDir);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
hgLoadMaf(argv[1], argv[2]);
return 0;
}
