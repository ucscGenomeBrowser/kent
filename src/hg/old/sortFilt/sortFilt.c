/* sortFilt - merge, sort, and filter patSpace .hit output. 
 *
 * The patSpace .hit files look like:
    Pattern space 0.2 cDNA matcher
    cDNA files:  ../h/bacEnds/bacEnds.fa
    26 genomic files
    5 els in ../h/bac/AC006330.fa 15265 17735 24312 54690 69355 
            ...
    20 els in ../h/bac/AC006428.fa 1207 1302 1254 1127 1440 1300 1447 1957 1477 1186 1524 1474 1706 1530 1344 1320 1976 1811 2681 2825 
    100.0% + AQ172447:0-414 (old 0-414) of 415 at AC006393.28.27:12342-12756
    54.6% + AQ172509:80-415 (old 74-415) of 427 at AC006400.3.2:22844-23174
 * sortFilt takes as input a list of these.  It verifies that each genomic file is
 * only used once.  Then it collects all of the hits above a threshold, and sorts 
 * them on the place in the query sequence where they occurred, and writes them
 * out.
 */

#include "common.h"
#include "hash.h"

struct targetFile
/* A list of names that's part of a hashTable too. */
    {
    struct targetFile *next;  /* Next in list. */
    char *name;               /* Shared with hash table. */
    int contigCount;          /* Number of contigs. */
    int *contigSizes;         /* Size of contigs. */
    };

struct hitHanger
/* This collects information on hits from all files. */
    {
    char *version;                      /* Version of file we're reading. */
    struct hash     *targetFileHash;    /* Hash table of all genomic (target) files. */
    struct targetFile *targetFileList;  /* List of all genomic files. */
    struct slName *queryFileList;       /* List of all cDNA files. */
    int hitFileCount;                   /* Number of hit files processed. */
    struct hash *queryHash;             /* Hash for all query sequences. */
    struct hash *targetHash;            /* Hash for all target sequences. */
    struct hit *hitList;                /* List of all hits. */
    };

struct hit
/* A patSpace hit. */
    {
    struct hit *next;       /* Next in list. */
    double score;           /* Score as a psuedo percent. */
    char *query;            /* Query name (allocated in hash table). */
    char qStrand;           /* Query strand (+ or -) */
    int qStart, qEnd;       /* Start and end of hit within query. */
    int qSize;              /* Query size. */
    char *target;           /* Target name (allocated in hash table). */
    int tStart, tEnd;       /* Start and end of hit within target. */
    int tSize;              /* Target size. */
    char *targetFile;       /* Name of file (as opposed to contig) target is in.  Not allocate here.*/
    };

static int qCmpHit(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct hit *a = *((struct hit **)va);
const struct hit *b = *((struct hit **)vb);
int dif;
dif = strcmp(a->query, b->query);
if (dif == 0)
    {
    UBYTE ao = a->qStart;
    UBYTE bo = b->qStart;
    if (ao < bo)
        dif =  -1;
    else if (ao > bo)
        dif = 1;
    else
        {
        bits32 ao = a->qEnd;
        bits32 bo = b->qEnd;
        if (ao < bo)
            dif =  -1;
        else if (ao == bo)
            dif = 0;
        else
            dif = 1;
        }
    }
return dif;
}


void usage()
/* Print usage instructions and exit. */
{
errAbort(
    "sortFilt - merge, sort, and filter patSpace .hit output.\n"
    "usage:\n"
    "   sortFilt output.sf histogram.txt matchThreshold sizeThreshold repeatMax infile(s).hit\n"
    "where the matchThreshold is the minimum 'psuedo percentage' match\n"
    "to take, size threshold is the minimum size (on the query sequence\n"
    "to take, and repeatMax is the maximum number of repeats to allow");
}

struct lineFile
/* Structure for files read a line at a time. Lets you
 * push back a line. */
    {
    char *fileName;
    FILE *file;
    char *line;
    int maxLineSize;
    int lineCount;
    boolean reuseLine;
    };

