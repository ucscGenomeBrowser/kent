/* MousePoster - Search database info for making foldout. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dlist.h"
#include "obscure.h"
#include "jksql.h"
#include "hdb.h"
#include "memgfx.h"
#include "psl.h"
#include "genePred.h"
#include "est3.h"
#include "gcPercent.h"
#include "rmskOut.h"
#include "agpFrag.h"
#include "agpGap.h"
#include "cpgIsland.h"
#include "wabAli.h"
#include "clonePos.h"
#include "ctgPos.h"
#include "glDbRep.h"
#include "rnaGene.h"
#include "snp.h"
#include "cytoBand.h"
#include "refLink.h"
#include "hCommon.h"
#include "axt.h"

static char const rcsid[] = "$Id: mousePoster.c,v 1.7.106.1 2008/07/31 02:24:42 markd Exp $";

/* Which database to use */
char *database = "mm2";
char *humanDb = "hg10";

/* Location of mouse/human axt files. */
char *axtDir = "/cluster/store2/mm.2002.02/mm2/bed/blastz.gs11.2002-06-05/axtBestUnzip";

/* File with human/mouse orthologs. */
char *orthoFile = "/cluster/store2/mm.2002.02/mm2/bed/poster/ortholog.rpt";

/* File with OMIM morbid map. */
char *morbidMapFile = "/cluster/store2/mm.2002.02/mm2/bed/poster/morbidMap";

/* File with orthologs to human disease genes. */
char *diseaseFile = "/cluster/store2/mm.2002.02/mm2/bed/poster/disease.rpt";

/* File with mouse stock strains with tweaked genes. */
char *stockFile = "/cluster/store2/mm.2002.02/mm2/bed/poster/stockMice.rpt";

/* File with stock strains orthologous to human diseases */
char *diseaseStockFile = "/cluster/store2/mm.2002.02/mm2/bed/poster/diseaseStockMice.rpt";

/* Quantitative trait locus file. */
char *qtlFile = "/cluster/store2/mm.2002.02/mm2/bed/poster/all.q";

/* File with synteny info */
char *syntenyFile = "/cluster/store2/mm.2002.02/mm2/bed/synteny/synteny.bed";

/* Resolved duplications file. */
char *bestDupFile = "/cluster/store2/mm.2002.02/mm2/bed/poster/dupe.rpt";

/* Misplaced gene file. */
char *misplacedFile = "/cluster/store2/mm.2002.02/mm2/bed/poster/misplace.rpt";

/* Unresolved dupe output. */
FILE *dupeFile;
char *dupeFileName = "dupes.out";

/* Centromere/big gap info.  Obsolete as of October 2000 assembly. */
//char *gapFile = "centro.tab";

/* Some colors. */
struct rgbColor rgbRed = {240,0,0};
struct rgbColor rgbRedOrange = {240, 100, 0};
struct rgbColor rgbYellow = {255, 255, 0};
static struct rgbColor blueGene = { 0, 0, 160};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mousePoster - Search database info for making foldout\n"
  "usage:\n"
  "   mousePoster chrM chrN ...\n"
  "Note - you'll need to edit the source to use different data\n");
}

struct chromGaps
/* Placeholder for obsolete structure. */
    {
    struct chromGaps *next;
    };

int gapOffset(struct chromGaps *gaps, int pos)
/* Placefiller for  old code that inserted gaps.  (Now they are included in sequence itself.) */
{
return 0;
}

void printTab(FILE *f, struct chromGaps *cg, char *chrom, int start, int end, 
	char *track, char *type, int r, int g, int b, char *val)
/* Print line to tab-delimited file. */
{
static double colScale = 1.0/255.0;
double red = r*colScale;
double green = g*colScale;
double blue = b*colScale;

if (!sameString(track, "heterochromatin"))
    {
    int offset = gapOffset(cg, (start+end)/2);
    start += offset;
    end += offset;
    }
fprintf(f, "%s\t%d\t%d\t%s\t%s\t%f,%f,%f\t%s\n",
    chrom, start, end, track, type, red, green, blue, val);
}

void printTabNum(FILE *f, struct chromGaps *cg, char *chrom, int start, int end, 
	char *track, char *type, int r, int g, int b, double num)
/* Print number-valued line to tab-delimited file. */
{
char buf[32];
sprintf(buf, "%f", num);
printTab(f, cg, chrom, start, end, track, type, r, g, b, buf);
}


struct resolvedDup
/* A duplication that has been resolved by hand. */
    {
    struct resolvedDup *next;	/* Next in list. */
    char *name;			/* Name - allocated in hash. */
    char *trueChrom;		/* Real chromosome - allocated here. */
    int trueStart;		/* Start of real gene. */
    int trueEnd;		/* End of true gene. */
    boolean used;		/* True when used - can be used only once. */
    };

