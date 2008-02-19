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
  "   mafToProtein dbName mafTable frameTable organism species.lst output\n"
  "arguments:\n"
  "   dbName      name of SQL database\n"
  "   mafTable    name of maf file table\n"
  "   frameTable  name of the frames table\n"
  "   organism    name of the organism to use in frames table\n"
  "   species.lst list of species names\n"
  "   output      put output here\n"
  "options:\n"
  "   -geneName=foobar   name of gene as it appears in frameTable\n"
  "   -geneName=foolst   name of file with list of genes\n"
  "   -chrom=chr1        name of chromosome from which to grab genes\n"
  "   -frames            output frames\n"
  "   -noTrans           don't translate output into amino acids\n"
  "   -delay=N           delay N seconds between genes (default 0)\n" 
  );
}

static struct optionSpec options[] = {
   {"geneName", OPTION_STRING},
   {"geneList", OPTION_STRING},
   {"chrom", OPTION_STRING},
   {"frames", OPTION_BOOLEAN},
   {"noTrans", OPTION_BOOLEAN},
   {"delay", OPTION_INT},
   {NULL, 0},
};

char *geneName = NULL;
char *geneList = NULL;
char *onlyChrom = NULL;
boolean inFrames = FALSE;
boolean noTrans = TRUE;
int delay = 0;

struct geneInfo
{
    struct geneInfo *next;
    struct mafFrames *frame;
    struct mafAli *ali;
    int frameStart;
    int frameSize;
    char *name;
};

struct speciesInfo
{
    struct speciesInfo *next;
    char *name;
    int size;
    char *nucSequence;
    char *aaSequence;
};

/* read a list of single words from a file */
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

slReverse(&list);

lineFileClose(&lf);
return list;
}

/* query the list of gene names from the frames table */
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

/* load a set of mafFrames from the database */
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


/* get the maf alignments for a particular mafFrame */
struct mafAli *getAliForFrame(char *mafTable, struct mafFrames *frame)
{
struct sqlConnection *conn = hAllocConn();
struct mafAli *aliAll = mafLoadInRegion(conn, mafTable,
	frame->chrom, frame->chromStart, frame->chromEnd);
struct mafAli *ali;
struct mafAli *list = NULL;
struct mafAli *nextAli;

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
slReverse(&list);

hFreeConn(&conn);

return list;
}

/* allocate space for the nuc and aa sequence for each species */
struct speciesInfo *getSpeciesInfo(struct geneInfo *giList, 
    struct slName *speciesNames, struct hash *siHash)
{
struct geneInfo *gi;
int size = 0;
struct speciesInfo *siList = NULL;

for(gi = giList ; gi ; gi = gi->next)
    size += gi->frame->chromEnd - gi->frame->chromStart;

struct slName *name = speciesNames;

for(; name ; name = name->next)
    {
    struct speciesInfo *si;

    AllocVar(si);
    si->name = name->name;
    si->size = size;
    si->nucSequence = needMem(size + 1);
    memset(si->nucSequence, '-', size);
    si->aaSequence = needMem(size/3 + 1);
    hashAdd(siHash, si->name, si);
    slAddHead(&siList, si);
    }
slReverse(&siList);

return siList;
}

/* this is was meant to be an exon output, but it turns out
 * that frames don't correspond to exons 
 */
void outSpeciesFrames(FILE *f, struct speciesInfo *si, struct geneInfo *gi)
{
errAbort("haven't implemented trans with frames");
}

/* this is really only useful for debug since frames 
 * don't correspond to exons
 */
void outSpeciesFramesNoTrans(FILE *f, struct speciesInfo *si, struct geneInfo *gi)
{
int start = 0;
int exonNum = 0;
int end;

for(; gi; gi = gi->next, exonNum++)
    {
    struct speciesInfo *siTemp = si;

    end = start + gi->frameSize;

    for(; siTemp ; siTemp = siTemp->next)
	{
	fprintf(f, ">%s.%s-%d\n",gi->name, siTemp->name, exonNum);
	for (; start < end; start++)
	    fprintf(f, "%c", siTemp->nucSequence[start]);
	fprintf(f, "\n");
	}
    }
}