struct lineFile *lfOpen(char *name)
/* Open file and create lineFile structure around it. */
{
struct lineFile *lf;
AllocVar(lf);
lf->fileName = cloneString(name);
lf->file = mustOpen(name, "r");
lf->maxLineSize = 16*1024;
lf->line = needLargeMem(lf->maxLineSize);
lf->line[0] = 0;
return lf;
}

void lfClose(struct lineFile **pLf)
/* Close file and free lineFile structure. */
{
struct lineFile *lf;
if ((lf = *pLf) != NULL)
    {
    freeMem(lf->fileName);
    carefulClose(&lf->file);
    freeMem(lf->line);
    freez(pLf);
    }
}

void lfReuse(struct lineFile *lf)
/* Reuse current line. */
{
assert(!lf->reuseLine);
lf->reuseLine = TRUE;
}

char *lfNextLine(struct lineFile *lf)
/* Get next line. */
{
if (lf->reuseLine)
    {
    lf->reuseLine = FALSE;
    return lf->line;
    }
++lf->lineCount;
return fgets(lf->line, lf->maxLineSize, lf->file); 
}

char *lfNeedNext(struct lineFile *lf)
/* Get next line or squawk and die. */
{
char *l = lfNextLine(lf);
if (l == NULL)
    errAbort("Unexpected end of file in %s", lf->fileName);
return l;
}

void lfError(struct lineFile *lf, char *msg)
/* Print line and file name and error message, and abort. */
{
errAbort("%s line %d: %s", lf->fileName, lf->lineCount, msg);
}

void lfWarn(struct lineFile *lf, char *msg)
/* Print line and file name and error message, and abort. */
{
warn("%s line %d: %s", lf->fileName, lf->lineCount, msg);
}

enum {
    accessionSize = 64,
};

void extractAccession(char *s, char acc[accessionSize+1])
/* Extract accession part of file name. */
{
char *t;

/* Advance s to past last directory. */
t = strrchr(s, '/');
if (t != NULL)
    s = t+1;
else
    {
    t = strrchr(s, '\\');
    if (t != NULL)
        s = t+1;
    }

if (strlen(s) > accessionSize)
    errAbort("Long accession");

/* Remove suffix. */
strcpy(acc, s);
t = strchr(acc, '.');
if (t != NULL)
    *t = 0;
touppers(acc);
}