void makeResolvedDupes(struct hash *hash, struct resolvedDup **retList)
/* Read in list of dupes that Deanna resolved by hand. */
{
struct lineFile *lf = lineFileMayOpen(bestDupFile, TRUE);
char *row[3], *parts[4];
int partCount;
struct resolvedDup *rd, *rdList = NULL;

if (lf == NULL)
    {
    warn("Can't find %s", bestDupFile);
    return;
    }
while (lineFileRow(lf, row))
    {
    AllocVar(rd);
    hashAddSaveName(hash, row[1], rd, &rd->name);
    partCount = chopString(row[2], ":-", parts, ArraySize(parts));
    if (partCount != 3)
        errAbort("Misformed chromosome location line %d of %s", lf->lineIx, lf->fileName);
    if (sameString(row[0], "Un"))
       rd->trueChrom = "Un";
    else
	{
	rd->trueChrom = cloneString(parts[0]);
	rd->trueStart = atoi(parts[1])-1;
	rd->trueEnd = atoi(parts[2]);
	}
    slAddHead(&rdList, rd);
    }
slReverse(&rdList);
*retList = rdList;
uglyf("Got %d resolved duplications\n", slCount(rdList));
}

void makeMisplaced(struct hash *misplacedHash, struct hash *resolvedDupHash)
/* Make up misplaced hash */
{
struct lineFile *lf = lineFileOpen(misplacedFile, TRUE);
char *row[8];
int missCount = 0, resolvedDupeCount = 0;
while (lineFileRow(lf, row))
    {
    if (!sameString(row[0], row[2]))
	{
	char *name = row[1];
	if (hashLookup(resolvedDupHash, name))
	    {
	    ++resolvedDupeCount;
	    }
	else
	    {
	    hashAdd(misplacedHash, row[1], NULL);
	    ++missCount;
	    }
	}
    }
lineFileClose(&lf);
printf("Found %d misplaced (%d resolved dupes) in %s\n", 
	missCount, resolvedDupeCount, misplacedFile);
}

struct hash *makeLocusLinkToJaxHash(struct sqlConnection *conn)
/* Make hash keyed by locus link ID with value 
 * of Jackson Labs ID. */
{
struct hash *hash = newHash(0);
struct sqlResult *sr;
char **row;
int count = 0;

sr = sqlGetResult(conn, "select LLid, MGIid from MGIid");
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *jaxId = cloneString(row[1]);
    hashAdd(hash, row[0], jaxId);
    ++count;
    }
sqlFreeResult(&sr);
return hash;
}

void fillFirstColumnHash(char *fileName, struct hash *hash)
/* Fill in hash with ID's in first column of file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[1];
while (lineFileRow(lf, row))
    hashStore(hash, row[0]);
lineFileClose(&lf);
}

struct hash *parseMorbidMap(char *fileName)
/* Parse morbidMap into hash */
{
struct hash *hash = newHash(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
char *fields[4], *names[16];
int fieldCount, nameCount;
int loci = 0, syns = 0;
int i;

while (lineFileNext(lf, &line, NULL))
    {
    line = skipLeadingSpaces(line);
    if (line[0] == '#' || line[0] == 0)
        continue;
    fieldCount = chopString(line, "|", fields, ArraySize(fields));
    if (fieldCount < 3)
        errAbort("Expecting at least 3 | separated fields line %d of %s",
		lf->lineIx, lf->fileName);
    nameCount = chopString(fields[1], ", ", names, ArraySize(names));
    for (i=0; i<nameCount; ++i)
        {
	hashStore(hash, names[i]);
	++syns;
	}
    ++loci;
    }
return hash;
}

struct hash *makeDiseaseOrthoHash(char *orthoFile, struct hash *morbidHash)
/* Fill in hash of orthologs of human disease genes keyed by MGI id . */
{
struct lineFile *lf = lineFileOpen(orthoFile, TRUE);
char *row[6];
int count = 0;
struct hash *hash = newHash(0);

while (lineFileRow(lf, row))
    {
    if (hashLookup(morbidHash, row[5]))
       {
       ++count;
       hashStore(hash, row[0]);
       }
    }
return hash;
}

struct hash *makeFirstColumnHash(char *fileName)
/* Make hash of ID's in first column of file. */
{
struct hash *hash = newHash(0);
fillFirstColumnHash(fileName, hash);
return hash;
}

void makeDiseaseStockHash(struct sqlConnection *conn, 
	struct hash *diseaseStockHash, struct hash *diseaseHash, 
	struct hash *stockHash)
/* Make hash of genes that are orthologs of human diseases,
 * genes that have mouse clones, and genes where have both.
 * Note that these are actually from three separate sources
 * though theoretically you could calculate the intersection set.
 * Safer to use stuff straight from Jackson Labs though I think. 
 * These hashes are all keyed by the RefSeq accession. */
{
struct hash *morbidHash = parseMorbidMap(morbidMapFile);
struct hash *llToJax = makeLocusLinkToJaxHash(conn);
struct hash *jaxDiseaseHash = makeDiseaseOrthoHash(orthoFile, morbidHash);
struct hash *jaxStockHash = makeFirstColumnHash(stockFile);
struct hash *jaxDiseaseStockHash = makeFirstColumnHash(diseaseStockFile);
struct sqlResult *sr;
char **row;

/* Using ensembl fillFirstColumnHash(diseaseFile, diseaseHash); */
sr = sqlGetResult(conn, "select mrnaAcc,locusLinkId from refLink");
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *acc = row[0];
    char *llId = row[1];
    if (!sameString(llId,"0"))
        {
	char *jaxId = hashFindVal(llToJax, llId);
	if (jaxId != NULL)
	    {
	    if (hashLookup(jaxStockHash, jaxId))
		{
		hashAdd(stockHash, acc, NULL);
		}
	    if (hashLookup(jaxDiseaseStockHash, jaxId))
		{
		hashAdd(diseaseStockHash, acc, NULL);
		}
	    if (hashLookup(jaxDiseaseHash, jaxId))
	        {
		hashAdd(diseaseHash, acc, NULL);
		}
	    }
	}
    }

/* Clean up time. */
sqlFreeResult(&sr);
freeHashAndVals(&llToJax);
freeHash(&jaxStockHash);
freeHash(&jaxDiseaseStockHash);
}

