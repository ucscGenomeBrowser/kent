/* foldDb - Search database info for making foldout. */
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
#include "knownInfo.h"
#include "exoFish.h"
#include "rnaGene.h"
#include "snp.h"
#include "cytoBand.h"
#include "knownMore.h"
#include "hCommon.h"


/* Coverage info file. */
char *covFile = "coverage.oct";

/* Disease genes file. */
char *diseaseFile = "disease.oct";

/* Resolved duplications file. */
char *bestDupFile = "bestDup.oct";

/* Rough location of Ensembl genes. */
char *ensTick = "/projects/hg/gs.5/oo.21/bed/ensTick/ensTick.bed";

/* Unresolved dupe output. */
FILE *dupeFile;
char *dupeFileName = "dupes.out";

struct knownGene
/* Enough info to draw a known gene. */
    {
    struct knownGene *next;
    char *name;		   /* Name of gene. */
    char omimIdString[16]; /* OMIM ID ascii representation. */
    char *chrom;           /* Name of chromosome. */
    int start,end;	   /* Position in chromosome. */
    boolean isDisease;	   /* is a disease gene? */
    };

int knownGeneCmpPos(const void *va, const void *vb)
/* Compare to sort based on start position. */
{
const struct knownGene *a = *((struct knownGene **)va);
const struct knownGene *b = *((struct knownGene **)vb);
int diff = strcmp(a->chrom, b->chrom);
if (diff == 0)
    diff = a->start - b->start;
return diff;
}

/* -------------- Some Stuff for NT Patch on October. ---------------*/
/* Locations of NT contigs that need patching in oo21 assembly. */
char *ntPosFile = "ntPos.oct";

/* Locations of genes within NT contigs. */
char *ntGeneFile = "ntKnownGene.oct";

struct ntPos
/* Position of an NT contig. */
    {
    struct ntPos *next;
    char *name;		/* Name of contig. */
    char *chrom;	/* Chromosome. */
    int start,end;	/* Range on chromosome. */
    char strand;	/* + or - */
    struct ntGene *geneList;  /* List of genes. */
    };

struct ntGene
/* Position of gene within an NT contig. */
    {
    struct ntGene *next;
    struct ntPos *nt;			/* Contig this is part of. */
    int start,end;			/* Position within contig. */
    char *hugoName;			/* Name of gene according to hugo. */
    char *omimIdString;			/* ASCII representation of OMIM id. */
    };

int ntGeneCmpPos(const void *va, const void *vb)
/* Compare to sort based on start position. */
{
const struct ntGene *a = *((struct ntGene **)va);
const struct ntGene *b = *((struct ntGene **)vb);
return a->start - b->start;
}

struct hash *ntPosHash;		/* Hash with ntPos values. */
struct ntPos *ntPosList;	/* List of all the places to patch. */
struct hash *ntGeneNameHash;   /* Hash of NT genes by name. */
struct hash *ntGeneOmimHash;   /* Hash of NT genes by hugo ID. */

void loadNtPos()
/* Read in NT pos file. */
{
struct lineFile *lf = lineFileOpen(ntPosFile, TRUE);
char *words[5];
struct ntPos *ntPos;

ntPosHash = newHash(12);
while (lineFileRow(lf, words))
    {
    AllocVar(ntPos);
    slAddHead(&ntPosList, ntPos);
    hashAddSaveName(ntPosHash, words[3], ntPos, &ntPos->name);
    ntPos->chrom = hgOfficialChromName(words[0]);
    ntPos->start = lineFileNeedNum(lf, words, 1);
    ntPos->end = lineFileNeedNum(lf, words, 2);
    ntPos->strand = words[4][0];
    }
lineFileClose(&lf);
slReverse(&ntPosList);
printf("Loaded %d NT positions\n", slCount(ntPosList));
}

void loadNtGenes()
/* Read in knownGene table to appropriate ntPos. */
{
struct lineFile *lf = lineFileOpen(ntGeneFile, TRUE);
char *words[6];
struct ntPos *ntPos;
struct ntGene *ntg;
int count = 0;
int skipped = 0;
int duped = 0;

ntGeneNameHash = newHash(12);
ntGeneOmimHash = newHash(12);
while (lineFileRow(lf, words))
    {
    if ((ntPos = hashFindVal(ntPosHash, words[0])) == NULL)
	{
	++skipped;
        continue;
	}
    if (hashLookup(ntGeneNameHash, words[4]) || 
    	(words[5][0] != '-' && hashLookup(ntGeneOmimHash,words[5])))
        {
	++duped;
	continue;
	}
    hashAdd(ntGeneNameHash, words[4], NULL);
    if (words[5][0] != '-')
	hashAdd(ntGeneOmimHash, words[5], NULL);
    AllocVar(ntg);
    slAddHead(&ntPos->geneList, ntg);
    ntg->start = lineFileNeedNum(lf, words, 1);
    ntg->end = lineFileNeedNum(lf, words, 2);
    ntg->hugoName = cloneString(words[4]);
    ntg->omimIdString = cloneString(words[5]);
    ++count;
    }
lineFileClose(&lf);

for (ntPos = ntPosList; ntPos != NULL; ntPos = ntPos->next)
    {
    if (ntPos->strand == '-')
	{
	int ntSize = ntPos->end - ntPos->start;
	int s, e;
        slReverse(&ntPos->geneList);
	for (ntg = ntPos->geneList; ntg != NULL; ntg = ntg->next)
	    {
	    s = ntSize - ntg->end;
	    e = ntSize - ntg->start;
	    //uglyAbort("%s in %s, ntSize %d, s %d, e %d, ntg->start %d, ntg->end %d\n",
	    //	ntg->hugoName, ntPos->name, ntSize, s, e, ntg->start, ntg->end);
	    ntg->start = s;
	    ntg->end = e;
	    }
	}
    slSort(&ntPos->geneList, ntGeneCmpPos);
    }
printf("Loaded %d NT genes, skipped %d, duped %d\n", count, skipped, duped);
}

