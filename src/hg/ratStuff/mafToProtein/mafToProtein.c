/* mafToProtein - output protein alignments using maf and frames. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"
#include "mafFrames.h"
#include "maf.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafToProtein - output protein alignments using maf and frames\n"
  "usage:\n"
  "   mafToProtein dbName mafTable frameTable organism output\n"
  "arguments:\n"
  "   dbName     name of SQL database\n"
  "   mafTable   name of maf file table\n"
  "   frameTable name of the frames table\n"
  "   organism   name of the organism to use in frames table\n"
  "   output     put output here\n"
  "options:\n"
  "   -geneName=foobar   name of gene as it appears in frameTable\n"
  "   -geneName=foolst   name of file with list of genes\n"
  "   -chrom=chr1        name of chromosome from which to grab genes\n"
  );
}

static struct optionSpec options[] = {
   {"geneName", OPTION_STRING},
   {"geneList", OPTION_STRING},
   {"chrom", OPTION_STRING},
   {NULL, 0},
};

char *geneName = NULL;
char *geneList = NULL;
char *onlyChrom = NULL;

struct geneInfo
{
    struct geneInfo *next;
    struct mafFrames *frame;
    struct mafAli *ali;
};

struct speciesInfo
{
    struct speciesInfo *next;
    char *name;
    int size;
    char *nucSequence;
    char *aaSequence;
};

struct slName *readList(char *fileName)
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct slName *list = NULL;
char *row[1];

while (lineFileRow(lf, row))
    {
    int len = strlen(row[0]);
    struct slName *sn = needMem(sizeof(*sn)+len);
    strcpy(sn->name, row[0]);
    slAddHead(&list, sn);
    }

lineFileClose(&lf);
return list;
}

struct slName *queryNames(char *dbName, char *frameTable, char *org)
{
struct sqlConnection *conn = hAllocConn();
struct slName *list = NULL;
char query[1024];
struct sqlResult *sr = NULL;
char **row;

if (onlyChrom != NULL)
    safef(query, sizeof query, 
	"select distinct(name) from %s where src='%s' and chrom='%s'\n",
	frameTable, org, onlyChrom);
else
    safef(query, sizeof query, 
	"select distinct(name) from %s where src='%s'\n",
	frameTable, org);

sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    int len = strlen(row[0]);
    struct slName *sn = needMem(sizeof(*sn)+len);
    strcpy(sn->name, row[0]);
    slAddHead(&list, sn);
    }
sr = sqlGetResult(conn, query);

sqlFreeResult(&sr);
hFreeConn(&conn);

return list;
}

struct mafFrames *getFrames(char *geneName, char *frameTable, char *org)
{
struct sqlConnection *conn = hAllocConn();
struct mafFrames *list = NULL;
char query[1024];
struct sqlResult *sr = NULL;
char **row;

safef(query, sizeof query, 
	"select * from %s where src='%s' and name='%s' \n",
	frameTable, org, geneName);

sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct mafFrames *frame = mafFramesLoad(&row[1]);

    slAddHead(&list, frame);
    }

if (list == NULL)
    errAbort("no frames for gene %s in %s with org %s \n",geneName,
	frameTable, org);

slReverse(&list);

sqlFreeResult(&sr);
hFreeConn(&conn);

return list;
}


struct mafAli *getAliForFrame(char *mafTable, struct mafFrames *frame)
{
struct sqlConnection *conn = hAllocConn();
struct mafAli *aliAll = mafLoadInRegion(conn, mafTable,
	frame->chrom, frame->chromStart, frame->chromEnd);
struct mafAli *ali;
struct mafAli *list = NULL;
struct mafAli *nextAli;

printf("getting alis for %s:%d-%d\n",frame->chrom, 
    frame->chromStart + 1, frame->chromEnd);
for(ali = aliAll; ali; ali = nextAli)
    {
    nextAli = ali->next;
    ali->next = NULL;

    char *masterSrc = ali->components->src;
    struct mafAli *subAli = NULL;

    if (mafNeedSubset(ali, masterSrc, frame->chromStart, frame->chromEnd))
	{
	subAli = mafSubset( ali, masterSrc, 
	    frame->chromStart, frame->chromEnd);
	if (subAli == NULL)
	    continue;
	}

    if (subAli)
	{
	slAddHead(&list, subAli);
	mafAliFree(&ali);
	}
    else
	slAddHead(&list, ali);
    }

hFreeConn(&conn);

return list;
}

struct speciesInfo *getSpeciesList(struct geneInfo *giList)
{
struct hash *nameHash = newHash(5);
struct geneInfo *gi;
int size = 0;

for(gi = giList ; gi ; gi = gi->next)
    {
    struct mafAli *ali = gi->ali;

    size += gi->frame->chromEnd - gi->frame->chromStart;

    for(; ali; ali = ali->next)
	{
	struct mafComp *comp = ali->components;

	for(; comp ; comp = comp->next)
	    {
	    char *ptr = strchr(comp->src, '.');

	    if (ptr == NULL)
		errAbort("expect all maf components src names to have '.'");

	    *ptr = 0;
	    hashStoreName(nameHash, comp->src);
	    *ptr = '.';
	    }
	}
    }

struct speciesInfo *siList = NULL;
struct hashCookie cookie = hashFirst(nameHash);
struct hashEl *hel;

while((hel = hashNext(&cookie)) != NULL)
    {
    struct speciesInfo *si;

    AllocVar(si);
    si->name = hel->name;
    si->size = size;
    si->nucSequence = needMem(size + 1);
    si->aaSequence = needMem(size/3 + 1);

    slAddHead(&siList, si);
    }

return siList;
}

void writeOutSpecies(FILE *f, struct speciesInfo *si)
{
for(; si ; si = si->next)
    fprintf(f, "%s %d\n", si->name, si->size);
}

void outGene(char *geneName, char *mafTable, char *frameTable, 
    char *org, char *outName)
{
struct mafFrames *frames = getFrames(geneName, frameTable, org);
struct mafFrames *frame;
//struct sqlConnection *conn = hAllocConn();
FILE *f = mustOpen(outName, "w");
struct geneInfo *giList = NULL;

for(frame = frames; frame; frame = frame->next)
    {
    struct geneInfo *gi;

    AllocVar(gi);
    gi->frame = frame;
    gi->ali = getAliForFrame(mafTable, frame);
    slAddHead(&giList, gi);
    }

struct speciesInfo *speciesList = getSpeciesList(giList);

writeOutSpecies(f, speciesList);

//hFreeConn(&conn);
}

void mafToProtein(char *dbName, char *mafTable, char *frameTable, 
    char *org, char *outName)
/* mafToProtein - output protein alignments using maf and frames. */
{
struct slName *names = NULL;

hSetDb(dbName);

if (geneList != NULL)
    names = readList(geneList);
else if (geneName != NULL)
    {
    int len = strlen(geneName);
    names = needMem(sizeof(*names)+len);
    strcpy(names->name, geneName);
    }
else
    names = queryNames(dbName, frameTable, org);

for(; names; names = names->next)
    outGene(names->name, mafTable, frameTable, org, outName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);

if (argc != 6)
    usage();

geneName = optionVal("geneName", geneName);
geneList = optionVal("geneList", geneList);
onlyChrom = optionVal("chrom", onlyChrom);

if ((geneName != NULL) && (geneList != NULL))
    errAbort("cannot specify both geneList and geneName");

mafToProtein(argv[1],argv[2],argv[3],argv[4],argv[5]);
return 0;
}