struct knownGene
/* Enough info to draw a known gene. */
    {
    struct knownGene *next;
    char *name;		   /* Name of gene. */
    char *acc;	      	   /* refSeq accession. */
    char *chrom;           /* Name of chromosome. */
    int start,end;	   /* Position in chromosome. */
    };


int cmpGenePred(const void *va, const void *vb)
/* Compare to sort based on chromosome, start. */
{
const struct genePred *a = *((struct genePred **)va);
const struct genePred *b = *((struct genePred **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->txStart - b->txStart;
return dif;
}



struct onePos
/* position of one instance of gene */
    {
    struct onePos *next;
    char *chrom;	/* Chromosome */
    int chromStart;	/* Start */
    int chromEnd;	/* End */
    };

int onePosCmp(const void *va, const void *vb)
/* Compare to sort based on chromosome pos. */
{
const struct onePos *a = *((struct onePos **)va);
const struct onePos *b = *((struct onePos **)vb);
int diff;
diff =  strcmp(a->chrom, b->chrom);
if (diff == 0)
    diff = a->chromStart - b->chromStart;
return diff;
}


struct allPos
/* Position of all instances of gene */
    {
    struct allPos *next;	
    char *name;		/* Gene name */
    boolean resolved;	/* Is this in resolved list? */
    int count;		/* Number of occurrences. */
    struct onePos *list; /* Position list. */
    };

int allPosCmp(const void *va, const void *vb)
/* Compare to sort based on name.. */
{
const struct allPos *a = *((struct allPos **)va);
const struct allPos *b = *((struct allPos **)vb);
return strcmp(a->name, b->name);
}


void getKnownGenes(struct chromGaps *cg, char *chrom, 
	struct sqlConnection *conn, FILE *f, 
	struct hash *dupHash, struct hash *resolvedDupHash, 
	struct hash *yellowHash, struct hash *redHash, struct hash *greenHash,
	struct hash *misplacedHash)
/* Get info on known genes. */
{
int rowOffset;
struct sqlResult *sr;
char **row;
char query[256];
struct genePred *gpList = NULL, *gp;
char geneName[64];
static struct rgbColor red = {255, 0, 0};
static struct rgbColor green = {0, 190, 0};
static struct rgbColor yellow = {190, 150, 0};
static struct rgbColor blue = {0, 0, 220};
struct rgbColor *col;
boolean keepGene;
struct dlList *geneList = newDlList();
struct dlNode *node;
struct knownGene *kg;

printf("  finding known genes for %s, chrom\n", chrom);

/* Get list of known genes. */
sr = hChromQuery(conn, "refGene", chrom, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row + rowOffset);
    slAddHead(&gpList, gp);
    }
sqlFreeResult(&sr);
slSort(&gpList, cmpGenePred);


/* Get name */
for (gp = gpList; gp != NULL; gp = gp->next)
    {
    keepGene = FALSE;
    sprintf(query, "select * from refLink where mrnaAcc = '%s'", gp->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	struct refLink rlk;
	refLinkStaticLoad(row, &rlk);
	strcpy(geneName, rlk.name);
	if (!sameString(geneName, "n/a")  && !endsWith(geneName, "Rik"))
	    {
	    keepGene = TRUE;
	    }
	}
    sqlFreeResult(&sr);

    if (keepGene)
	{
	boolean showIt = FALSE;
	struct resolvedDup *rd;
	struct allPos *ap;
	struct onePos *op;

	/* Keep track for dupes.  Show it first time around */
	if ((ap = hashFindVal(dupHash, geneName)) == NULL)
	    {
	    AllocVar(ap);
	    hashAddSaveName(dupHash, geneName, ap, &ap->name);
	    }
	showIt = TRUE;
	if (ap->list == NULL || !sameString(ap->list->chrom, chrom) || 
		abs(gp->cdsStart - ap->list->chromStart) > 300000)
	    {
	    AllocVar(op);
	    op->chrom = chrom;
	    op->chromStart = gp->cdsStart;
	    op->chromEnd = gp->cdsEnd;
	    slAddHead(&ap->list, op);
	    ap->count += 1;
	    }

	/* If it's a resolved dupe then instead show the preferred position. */
	if ((rd = hashFindVal(resolvedDupHash, geneName)) != NULL)
	    {
	    ap->resolved = TRUE;
	    if (sameString(chrom, rd->trueChrom) && gp->cdsStart == rd->trueStart && gp->cdsEnd == rd->trueEnd)
		showIt = TRUE;
	    else
		showIt = FALSE;
	    uglyf("Resolving dup %s (%d) at %s:%d-%d\n", geneName, showIt, chrom, gp->cdsStart, gp->cdsEnd);
	    }
	if (hashLookup(misplacedHash, geneName))
	    {
	    uglyf("Suppressing misplaced %s\n", geneName);
	    showIt = FALSE;
	    }
	if (showIt)
	    {
	    char *s;
	    AllocVar(kg);
	    /* Chop off pending suffix if any. */
	    s = stringIn("-pending", geneName);
	    if (s != NULL)
	       *s = 0;
	    kg->name = cloneString(geneName);
	    kg->acc = cloneString(gp->name);
	    kg->chrom = hgOfficialChromName(sqlGetDatabase(conn), chrom);
	    kg->start = gp->cdsStart;
	    kg->end = gp->cdsEnd;
	    dlAddValTail(geneList, kg);
	    }
	}
    }

for (node = geneList->head; !dlEnd(node); node = node->next)
    {
    char *acc;
    kg = node->val;

    acc = kg->acc;
    if (hashLookup(yellowHash, acc))
        col = &yellow;
    else if (hashLookup(redHash, acc))
        col = &red;
    else if (hashLookup(greenHash, acc))
        col = &green;
    else
        col = &blue;
    printTab(f, cg, chrom, kg->start, kg->end, "hugo", 
	    "word", col->r, col->g, col->b, kg->name);
    }
freeDlList(&geneList);
}


struct tickPos
/* A tick position */
    {
    struct tickPos *next;
    int start, end;
    char *val;
    };

void getPredGenes(struct chromGaps *cg, char *chrom, struct sqlConnection *conn, FILE *f, char *table,
  int red, int green, int blue)
/* Get predicted genes. */
{
char **row;
int rowOffset;
struct sqlResult *sr = hChromQuery(conn, table, chrom, NULL, &rowOffset);
struct genePred *gp;

printf("  Getting %s predicted genes\n", table);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row + rowOffset);
    printTab(f, cg, chrom, gp->cdsStart, gp->cdsEnd, 
	    "genePred", "tick", red, green, blue, ".");
    genePredFree(&gp);
    }