/* translate a nuc sequence into amino acids. If there
 * are any dashes in any of the three nuc positions
 * make the AA a dash.
 */
aaSeq *doTranslate(struct dnaSeq *inSeq, unsigned offset, 
    unsigned inSize, boolean stop)
{
aaSeq *seq;
DNA *dna = inSeq->dna;
AA *pep, aa;
int i, lastCodon;
int actualSize = 0;

assert(offset <= inSeq->size);
if ((inSize == 0) || (inSize > (inSeq->size - offset)))
    inSize = inSeq->size - offset;
lastCodon = offset + inSize - 3;

AllocVar(seq);
seq->dna = pep = needLargeMem(inSize/3+1);
for (i=offset; i <= lastCodon; i += 3)
    {
    aa = lookupCodon(dna+i);
    if (aa == 'X')
	{
	if ((dna[i] == '-') ||
	    (dna[i+1] == '-') ||
	    (dna[i+2] == '-'))
	    aa = '-';
	}
    if (aa == 0)
	{
        if (stop)
	    break;
	else
	    aa = 'Z';
	}
    *pep++ = aa;
    ++actualSize;
    }
*pep = 0;
assert(actualSize <= inSize/3+1);
seq->size = actualSize;
seq->name = cloneString(inSeq->name);
return seq;
}

/* translate nuc sequence into an sequence of amino acids */
void translateProtein(struct speciesInfo *si)
{
struct dnaSeq thisSeq;
aaSeq *outSeq;

thisSeq.dna = si->nucSequence;
thisSeq.size = si->size;
outSeq =  doTranslate(&thisSeq, 0,  0, FALSE);
si->aaSequence  = outSeq->dna;
}

/* is the sequence all dashes ? */
boolean allDashes(char *seq)
{
while (*seq)
    if (*seq++ != '-')
	return FALSE;

return TRUE;
}

/* output a particular species sequence to the file stream */
void writeOutSpecies(FILE *f, struct speciesInfo *si, struct geneInfo *gi)
{
if (noTrans)
    {
    if (inFrames)
	outSpeciesFramesNoTrans(f, si, gi);
    else
	{
	for(; si ; si = si->next)
	    {
	    fprintf(f, ">%s-%s\n", gi->name, si->name);
	    fprintf(f, "%s\n", si->nucSequence);
	    }
	}
    }
else
    {
    if (inFrames)
	outSpeciesFrames(f, si, gi);
    else
	{
	for(; si ; si = si->next)
	    {
	    translateProtein(si);
	    if (!allDashes(si->aaSequence))
		{
		fprintf(f, ">%s-%s\n", gi->name, si->name);
		fprintf(f, "%s\n", si->aaSequence);
		}
	    }
	}
    }
}

/* rather than allocate space for these every time,
 * just re-use a global resource
 */
#define MAX_COMPS  	5000 
char *compText[MAX_COMPS]; 
char *siText[MAX_COMPS]; 

/* copy the maf alignments into the species sequence buffers.
 * remove all the dashes from the reference sequence, and collapse
 * all the separate maf blocks into one sequence
 */
int copyAli(struct hash *siHash, struct mafAli *ali, int start)
{
struct mafComp *comp = ali->components;
int num = 0;

for(; comp; comp = comp->next)
    {
    char *ptr = strchr(comp->src, '.');

    if (ptr == NULL)
	errAbort("all components must have a '.'");

    *ptr = 0;

    struct speciesInfo *si = hashFindVal(siHash, comp->src);
    if (si == NULL)
	continue;

    compText[num] = comp->text;
    siText[num] = &si->nucSequence[start];
    ++num;
    if (num == MAX_COMPS)
	errAbort("can only deal with maf's with less than %d components",
	    MAX_COMPS);
    }

int count = 0; 
int ii, jj;

for(jj = 0 ; jj < ali->textSize; jj++)
    {
    if (*compText[0] != '-')
	{
	for(ii=0; ii < num; ii++)
	    {
	    if (compText[ii] != NULL)
		*siText[ii] = *compText[ii];
	    siText[ii]++;
	    }
	count++;
	}
    for(ii=0; ii < num; ii++)
	if (compText[ii] != NULL)
	    compText[ii]++;
    }

return start + count;
}

