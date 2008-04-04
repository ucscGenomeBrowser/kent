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
  "   -exons             output exons\n"
  "   -noTrans           don't translate output into amino acids\n"
  "   -delay=N           delay N seconds between genes (default 0)\n" 
  "   -transUC           use kgXref table to translate uc* names\n"
  );
}

static struct optionSpec options[] = {
   {"geneName", OPTION_STRING},
   {"geneList", OPTION_STRING},
   {"chrom", OPTION_STRING},
   {"exons", OPTION_BOOLEAN},
   {"noTrans", OPTION_BOOLEAN},
   {"transUC", OPTION_BOOLEAN},
   {"delay", OPTION_INT},
   {NULL, 0},
};

char *geneName = NULL;
char *geneList = NULL;
char *onlyChrom = NULL;
boolean inExons = FALSE;
boolean noTrans = TRUE;
boolean transUC = FALSE;
int delay = 0;
boolean newTableType;

struct exonInfo
{
    struct exonInfo *next;
    struct mafFrames *frame;
    struct mafAli *ali;
    int exonStart;
    int exonSize;
    int chromStart;
    int chromEnd;
    char *name;
};

struct speciesInfo
{
    struct speciesInfo *next;
    char *name;
    int size;
    char *nucSequence;
    char *aaSequence;
    int aaSize;
};

/* is the sequence all dashes ? */
boolean allDashes(char *seq)
{
while (*seq)
    if (*seq++ != '-')
	return FALSE;

return TRUE;
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
return seq;
}
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
    struct mafFrames *frame;
    
    if (newTableType)
	frame = mafFramesLoad(&row[1]);
    else
	frame = mafFramesLoadOld(&row[1]);

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
	frame->chrom, frame->chromStart, frame->chromEnd );
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

int size = 0;
for(ali = list; ali; ali = ali->next)
    {
    size += ali->components->size;
    }
assert(size == frame->chromEnd - frame->chromStart);

hFreeConn(&conn);

return list;
}