sqlFreeResult(&sr);
}

void getCpgIslands(struct chromGaps *cg, char *chrom, struct sqlConnection *conn, FILE *f)
/* Get cpgIslands. */
{
int rowOffset;
char **row;
struct sqlResult *sr = hChromQuery(conn, "cpgIsland", chrom, NULL, &rowOffset);
struct cpgIsland el;

while ((row = sqlNextRow(sr)) != NULL)
    {
    cpgIslandStaticLoad(row + rowOffset, &el);
    printTab(f, cg, chrom, el.chromStart, el.chromEnd, 
	    "cpgIsland", "tick", 0, 120, 0, ".");
    }
sqlFreeResult(&sr);
}


void getRnaGenes(struct chromGaps *cg, char *chrom, struct sqlConnection *conn, FILE *f)
/* Get RNA gene stuff. */
{
char **row;
int rowOffset;
struct sqlResult *sr = hChromQuery(conn, "rnaGene", chrom, NULL, &rowOffset);
struct rnaGene el;
int r,g,b;

while ((row = sqlNextRow(sr)) != NULL)
    {
    rnaGeneStaticLoad(row + rowOffset, &el);
    if (el.isPsuedo)
        {
	r = 250;
	g = 210;
	b = 175;
	}
    else
        {
	r = 120;
	g = 60;
	b = 0;
	}
    printTab(f, cg, chrom, el.chromStart, el.chromEnd, 
	    "rnaGenes", "tick", r, g, b, el.name);
    }
sqlFreeResult(&sr);
}


void getPslTicks(char *track, char *outputName, int r, int g, int b,
	struct chromGaps *cg, char *chrom, struct sqlConnection *conn, FILE *f)