struct hitHanger *hangHits(struct hitHanger *hh, char *fileName, double minMatch, int minSize)
/* Hang the hits in file onto hhIn.  If hhIn is NULL allocate a
 * new hitHanger.  In either case return the hitHanger.  The fileName
 * you pass in should persist - it gets put into some structures.*/
{
boolean firstFile = FALSE;
struct lineFile *lf;
struct targetFile *tf;
int targetCount;
char *line;
char *words[512];
char accessionBuf[accessionSize+1];
int wordCount;
int i;
struct hashEl *hel;

printf("Reading %s\n", fileName);
if (hh == NULL)
    {
    AllocVar(hh);
    hh->targetFileHash = newHash(8);
    hh->queryHash = newHash(19);
    hh->targetHash = newHash(19);
    firstFile = TRUE;
    }

/* Open file and process header. */
    {
    char *prefix = "Pattern space ";
    char *version;

    lf = lfOpen(fileName);

    /* Get first line - make sure it looks like the right type of
     * file and check version number. Also check that this file is
     * not duplicated. */
    line = lfNeedNext(lf);
    if (!startsWith(prefix, line))
        errAbort("%s doesn't start with %s", fileName, prefix);
    line += strlen(prefix);
    wordCount = chopByWhite(line, words, 1);
    if (wordCount < 0)
        errAbort("Expecting version number on first line of %s", fileName);
    version = words[0];
    if (firstFile)
        hh->version = cloneString(version);
    else
        {
        if (!sameString(version, hh->version))
            errAbort("Mixing versions %s and %s", hh->version, version);
        }
    ++hh->hitFileCount;

    /* Next line is about the query (cdna) files, something like: 
     *    cDNA files:  ../h/mrna/est.fa ../h/mrna/mrna.fa 
     * On first hit file save this.  On other hit files make sure
     * it matches. */
    line = lfNeedNext(lf);
    prefix = "cDNA files: ";
    if (!startsWith(prefix, line))
        lfError(lf, "Expecting cDNA files");
    line += strlen(prefix);
    wordCount = chopLine(line, words);
    if (wordCount < 1)
        lfError(lf, "Expecting list of cDNA files");
    for (i=0; i<wordCount; ++i)
        {
        char *qName = words[i];
        if (firstFile)
            {
            struct slName *sn = newSlName(qName);
            slAddHead(&hh->queryFileList, sn);
            }
        else 
            {
            if (!slNameInList(hh->queryFileList, qName))
                errAbort("query %s is in some but not all files", qName);
            }
        }

    
    /* Next line tells us how many target (genomic) files there are. */
    line = lfNeedNext(lf);
    targetCount = atoi(line);
    if (targetCount <= 0)
        lfError(lf, "Expecting count");

    /* Read in info on each target file.  Each line looks something like:
     *    5 els in ../h/bac/AC006330.fa 15265 17735 24312 54690 69355  
     * Store this away and insure that the target files are unique across
     * all hit files. */
    for (i=0; i<targetCount; ++i)
        {
        int elCount;
        int j;
        line = lfNeedNext(lf);
        wordCount = chopLine(line, words);
        if (wordCount < 5)
            lfError(lf, "short line");
        elCount = atoi(words[0]);
        if (elCount <= 0)
            lfError(lf, "element count");
        if (wordCount != elCount + 4)
            lfError(lf, "bad element count");
        extractAccession(words[3], accessionBuf);
        if (hashLookup(hh->targetFileHash, accessionBuf))
            errAbort("Target file %s.fa is repeated in %s", accessionBuf, fileName);

        AllocVar(tf);
        hel = hashAdd(hh->targetFileHash, accessionBuf, tf);
        tf->name = hel->name;
        tf->contigCount = elCount;
        tf->contigSizes = needMem(elCount * sizeof(tf->contigSizes[0]));
        for (j=0; j<elCount; ++j)
            {
            if ((tf->contigSizes[j] = atoi(words[j+4])) <= 0)
                lfError(lf, "bad element count");
            }
        slAddHead(&hh->targetFileList, tf);
        }
    }

/* Read and store each hit until end of file. A hit line looks like:
 *        100.0% + AQ172447:0-414 (old 0-414) of 415 at AC006393.28.27:12342-12756 */
while ((line = lfNextLine(lf)) != NULL)
    {
    double score;
    struct hit *hit;
    char *s, *t;
    char *parts[4];
    int partCount;
    int contigIx;
    int qStart, qEnd, qSize;

    wordCount = chopLine(line, words);
    if (wordCount != 9)
        lfError(lf, "short line");
    score = atof(words[0]);
    if (score < 0.0 || score > 100.1 || !isdigit(words[0][0]))
        lfError(lf, "Expecting score");
    
    if (score >= minMatch)
        {
        partCount = chopString(words[2], ":-", parts, ArraySize(parts));
        if (partCount != 3)
            lfError(lf, "bad query range");
        qStart = atoi(parts[1]);
        qEnd = atoi(parts[2]);
        qSize = atoi(words[6]);
        if (qEnd - qStart >= minSize)
            {
            AllocVar(hit);
            hel = hashStore(hh->queryHash, parts[0]);
            hit->score = score;
            hit->qStrand = words[1][0];
            hit->query = hel->name;
            hit->qStart = qStart;
            hit->qEnd = qEnd;
            hit->qSize = qSize;
        
            partCount = chopString(words[8], ":-", parts, ArraySize(parts));
            if (partCount != 3)
                lfError(lf, "bad target range");

            /* The last .NN is the number of the target contig within the target file. */
            s = parts[0];
            t = strrchr(s, '.');
            if (t == NULL)
                lfError(lf, "bad target range");
            *t++ = 0;
            contigIx = atoi(t);
    
            hel = hashStore(hh->targetHash, s);
            hit->target = hel->name;
            hit->tStart = atoi(parts[1]);
            hit->tEnd = atoi(parts[2]);

            extractAccession(hit->target, accessionBuf);
            if ((hel = hashLookup(hh->targetFileHash, accessionBuf)) == NULL)
                {
                errAbort("Can't find target %s in file list line %d of %s",
                    accessionBuf, lf->lineCount, lf->fileName);
                }
            tf = hel->val;
            if (contigIx >= tf->contigCount)
                lfError(lf, ".contig out of range");  
            hit->tSize = tf->contigSizes[contigIx];
            hit->targetFile = hel->name;
    
            slAddHead(&hh->hitList, hit);
            }
        }
    }

return hh;
}