/* allocate space for the nuc and aa sequence for each species */
struct speciesInfo *getSpeciesInfo(struct exonInfo *giList, 
    struct slName *speciesNames, struct hash *siHash)
{
struct exonInfo *gi;
int size = 0;
struct speciesInfo *siList = NULL;

for(gi = giList ; gi ; gi = gi->next)
    size += gi->exonSize;

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

#define MAX_EXON_SIZE 100 * 1024
char exonBuffer[MAX_EXON_SIZE];

char geneNameBuffer[5000];

char *getGeneName(char *ucName)
{
struct sqlConnection *conn = hAllocConn();
char query[1024];
struct sqlResult *sr = NULL;
char **row;

safef(query, sizeof query, 
    "select geneSymbol from kgXref where kgID='%s'\n", ucName);
sr = sqlGetResult(conn, query);

if ((row = sqlNextRow(sr)) == NULL)
    {
    hFreeConn(&conn);
    return NULL;
    }

safef(geneNameBuffer, sizeof geneNameBuffer, "%s", row[0]);
sqlFreeResult(&sr);
hFreeConn(&conn);
return geneNameBuffer;
}

void maybePrintGeneName(char *name, FILE *f)
{
if (transUC)
    {
    char *geneName = getGeneName(name);
    fprintf(f, " %s", geneName);
    }
}

void outSpeciesExons(FILE *f, char *dbName, struct speciesInfo *si, 
    struct exonInfo *giList)
{
int exonNum = 1;
struct dnaSeq thisSeq;
aaSeq *outSeq;
int exonCount = 0;
struct exonInfo *gi = giList;

for(; gi; gi = gi->next)
    exonCount++;

for(gi = giList; gi; gi = gi->next, exonNum++)
    {
    struct speciesInfo *siTemp = si;
    if (gi->frame->strand[0] == '-')
	slReverse(&gi->frame);

    struct mafFrames *startFrame = gi->frame;
    assert(startFrame->isExonStart == TRUE);
    struct mafFrames *lastFrame = startFrame;
    assert(gi->exonSize < MAX_EXON_SIZE);

    while(lastFrame->next)
	lastFrame = lastFrame->next;
    assert(lastFrame->isExonEnd == TRUE);

    for(; siTemp ; siTemp = siTemp->next)
	{
	char *ptr = exonBuffer;

	switch(startFrame->frame)
	    {
	    case 0: /* just copy the sequence over */
		memcpy(ptr, 
		    &siTemp->nucSequence[gi->exonStart], gi->exonSize);
		ptr += gi->exonSize;
		break;
	    case 1: /* we need to grab one nuc from the end 
	             * of the previous exon */
		*ptr++ = siTemp->nucSequence[gi->exonStart - 1];
		memcpy(ptr, 
		    &siTemp->nucSequence[gi->exonStart], gi->exonSize);
		ptr += gi->exonSize;
		break;

	    case 2: /* we need to delete the first nuc from this exon
	             * because we included it on the last exon */
		memcpy(ptr, 
		    &siTemp->nucSequence[gi->exonStart+1], gi->exonSize - 1);
		ptr += gi->exonSize - 1;
		break;
	    }

	int lastFrame = (startFrame->frame + gi->exonSize) % 3;
	if (lastFrame == 1) /* delete the last nucleotide */
	    --ptr;
	else if (lastFrame == 2) /* add one more nucleotide from
	                          * the next exon */
	    *ptr++ = siTemp->nucSequence[gi->exonStart + gi->exonSize];
	*ptr++ = 0;   /* null terminate */

	thisSeq.dna = exonBuffer;
	thisSeq.size = ptr - exonBuffer;
	outSeq =  doTranslate(&thisSeq, 0,  0, FALSE);
	if (!allDashes(outSeq->dna))
	    {
	    fprintf(f, ">%s_%s_%d_%d %d %d %d %s.%s:%d-%d %c",
		gi->name, 
		siTemp->name, exonNum, exonCount, 
		outSeq->size,
		startFrame->frame, lastFrame,
		dbName, gi->frame->chrom,
		gi->chromStart+1, gi->chromEnd, startFrame->strand[0]);

	    maybePrintGeneName(gi->name, f);

	    fprintf(f, "\n%s\n",  outSeq->dna);
	    }
	}
    fprintf(f, "\n");
    }
fprintf(f, "\n");
}

void outSpeciesExonsNoTrans(FILE *f, char *dbName, struct speciesInfo *si, 
    struct exonInfo *giList)
{
int exonNum = 1;
int exonCount = 0;
struct exonInfo *gi;

for(gi = giList; gi; gi = gi->next)
    exonCount++;

for(gi = giList; gi; gi = gi->next, exonNum++)
    {
    struct speciesInfo *siTemp = si;

    for(; siTemp ; siTemp = siTemp->next)
	{
	int start = gi->exonStart;
	int end = start + gi->exonSize;
	char *ptr = &siTemp->nucSequence[gi->exonStart];

	for (; start < end; start++)
	    if (*ptr != '-')
		break;
	if (start == end)
	    continue;

	start = gi->exonStart;
	fprintf(f, ">%s_%s_%d_%d %d %s.%s:%d-%d %c",
	    gi->name, 
	    siTemp->name, exonNum, exonCount, 
	    gi->exonSize,
	    dbName, gi->frame->chrom,
	    gi->chromStart+1, gi->chromEnd, gi->frame->strand[0]);

	maybePrintGeneName(gi->name, f);

	fprintf(f, "\n");
	for (; start < end; start++)
	    fprintf(f, "%c", *ptr++);
	fprintf(f, "\n");
	}
    }
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
si->aaSize = outSeq->size;
}

/* output a particular species sequence to the file stream */
void writeOutSpecies(FILE *f, char *dbName, struct speciesInfo *si, 
    struct exonInfo *giList)
{
if (inExons)
    {
    if (noTrans)
	outSpeciesExonsNoTrans(f, dbName, si, giList);
    else
	outSpeciesExons(f, dbName, si, giList);
    return;
    }

struct exonInfo *lastGi;
int start = giList->chromStart + 1;

for(lastGi = giList; lastGi->next ; lastGi = lastGi->next)
    ;
int end = lastGi->chromEnd;

if (noTrans)
    {
    for(; si ; si = si->next)
	{
	if (!allDashes(si->nucSequence))
	    {
	    fprintf(f, ">%s_%s %d %s.%s:%d-%d %c",
		giList->name, si->name, si->size, dbName,
		giList->frame->chrom, start, end, giList->frame->strand[0]);

	    maybePrintGeneName(giList->name, f);
	    fprintf(f, "\n%s\n", si->nucSequence);
	    }
	}
    }
else
    {
    for(; si ; si = si->next)
	{
	translateProtein(si);
	if (!allDashes(si->aaSequence))
	    {
	    fprintf(f, ">%s_%s %d %s.%s:%d-%d %c",
		giList->name, si->name, si->aaSize, dbName,
		giList->frame->chrom, start, end, giList->frame->strand[0]);

	    maybePrintGeneName(giList->name, f);

	    fprintf(f, "\n%s\n", si->aaSequence);
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
int jj;

for(; comp; comp = comp->next)
    {
    char *ptr = strchr(comp->src, '.');

    if (ptr == NULL)
	errAbort("all components must have a '.'");

    *ptr = 0;

    struct speciesInfo *si = hashFindVal(siHash, comp->src);
    if (si == NULL)
	continue;

    char *mptr = ali->components->text;
    char *cptr = comp->text;
    char *sptr = &si->nucSequence[start];

    if (cptr != NULL)
	{
	for(jj = 0 ; jj < ali->textSize; jj++)
	    {
	    if (*mptr++ != '-')
		{
		if (cptr != NULL)
		    {
		    *sptr++ = *cptr++;
		    }
		}
	    else 
		cptr++;
	    }
	}
    }

char *mptr = ali->components->text;
int count = 0;

for(jj = 0 ; jj < ali->textSize; jj++)
    if (*mptr++ != '-')
	count++;

return start + count;

}

/* copyMafs - copy all the maf alignments into 
 * one sequence for each species
 */
void copyMafs(struct hash *siHash, struct exonInfo **giList)
{
int start = 0;
struct exonInfo *gi = *giList;

for(; gi; gi = gi->next)
    {
    int thisSize = 0;
    struct mafAli *ali = gi->ali;

    for(; ali; ali = ali->next)
	{
	int newStart = copyAli(siHash, ali, start);
	thisSize += newStart - start;
	start = newStart;
	}
    }

boolean frameNeg = ((*giList)->frame->strand[0] == '-');

if (frameNeg)
    {
    int size = 0;
    struct hashCookie cookie =  hashFirst(siHash);
    struct hashEl *hel;
    
    while ((hel = hashNext(&cookie)) != NULL)
	{
	struct speciesInfo *si = hel->val;

	if (si == NULL)
	    continue;

	if (size != 0)
	    assert(size == si->size);
	else
	    size = si->size;
	reverseComplement(si->nucSequence, si->size);
	}

    gi = *giList;
    for(; gi; gi = gi->next)
	{
	gi->exonStart = size - (gi->exonStart + gi->exonSize);
	if (gi->next == NULL)
	    assert(gi->exonStart == 0);
	}

    slReverse(giList);
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

/* free exonInfo list */
void freeGIList(struct exonInfo *list)
{
struct exonInfo *giNext;

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
void outGene(FILE *f, char *geneName, char *dbName, char *mafTable, 
    char *frameTable, char *org, struct slName *speciesNameList)
{
struct mafFrames *frames = getFrames(geneName, frameTable, org);
struct mafFrames *frame, *nextFrame;
struct exonInfo *giList = NULL;
struct exonInfo *gi = NULL;
int start = 0;

for(frame = frames; frame; frame = nextFrame)
    {
    nextFrame = frame->next;
    frame->next = NULL;
    boolean exonEdge = (frame->strand[0] == '-') ? 
	frame->isExonEnd : frame->isExonStart;

    if (!newTableType || exonEdge)
	{
	AllocVar(gi);
	gi->frame = frame;
	gi->name = frame->name;
	gi->ali = getAliForFrame(mafTable, frame);
	gi->chromStart = frame->chromStart;
	gi->chromEnd = frame->chromEnd;
	gi->exonStart = start;
	gi->exonSize = frame->chromEnd - frame->chromStart;
	start += gi->exonSize;
	slAddHead(&giList, gi);
	}
    else
	{
	struct mafAli *newAli;

	assert(gi != NULL);
	int frameWidth = frame->chromEnd - frame->chromStart;
	gi->exonSize += frameWidth;
	start += frameWidth;
	gi->chromEnd = frame->chromEnd;
	newAli = getAliForFrame(mafTable, frame);
	gi->ali = slCat(gi->ali, newAli);
	slAddTail(&gi->frame, frame);
	}
    }

slReverse(&giList);

struct hash *speciesInfoHash = newHash(5);
struct speciesInfo *speciesList = getSpeciesInfo(giList, speciesNameList, 
    speciesInfoHash);

copyMafs(speciesInfoHash, &giList);
writeOutSpecies(f, dbName, speciesList, giList);

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

newTableType = hHasField(frameTable, "isExonStart");

if (inExons && !newTableType)
    errAbort("must have new mafFrames type to output in exons");

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
    outGene(f, geneNames->name, dbName, mafTable, 
	frameTable, org, speciesNames);
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
inExons = optionExists("exons");
noTrans = optionExists("noTrans");
transUC = optionExists("transUC");
delay = optionInt("delay", delay);

if ((geneName != NULL) && (geneList != NULL))
    errAbort("cannot specify both geneList and geneName");

mafToProtein(argv[1],argv[2],argv[3],argv[4],argv[5],argv[6]);
return 0;
}