/* Get PSL track stuff. */
{
int rowOffset;
struct sqlResult *sr = hChromQuery(conn, track, chrom, NULL, &rowOffset);
char **row;
struct psl *psl;
int lastEnd = -BIGNUM;
int sepDistance = 20000;

while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row + rowOffset);
    if (psl->tStart > lastEnd + sepDistance)
	{
	printTab(f, cg, chrom, psl->tStart, psl->tStart+1, 
		outputName, "tick", r, g, b, ".");
	lastEnd = psl->tStart;
	}
    pslFree(&psl);
    }
sqlFreeResult(&sr);
}


void getFishBlatHits(struct chromGaps *cg, char *chrom, struct sqlConnection *conn, FILE *f)
/* Get Fish Blat stuff. */
{
getPslTicks("blatFish", "tetraodon", 0, 90, 180, cg, chrom, conn, f);
}


void getEstTicks(struct chromGaps *cg, char *chrom, struct sqlConnection *conn, FILE *f)
/* Get ests with introns. */
{
getPslTicks("intronEst", "est3", 0, 0, 0, cg, chrom, conn, f);
}


void getGc(struct chromGaps *cg, char *chrom, struct sqlConnection *conn, FILE *f)
/* Write wiggle track for GC. */
{
int rowOffset;
struct sqlResult *sr = hChromQuery(conn, "gcPercent", chrom, NULL, &rowOffset);
char **row;
struct gcPercent *el;
double minScale = 5, maxScale = 0;

printf("  getting GC wiggle\n");
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = gcPercentLoad(row + rowOffset);
    if (el->gcPpt != 0)
        {
	double scale;
	scale = (el->gcPpt - 320.0)*1.0/(570-320);
	if (scale < minScale) minScale = scale;
	if (scale > maxScale) maxScale = scale;
	if (scale > 1) scale = 1;
	if (scale < 0) scale = 0;
	printTabNum(f, cg, chrom, el->chromStart, el->chromEnd, 
		"gcPercent", "wiggle", 0, 0, 0, scale);
	}
    gcPercentFree(&el);
    }
printf("   min %f, max %f\n", minScale, maxScale);
sqlFreeResult(&sr);
}

void addSegTotals(int chromStart, int chromEnd, int *countArray, 
	int windowSize, int chromSize)
/* Store the total number of bases in each window between chromStart and chromEnd
 * in the appropriate countArray element. */
{
int startWinIx = chromStart/windowSize;
int endWinIx = (chromEnd-1)/windowSize;
int winBoundary;
int winIx;
int size;

if (chromStart < 0 || chromStart > chromSize)
    {
    warn("Clipping start %d (%d) in addSegTotals\n", chromStart, chromSize);
    return;
    }
if (chromEnd < 0 || chromEnd > chromSize || chromStart >= chromEnd)
    {
    warn("Clipping end %d (%d) in addSegTotals\n", chromStart, chromSize);
    return;
    }
if (startWinIx == endWinIx)
    {
    countArray[startWinIx] += (chromEnd - chromStart);
    return;
    }
winBoundary = (startWinIx+1)*windowSize;
size = winBoundary - chromStart;
assert(size > 0);
assert(size <= windowSize);
countArray[startWinIx] += size;
for (winIx = startWinIx+1; winIx < endWinIx; ++winIx)
    {
    countArray[winIx] += windowSize;
    }
winBoundary = (endWinIx)*windowSize;
size = chromEnd - winBoundary;
assert(size > 0);
assert(size <= windowSize);
countArray[endWinIx] += size;
}


int roundUp(int p, int q)
/* Return p/q rounded up. */
{
return (p + q-1)/q;
}

int *getWinBases(struct sqlConnection *conn, 
	int winsPerChrom, int windowSize, char *chrom, int chromSize)
/* Return an array with the number of non-N bases in each
 * window. */
{
int rowOffset;
struct sqlResult *sr;
char **row;
int *winBases;
struct agpFrag frag;

/* Count up bases in each window. */
AllocArray(winBases, winsPerChrom);
sr = hChromQuery(conn, "gold", chrom, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    agpFragStaticLoad(row + rowOffset, &frag);
    addSegTotals(frag.chromStart, frag.chromEnd, winBases, windowSize, chromSize);
    }
sqlFreeResult(&sr);
return winBases;
}

void outputWindowedWiggle(struct chromGaps *cg, FILE *f, 
	int *hitBases, int *winBases, 
	int windowSize, int winsPerChrom, 
	char *chrom, int chromSize, 
	int red, int green, int blue, char *type, double offset, double scale,
	double minData)