void loadNtPatchFiles()
/* Read in info on NT patches into ntPosHash/ntPosList. */
{
loadNtPos();
loadNtGenes();
}

void ntPatchChromosome(char *chromosome, struct dlList *geneList)
/* Apply NT patches to geneList. */
{
struct knownGene *kg;
struct dlNode *node;
struct ntPos *ntChromStart, *ntPos;
struct ntGene *ntg;

printf("Patching NT genes\n");
/* Find start of this chromosome in ntPosList. */
chromosome = hgOfficialChromName(chromosome);
for (ntChromStart = ntPosList; ntChromStart != NULL; ntChromStart = ntChromStart->next)
    {
    if (ntChromStart->chrom == chromosome)
        break;
    }


/* Get rid of any existing genes that share either space, name or OMIM ID
 * with the NT genes to be patched in. */
for (node = geneList->head; !dlEnd(node); node = node->next)
    {
    kg = node->val;
    if (hashLookup(ntGeneNameHash, kg->name) )
         dlRemove(node);
    else if (isdigit(kg->omimIdString[0]) && hashLookup(ntGeneOmimHash, kg->omimIdString))
         dlRemove(node);
    else
         {
	 int genePos = (kg->start + kg->end)/2;
	 for (ntPos = ntChromStart; ntPos != NULL && ntPos->chrom == chromosome; ntPos = ntPos->next)
	     {
	     if (ntPos->start <= genePos && genePos < ntPos->end)
	         {
		 dlRemove(node);
		 break;
		 }
	     }
	 }
    }

/* Add in NT genes. */
for (ntPos = ntChromStart; ntPos != NULL && ntPos->chrom == chromosome; ntPos = ntPos->next)
    {
    for (ntg = ntPos->geneList; ntg != NULL; ntg = ntg->next)
	{
	AllocVar(kg);
	kg->name = ntg->hugoName;
	strcpy(kg->omimIdString, ntg->omimIdString);
	kg->chrom = chromosome;
	kg->start = ntg->start + ntPos->start;
	kg->end = ntg->end + ntPos->start;
	dlAddValTail(geneList, kg);
	}
    }
dlSort(geneList, knownGeneCmpPos);
}



/* --------------  End of (most) Stuff for NT Patch on October. ---------------*/

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
  "foldDb - Search database info for making foldout\n"
  "usage:\n"
  "   foldDb chrM chrN ...\n");
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

void getHeterochromatin(struct chromGaps *cg, char *chrom, struct sqlConnection *conn, FILE *f)
/* Print out centromere info (and other big gaps). */
{
struct sqlResult *sr;
char **row;
char query[256];
struct cytoBand el;
int r,g,b;
char *stain;
boolean inCen = FALSE, lastInCen = FALSE;
boolean cenStart = BIGNUM, cenEnd = -BIGNUM;

printf("  getting heterochromatin\n");
sqlSafef(query, sizeof query, "select * from cytoBand where chrom = '%s'", chrom);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct rgbColor *col;
    cytoBandStaticLoad(row, &el);
    stain = el.gieStain;
    inCen = FALSE;
    if (startsWith("acen", stain))
        {
	if (!lastInCen)
	    cenStart = el.chromStart;
	cenEnd = el.chromEnd;
	inCen = TRUE;
	}
    if (lastInCen && !inCen)
        {
	col = &rgbRed;
	printTab(f, cg, chrom, cenStart, cenEnd,
	    "heterochromatin", "box", col->r, col->g, col->b, "centromere");
	}
    lastInCen = inCen;
    if (startsWith("hetero", stain))
        {
	col = &rgbRedOrange;
	printTab(f, cg, chrom, el.chromStart, el.chromEnd,
	    "heterochromatin", "box", col->r, col->g, col->b, "other");
	}
    }
sqlFreeResult(&sr);
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

