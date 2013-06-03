/* clonePos.c - create table for clonePos. */
#include "common.h"
#include "linefile.h"
#include "portable.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "psl.h"
#include "glDbRep.h"


char *mapFile = "/projects/compbio/experiments/hg/clonemap.wu/jun15a.wu";
char *infFile = "/projects/cc/hg/gs.2/ffa/sequence.inf";
char *posName = "clonePos.tab";
char *aliName = "cloneAliPos.tab";

void usage()
/* Explain usage and exit. */
{
errAbort(
 "clonePos - make clonePos and cloneAliPos tables\n"
 "usage:\n"
 "   clonePos here\n"
 "This will make clonePos.tab and cloneAliPos.tab in the current dir");
}

struct cloneInfo
/* Info on one clone. */
    {
    struct cloneInfo *next;	/* Next in list. */
    char *name;			/* Name of clone. */
    int size;                   /* Clone size. */
    int phase;                  /* HTG Phase. */
    struct clonePos *pos;       /* Position in chromosome. */
    struct clonePos *aliPos;    /* Position of alignment in chromosome. */
    };

struct clonePos
/* Position of a clone. */
    {
    struct clonePos *next;	/* Next in list. */
    struct cloneInfo *info;     /* Info about clone. */
    int start, end;             /* Offset within contig. */
    };

int cmpClonePos(const void *va, const void *vb)
/* Compare two slNames. */
{
const struct clonePos *a = *((struct clonePos **)va);
const struct clonePos *b = *((struct clonePos **)vb);
return a->start - b->start;
}


void readSeqInfo(char *fileName, struct hash **pHash, struct cloneInfo **pList)
/* Read info about clones. */
{
struct lineFile *lf;
struct hash *hash = newHash(16);
struct hashEl *hel;
struct cloneInfo *infoList = NULL, *info;
int lineSize, wordCount;
char *line, *words[16];
char *acc;
char c;

lf = lineFileOpen(fileName, TRUE);
if (!lineFileNext(lf, &line, &lineSize))
    errAbort("Empty %s", fileName);
if (!startsWith("#Accession.ver", line))
    errAbort("Unrecognized format on %s", fileName);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
	continue;
    wordCount = chopLine(line,words);
    if (wordCount != 8)
	errAbort("Expecting 8 words line %d of %s", lf->lineIx, lf->fileName);
    AllocVar(info);
    acc = words[0];
    if (hashLookup(hash, acc) != NULL)
	errAbort("Duplicate %s line %d of %s", acc, lf->lineIx, lf->fileName);
    hel = hashAdd(hash, acc, info);
    info->name = hel->name;
    info->size = atoi(words[2]);
    if (info->size == 0)
	errAbort("Expecting clone size field 2 of line %d of %s", 
		lf->lineIx, lf->fileName);
    c = words[3][0];
    if (c == '0' || c == '1' || c == '2' || c == '3')
	info->phase = atoi(words[3]);
    else
	errAbort("Expecting phase field 3 of line %d of %s",
		lf->lineIx, lf->fileName);
    slAddHead(&infoList, info);
    }
lineFileClose(&lf);
slReverse(&infoList);
*pList = infoList;
*pHash = hash; 
}

struct cloneInfo *findClone(struct hash *cloneHash, char *name)
/* Find named clone in hash table. */
{
struct hashEl *hel;
if ((hel = hashLookup(cloneHash, name)) == NULL)
    errAbort("Clone %s is not in %s", name, infFile);
return hel->val;
}

void fragNameToCloneName(char *cloneName)
/* Chop off suffix to get clone name. */
{
char *s = strchr(cloneName, '_');
if (s != NULL)
    *s = 0;
}

void writePosList(char *fileName, struct clonePos *posList, char *chromName)
/* Write out tab-delimited position list. */
{
struct clonePos *pos;
struct cloneInfo *info;
FILE *f = mustOpen(fileName, "w");

printf("Writing %d entries to %s\n", slCount(posList), fileName);
for (pos = posList; pos != NULL; pos = pos->next)
    {
    info = pos->info;
    fprintf(f, "%s\t%d\t%d\t%s\t%d\t%d\n", 
    	info->name, info->size, info->phase, chromName, pos->start, pos->end);
    }
fclose(f);
}

void clonePosTab(char *fileName, struct hash *cloneHash)
/* Write out clonePos.tab. */
{
char query[256];
struct sqlResult *sr;
char **row;
struct clonePos *posList = NULL, *pos;
struct gl gl;
struct cloneInfo *info;
struct sqlConnection *conn = hAllocConn();

sqlSafef(query, sizeof query, "select * from chr18_gl");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    glStaticLoad(row, &gl);
    fragNameToCloneName(gl.frag);
    info = findClone(cloneHash, gl.frag);
    if ((pos = info->pos) == NULL)
	{
	AllocVar(pos);
	pos->info = info;
	info->pos = pos;
	pos->start = gl.start;
	pos->end = gl.end;
	slAddHead(&posList, pos);
	}
    else
	{
	if (pos->start > gl.start)
	    pos->start = gl.start;
	if (pos->end < gl.end)
	    pos->end = gl.end;
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slSort(&posList, cmpClonePos);
writePosList(fileName, posList, "chr18");
}

void cloneAliPosTab(char *fileName, struct hash *cloneHash)
/* Write out clonePos.tab. */
{
char query[256];
struct sqlResult *sr;
char **row;
struct clonePos *posList = NULL, *pos;
struct cloneInfo *info;
struct sqlConnection *conn = hAllocConn();

sqlSafef(query, sizeof query, "select * from chr18_frags");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct psl *psl = pslLoad(row);
    fragNameToCloneName(psl->qName);
    info = findClone(cloneHash, psl->qName);
    if ((pos = info->aliPos) == NULL)
	{
	AllocVar(pos);
	pos->info = info;
	info->aliPos = pos;
	pos->start = psl->tStart;
	pos->end = psl->tEnd;
	slAddHead(&posList, pos);
	}
    else
	{
	if (pos->start > psl->tStart)
	    pos->start = psl->tStart;
	if (pos->end < psl->tEnd)
	    pos->end = psl->tEnd;
	}
    pslFree(&psl);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slSort(&posList, cmpClonePos);
writePosList(fileName, posList, "chr18");
}

void clonePos()
/* create tables for clonePos. */
{
struct cloneInfo *infoList, *info;
struct clonePos *aliList, *posList, *pos;
struct hash *cloneHash;

uglyf("mysqlHost is %s\n", mysqlHost());

readSeqInfo(infFile, &cloneHash, &infoList);
clonePosTab(posName, cloneHash);
cloneAliPosTab(aliName, cloneHash);
}

int main(int argc, char *argv[])
{
if (argc != 2)
    usage();
clonePos();
}