/* Output a wiggle plot. */
{
int i;
int baseCount, repCount;
double density;
double minDen = 1.0, maxDen = 0.0;
int winStart, winEnd;

/* Output density. */
for (i=0; i<winsPerChrom; ++i)
    {
    if ((baseCount = winBases[i]) > windowSize * minData)
        {
	repCount = hitBases[i];
	density = (double)repCount/(double)baseCount;
	if (density < minDen) minDen = density;
	if (density > maxDen) maxDen = density;
	density -= offset;
	density *= scale;
	if (density > 1.0) density = 1.0;
	if (density < 0.0) density = 0.0;
	winStart = i*windowSize;
	winEnd = winStart + windowSize;
	if (winEnd > chromSize) winEnd = chromSize;
	printTabNum(f, cg, chrom, winStart, winEnd, 
		type, "wiggle", red, green, blue, density);
	}
    }
printf("   %s minDen %f, maxDen %f, scaledMin %f, scaledMax %f\n", 
	chrom, minDen, maxDen, (minDen-offset)*scale, (maxDen-offset)*scale);
}


void getRepeatDensity(struct chromGaps *cg, char *chrom, int chromSize, 
	struct sqlConnection *conn, FILE *f, char *repClass, 
	int windowSize, double scale, int red, int green, int blue)
/* Put out repeat density info. */
{
int rowOffset;
struct sqlResult *sr;
char **row;
char extraWhere[256];
struct rmskOut rmsk;
int winsPerChrom = roundUp(chromSize, windowSize);
int *winBases;
int *winRepBases;

printf("  Getting %s repeats in %d windows of size %d\n", 
	repClass, winsPerChrom, windowSize);
fflush(stdout);

/* Count up repeats in each window. */
AllocArray(winRepBases, winsPerChrom);
sprintf(extraWhere, "repClass = '%s'", repClass);
sr = hChromQuery(conn, "rmsk", chrom, extraWhere, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    rmskOutStaticLoad(row + rowOffset, &rmsk);
    addSegTotals(rmsk.genoStart, rmsk.genoEnd, winRepBases, windowSize, chromSize);
    }
sqlFreeResult(&sr);

/* Call routines to get the bases in each window and to output
 * the density as a wiggle. */
winBases = getWinBases(conn, winsPerChrom, windowSize, chrom, chromSize);
outputWindowedWiggle(cg, f, winRepBases, winBases, windowSize, winsPerChrom, 
	chrom, chromSize, red, green, blue, repClass, 0.0, scale, 0.333);

freeMem(winBases);
freeMem(winRepBases);
}

void getMouseAli(struct chromGaps *cg, char *chrom, int chromSize, 
	char *axtFile, struct sqlConnection *conn, FILE *f, char *type, 
	int windowSize, double scale, int red, int green, int blue)
/* Get human/mouse coverage by alignments. */
{
struct axt *axt;
int winsPerChrom = roundUp(chromSize, windowSize);
int *winBases;
int *covBases;
struct lineFile *lf = NULL;

printf("  Getting %s in %d windows of size %d\n", 
	type, winsPerChrom, windowSize);
fflush(stdout);

/* Count up bases covered by alignments in each window. */
AllocArray(covBases, winsPerChrom);
lf = lineFileOpen(axtFile, TRUE);
while ((axt = axtRead(lf)) != NULL)
    {
    addSegTotals(axt->tStart, axt->tEnd, covBases, windowSize, chromSize);
    axtFree(&axt);
    }
lineFileClose(&lf);

/* Call routines to get the bases in each window and to output
 * the density as a wiggle. */
winBases = getWinBases(conn, winsPerChrom, windowSize, chrom, chromSize);
outputWindowedWiggle(cg, f, covBases, winBases, windowSize, winsPerChrom, 
	chrom, chromSize, red, green, blue, type, 0.0, scale, 0.333);

freeMem(winBases);
freeMem(covBases);
}

void axtIdAndCov(struct axt *axt, int *idBases, int *covBases, int windowSize)
/* Add bases and matching bases to appropriate count for window. */
{
int tOff = axt->tStart;
int symCount = axt->symCount;
char *qSym = axt->qSym, q;
char *tSym = axt->tSym, t;
int i, win;

for (i=0; i<symCount; ++i)
    {
    q = qSym[i];
    t = tSym[i];
    if (t != '-')
        {
	win = tOff/windowSize;
	covBases[win] += 1;
	if (toupper(q) == toupper(t))
	    {
	    idBases[win] += 1;
	    }
	++tOff;
	}
    }
assert(tOff == axt->tEnd);
}

void getMouseId(struct chromGaps *cg, char *chrom, int chromSize, 
	char *axtFile, struct sqlConnection *conn, FILE *f, char *type, 
	int windowSize, int red, int green, int blue)
