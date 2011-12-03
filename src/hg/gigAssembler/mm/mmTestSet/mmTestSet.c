/* mmTestSet - Make test set for map-mender.. */
#include "common.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "mmTestSet - Make test set for map-mender.\n"
  "usage:\n"
  "   mmTestSet setName mapSize cloneSize cloneCount finPercent endPercent dupePercent noisePercent\n"
  "mapSize and cloneSize should be in 1000's of bases");
}

struct clone
/* Info on a faked clone. */
    {
    struct clone *next;
    char name[32];		/* Clone name. */
    int start;                  /* Start in map (in 1000's) */
    int end;		        /* End in map in 1000's */
    int phase;                  /* Clone phase 1, 2, or 3 (unordered, ordered, finished). */
    double coverage;            /* Coverage % for phase 2,3 clones. */
    boolean xEnded, yEnded;     /* True if one or both end sequences are known. */
    boolean xStarts;		/* True if start with x end. */
    int mapPos;			/* Map position roughly. */
    };

int cloneCmpStart(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct clone *a = *((struct clone **)va);
const struct clone *b = *((struct clone **)vb);
return a->start - b->start;
}

void dumpClone(struct clone *clone, FILE *f)
{
fprintf(f, "%-12s %d %d-%d %d %5.2f%% %d %d %d\n", 
    clone->name, clone->mapPos, clone->start*1000, clone->end*1000, clone->phase, 100.0*clone->coverage, 
    clone->xEnded, clone->yEnded, clone->xStarts);
}

struct clone *cloneListFromArray(struct clone *array, int arraySize)
/* Make up clone list from array. */
{
struct clone *list = NULL, *el;
int i;

for (i=0; i<arraySize; ++i)
   {
   el = &array[i];
   slAddHead(&list, el);
   }
slReverse(&list);
return list;
}

int *makeRepMap(int mapSize, double repRatio, char *fileName)
/* Allocate a map with each number representing 1000 bases. */
{
int *map;
FILE *f = mustOpen(fileName, "w");
int i;
int repCount;
int oneRepSize, repTotal, repLeft;
int start, end, size, dir, source, dest;

AllocArray(map, mapSize);
for (i=0; i<mapSize; ++i)
    map[i] = i;
repTotal = round(repRatio * mapSize);

for (oneRepSize = 0, repCount = 0; repCount < repTotal; repCount += oneRepSize)
    {
    repLeft = repTotal - repCount;
    ++oneRepSize;
    if (oneRepSize > repLeft) oneRepSize = repLeft;
    size = mapSize - oneRepSize;
    source = rand()%size;
    dir = rand()%2;
    if (dir == 0) 
	{
        dir = -1;
	source += oneRepSize;
	}
    dest = rand()%size;
    for (i=0; i<oneRepSize; ++i)
        {
	map[dest] = map[source];
	dest += 1;
	source += dir;
	}
    }
for (i=0; i<mapSize; ++i)
    {
    fprintf(f, "%2d ", map[i]);
    if (i%25 == 0)
	fprintf(f, "\n");
    }
fprintf(f, "\n");
fclose(f);
return map;
}

struct clone *makeClones(int mapSize, int cloneSize, int cloneCount, 
	double finRatio, double endRatio, int *repMap, char *cloneDumpFile)
/* Create array of clones one map. */
{
struct clone *clones, *clone;
int i, j, oneSize, mapInnerSize;
int cut;
int finCut = finRatio*100;
int orderedCut = finCut + finRatio*50;
int endCut = endRatio*100;
static char phaseLetters[4] = "-uof";
FILE *f = mustOpen(cloneDumpFile, "w");
int endCount;

if (orderedCut > 100)
   orderedCut = 100;

AllocArray(clones, cloneCount);
fprintf(f, "#name\t\tphase\tmapPos\tends\tsize\tseq\n");
for (i = 0; i<cloneCount; ++i)
    {
    clone = clones+i;

    /* Figure out which phase to make this clone. */
    cut = rand()%100;
    if (cut < finCut) clone->phase = 3;
    else if (cut < orderedCut) clone->phase = 2;
    else clone->phase = 1;

    /* Figure out size and start of clone. */
    oneSize = cloneSize + cloneSize/2 - rand()%cloneSize;
    mapInnerSize = mapSize - oneSize;
    clone->start = rand()%mapInnerSize;
    clone->end = clone->start + oneSize;
    clone->mapPos = clone->start + 6*(rand()%cloneSize) - 3-cloneSize;

    if (clone->phase == 3)
        clone->coverage = 1.0;
    else
        //clone->coverage = 0.9 + 0.01*(rand()%12);
        clone->coverage = 1.0;

    if (clone->phase > 1)
	{
        clone->xEnded = clone->yEnded = clone->xStarts = TRUE;
	endCount = 2;
	}
    else
        {
	endCount = 0;
	cut = rand()%100;
	if (cut < endCut)
            {
	    endCount = 1;
	    clone->xEnded = TRUE;
	    if (rand()%10 <= 7)
	       {
	       clone->yEnded = TRUE;
	       endCount = 2;
	       }
	    clone->xStarts = rand()%2;
	    }
	}
    /* Give clone a semi-easy-to-track name. */
    sprintf(clone->name, "%c%c%d@%d_%d", phaseLetters[clone->phase], i%26 + 'A',  i/26, clone->start, clone->end);
    //sprintf(clone->name, "%c%c%d", phaseLetters[clone->phase], i%26 + 'A',  i/26);

    fprintf(f, "%s\tH%d\t%d\t%d\t%d\t", clone->name, clone->phase, clone->mapPos, endCount, round(clone->coverage*oneSize*1000) );
#ifdef SOMETIMES
    for (j=clone->start; j<clone->end; j++)
       {
       if (j != clone->start)
          fprintf(f,".");
       fprintf(f, "%d", repMap[j]);
       }
#endif /* SOMETIMES */
    fprintf(f, "\n");
    }
fclose(f);
return clones;
}