void makeResolvedDupes(struct hash **retHash, struct resolvedDup **retList)
/* Read in list of dupes that John resolved by hand. */
{
struct hash *hash = newHash(6);
struct lineFile *lf = lineFileOpen(bestDupFile, TRUE);
char *words[32], *parts[4];
int wordCount, partCount;
struct resolvedDup *rd, *rdList = NULL;

char *bestDupFile = "bestDup.txt";
while ((wordCount = lineFileChop(lf, words)) > 0)
    {
    lineFileExpectWords(lf, 4, wordCount);
    AllocVar(rd);
    hashAddSaveName(hash, words[1], rd, &rd->name);
    partCount = chopString(words[3], ":-", parts, ArraySize(parts));
    if (partCount != 3)
        errAbort("Misformed chromosome location line %d of %s", lf->lineIx, lf->fileName);
    rd->trueChrom = cloneString(parts[0]);
    rd->trueStart = atoi(parts[1]);
    rd->trueEnd = atoi(parts[2]);
    slAddHead(&rdList, rd);
    }
slReverse(&rdList);
*retHash = hash;
*retList = rdList;
uglyf("Got %d resolved duplications\n", slCount(rdList));
}



struct omimInfo
/* Info on OMIM gene. */
    {
    struct omimInfo *next;	/* Next in list. */
    char *name;			/* HUGO/OMIM name, allocated in hash. */
    int uses;			/* Number of times used. */
    struct genePred *gpExtra;    /* Extra info if it's on GenieAlt rather than GenieKnown table. */ 
    };


void makeDiseaseHash(struct sqlConnection *conn,
	struct hash **retNameHash, struct hash **retOmimHash)
/* Make hash of all disease genes. */
{
struct hash *nameHash = newHash(12);
struct hash *omimHash = newHash(12);
struct lineFile *lf = lineFileOpen(diseaseFile, TRUE);
char *row[3];

while (lineFileRow(lf, row))
    {
    hashAdd(nameHash, row[1], NULL);
    hashAdd(omimHash, row[2], NULL);
    }
lineFileClose(&lf);
*retNameHash = nameHash;
*retOmimHash = omimHash;
}


struct knownPos
/* Position of a known gene. */
    {
    struct knownPos *next;	/* Next in list. */
    char *name;			/* Name. */
    char *chrom;		/* Chromosome. */
    int chromStart;
    int chromEnd;
    int dupeCount;		/* Number of times duplicated. */
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


void getKnownGenes(struct chromGaps *cg, char *chrom, struct sqlConnection *conn, FILE *f, 
	struct hash *hugoDupHash, struct hash *knownUniqHash, struct hash *resolvedDupHash, 
	struct hash *diseaseNameHash, struct hash *diseaseOmimHash)
/* Get info on known genes. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct genePred *gpList = NULL, *gp;
char geneName[64];
static struct rgbColor diseaseColor = {255, 0, 0};
struct rgbColor *col;
boolean keepGene;
int lastStart = -BIGNUM;
struct knownPos *kp;
char omimIdAsString[16];
struct dlList *geneList = newDlList();
struct dlNode *node;
struct knownGene *kg;
bool isDisease;

printf("  finding known genes for %s, chrom\n");


/* Get list of known genes. */
sqlSafef(query, sizeof query, "select * from genieKnown where chrom = '%s' order by cdsStart", chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row);
    slAddHead(&gpList, gp);
    }
sqlFreeResult(&sr);
slSort(&gpList, cmpGenePred);