/* Get human/mouse %ID within aligned regions. */
{
struct axt *axt;
int winsPerChrom = roundUp(chromSize, windowSize);
int *idBases;
int *covBases;
struct lineFile *lf = NULL;

printf("  Getting %s in %d windows of size %d\n", 
	type, winsPerChrom, windowSize);
AllocArray(covBases, winsPerChrom);
AllocArray(idBases, winsPerChrom);
lf = lineFileOpen(axtFile, TRUE);
while ((axt = axtRead(lf)) != NULL)
    {
    if (axt->tEnd > chromSize)
        errAbort("alignment from %d-%d but %s only has %d bases line %d of %s",
		axt->tStart, axt->tEnd, chrom, chromSize, lf->lineIx-3, lf->fileName);
    if (axt->tStrand == '-')
	errAbort("Sorry, can't handle minus target strands line %d of %s", 
		lf->lineIx-3, lf->fileName);
    axtIdAndCov(axt, idBases, covBases, windowSize);
    axtFree(&axt);
    }
lineFileClose(&lf);

/* Call routines to get the bases in each window and to output
 * the density as a wiggle. */
outputWindowedWiggle(cg, f, idBases, covBases, windowSize, winsPerChrom, 
	chrom, chromSize, red, green, blue, type, 0.54, 4.0, 0.1);

freeMem(idBases);
freeMem(covBases);
}


double nRatio(struct sqlConnection *conn, 
	char *chrom, int chromStart, int chromEnd)
/* Return percentage of N's in range. */
{
char query[256];
struct sqlResult *sr;
char **row;
struct agpGap gap;
int baseTotal = chromEnd - chromStart;
int nCount = 0, s, e, size;

sprintf(query, "select * from %s_gap where chromStart < %d and chromEnd > %d",
	chrom, chromEnd, chromStart);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    agpGapStaticLoad(row, &gap);
    s = gap.chromStart;
    if (s < chromStart) s = chromStart;
    e = gap.chromEnd;
    if (e > chromEnd) e = chromEnd;
    size = e - s;
    nCount += size;
    }
sqlFreeResult(&sr);
return (double)nCount/(double)baseTotal;
}

int chromNum(char *chrom)
/* Convert chrN to N,  also handle X and Y. */
{
char c;
if (startsWith("chr", chrom))
    chrom += 3;
c = chrom[0];
if (c == 'X')
    return 23;
if (c == 'Y')
    return 24;
return atoi(chrom);
}

static struct rgbColor chromColorTable[] = {
	{ 0,0, 0},
	{ 255,204, 204},  /* light red */
	{ 204,0, 0},      /* med red */
	{ 255,0, 0},      /* bright red */
	{ 255,102,0},     /* bright orange */
	{ 255,153,0},     /* yellow orange */
	{ 255,0,204},     /* magenta */
	{ 255,255,204},   /* light yellow  */
	{ 255,255,153},   /* med yellow */
	{ 255,255,0},     /* bright yellow*/
	{ 0,255,0},       /*bt gr*/
	{ 204,255,0},     /* yellow green */
	{ 102,102,0},     /* dark  green*/
	{ 204,255,255},   /*lt cyan*/
	{ 153,204,204},   /* gray cyan */
	{ 0,255,255},     /*med cyan*/
	{ 153,204,255},   /*light med blue */
	{ 102,153,255},   /* med blue */
	{ 0,0 ,204},      /* deep blue */
	{ 204,153,255},   /*lt violet*/
	{ 204,051,255},   /* med violet */
	{ 153,0,204},     /* dark violet */
	{ 204,204,204},   /* light gray */
	{ 153,153,153},   /* med gray */
	{ 102,102,102},   /* dark gray */
	{ 255,255,255},   /* black */
};

struct rgbColor *chromColor(char *chrom)
/* Return color for chromosome. */
{
int ix = chromNum(chrom);
assert(ix < ArraySize(chromColorTable));
return &chromColorTable[ix];
}

void getSynteny(struct chromGaps *cg, char *chrom, FILE *f)
/* Read synteny file and put relevant parts in output */
{
struct sqlConnection *hConn = sqlConnect(humanDb);
struct lineFile *lf = lineFileOpen(syntenyFile, TRUE);
char *row[7];
while (lineFileRow(lf, row))
    {
    if (sameString(chrom, row[0]))
        {
	char *hChrom = row[3];
	int hs = lineFileNeedNum(lf, row, 4);
	int he = lineFileNeedNum(lf, row, 5);
	char *strand = row[6];
	char band[65];
	struct rgbColor *col = chromColor(hChrom);
	band[0] = strand[0];
	hChromBandConn(hConn, hChrom, (hs+he)/2, band+1);
	chopSuffix(band+1);
	printTab(f, cg, chrom, atoi(row[1]), atoi(row[2]), 
		"synteny", "box", col->r, col->g, col->b, band);
	}
    }
lineFileClose(&lf);
sqlDisconnect(&hConn);
}