/* copyMafs - copy all the maf alignments into 
 * one sequence for each species
 */
void copyMafs(struct hash *siHash, struct geneInfo *giList)
{
int start = 0;
struct geneInfo *gi = giList;

for(; gi; gi = gi->next)
    {
    struct mafAli *ali = gi->ali;

    gi->frameStart = start;
    for(; ali; ali = ali->next)
	start = copyAli(siHash, ali, start);

    gi->frameSize = start - gi->frameStart;
    }

struct mafComp *comp = giList->ali->components;
boolean frameNeg = (giList->frame->strand[0] == '-');

if (frameNeg)
    {
    int size = 0;

    for(; comp; comp = comp->next)
	{
	struct speciesInfo *si = hashFindVal(siHash, comp->src);

	if (si == NULL)
	    continue;

	size = si->size;
	reverseComplement(si->nucSequence, si->size);
	}

    gi = giList;
    for(; gi; gi = gi->next)
	{
	gi->frameStart = size - (gi->frameStart + gi->frameSize);
	}
    }
}

/* free species info list */
void freeSpeciesInfo(struct speciesInfo *list)
{
struct speciesInfo *siNext;

for(; list ; list = siNext)
    {
    siNext = list->next;

    freez(&list->nucSequence);
    freez(&list->aaSequence);
    }
}

/* free geneInfo list */
void freeGIList(struct geneInfo *list)
{
struct geneInfo *giNext;

for(; list ; list = giNext)
    {
    giNext = list->next;

    mafFramesFreeList(&list->frame);
    mafAliFreeList(&list->ali);
    }
}
 
/* output the sequence for one gene for every species to 
 * the file stream 
 */
void outGene(FILE *f, char *geneName, char *mafTable, char *frameTable, 
    char *org, struct slName *speciesNameList)
{
struct mafFrames *frames = getFrames(geneName, frameTable, org);
struct mafFrames *frame, *nextFrame;
struct geneInfo *giList = NULL;

for(frame = frames; frame; frame = nextFrame)
    {
    struct geneInfo *gi;

    AllocVar(gi);
    nextFrame = frame->next;
    frame->next = NULL;
    gi->frame = frame;
    gi->name = frame->name;
    gi->ali = getAliForFrame(mafTable, frame);
    slAddHead(&giList, gi);
    }
slReverse(&giList);

struct hash *speciesInfoHash = newHash(5);
struct speciesInfo *speciesList = getSpeciesInfo(giList, speciesNameList, 
    speciesInfoHash);

copyMafs(speciesInfoHash, giList);
writeOutSpecies(f, speciesList, giList);

freeSpeciesInfo(speciesList);
freeGIList(giList);
}

void mafToProtein(char *dbName, char *mafTable, char *frameTable, 
    char *org,  char *speciesList, char *outName)
/* mafToProtein - output protein alignments using maf and frames. */
{
struct slName *geneNames = NULL;
struct slName *speciesNames = readList(speciesList);
FILE *f = mustOpen(outName, "w");

hSetDb(dbName);

if (geneList != NULL)
    geneNames = readList(geneList);
else if (geneName != NULL)
    {
    int len = strlen(geneName);
    geneNames = needMem(sizeof(*geneNames)+len);
    strcpy(geneNames->name, geneName);
    }
else
    geneNames = queryNames(dbName, frameTable, org);

for(; geneNames; geneNames = geneNames->next)
    {
    verbose(2, "outting  gene %s \n",geneNames->name);
    outGene(f, geneNames->name, mafTable, frameTable, org, speciesNames);
    if (delay)
	{
	verbose(2, "delaying %d seconds\n",delay);
	sleep(delay);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);

if (argc != 7)
    usage();

geneName = optionVal("geneName", geneName);
geneList = optionVal("geneList", geneList);
onlyChrom = optionVal("chrom", onlyChrom);
inFrames = optionExists("frames");
noTrans = optionExists("noTrans");
delay = optionInt("delay", delay);

if ((geneName != NULL) && (geneList != NULL))
    errAbort("cannot specify both geneList and geneName");

mafToProtein(argv[1],argv[2],argv[3],argv[4],argv[5],argv[6]);
return 0;
}