struct hitHanger *hangAllHits(char *fileNames[], int fileCount, double minMatch, int minSize)
/* Get information from a list of files. */
{
int i;
struct hitHanger *hh = NULL;

for (i=0; i<fileCount; ++i)
    {
    hh = hangHits(hh, fileNames[i], minMatch, minSize);
    }
printf("Sorting\n");
slSort(&hh->hitList, qCmpHit);
return hh;
}

void writeHits(struct hitHanger *hh, char *fileName)
/* Write out hits to file. */
{
FILE *f = mustOpen(fileName, "w");
struct hit *hit;

printf("Writing %s\n", fileName);
for (hit = hh->hitList; hit != NULL; hit = hit->next)
    {
    fprintf(f, "%4.2f%% %c %s:%d-%d of %d at %s:%d-%d of %d\n",
        hit->score, hit->qStrand, hit->query, hit->qStart, hit->qEnd, hit->qSize,
        hit->target, hit->tStart, hit->tEnd, hit->tSize);
    }
fclose(f);
}

int bounds[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 
    10, 20, 30, 40, 50, 60, 70, 80, 90, 
    100, 200, 300, 400, 500, 600, 700, 800, 900, 
    1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000,
    10000, 20000, 30000, 40000, 50000, 60000, 70000, 80000, 90000,
    100000000,};
int histogram[50];

void setHistogram(int count)
/* Inc member of histogram corresponding to count. */
{
int i;
for (i=0; i<ArraySize(bounds); ++i)
    {
    if (bounds[i] >= count)
        {
        histogram[i] += 1;
        break;
        }
    }
}

void printHistogram(char *fileName)
/* Print out histogram file. */
{
FILE *f = mustOpen(fileName, "w");
int i;
int maxHist = 0;

for (i = 0; i < ArraySize(bounds); ++i)
    {
    if (histogram[i] != 0)
        maxHist = i;
    }
for (i=0; i<maxHist; ++i)
    {
    fprintf(f, "<= %d\t %5d\n", bounds[i], histogram[i]);
    }
}

void makeHistogram(struct hit *hitList)
/* Create histogram. */
{
struct hit *hit;
char *lastQuery = NULL, *query;
int count;

if (hitList == NULL)
    return;
if (hitList->next == NULL)
    {
    histogram[0] = 1;
    return;
    }
lastQuery = hitList->query;
count = 1;
for (hit = hitList->next; hit != NULL; hit = hit->next)
    {
    query = hit->query;
    if (query != lastQuery)
        {
        setHistogram(count);
        count = 0;
        }
    ++count;
    lastQuery = query;
    }
setHistogram(count);
}

void countSameQuery(struct hit *hitList, int *retCount, struct hit **retEnd)
/* Count up number of hits that have the same query as the first element. */
{
char *query = hitList->query;
struct hit *hit;
int count = 1;
for (hit = hitList->next; hit != NULL; hit = hit->next)
    {
    if (query != hit->query)
        break;
    ++count;
    }
*retCount = count;
*retEnd = hit;
}