void getQtl(struct chromGaps *cg, char *fileName, char *chrom, FILE *f)
/* Get data for Quantitative Trait Locus track. */
{
char *line;
char *row[9];
int i;
struct lineFile *lf = lineFileOpen(qtlFile, TRUE);
while (lineFileNext(lf, &line, NULL))
    {
    line = skipLeadingSpaces(line);
    if (line[0] == '#' || line[0] == 0)
	continue;
    for (i=0; i<3; ++i)
        row[i] = nextWord(&line);
    line = skipLeadingSpaces(line);
    if (line == NULL || line[0] == 0)
        errAbort("Expecting at least 4 words line %d of %s\n", lf->lineIx, lf->fileName);
    if (sameString(chrom, row[0]))
	{
	int start = lineFileNeedNum(lf, row, 1);
	int end = lineFileNeedNum(lf, row, 2);
	printTab(f, cg, chrom, start, end, 
		"QTL", "text", 0, 0, 0, line);
	}
    }
lineFileClose(&lf);
}

void oneChrom(char *chrom, struct sqlConnection *conn, 
	struct hash *dupHash, struct hash *resolvedDupHash, 
	struct hash *diseaseStockHash, struct hash *diseaseHash, 
	struct hash *stockHash, struct hash *misplacedHash,
	FILE *f)
/* Get info for one chromosome.  */
{
int chromSize = hChromSize(database, chrom);
struct chromGaps *cg = NULL;
char axtFile[512];

printf("Processing %s\n", chrom);
sprintf(axtFile, "%s/%s.axt", axtDir, chrom);

getSynteny(cg, chrom, f);
getMouseAli(cg, chrom, chromSize, axtFile, conn, f, 
	"HS_ALI", 50000, 1.0/0.85, 0, 0, 255);
getMouseId(cg, chrom, chromSize, axtFile, conn, f,
	"HS_ID", 25000, 255, 0, 0);
getGc(cg, chrom, conn, f);
getRepeatDensity(cg, chrom, chromSize, conn, f, "SINE", 100000, 1.0/0.33, 255, 0, 0);
getRepeatDensity(cg, chrom, chromSize, conn, f, "LINE", 100000, 1.0/0.66, 0, 0, 255);
getQtl(cg, qtlFile, chrom, f);
getRnaGenes(cg, chrom, conn, f);
getCpgIslands(cg, chrom, conn, f);
getFishBlatHits(cg,chrom,conn,f);
getEstTicks(cg, chrom, conn, f);
getPredGenes(cg, chrom, conn, f, "genieAlt", 160, 10, 0);
getPredGenes(cg, chrom, conn, f, "ensGene", 160, 10, 0);
getPredGenes(cg, chrom, conn, f, "refGene", blueGene.r, blueGene.g, blueGene.b);
getKnownGenes(cg, chrom, conn, f, dupHash, resolvedDupHash, 
	diseaseStockHash, diseaseHash, stockHash, misplacedHash);
}

void printDupes(struct hash *dupHash, FILE *f)
/* Print out duplication info. */
{
struct allPos *apList = NULL, *ap;
struct hashEl *elList = hashElListHash(dupHash), *el;
int dupCount = 0;

/* Build up and alphabetize gene list. */
for (el = elList; el != NULL; el = el->next)
    {
    ap = el->val;
    slAddHead(&apList, ap);
    }

/* Print out info on ones with more than one copy */
for (ap = apList; ap != NULL; ap = ap->next)
    {
    if (ap->count > 1 && !ap->resolved)
        {
	struct onePos *op;
	for (op = ap->list; op != NULL; op = op->next)
	    {
	    fprintf(f, "%s\t%s:%d-%d\n", ap->name, 
	    	op->chrom, op->chromStart+1, op->chromEnd);
	    }
	fprintf(f, "\n");
	++dupCount;
	}
    }
printf("%d unresolved dupes\n", dupCount);
}

void mousePoster(int chromCount, char *chromNames[])
/* mousePoster - Search database info for making foldout. */
{
int i;
struct sqlConnection *conn = sqlConnect(database);
struct hash *dupHash = newHash(0);
struct hash *resolvedDupHash = newHash(8);
struct resolvedDup *rdList = NULL;
struct hash *diseaseStockHash = newHash(0);
struct hash *diseaseHash = newHash(0);
struct hash *stockHash = newHash(0);
struct hash *misplacedHash = newHash(0);

dupeFile = mustOpen(dupeFileName, "w");
makeDiseaseStockHash(conn, diseaseStockHash, diseaseHash, stockHash);
makeResolvedDupes(resolvedDupHash, &rdList);
makeMisplaced(misplacedHash, resolvedDupHash);
for (i=0; i<chromCount; ++i)
    {
    char fileName[512];
    FILE *f;
    sprintf(fileName, "%s.tab", chromNames[i]);
    f = mustOpen(fileName, "w");
    oneChrom(chromNames[i], conn, dupHash, resolvedDupHash, 
    	diseaseStockHash, diseaseHash, stockHash, misplacedHash, f);
    fclose(f);
    }
printDupes(dupHash, dupeFile);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 2)
    usage();
mousePoster(argc-1, argv+1);
return 0;
}