/* Now find out name - either from disease gene hash or known table in database. */
for (gp = gpList; gp != NULL; gp = gp->next)
    {
    keepGene = FALSE;
    isDisease = FALSE;
    sqlSafef(query, sizeof query, "select * from knownMore where transId = '%s'", gp->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	struct knownMore km;
	knownMoreStaticLoad(row, &km);
	strcpy(geneName, km.name);
	if (km.omimId != 0)
	    sprintf(omimIdAsString, "%d", km.omimId);
	else
	    strcpy(omimIdAsString, "-");
	if (!sameString(geneName, "n/a") )
	    {
	    if (gp->cdsStart != lastStart)
		{
		keepGene = TRUE;
		lastStart = gp->cdsStart;
		}
	    if (hashLookup(diseaseNameHash, geneName))
		isDisease = TRUE;
	    else if (hashLookup(diseaseOmimHash, omimIdAsString) ||
		(km.omimName[0] != 0 && hashLookup(diseaseNameHash, km.omimName)))
		{
		if (km.omimName[0] != 0)
		    strcpy(geneName, km.omimName);
		isDisease = TRUE;
		}
	    }
	}
    sqlFreeResult(&sr);

    if (keepGene)
	{
	boolean showIt = FALSE;
	boolean doWarn = TRUE;
	struct resolvedDup *rd;

	if ((kp = hashFindVal(knownUniqHash, geneName)) == NULL)
	    {
	    showIt = TRUE;
	    AllocVar(kp);
	    hashAddSaveName(knownUniqHash, geneName, kp, &kp->name);
	    kp->chrom = chrom;
	    kp->chromStart = gp->cdsStart;
	    kp->chromEnd = gp->cdsEnd;
	    kp->dupeCount = 1;
	    }
	else
	    {
	    kp->dupeCount += 1;
	    }
	if ((rd = hashFindVal(resolvedDupHash, geneName)) != NULL)
	    {
	    if (sameString(chrom, rd->trueChrom) && gp->cdsStart == rd->trueStart && gp->cdsEnd == rd->trueEnd)
		showIt = TRUE;
	    else
		showIt = FALSE;
	    doWarn = FALSE;
	    uglyf("Resolving dup %s (%d) at %s:%d-%d\n", geneName, showIt, chrom, gp->cdsStart, gp->cdsEnd);
	    }
	if (kp != NULL && !showIt)
	    {
	    if (kp->chrom != chrom || kp->chromStart > gp->cdsStart + 300000 || kp->chromStart < gp->cdsStart - 300000)
		{
		if (doWarn)
		    {
		    fprintf(dupeFile, "%s\t%s\t%4.1f\n", geneName, chrom, 0.000001*kp->chromStart);
		    warn("Duplicate %s at %s:%d-%d and %s:%d-%d, skipping all but first,", 
			    geneName, kp->chrom, kp->chromStart, kp->chromEnd, chrom, gp->cdsStart, gp->cdsEnd);
		    }
		}
	    }
	if (showIt)
	    {
	    AllocVar(kg);
	    kg->name = cloneString(geneName);
	    strcpy(kg->omimIdString, omimIdAsString);
	    kg->chrom = hgOfficialChromName(chrom);
	    kg->start = gp->cdsStart;
	    kg->end = gp->cdsEnd;
	    kg->isDisease = isDisease;
	    dlAddValTail(geneList, kg);
	    }
	}
    }

ntPatchChromosome(chrom, geneList);
for (node = geneList->head; !dlEnd(node); node = node->next)
    {
    int len;
    char *name;
    kg = node->val;

    name = kg->name;
    len = strlen(name);
    if (kg->isDisease || hashLookup(diseaseNameHash, name) || hashLookup(diseaseOmimHash, kg->omimIdString))
	col = &diseaseColor;
    else
        col = &blueGene;
    if (len < 8  && !sameString(name, "pseudo") &&
	    !startsWith("DKFZ", name) && !startsWith("KIAA", name) && 
	    !(startsWith("LOC", name) && len > 5))
	{
	printTab(f, cg, chrom, kg->start, kg->end, "hugo", 
		"word", col->r, col->g, col->b, kg->name);
	}
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
struct sqlResult *sr;
char **row;
char query[256];
struct genePred *gp;

printf("  Getting %s predicted genes\n", table);
sqlSafef(query, sizeof query, "select * from %s where chrom = '%s' order by cdsStart", table, chrom);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row);
    printTab(f, cg, chrom, gp->cdsStart, gp->cdsEnd, 
	    "genePred", "tick", red, green, blue, ".");
    genePredFree(&gp);
    }
sqlFreeResult(&sr);
}