void writeAnswer(struct clone *cloneList, char *fileName)
/* Write out answer, assuming cloneList is sorted. */
{
FILE *f = mustOpen(fileName, "w");
struct clone *nextClone = NULL, *clone;
int end = 0;

for (clone = cloneList; clone != NULL; clone = nextClone)
    {
    nextClone = clone->next;
    dumpClone(clone, f);
    if (clone->end > end) end = clone->end;
    if (nextClone == NULL || rangeIntersection(clone->start, end, nextClone->start, nextClone->end) <= 0)
        {
	fprintf(f, "\n");
	}
    }
carefulClose(&f);
}

int matchInRange(int match, int *repMap, int s, int e)
/* Return 1 if map is in repMap between s and e, 0 otherwise. */
{
int i;
for (i=s; i<e; ++i)
   if (repMap[i] == match)
	return TRUE;
return FALSE;
}

void oneOverlap(struct clone *a, struct clone *b, int *repMap, double noiseRatio, FILE *f)
/* Calculate overlap between a and b, and write it (and additional info)
 * to file. */
{
int aX, aY, bX, bY;
char axEnd = '?', ayEnd = '?', bxEnd = '?', byEnd = '?';
int i, s, e;
int overlap = 0;

for (i=a->start; i<a->end; ++i)
    overlap += matchInRange(repMap[i], repMap, b->start, b->end);


if (overlap)
    {
    if (noiseRatio > 0)
	{
	int toUse = rand()%10 + 1;
	double useFactor = 1.0 - (noiseRatio/toUse);
	overlap *= useFactor;
	}
    if (a->xStarts)
	{
	aX = repMap[a->start];
	aY = repMap[a->end-1];
	}
    else
	{
	aY = repMap[a->start];
	aX = repMap[a->end-1];
	}
    if (b->xStarts)
	{
	bX = repMap[b->start];
	bY = repMap[b->end-1];
	}
    else
	{
	bY = repMap[b->start];
	bX = repMap[b->end-1];
	}
    if (a->xEnded)
	axEnd = (matchInRange(aX, repMap, b->start, b->end) ? 'Y' : 'N');
    if (a->yEnded)
	ayEnd = (matchInRange(aY, repMap, b->start, b->end) ? 'Y' : 'N');
    if (b->xEnded)
	bxEnd = (matchInRange(bX, repMap, a->start, a->end) ? 'Y' : 'N');
    if (b->yEnded)
	byEnd = (matchInRange(bY, repMap, a->start, a->end) ? 'Y' : 'N');
    if (overlap > 0)
	fprintf(f, "%-12s %c %c     %-12s %c %c   %6d  0\n",  
		a->name, axEnd, ayEnd, b->name, bxEnd, byEnd, overlap*1000);
    }
}

void makeOverlaps(int *repMap, struct clone *cloneList, char *fileName, double noiseRatio)
/* Write list of overlapping clones. */
{
struct clone *a, *b;
FILE *f = mustOpen(fileName, "w");

printf("Making overlaps");
for (a = cloneList; a != NULL; a = a->next)
   {
   for (b = a->next; b != NULL; b = b->next)
       oneOverlap(a, b, repMap, noiseRatio, f);
   printf(".");
   fflush(stdout);
   }
printf("\n");
fclose(f);
}

void mmTestSet(char *setName, int mapSize, int cloneSize, int cloneCount, double finRatio,
	double endRatio, double repRatio, double noiseRatio)
/* mmTestSet - Make test set for map-mender.. */
{
char fileName[512], answerName[512];
int *repMap;
struct clone *clones, *cloneList = NULL;
FILE *f;

//srand( (unsigned)time( NULL ) );
srand( 1235 );
sprintf(fileName, "%s.repMap", setName);
repMap = makeRepMap(mapSize, repRatio, fileName);
printf("Made repeat map of %d kb in %s\n", mapSize, fileName);

/* Make clone list. */
sprintf(fileName, "%s.clones", setName);
clones = makeClones(mapSize, cloneSize, cloneCount, finRatio, endRatio, repMap, fileName);
cloneList = cloneListFromArray(clones, cloneCount);
printf("Made %d clones in %s\n", cloneCount, fileName);

/* Make up overlap info. */
sprintf(fileName, "%s.overlap", setName);
makeOverlaps(repMap, cloneList, fileName, noiseRatio);

/* Write out answer. */
slSort(&cloneList, cloneCmpStart);
sprintf(fileName, "%s.answer", setName);
writeAnswer(cloneList, fileName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int i;

if (argc != 9)
    usage();
for (i=2; i<9; ++i)
   if (!isdigit(argv[i][0]))
       usage();
mmTestSet(argv[1], atoi(argv[2]), atoi(argv[3]), atoi(argv[4]),
	atof(argv[5])*0.01, atof(argv[6])*0.01, atof(argv[7])*0.01, atof(argv[8])*0.01);
return 0;
}
