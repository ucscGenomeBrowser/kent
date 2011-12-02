/* dupeFoo - Do some duplication analysis. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "psl.h"
#include "fa.h"


char *chromPrefix = "piece";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dupeFoo - Do some duplication analysis\n"
  "usage:\n"
  "   dupeFoo dupe.psl dupe.fa region.out\n"
  );
}

struct frag 
/* A chromosome fragment. */
    {
    struct frag *next;
    char *name;		/* Frag name. */
    char *chrom;	/* Chromosome name. */
    int start,end;	/* Start/end pos */
    struct psl *pslList;	/* List of associated psls. */
    };

struct frag *readFragList(char *fileName)
/* Read list of frags from file. */
{
struct frag *list = NULL, *frag;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct dnaSeq seq;
char *s;
int fragIx;
struct hash *chromHash = newHash(5);
ZeroVar(&seq);

printf("Reading %s\n", fileName);
while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
    {
    AllocVar(frag);
    frag->name = cloneString(seq.name);
    s = strrchr(seq.name, '_');
    if (s == NULL || !isdigit(s[1]))
        errAbort("Expecting _ and number in %s", seq.name);
    fragIx = atoi(s+1);
    frag->chrom = "chr14";
    frag->start = fragIx*1000;
    frag->end = frag->start + 1000;
    slAddHead(&list, frag);
    }
lineFileClose(&lf);
printf("Read %d fragments from %s\n", slCount(list), fileName);
slReverse(&list);
return list;
}

void printPercent(char *title, int p, int q)
/* Print a line containing title and p/q as a percentage. */
{
if (q == 0)
    printf("%-12s n/a\n", title);
else
    printf("%-12s %5.2f%% (%d/%d)\n", title, 100.0*p/q, p, q);
}

struct region
/* Summary info about a patSpace alignment */
    {
    struct region *next;  /* Next in singly linked list. */
    unsigned match;	/* Number of bases that match that aren't repeats */
    unsigned misMatch;	/* Number of bases that don't match */
    unsigned repMatch;	/* Number of bases that match but are part of repeats */
    unsigned nCount;	/* Number of 'N' bases */
    unsigned qNumInsert;	/* Number of inserts in query */
    int qBaseInsert;	/* Number of bases inserted in query */
    unsigned tNumInsert;	/* Number of inserts in target */
    int tBaseInsert;	/* Number of bases inserted in target */
    char strand[3];	/* + or - for strand */
    char *qName;	/* Query sequence name */
    unsigned qSize;	/* Query sequence size */
    int qStart;	/* Alignment start position in query */
    int qEnd;	/* Alignment end position in query */
    char *tName;	/* Target sequence name */
    unsigned tSize;	/* Target sequence size */
    int tStart;	/* Alignment start position in target */
    int tEnd;	/* Alignment end position in target */
    boolean isHit;	/* Little flag to keep track of if hit. */
    };

struct region *fragConnects(struct region *openList, struct psl *psl, int distance)
/* Return a region the psl extends if possible. */
{
struct region *region;
for (region = openList; region != NULL; region = region->next)
    {
    if (sameString(region->tName, psl->tName) && region->strand[0] == psl->strand[0])
        {
	int dif;
	if (psl->strand[0] == '+')
	    {
	    dif = psl->tEnd - region->tEnd;
	    if (0.80 * distance < dif && dif < 1.20 * distance)
		{
		region->tEnd =  psl->tEnd;
		region->qEnd += distance;
		return region;
		}
	    }
	else
	    {
	    dif = region->tStart - psl->tStart;
	    if (0.80 * distance < dif && dif < 1.20 * distance)
	        {
		region->tStart = psl->tStart;
		region->qEnd += distance;
		return region;
		}
	    }
	}
    }
return NULL;
}

struct region *stitchedList(struct frag *fragList)
/* Stitch things together into list of regions. */
{
struct frag *frag, *nextFrag;
struct region *openList = NULL, *closedList = NULL, *region, *nextRegion, *stillOpenList = NULL;
struct psl *psl;
int distance = 0;

for (frag = fragList; frag != NULL; frag = nextFrag)
    {
    nextFrag = frag->next; 

    /* Clear hit flags. */
    for (region = openList; region != NULL; region = region->next)
	region->isHit = FALSE;

    /* Loop through psl's extending open regions where possible
     * and creating new open regions where not. */
    for (psl = frag->pslList; psl != NULL; psl = psl->next)
        {
	if ((region = fragConnects(openList, psl, distance)) == NULL)
	    {
	    AllocVar(region);
	    slAddHead(&openList, region);
	    region->qName = frag->chrom;
	    region->qStart = frag->start;
	    region->qEnd = frag->end;
	    region->tName = psl->tName;
	    region->tStart = psl->tStart;
	    region->tEnd = psl->tEnd;
	    region->strand[0] = psl->strand[0];
	    }
	region->match += psl->match;
	region->misMatch += psl->misMatch;
	region->repMatch += psl->repMatch;
	region->nCount += psl->nCount;
	region->qNumInsert += psl->qNumInsert;
	region->qBaseInsert += psl->qBaseInsert;
	region->tNumInsert += psl->tNumInsert;
	region->tBaseInsert += psl->tBaseInsert;
	region->isHit = TRUE;
	}

    /* Move regions not hit by this fragment to closed list. */
    for (region = openList; region != NULL; region = nextRegion)
        {
	nextRegion = region->next;
	if (region->isHit)
	    {
	    slAddHead(&stillOpenList, region);
	    }
	else
	    {
	    slAddHead(&closedList, region);
	    }
	}
    openList = stillOpenList;
    stillOpenList = NULL;
    if (nextFrag != NULL)
        distance = nextFrag->start - frag->start;
    }

/* Move remainder of open list to cloest list. */
for (region = openList; region != NULL; region = nextRegion)
    {
    nextRegion = region->next;
    slAddHead(&closedList, region);
    }
slReverse(&closedList);
return closedList;
}