void getCpgIslands(struct chromGaps *cg, char *chrom, struct sqlConnection *conn, FILE *f)
/* Get cpgIslands. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct cpgIsland el;

sqlSafef(query, sizeof query, "select * from cpgIsland where chrom = '%s'", chrom);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    cpgIslandStaticLoad(row, &el);
    printTab(f, cg, chrom, el.chromStart, el.chromEnd, 
	    "cpgIsland", "tick", 0, 120, 0, ".");
    }
sqlFreeResult(&sr);
}

void getExofishHits(struct chromGaps *cg, char *chrom, struct sqlConnection *conn, FILE *f)
/* Get Exofis stuff. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct exoFish el;

sqlSafef(query, sizeof query, "select * from exoFish where chrom = '%s'", chrom);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    exoFishStaticLoad(row, &el);
    printTab(f, cg, chrom, el.chromStart, el.chromEnd, 
	    "tetraodon", "tick", 0, 70, 120, ".");
    }
sqlFreeResult(&sr);
}


void getRnaGenes(struct chromGaps *cg, char *chrom, struct sqlConnection *conn, FILE *f)
/* Get RNA gene stuff. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct rnaGene el;
int r,g,b;

sqlSafef(query, sizeof query, "select * from rnaGene where chrom = '%s'", chrom);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    rnaGeneStaticLoad(row, &el);
    if (el.isPsuedo)
        {
	r = 255;
	g = 200;
	b = 150;
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

void getTetHits(struct chromGaps *cg, char *chrom, struct sqlConnection *conn, FILE *f)
/* Get tetroadon hits. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct wabAli el;

sqlSafef(query, sizeof query, "select * from waba_tet where chrom = '%s'", chrom);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    wabAliStaticLoad(row, &el);
    printTab(f, cg, chrom, el.chromStart, el.chromEnd, 
	    "tetraodon", "tick", 0, 70, 120, ".");
    }
sqlFreeResult(&sr);
}


void getEst3Ticks(struct chromGaps *cg, char *chrom, struct sqlConnection *conn, FILE *f)
/* Get est 3' ends. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct est3 *el;

sqlSafef(query, sizeof query, "select * from %s where chrom = '%s'", "est3", chrom);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    el = est3Load(row);
    printTab(f, cg, chrom, el->chromStart, el->chromEnd, 
	    "est3", "tick", 0, 0, 0, ".");
    est3Free(&el);
    }
sqlFreeResult(&sr);
}

void getEstIntronTicks(struct chromGaps *cg, char *chrom, struct sqlConnection *conn, FILE *f)
/* Get ests with introns. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct psl *psl;
int lastEnd = -BIGNUM;
int sepDistance = 20000;

sqlSafef(query, sizeof query, "select * from %s_intronEst", chrom);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row);
    if (psl->tStart > lastEnd + sepDistance)
	printTab(f, cg, chrom, psl->tStart, psl->tStart+1, 
		"est3", "tick", 0, 0, 0, ".");
    lastEnd = psl->tEnd;
    pslFree(&psl);
    }
sqlFreeResult(&sr);
}


void getEstTicks(struct chromGaps *cg, char *chrom, struct sqlConnection *conn, FILE *f)
{
if (sqlTableExists(conn, "est3"))
   getEst3Ticks(cg, chrom, conn, f);
else
   getEstIntronTicks(cg,chrom,conn, f);
}

void getGc(struct chromGaps *cg, char *chrom, struct sqlConnection *conn, FILE *f)
/* Write wiggle track for GC. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct gcPercent *el;

printf("  getting GC wiggle\n");
sqlSafef(query, sizeof query, "select * from %s where chrom = '%s'", "gcPercent", chrom);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    el = gcPercentLoad(row);
    if (el->gcPpt != 0)
        {
	double scale;
	scale = (el->gcPpt - 300)*0.00300;
	if (scale > 1) scale = 1;
	if (scale < 0) scale = 0;
	printTabNum(f, cg, chrom, el->chromStart, el->chromEnd, 
		"gcPercent", "wiggle", 0, 0, 0, scale);
	}
    gcPercentFree(&el);
    }
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

void getRepeatDensity(struct chromGaps *cg, char *chrom, int chromSize, struct sqlConnection *conn, FILE *f,
	char *repClass, int windowSize, double scale, int red, int green, int blue)
/* Put out repeat density info. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct rmskOut rmsk;
struct agpFrag frag;
int winsPerChrom = (chromSize + windowSize-1)/windowSize;
int *winBases;
int *winRepBases;
int i;
int baseCount, repCount;
double density;
double minDen = 1.0, maxDen = 0.0;
int winStart, winEnd;

printf("  Getting %s repeats in %d windows of size %d\n", repClass, winsPerChrom, windowSize);
fflush(stdout);

/* Count up repeats in each window. */
AllocArray(winRepBases, winsPerChrom);
sqlSafef(query, sizeof query, "select * from %s_rmsk where repClass = '%s'", chrom, repClass);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    rmskOutStaticLoad(row, &rmsk);
    addSegTotals(rmsk.genoStart, rmsk.genoEnd, winRepBases, windowSize, chromSize);
    }
sqlFreeResult(&sr);
/* Count up bases in each window. */
AllocArray(winBases, winsPerChrom);
sqlSafef(query, sizeof query, "select * from %s_gold", chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    agpFragStaticLoad(row, &frag);
    addSegTotals(frag.chromStart, frag.chromEnd, winBases, windowSize, chromSize);
    }
sqlFreeResult(&sr);

/* Output density. */
for (i=0; i<winsPerChrom; ++i)
    {
    if ((baseCount = winBases[i]) > windowSize/3)
        {
	repCount = winRepBases[i];
	density = (double)repCount/(double)baseCount;
	if (density < minDen) minDen = density;
	if (density > maxDen) maxDen = density;
	density *= scale;
	if (density > 1.0) density = 1.0;
	winStart = i*windowSize;
	winEnd = winStart + windowSize;
	if (winEnd > chromSize) winEnd = chromSize;
	printTabNum(f, cg, chrom, winStart, winEnd, 
		repClass, "wiggle", red, green, blue, density);
	}
    }
printf("   %s minDen %f, maxDen %f\n", chrom, minDen, maxDen);
freeMem(winBases);
freeMem(winRepBases);
}

#define finCov 100

struct rgbColor draftCols[10];


void initDraftCols()
/* Initialize draft colors with a range. */
{
int i;
int dif;
int maxCol = ArraySize(draftCols)-1;
double scale;

for (i=0; i<= maxCol; ++i)
    {
    scale = (double)i/(double)maxCol;
    dif = rgbRedOrange.r - rgbYellow.r;
    draftCols[i].r = rgbYellow.r + round(dif*scale);
    dif = rgbRedOrange.g - rgbYellow.g;
    draftCols[i].g = rgbYellow.g + round(dif*scale);
    dif = rgbRedOrange.b - rgbYellow.b;
    draftCols[i].b = rgbYellow.b + round(dif*scale);
    }
}

