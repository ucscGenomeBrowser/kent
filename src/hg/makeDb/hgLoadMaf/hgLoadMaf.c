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

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadMaf - Load a maf file index into the database\n"
  "usage:\n"
  "   hgLoadMaf database table\n"
  "The maf files need to already exist in chromosome coordinates\n"
  "in the directory /gbdb/database/table.  No file should be larger\n"
  "than four gig.\n"
  );
}

char *createString =
"#High level information about a multiple alignment. Link to details in maf file.\n"
"CREATE TABLE %s (\n"
"    bin smallint unsigned not null,\n"
"    chrom varchar(255) not null,	# Chromosome (this species)\n"
"    chromStart int unsigned not null,	# Start position in chromosome (forward strand)\n"
"    chromEnd int unsigned not null,	# End position in chromosome\n"
"    extFile int unsigned not null,	# Pointer to associated MAF file\n"
"    offset bigint not null,	# Offset in MAF file\n"
"    score float not null,	# Score\n"
"              #Indices\n"
"    INDEX(chrom(8),bin),\n"
"    INDEX(chrom(8),chromStart),\n"
"    INDEX(chrom(8),chromStart)\n"
")\n";

int addToExtFile(char *path, struct sqlConnection *conn)
/* Add entry to ext file table.  Delete it if it already exists. 
 * Returns extFile id. */
{
char root[128], ext[64], name[256];
struct dyString *dy = newDyString(1024);
long long size = fileSize(path);
HGID id = hgNextId();

/* Construct file name without the directory. */
splitPath(path, NULL, root, ext);
safef(name, sizeof(name), "%s%s", root, ext);

/* Delete it from database. */
dyStringPrintf(dy, "delete from extFile where path = '%s'", path);
sqlUpdate(conn, dy->string);

/* Add it to table. */
dyStringClear(dy);
dyStringPrintf(dy, "INSERT into extFile VALUES(%u,'%s','%s',%lld)",
    id, name, path, size);
sqlUpdate(conn, dy->string);

dyStringFree(&dy);
return id;
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
struct dyString *dy = newDyString(1024);

hSetDb(database);
safef(extFileDir, sizeof(extFileDir), "/gbdb/%s/%s", database, table);
fileList = listDirX(extFileDir, "*.maf", TRUE);
conn = hgStartUpdate();
dyStringPrintf(dy, createString, table);
sqlRemakeTable(conn, table, dy->string);
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
    HGID extId = addToExtFile(fileName, conn);
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