#ifdef OLD
void writeRegions(struct frag *fragList, char *fileName)
/* Write out list of regions that seem to be duplicated. */
{
struct region *region, *regionList = stitchedList(fragList);
FILE *f = mustOpen(fileName, "w");

for (region = regionList; region != NULL; region = region->next)
    {
    if (sameString(region->qName, region->tName) && region->qStart == region->tStart &&
	    region->strand[0] == '+')
	continue;
    fprintf(f, "%d\t", region->match);
    fprintf(f, "%d\t", region->misMatch);
    fprintf(f, "%d\t", region->repMatch);
    fprintf(f, "%d\t", region->nCount);
    fprintf(f, "%d\t", region->qNumInsert);
    fprintf(f, "%d\t", region->qBaseInsert);
    fprintf(f, "%d\t", region->tNumInsert);
    fprintf(f, "%d\t", region->tBaseInsert);
    fprintf(f, "%s\t", region->strand);
    fprintf(f, "%s\t", region->qName);
    fprintf(f, "%d\t", region->qStart);
    fprintf(f, "%d\t", region->qEnd);
    fprintf(f, "%s\t", region->tName);
    fprintf(f, "%d\t", region->tStart);
    fprintf(f, "%d\n", region->tEnd);
    }
carefulClose(&f);
}
#endif /* OLD */

int calcRegionScore(struct region *region)
/* Calculate score for region as a number between 0 and 1000 */
{
double score = (region->match + region->repMatch);

/* calc percent identity more or less. */
score /= region->match + region->repMatch + region->misMatch + 
	region->qNumInsert*2 + region->tNumInsert*2;

score -= 0.975;
score *= 40000;
if (score < 0) score = 0;
if (score > 1000) score = 1000;
return round(score);
}

void writeRegions(struct frag *fragList, char *fileName)
/* Write out list of regions that seem to be duplicated in a BED format. */
{
struct region *region, *regionList = stitchedList(fragList);
FILE *f = mustOpen(fileName, "w");
int score;

for (region = regionList; region != NULL; region = region->next)
    {
    if (sameString(region->qName, region->tName) && region->qStart == region->tStart &&
	    region->strand[0] == '+')
	continue;
    score = calcRegionScore(region);

    /* Print out one side of dupe. */
    fprintf(f, "%s\t%d\t%d\t", region->qName, region->qStart, region->qEnd);
    fprintf(f, "%s:%d%s%d\t", region->tName, region->tStart/1000, region->strand, region->tEnd/1000);
    fprintf(f, "%d\t%s\n", score, region->strand);

    /* Print out other side of dupe. */
    fprintf(f, "%s\t%d\t%d\t", region->tName, region->tStart, region->tEnd);
    fprintf(f, "%s:%d%s%d\t", region->qName, region->qStart/1000, region->strand, region->qEnd/1000);
    fprintf(f, "%d\t%s\n", score, region->strand);
    }
carefulClose(&f);
}


void dupeFoo(char *pslName, char *faName, char *regionFile)
/* dupeFoo - Do some duplication analysis. */
{
struct lineFile *lf;
struct frag *fragList = NULL, *frag;
struct hash *fragHash = newHash(16);
struct psl *psl;
int fragCount=0,missCount=0,dupeCount=0,kSub=0,
   k1=0, k10=0,k100=0,k1000=0,k10000=0,diffChrom=0,distance;

/* Read in fragment list and put it in hash. */
fragList = readFragList(faName);
for (frag = fragList; frag != NULL; frag = frag->next)
    hashAdd(fragHash, frag->name, frag);

/* Read psl's and store under the fragment the belong to. */
lf = pslFileOpen(pslName);
while ((psl = pslNext(lf)) != NULL)
    {
    if ((frag = hashFindVal(fragHash, psl->qName)) == NULL)
        errAbort("Couldn't find %s in %s line %d of %s", 
		psl->qName, faName, lf->lineIx, lf->fileName);
    slAddHead(&frag->pslList, psl);
    }
lineFileClose(&lf);

/* Look through fragments and report missing and dupes. */
for (frag = fragList; frag != NULL; frag = frag->next)
    {
    ++fragCount;
    if ((psl = frag->pslList) == NULL)
        {
	++missCount;
	printf("missing %s\n", frag->name);
	}
    else
        {
	for (psl = frag->pslList; psl != NULL; psl = psl->next)
	    {
	    if (sameString(psl->tName, frag->chrom))
	        {
		distance = frag->start - psl->tStart;
		if (distance != 0)
		    {
		    if (distance < 0) distance = -distance;
		    if (distance >= 10000000) ++k10000;
		    else if (distance >= 1000000) ++k1000;
		    else if (distance >= 100000) ++k100;
		    else if (distance >= 10000) ++k10;
		    else if (distance >= 1000) ++k1;
		    else ++kSub;
		    }
		}
	    else
	        {
		++diffChrom;
		}
	    }
	}
    }
printPercent("Total", fragCount, fragCount);
printPercent("Unaligned", missCount, fragCount);
printPercent("Other Chrom", diffChrom, fragCount);
printPercent("Same Chrom >10M", k10000, fragCount);
printPercent("Same Chrom >1M", k1000, fragCount);
printPercent("Same Chrom >10Ok", k100, fragCount);
printPercent("Same Chrom >1Ok", k10, fragCount);
printPercent("Same Chrom >1k", k1, fragCount);
printPercent("Self-overlap", kSub, fragCount);
writeRegions(fragList, regionFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
dupeFoo(argv[1], argv[2], argv[3]);
return 0;
}