void makeBackupCoverageHash(struct sqlConnection *conn, struct hash *hash)
/* Make hash table that describe coverage of each clone just based on
 * whether it's draft/predraft/etc. Only do this if clone not already
 * in hash. */
{
char query[512];
struct clonePos cp;
struct sqlResult *sr;
char **row;
int coverage;
char stage;

sqlSafef(query, sizeof query, "select * from clonePos");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    clonePosStaticLoad(row, &cp);
    stage = cp.stage[0];
    if (stage == 'D')
	coverage = 5;
    else if (stage == 'F')
	coverage = finCov;
    else if (stage == 'P')
	coverage = 2;
    else
	errAbort("Unknown stage %c for clone %s", stage, cp.name);
    chopSuffix(cp.name);
    if (!hashLookup(hash, cp.name))
	hashAdd(hash, cp.name, intToPt(coverage));
    }
sqlFreeResult(&sr);
}

void makeMainCoverageHash(char *fileName, struct hash *hash)
/* Scan file for clone coverage. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[16];
int wordCount;
double d;
int rd;
char *acc;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    lineFileExpectWords(lf, wordCount, 2);
    rd = round(atof(words[1]));
    if (rd > 20) rd = 20;
    hashAddUnique(hash, words[0], intToPt(rd));
    }
lineFileClose(&lf);
}

struct hash *makeCoverageHash(struct sqlConnection *conn)
/* Get coverage hash, making it if necessary. */
{
static struct hash *hash = NULL;
if (hash == NULL)
    {
    hash = newHash(16);
    makeMainCoverageHash(covFile, hash);
    makeBackupCoverageHash(conn, hash);
    }
return hash;
}

void covInWin(struct hash *covHash, struct chromGaps *cg,
	char *chrom, int start, int end, struct gl *glList, FILE *f)
/* Process coverage in a window of chromosome. */
{
int size = end-start;
UBYTE *cov = needLargeZeroedMem(size+1);  /* A byte for each base in window. */
struct gl *gl;
int s, e;
int coverage;
char cloneName[64];
void *covAsPt;
int i,c;
int startIx, runSize;
UBYTE val, lastVal;

for (gl = glList; gl != NULL; gl = gl->next)
    {
    if ((s = gl->start) < end && (e = gl->end) > start)
        {
	if (s < start) s = start;
	if (e > end) e = end;
	assert(s<e);
	s -= start;
	e -= start;
	strcpy(cloneName, gl->frag);
	chopSuffix(cloneName);
	covAsPt = hashMustFindVal(covHash, cloneName);
	coverage = ptToInt(covAsPt);
	for (i=s; i<e; ++i)
	    {
	    c = coverage + cov[i];
	    if (c > finCov) c = finCov;
	    cov[i] = c;
	    }
	}
    }

/* Add sentinal value to simplify run-length encoding loop. */
cov[size] = finCov+1;
startIx = 0;
lastVal = cov[0];
for (i=1; i<=size; ++i)
    {
    if ((val = cov[i]) != lastVal)
        {
	if (lastVal != 0)
	    {
	    struct rgbColor *col;
	    runSize = i-startIx;
	    if (lastVal == finCov)
	        col = &rgbRed;
	    else if (lastVal >= ArraySize(draftCols))
	        col = &rgbRedOrange;
	    else
	        col = &draftCols[lastVal-1];
	    s = startIx + start;
	    e = i + start;
	    printTabNum(f, cg, chrom, s, e, 
		    "coverage", "box", col->r, col->g, col->b, lastVal);
	    }
	startIx = i;
	lastVal = val;
	}
    }
freeMem(cov);
}

void getCoverage(struct chromGaps *cg, char *chrom, int chromSize, struct sqlConnection *conn, FILE *f)
/* Make coverage track. */
{
struct hash *covHash = makeCoverageHash(conn);
char query[512];
struct gl *glList = NULL, *gl;
struct sqlResult *sr;
char **row;
int coverage;
int start, end;
int winSize = 1024*1024*64;

printf("  getting coverage\n");
initDraftCols();
sqlSafef(query, sizeof query, "select * from %s_gl", chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gl = glLoad(row);
    slAddHead(&glList, gl);
    }
sqlFreeResult(&sr);
slReverse(&glList);

for (start = 0; start < chromSize; start = end)
    {
    end = start + winSize;
    if (end > chromSize)
        end = chromSize;
    covInWin(covHash, cg, chrom, start, end, glList, f);
    }
glFreeList(&glList);
}

void getEnsTicks(struct chromGaps *cg, char *chrom, int chromSize, 
	FILE *f, char *tabName)