void filterRepeats(struct hitHanger *hh, int maxRepeats, int minSize)
/* Filter out repeating elements. Assumes hh->hitList is sorted. */
{
struct hit *newList = NULL;
struct hit *hit, *endHit;
int sameCount;
int histAlloc = 0;
int *hist = NULL;

printf("Filtering repeats more than %d\n", maxRepeats);
for (hit = hh->hitList; hit != NULL; hit = endHit)
    {
    countSameQuery(hit, &sameCount, &endHit);
    if (sameCount <= maxRepeats)
        {
        struct hit *next;
        /* If below repeat threshold, just copy to new list. */
        for ( ; hit != endHit; hit = next)
            {
            next = hit->next;
            slAddHead(&newList, hit);
            }
        }
    else
        {
        /* Here I should really chop up the hits into repeating and
         * non-repeating parts. */
        int qSize = hit->qSize;
        int i;
        int rStart, rEnd;
        struct hit *h, *next;

        /* Make sure base by base histogram is large enough and that
         * it is zeroed out. */
        if (qSize >= histAlloc)
            {
            freeMem(hist);
            histAlloc = 2*qSize;
            hist = needMem(histAlloc * sizeof(hist[0]));
            }
        else
            {
            for (i=0; i<qSize; ++i)
                hist[i] = 0;
            }
        
        /* Make up base by base use histogram. */
        for (h = hit; h != endHit; h = h->next)
            {
            int qEnd = h->qEnd;
            for (i=h->qStart; i<qEnd; ++i)
                hist[i] += 1;
            }
        
        /* Go through and find start and end of repeating region.
         * This does assume there is only one repeating region per
         * sequence, which is probably good enough for our purposes. */
        for (rStart = 0; rStart < qSize; ++rStart)
            {
            if (hist[rStart] > maxRepeats)
                break;
            }
        for (rEnd = qSize-1; rEnd >= 0; --rEnd)
            {
            if (hist[rEnd] > maxRepeats)
                break;
            }
        rEnd += 1;      /* Convert to half open interval */
        if (rEnd <= rStart)
            {
            /* False alarm - though sequence as a whole
             * hits more than maxRepeats time, no single
             * part of it does. */
            for ( ; hit != endHit; hit = next)
                {
                next = hit->next;
                slAddHead(&newList, hit);
                }
            }
        else
            {
            /* Break up sequence into before and after repeat
             * interval. */
            for (h = hit; h != endHit; h = next)
                {
                int pieceCount = 0;
                boolean gotBefore = (rStart - h->qStart >= minSize);
                boolean gotAfter = (h->qEnd - rEnd >= minSize);
                next = h->next;
                if (!gotBefore  && !gotAfter)
                    {
                    /* Whole hit swallowed by repeat. Leave it off list */
                    }
                else if (gotBefore && gotAfter)
                    {
                    struct hit *after;
                    /* Have to break hit into two. Allocate new second half. */
                    AllocVar(after);
                    *after = *h;
                
                    h->qEnd = rStart;   /* Before hit reuses old hit. */
                    slAddHead(&newList, h);
                
                    after->qStart = rEnd;
                    slAddHead(&newList, after);

                    /* Note at this point the score and the target position
                     * will be out of date.  Oh well.... */
                    }
                else if (gotBefore)
                    {
                    h->qEnd = min(rStart, h->qEnd);
                    slAddHead(&newList, h);
                    }
                else if (gotAfter)
                    {
                    h->qStart = max(rEnd, h->qStart);
                    slAddHead(&newList, h);
                    }
                }
            }
        }
    }
slReverse(&newList);
hh->hitList = newList;
}

int main(int argc, char *argv[])
/* Process command line parameters and set up main loop. */
{
char *outName, *histName, **inNames;
int inCount;
double minMatch;
int minSize;
int maxRepeats;
char *s;
struct hitHanger *hh;

if (argc < 7)
    usage();

/* Get minMatch percentage. */
s = argv[3];
if (!isdigit(s[0]))
    usage();
minMatch = atof(s);

/* Get minSize */
s = argv[4];
if (!isdigit(s[0]))
    usage();
minSize = atoi(s);

/* Get max number of repeats. */
s = argv[5];
maxRepeats = atoi(s);
if (maxRepeats <= 0)
    usage();

/* Get in and out files. */
outName = argv[1];
histName = argv[2];
inNames = &argv[6];
inCount = argc-6;

hh = hangAllHits(inNames, inCount, minMatch, minSize);
makeHistogram(hh->hitList);
printHistogram(histName);
filterRepeats(hh, maxRepeats, minSize);
writeHits(hh, outName);
return 0;
}