/* Get Ensembl gene ticks from tab separated file. */
{
struct lineFile *lf = lineFileOpen(tabName, TRUE);
char *words[8];
int wordCount;
int start;

printf("  Getting Ensembl gene ticks\n");
while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    lineFileExpectWords(lf, 4, wordCount);
    if (sameString(words[0], chrom))
        {
	start = atoi(words[1]);
	printTab(f, cg, chrom, start, start+1, 
		"genePred", "tick", 150, 0, 0, ".");
	}
    }
lineFileClose(&lf);
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

sqlSafef(query, sizeof query, "select * from %s_gap where chromStart < %d and chromEnd > %d",
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

int countSnps(struct sqlConnection *conn, 
	char *chrom, int chromStart, int chromEnd)
/* Count the number of SNPs in window. */
{
char query[256];

sqlSafef(query, sizeof query, 
    "select count(*) from snpTsc where chrom = '%s' and chromStart < %d and chromEnd > %d",
    chrom, chromEnd, chromStart);
return sqlQuickNum(conn, query);
}

void slowGetSnpDensity(struct chromGaps *cg, char *chrom, int chromSize, struct sqlConnection *conn, FILE *f)
/* Make SNP density track. */
{
int windowSize = 50000;
int s, e;
int size;
double ratio, totalRatio = 0;
double maxCount = 0, minCount=windowSize*2, totalCount = 0;

for (s = 0; s < chromSize; s += 50000)
    {
    e = s + windowSize;
    if (e > chromSize) e = chromSize;
    size = e - s;
    if (size > windowSize/2)
        {
	ratio = 1.0 - nRatio(conn, chrom, s, e);
	uglyf("%s:%d-%d ratio %f\n", chrom, s, e, ratio);
	if (ratio >= 0.5)
	    {
	    int rawCount = countSnps(conn, chrom, s, e);
	    double count = rawCount/ratio;
	    uglyf(" rawCount %d, count %f\n", rawCount, count);
	    if (count < minCount)
	        minCount = count;
	    if (count > maxCount)
	        maxCount = count;
	    totalRatio += ratio;
	    totalCount += count;
	    }
	}
    }
printf("%s SNP density in %d windows is between %f and %f, average %f\n",
	chrom, windowSize, minCount, maxCount, totalCount/totalRatio);
}

void getSnpDensity(struct chromGaps *cg, char *chrom, int chromSize, struct sqlConnection *conn, FILE *f)
/* Put out SNP density info. */
{
int windowSize = 50000;
double scale = 500.0;
struct sqlResult *sr;
char **row;
char query[256];
struct snp snp;
struct agpFrag frag;
int winsPerChrom = (chromSize + windowSize-1)/windowSize;
int *winBases;
int *winRepBases;
int i;
int baseCount, repCount;
double density;
double minDen = 1.0, maxDen = 0.0;
int winStart, winEnd;

printf("  Getting SNPs in %d windows of size %d\n", winsPerChrom, windowSize);
fflush(stdout);

/* Count up SNPs in each window. */
AllocArray(winRepBases, winsPerChrom);
sqlSafef(query, sizeof query, "select * from snpTsc where chrom = '%s'", chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    snpStaticLoad(row, &snp);
    addSegTotals(snp.chromStart, snp.chromEnd, winRepBases, windowSize, chromSize);
    }
sqlFreeResult(&sr);

/* Count up bases in each window. */
AllocArray(winBases, winsPerChrom);
sqlSafef(query, sizeof query, "select * from %s_gold", chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    agpFragStaticLoad(row, &frag);
    addSegTotals(frag.chromStart, frag.chromEnd, winBases, windowSize, chromSize);
    }
sqlFreeResult(&sr);

/* Output density. */
for (i=0; i<winsPerChrom; ++i)
    {
    if ((baseCount = winBases[i]) > windowSize/2)
        {
	repCount = winRepBases[i];
	density = (double)repCount/(double)baseCount;
	if (density < minDen) minDen = density;
	if (density > maxDen) maxDen = density;
	density *= scale;
	if (density > 1.0) density = 1.0;
	winStart = i*windowSize;
	winEnd = winStart + windowSize;
	if (winEnd > chromSize) winEnd = chromSize;
	printTabNum(f, cg, chrom, winStart, winEnd, 
		"SNP", "wiggle", 128, 0, 128, density);
	}
    }
printf("   %s minDen %f, maxDen %f\n", chrom, minDen, maxDen);
freeMem(winBases);
freeMem(winRepBases);
}

void getBands(struct chromGaps *cg, char *chrom, int chromSize, struct sqlConnection *conn, FILE *f)
/* Get chromosome bands stuff. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct cytoBand el;
int r,g,b;
char *stain;

printf("  getting bands\n");
sqlSafef(query, sizeof query, "select * from cytoBand where chrom = '%s'", chrom);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    cytoBandStaticLoad(row, &el);
    stain = el.gieStain;
    if (startsWith("gneg", stain))
        {
	r = g = b = 220;
	}
    else if (startsWith ("gpos", stain))
        {
	int percentage = 100;
	stain += 4;
	if (isdigit(stain[0]))
	    percentage = atoi(stain);
	r = g = b = round((100.0-percentage)*200);
	}
    else if (startsWith("gvar", stain))
        {
	r = 180;
	g = 180;
	b = 120;
	}
    else
        {
	r = 150;
	g = 50;
	b = 50;
	}
    printTab(f, cg, chrom, el.chromStart, el.chromEnd, 
	    "band", "box", r, g, b, el.name);
    }
sqlFreeResult(&sr);
}


void oneChrom(char *chrom, struct sqlConnection *conn, 
	struct hash *hugoDupHash, struct hash *knownUniqHash, 
	struct hash *resolvedDupHash, struct hash *diseaseNameHash, 
	struct hash *diseaseOmimHash, FILE *f)
/* Get info for one chromosome.  */
{
int chromSize = hChromSize(chrom);
struct chromGaps *cg = NULL;

printf("Processing %s\n", chrom);

getHeterochromatin(cg, chrom, conn, f);
getBands(cg, chrom, chromSize, conn, f);
getCoverage(cg, chrom, chromSize, conn, f);
getGc(cg, chrom, conn, f);
getRepeatDensity(cg, chrom, chromSize, conn, f, "SINE", 100000, 1.0/0.66, 255, 0, 0);
getRepeatDensity(cg, chrom, chromSize, conn, f, "LINE", 100000, 1.0/0.66, 0, 0, 255);
getSnpDensity(cg, chrom, chromSize, conn, f);
getRnaGenes(cg, chrom, conn, f);
getCpgIslands(cg, chrom, conn, f);
getExofishHits(cg,chrom,conn,f);
//getTetHits(cg, chrom, conn, f);		/* WABA Tet alignments. */
getEstTicks(cg, chrom, conn, f);
getPredGenes(cg, chrom, conn, f, "genieAlt", 150, 0, 0);
//getPredGenes(cg, chrom, conn, f, "ensGene", 150, 0, 0);	/* Ensemble full predictions. */
getEnsTicks(cg, chrom, chromSize, f, ensTick);
getPredGenes(cg, chrom, conn, f, "genieKnown", blueGene.r, blueGene.g, blueGene.b);
getKnownGenes(cg, chrom, conn, f, hugoDupHash, knownUniqHash, resolvedDupHash, 
	diseaseNameHash, diseaseOmimHash);
}

void getHugoDupes(struct sqlConnection *conn, struct hash *hugoDupHash)
/* Read in all hugo genes and save the duplicated ones in hugoDupHash */
{
struct hash *hash = newHash(0);
struct sqlResult *sr;
char **row;
char *name;
struct slName *txIdList = NULL, *txId;
char query[256];
char geneName[256];

printf("Looking for duplicated HUGO genes:\n");
sr = sqlGetResult(conn, "NOSQLINJ select name from genieKnown");
while ((row = sqlNextRow(sr)) != NULL)
    {
    txId = newSlName(row[0]);
    slAddHead(&txIdList, txId);
    }
sqlFreeResult(&sr);


for (txId = txIdList; txId != NULL; txId = txId->next)
    {
    sqlSafef(query, sizeof query, "select name from knownMore where transId = '%s'",
		   txId->name);
    if (sqlQuickQuery(conn, query, geneName, sizeof(geneName)))
	{
	if (hashLookup(hash, geneName) && !hashLookup(hugoDupHash, geneName))
	    {
	    printf("    %s\n", geneName);
	    hashStore(hugoDupHash, geneName);
	    }
	else
	    hashAdd(hash, geneName, NULL);
	}
    }
freeHash(&hash);
}

void foldDb(int chromCount, char *chromNames[])
/* foldDb - Search database info for making foldout. */
{
int i;
char *database = "oo21";
struct sqlConnection *conn = sqlConnect(database);
struct hash *hugoDupHash = newHash(0);
struct hash *knownUniqHash = newHash(0);
struct hash *resolvedDupHash = NULL;
struct resolvedDup *rdList = NULL;
struct hash *diseaseNameHash = NULL;
struct hash *diseaseOmimHash = NULL;

dupeFile = mustOpen(dupeFileName, "w");
hSetDb(database);
loadNtPatchFiles();
// setupHugeGaps(conn);
getHugoDupes(conn, hugoDupHash);
makeDiseaseHash(conn, &diseaseNameHash, &diseaseOmimHash);
makeResolvedDupes(&resolvedDupHash, &rdList);
for (i=0; i<chromCount; ++i)
    {
    char fileName[512];
    FILE *f;
    sprintf(fileName, "%s.tab", chromNames[i]);
    f = mustOpen(fileName, "w");
    oneChrom(chromNames[i], conn, hugoDupHash, knownUniqHash, resolvedDupHash, 
    	diseaseNameHash, diseaseOmimHash, f);
    fclose(f);
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 2)
    usage();
foldDb(argc-1, argv+1);
return 0;
}
