
/* idbQuery.c - This cgi script queries the Intronerator Data Base for
 * things that match what the user has set up in idbQuery.html. */
#include "common.h"
#include "portable.h"
#include "errabort.h"
#include "localmem.h"
#include "wormdna.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "gdf.h"
#include "cda.h"
#include "xa.h"

char **chromNames;
int chromCount;

/* Local memory */
struct lm *localMem;
#define lAlloc(size) (lmAlloc(localMem, (size) ))

struct liteFeature
/* A feature without many features - ha! 
 * Lists of this type tend to be short lived and in local memory. */
    {
    struct liteFeature *next;
    int start, end;
    char strand;
    };

struct idbFeature
/* A feature with more feature.  
 * lists of this type are longer lived and in global memory. */
    {
    struct idbFeature *next;
    int start, end;
    char strand;
    char *chrom;
    char *name;
    char *orfName;
    };

struct liteFeature *liteFreeList = NULL;  /* Free liteFeatures ready for reuse */

struct liteFeature *newLiteFeature(int start, int end, char strand)
/* Return a new lite feature. */
{
struct liteFeature *feat;
if (liteFreeList != NULL)
    {
    feat = liteFreeList;
    liteFreeList = liteFreeList->next;
    zeroBytes(feat, sizeof(*feat));
    }
else
    feat = lAlloc(sizeof(*feat));
feat->start = start;
feat->end = end;
feat->strand = strand;
return feat;
}

void freeLiteFeature(struct liteFeature *feat)
/* Free a single lite feature. */
{
slAddHead(&liteFreeList, feat);
}

void freeLiteFeatureList(struct liteFeature **pList)
/* Free lite features list (return to free list). */
{
struct liteFeature *feat, *next;
for (feat = *pList; feat != NULL; feat = next)
    {
    next = feat->next;
    slAddHead(&liteFreeList, feat);
    }
*pList = NULL;
}

int cmpLiteFeatures(const void *va, const void *vb)
/* Compare two lite features. */
{
struct liteFeature **pA = (struct liteFeature **)va;
struct liteFeature **pB = (struct liteFeature **)vb;
struct liteFeature *a = *pA, *b = *pB;
int diff;

if ((diff = a->strand - b->strand) != 0)
    return diff;
if ((diff = a->start - b->start) != 0)
    return diff;
return a->end - b->end;
}

struct idbFeature *newIdbFeature(char *name, char *chrom, int start, int end, char strand)
/* Return a new idbFeature. */
{
struct idbFeature *feat;
AllocVar(feat);
feat->name = cloneString(name);
feat->chrom = wormOfficialChromName(chrom);
feat->start = start;
feat->end = end;
feat->strand = strand;
return feat;
}

int cmpIdbFeatures(const void *va, const void *vb)
/* Compare two idb features. */
{
struct idbFeature **pA = (struct idbFeature **)va;
struct idbFeature **pB = (struct idbFeature **)vb;
struct idbFeature *a = *pA, *b = *pB;
int diff;

if ((diff = strcmp(a->chrom, b->chrom)) != 0)
    return diff;
if ((diff = a->start - b->start) != 0)
    return diff;
return a->end - b->end;
}

struct idbFeature *inIdbList(struct idbFeature *list, char *chrom, int start, int end, char strand)
/* Return feature with matching start and end, or NULL if no match. */
{
struct idbFeature *el;
for (el = list; el != NULL; el = el->next)
    if (el->start == start && el->end == end && el->strand == strand && sameString(chrom, el->chrom))
        return el;
return NULL;
}

void freeIdbFeature(struct idbFeature *feat)
/* Free a single feature. */
{
if (feat->orfName != feat->name)
    freeMem(feat->orfName);
freeMem(feat->name);
freeMem(feat);
}

void freeIdbFeatureList(struct idbFeature **pFeatList)
/* Free a whole features list. */
{
struct idbFeature *feat, *next;
for (feat = *pFeatList; feat != NULL; feat = next)
    {
    next = feat->next;
    freeIdbFeature(feat);
    }
*pFeatList = NULL;
}

enum sourceTypes
    {
    stRecursive, stGenome, stChromosomes, stCosmids, stOrfs,
    };

struct cgiChoice sourceChoices[] =
    {
        {"recursive", stRecursive,},
        {"genome", stGenome,},
        {"chromosomes", stChromosomes, },
        {"cosmids", stCosmids, },
        {"orfs", stOrfs,},
    };

enum regionTypes
    {
    rtAll, rtExons, rtIntrons, rtIntergenic, rtSkippedExon, rtSkippedIntron, 
    rtAlt3Intron, rtAlt5Intron,
    };

struct cgiChoice regionChoices[] =
    {
        {"", rtAll},
        {" ", rtAll},
        {"exons", rtExons},
        {"introns", rtIntrons},
        {"intergenic", rtIntergenic},
        {"skipped exon", rtSkippedExon},
        {"skipped intron", rtSkippedIntron},
        {"alt 5' intron", rtAlt5Intron},
        {"alt 3' intron", rtAlt3Intron},
        {"alt5", rtAlt5Intron},
        {"alt3", rtAlt3Intron},
    };


enum relativeToTypes
    {
    rttRegion, rttStart, rttEnd,
    };

struct cgiChoice relativeToChoices[] =
    {
        {"Region", rttRegion, },
        {"Start", rttStart, },
        {"End", rttEnd, },
    };

enum cbHomologyTypes
    {
    cbtDontCare, cbtAny, cbtHigh,
    };

enum logicTypes
    {
    logicPos, logicNeg,
    };

struct cgiChoice logicChoices[] =
    {
        {"", logicPos,},
        {" ", logicPos,},
        {"Not", logicNeg},
        {"Invert", logicNeg},
    };

struct cgiChoice cbHomologyChoices[] =
    {
        {" ", cbtDontCare, },
        {"", cbtDontCare, },
        {"Any", cbtAny, },
        {"High", cbtHigh, },
    };

enum cWhereTypes
    {
    cwAnywhere, cwMostly, cwThroughout, cwPiecemeal,
    };

struct cgiChoice cWhereChoices[] = 
    {
        {"Anywhere", cwAnywhere,},
        {"Mostly", cwMostly,},
        {"Throughout", cwThroughout,}, 
        {"Piecemeal", cwPiecemeal,},
    };

enum cdnaHitsTypes
    {
    cdtDontCare, cdtFullLength, cdtEst, cdtEither,
    };

struct cgiChoice cdnaHitsChoices[] = 
    {
        {" ", cdtDontCare, },
        {"", cdtDontCare, },
        {"Full Length", cdtFullLength, },
        {"FullLength", cdtFullLength, },
        {"EST", cdtEst, },
        {"Either", cdtEither, },
    };

enum cdnaStageTypes 
    {
    cstEither, cstMixed, cstEmbryo, cstMixedOnly, cstEmbryoOnly,
    };

struct cgiChoice cdnaStageChoices[] =
    {
        {"Either", cstEither, },
        {"Embryo", cstEmbryo, },
        {"Mixed", cstMixed, },
        {"Embryo Only", cstEmbryoOnly, },
        {"embryoOnly", cstEmbryoOnly, },
        {"Mixed Only", cstMixedOnly, },
        {"mixedOnly", cstMixedOnly, },
    };

enum gpTypes
/* Gene prediction types. */
    {
    gpDontCare, gpCds, gpCoding,
    };

struct cgiChoice gpChoices[] = 
    {
        {"", gpDontCare, },
        {" ", gpDontCare, },
        {"CDS", gpCds, },
        {"Coding", gpCoding, },
    };

enum wgpTypes
/* Which Gene prediction types. */
    {
    wgpAceDb, wgpGenie,
    };

struct cgiChoice wgpChoices[] =
    {
        {"AceDB", wgpAceDb, },
        {"Genie", wgpGenie, },
    };

enum outputTypes
    {
    otFasta, otNameOnly, otRecursive, otHyperlinked
    };

struct cgiChoice outputChoices[] =
    {
    {"Fasta", otFasta, }, 
    {"Name Only", otNameOnly, },
    {"nameOnly", otNameOnly, },
    {"Recursive", otRecursive, },
    {"Hyperlinked", otHyperlinked, },
    };

enum capTypes 
    {
    ctCoding, ctAll, cdNone,
    };

struct cgiChoice capChoices[] =
    {
    {"Coding", ctCoding,},
    {"All", ctAll, },
    {"None", cdNone,},
    };

boolean wildMatchAny(char *name, char *wildWords[], int wildCount, char usedFlags[])
/* Return true if name matches any of wildWords. */
{
int i;
for (i=0; i<wildCount; ++i)
    {
    if (wildMatch(wildWords[i], name))
        {
        usedFlags[i] = TRUE;
        return TRUE;
        }
    }
return FALSE;
}

struct idbFeature *getMatchingC2Names(char *c2FileName, char *wildWords[], int wildCount, char usedFlags[])
/* Get all names from c2g or c2c file that match words, which may have
 * wildcards. */
{
char pathName[512];
FILE *f;
char line[256];
int lineCount=0;
char *words[3];
int wordCount;
struct idbFeature *featList = NULL, *feat;
char *chrom;
int start, end;
char *name;

sprintf(pathName, "%s%s", wormFeaturesDir(), c2FileName);
f = mustOpen(pathName, "r");
while (fgets(line, sizeof(line), f))
    {
    ++lineCount;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;
    if (wordCount < 3)
        errAbort("Short line %d in %s\n", lineCount, pathName);
    name = words[2];
    if (wildMatchAny(name, wildWords, wildCount, usedFlags))
        {
        wormParseChromRange(words[0], &chrom, &start, &end);
        feat = newIdbFeature(name, chrom, start, end, words[1][0]);
        slAddHead(&featList, feat);
        }
    }
fclose(f);
return featList;
}

struct idbFeature *getCosmidNames(char *wildWords[], int wildCount, char usedFlags[])
/* Get all cosmid names that match words */
{
struct idbFeature *list;

list = getMatchingC2Names("c2c", wildWords, wildCount, usedFlags);
slReverse(&list);
return list;
}

struct idbFeature *getOrfNames(char *wildWords[], int wildCount, char usedFlags[])
/* Get all ORF or gene names that match words */
{
struct idbFeature *featList, *feat;
FILE *f;
char synFileName[512];
char line[256];
int lineCount=0;
char *words[5];
int wordCount;
char *geneName, *orfName;

/* Get ORF list and fill in orfName pointers. */
featList = getMatchingC2Names("c2g", wildWords, wildCount, usedFlags);
for (feat = featList; feat != NULL; feat = feat->next)
    feat->orfName = feat->name;

/* Add in genes from synonym list. */
sprintf(synFileName, "%s%s", wormFeaturesDir(), "syn");
f = mustOpen(synFileName, "r");
while (fgets(line, sizeof(line), f))
    {
    ++lineCount;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;
    if (wordCount < 2)
        errAbort("Short line %d of %s\n", lineCount, synFileName);
    geneName = words[0];
    if (wildMatchAny(geneName, wildWords, wildCount, usedFlags))
        {
        int start, end;
        char strand;
        char *chrom;

        orfName = words[1];
        if (!wormGeneRange(orfName, &chrom, &strand, &start, &end) )
            {
            errAbort("Can't find %s (which should be same as %s). Sorry, the database is messed up!",
                orfName, geneName);
            }
        feat = newIdbFeature(geneName, chrom, start, end, strand);
        feat->orfName = cloneString(orfName);
        slAddHead(&featList, feat);
        }
    }
fclose(f); 

slReverse(&featList);
return featList;
}

void explainRecursiveInput(int lineCount)
/* Explain how recursive input should look and exit. */
{
errAbort("The program can't cope with line %d of the input.\n"
         "Each line should be of form:\n"
         "   >name chromosome chrom:start-end strand\n"
         "Where 'name' is the name of the sequence; 'chromosome' is\n"
         "just the word 'chromosome'; 'chrom' is a worm chromosome, either\n"
         "'i', 'ii', 'iii', 'iv', 'v', 'x', or 'm'; 'start' is the starting\n"
         "position in the chromosome; 'end' is the ending position in the\n"
         "chromosome; and 'strand' is either '+' or '-'\n"
         "An example of this is:\n"
         "    >F36A2 chromosome i:8332665-8355411 +\n"
         "Normally you generate input by selecting 'recursive' for the\n"
         "output type, and then reusing the output as input for the next\n"
         "round.", lineCount);
}

struct idbFeature *getRecursiveInput(char *rawText)
/* Parse recursive data. */
{
struct idbFeature *idbList = NULL, *idb;
char *line, *nextLine;
int lineCount=0;
char *words[6];
int wordCount;

/* Break up input to a line at a time. */
for (line = rawText; line != NULL; line = nextLine)
    {
    nextLine = strchr(line, '\n');
    if (nextLine != NULL)
        *nextLine++ = 0;
    ++lineCount;

    /* Chop up lines, and process non-blank lines. */
    wordCount = chopLine(line, words);
    if (wordCount > 0)
        {
        int start, end;
        char *chrom;
        char strand;
        char *name;

        if (wordCount < 4 || !sameWord(words[1], "chromosome") || words[0][0] != '>')
            explainRecursiveInput(lineCount);
        name = words[0]+1;
        if (!wormParseChromRange(words[2], &chrom, &start, &end))
            explainRecursiveInput(lineCount);
        strand = words[3][0];
        if (strand != '+' && strand != '-')
            explainRecursiveInput(lineCount);
        idb = newIdbFeature(name, chrom, start, end, strand);
        slAddHead(&idbList, idb);
        }
    }
slReverse(&idbList);
return idbList;
}

struct idbFeature *getSequenceNames(int sourceType, char *rawNames)
/* Return a list of names from rawNames. */
{
int i;
struct idbFeature *list = NULL;
char *words[256];
char usedFlags[256];
int wordCount;

/* Full genome request is an easy special case.  Deal with it
 * here. */
if (sourceType == stGenome)
    {
    for (i=0; i<chromCount; ++i)
        {
        char *name = chromNames[i];
        slAddTail(&list, newIdbFeature(name, name, 0, wormChromSize(name), '+'));
        }
    return list;
    }
else if (sourceType == stRecursive)
    return getRecursiveInput(rawNames);

/* Everyone else will want the rawNames list chopped into words. */
wordCount = chopLine(rawNames, words);
if (wordCount < 1)
    errAbort("Please go back and enter some sequence names or select Full Genome.");
zeroBytes(usedFlags, wordCount);

if (sourceType == stChromosomes)
    {
    /* Deal with chromosome request. */
    for (i=0; i<chromCount; ++i)
        {
        char *name = chromNames[i];
        if (wildMatchAny(name, words, wordCount, usedFlags))
            slAddTail(&list, newIdbFeature(name, name, 0, wormChromSize(name), '+'));
        }
    }
else if (sourceType == stCosmids)
    {
    list = getCosmidNames(words, wordCount, usedFlags);
    }
else if (sourceType == stOrfs)
    {
    list = getOrfNames(words, wordCount, usedFlags);
    }
for (i=0; i<wordCount; ++i)
    {
    if (!usedFlags[i])
        warn("%s not found", words[i]);
    }
return list;
}
 
void printSequence(FILE *f, char *seq, int seqSize, 
    int lineSize, int wordSize, boolean numberLines, int startNumber)
/* Print sequence to file possibly with line breaks and word breaks and
 * numbers at the end of each line. Pass in zero for line size if you don't
 * want any line breaks; zero in wordSize for no word breaks. */
{
int wordRing, lineRing;
int i;

if (lineSize == 0)
    lineSize = seqSize+1;
if (wordSize == 0)
    wordSize = seqSize+1;
wordRing = wordSize;
lineRing = lineSize;

for (i=0; i<seqSize; ++i)
    {
    fputc(seq[i], f);
    if (--wordRing <= 0)
        {
        fputc(' ', f);
        wordRing = wordSize;
        }
    if (--lineRing <= 0)
        {
        if (numberLines)
            fprintf(f, " %d", startNumber+i+1);
        fputc('\n', f);
        lineRing = lineSize;
        }
    }
if (lineRing != lineSize)
    {
    if (numberLines)
        fprintf(f, " %d", startNumber+i);
    fputc('\n', f);
    }
}

void findGdf(char **retGdfDir, struct wormGdfCache **retCache)
/* Go figure out which gene-finder's predictions to use. */
{
int wgpType = cgiOneChoice("gpWhich", wgpChoices, ArraySize(wgpChoices));

if (wgpType == wgpAceDb)
    {
    *retGdfDir = wormSangerDir();
    *retCache = &wormSangerGdfCache;
    }
else
    {
    *retGdfDir = wormGenieDir();
    *retCache = &wormGenieGdfCache;
    }
}

void outputList(struct idbFeature *featList, int outputType,
    int lineSize, int wordSize,
    boolean numberLines, int capType)
/* Write out features list. */
{
struct idbFeature *feat;
DNA *dna;
int featSize;

if (featList == NULL)
    errAbort("Sorry, no matching features found.");
for (feat = featList; feat != NULL; feat = feat->next)
    {
    featSize = feat->end - feat->start;
    if (featSize > 0)
        {
        if (outputType == otNameOnly)
            printf("%s\n", feat->name);
        else if (outputType == otHyperlinked)
            {
            printf("<A HREF=\"../cgi-bin/tracks.exe?where=%s:%d-%d\">%s</A>\n",
                feat->chrom, feat->start, feat->end, feat->name);
            }
        else
            printf(">%s chromosome %s:%d-%d %c\n", 
                feat->name, feat->chrom, feat->start, feat->end, feat->strand);
        if (outputType == otFasta)
            {
            dna = wormChromPart(feat->chrom, feat->start, featSize);
            if (capType == ctAll)
                toUpperN(dna, featSize);
            else if (capType == ctCoding)
                {
                char *gdfDir;
                struct wormGdfCache *gdfCache;
                struct wormFeature *geneList, *gene;

                findGdf(&gdfDir, &gdfCache);
                geneList = wormSomeGenesInRange(feat->chrom, feat->start, feat->end, gdfDir);
                for (gene = geneList; gene != NULL; gene = gene->next)
                    {
                    char *name = gene->name;
                    if (!wormIsNamelessCluster(name))
                        {
                        struct gdfGene *gdf = wormGetSomeGdfGene(name, gdfCache);
                        gdfUpcExons(gdf, gene->start, dna, featSize, feat->start);
                        gdfFreeGene(gdf);
                        }
                    }
                slFreeList(&geneList);
                }
            if (feat->strand == '-')
                reverseComplement(dna, featSize);
            printSequence(stdout, dna, featSize, lineSize, wordSize, numberLines, 0);
            freez(&dna);
            }
        }
    }
}


struct idbFeature *breakIntoIntronsOrExons(struct idbFeature *oldList, boolean doIntrons)
/* Create a new features list containing exons from old features list. */
{
struct idbFeature *newList = NULL, *oFeat, *nFeat;
char nameBuf[128];
char *gdfDir;
struct wormGdfCache *gdfCache;
struct wormFeature *geneList, *gene;
struct gdfGene *gdf;
int i;
int startIx, endIx;
char *ieName = (doIntrons ? "intron" : "exon");
int ieCount;
char *geneName;

findGdf(&gdfDir, &gdfCache);
for (oFeat = oldList; oFeat != NULL; oFeat = oFeat->next)
    {
    geneList = wormSomeGenesInRange(oFeat->chrom, oFeat->start, oFeat->end, gdfDir);
    for (gene = geneList; gene != NULL; gene = gene->next)
        {
        if (oFeat->orfName && differentString(oFeat->orfName, gene->name) )
            continue;
        if (wormIsNamelessCluster(gene->name))
            continue;
        gdf = wormGetSomeGdfGene(gene->name, gdfCache);
        if (oFeat->orfName)
            geneName = oFeat->name;
        else
            geneName = gdf->name;
        startIx = 0;
        endIx = gdf->dataCount-2;
        if (doIntrons)
            {
            startIx += 1;
            endIx -= 1;
            }
        if (gdf->strand == '+')
            {
            for (i=startIx, ieCount = 1; i<=endIx; i+=2, ieCount+=1)
                {
                sprintf(nameBuf, "%s_%s_%d", geneName, ieName, ieCount);
                nFeat = newIdbFeature(nameBuf, oFeat->chrom, 
                    gdf->dataPoints[i].start, gdf->dataPoints[i+1].start, gdf->strand);
                slAddHead(&newList, nFeat);
                }            
            }
        else
            {
            for (i=endIx, ieCount = 1; i>=startIx; i-=2, ieCount+=1)
                {
                sprintf(nameBuf, "%s_%s_%d", geneName, ieName, ieCount);
                nFeat = newIdbFeature(nameBuf, oFeat->chrom, 
                    gdf->dataPoints[i].start, gdf->dataPoints[i+1].start, gdf->strand);
                slAddHead(&newList, nFeat);
                }            
            }
        gdfFreeGene(gdf);
        }
    slFreeList(&geneList);
    }
slReverse(&newList);
return newList;
}

struct idbFeature *igFeature(char *chrom, int start, int end)
/* Make up a new feature for an intergenic region. */
{
char nameBuf[128];
assert(start < end);
assert(start >= 0 && end >= 0);
sprintf(nameBuf, "IG_%s_%d_%d", chrom, start, end);
return newIdbFeature(nameBuf, chrom, start, end, '+');
}


struct idbFeature *intergenicRegions(struct idbFeature *oldList, int sourceType)
/* Get intergenic regions of features. */
{
struct idbFeature *newList = NULL, *oFeat, *nFeat;
struct wormFeature *geneList, *gene;
char *gdfDir;
struct wormGdfCache *gdfCache;
int geneEnd;
char *chrom;

findGdf(&gdfDir, &gdfCache);
if (sourceType == stOrfs)
    warn("Usually it's a mistake to select intergenic regions on ORF features.");

for (oFeat = oldList; oFeat != NULL; oFeat = oFeat->next)
    {
    chrom = oFeat->chrom;
    geneList = wormSomeGenesInRange(oFeat->chrom, oFeat->start, oFeat->end, gdfDir);
    if ((gene = geneList) == NULL)
        {
        /* Entire region is intragenic */
        nFeat = igFeature(chrom, oFeat->start, oFeat->end);
        slAddHead(&newList, nFeat);
        }
    else
        {
        /* Do region before first gene. */
        geneEnd = oFeat->start;
        if (oFeat->start < gene->start)
            {
            nFeat = igFeature(chrom, oFeat->start, gene->start);
            slAddHead(&newList, nFeat);
            geneEnd = gene->end;
            gene = gene->next;
            }
        /* Do regions between genes. */
        for (; gene != NULL; gene = gene->next)
            {
            if (gene->start > geneEnd)
                {
                nFeat = igFeature(chrom, geneEnd, gene->start);
                slAddHead(&newList, nFeat);
                }
            geneEnd = max(geneEnd, gene->end);
            }
        /* Do region after last gene. */
        if (geneEnd < oFeat->end)
            {
            nFeat = igFeature(chrom, geneEnd, oFeat->end);
            slAddHead(&newList, nFeat);
            }        
        slFreeList(&geneList);
        }
    }
slReverse(&newList);
return newList;
}

enum intronConsts {minIntronSize = 24, minGoodEnd = 5, minExonSize = 12};

boolean isIntronBetween(struct cdaBlock *block, struct cdaBlock *nextBlock)
/* Return TRUE if gap between block and nextBlock in n is an intron. */
{
int nGap, hGap, thisSize, nextSize;

nGap = nextBlock->nStart - block->nEnd;
hGap = nextBlock->hStart - block->hEnd;
thisSize = block->nEnd - block->nStart;
nextSize = nextBlock->nEnd - nextBlock->nStart;
return (nGap == 0 && hGap >= minIntronSize && thisSize >= minExonSize && nextSize >= minExonSize
    && block->endGood > minGoodEnd && nextBlock->startGood > minGoodEnd);
}

boolean hasIntron(struct cdaAli *cda)
/* Returns TRUE if the cda has an intron. */
{
struct cdaBlock *b = cda->blocks;
int blockCount = cda->blockCount;
int i;

for (i=1; i<blockCount; ++i)
    {
    if (isIntronBetween(b, b+1))
        return TRUE;
    ++b;
    }
return FALSE;
}
   
struct liteFeature *liteSkipIrrelevant(struct liteFeature *listSeg, struct liteFeature *feat,
    int extraSpace)
/* Skip parts of listSeg that couldn't matter to feat. Assumes
 * listSeg is sorted on start. Returns first possibly relevant
 * item of listSeg. */
{
struct liteFeature *seg = listSeg;
char strand = feat->strand;
int start = feat->start - extraSpace;
int skipCount = 0;

/*Skip past wrong strand. */
while (seg != NULL && seg->strand  < strand)
    {  
    seg = seg->next;
    ++skipCount;
    }
/* Skip past stuff that we've passed on this chromosome. */
while (seg != NULL && seg->strand == strand && seg->end < start)
    {
    seg = seg->next;
    ++skipCount;
    }
return seg;
}

struct liteFeature *liteNextOverlap(struct liteFeature *listSeg, struct liteFeature *liteFeature, int extra)
/* Assuming that listSeg is sorted on start, search through list
 * until find something that overlaps with liteFeature, or have gone
 * past where liteFeature could be. Returns overlapping liteFeature or NULL. */
{
struct liteFeature *seg = listSeg;
char strand = liteFeature->strand;
int start = liteFeature->start;
int end = liteFeature->end;

while (seg != NULL && seg->strand == strand && seg->start <= end + extra)
    {
    if (!(seg->start - extra >= end || seg->end + extra <= start))
        return seg;
    seg = seg->next;
    }
return NULL;
}

void dumpLiteList(char *name, struct liteFeature *list)
/* Print out lite list for debugging. */
{
struct liteFeature *el;
printf("%s\n", name);
for (el = list; el != NULL; el = el->next)
    printf("%d-%d %c\n", el->start, el->end, el->strand);
}

struct idbFeature *filterLite(struct idbFeature *filteredList, struct liteFeature *aList, struct liteFeature *bList,
    char *featName, char *featChrom, char *typeName, 
    boolean (*filter)(struct liteFeature *a, struct liteFeature *b))
/* Return subset of a that overlaps with b, and also passes through filter. */
{
struct liteFeature *relevantB = bList;
struct liteFeature *a, *b;

for (a = aList; a != NULL; a = a->next)
    {
    relevantB = liteSkipIrrelevant(relevantB, a, 0);
    for (b = relevantB; (b = liteNextOverlap(b, a, 0)) != NULL; b = b->next)
        {
        if (filter(a,b))
            {
            char nameBuf[128];
            struct idbFeature *idbFeat;
            int start = a->start, end = a->end;

            sprintf(nameBuf, "%s_%s_%s_%d_%d", featName, typeName, featChrom, start, end);
            idbFeat = newIdbFeature(nameBuf, featChrom, start, end, a->strand);
            slAddHead(&filteredList, idbFeat);
            }                
        }
    }
return filteredList;
}

boolean encompassedFilter(struct liteFeature *a, struct liteFeature *b)
/* Return TRUE if a is encompassed by b */
{
return (b->start <= a->start && a->end <= b->end);
}

struct idbFeature *skippedExons(struct idbFeature *featList, int sourceType)
/* Get all the exons that are sometimes spliced in and sometimes not. */
{
struct idbFeature *feat;
struct idbFeature *skippedExonList = NULL;

for (feat = featList; feat != NULL; feat = feat->next)
    {
    struct cdaAli *cdaList, *cda;
    struct liteFeature *exList = NULL, *ex;
    struct liteFeature *inList = NULL, *in;

    cdaList = wormCdaAlisInRange(feat->chrom, feat->start, feat->end);
    cdaCoalesceFast(cdaList);
    /* Build up list of exons with known bounds (introns on either side) 
     * and list of introns. */
    for (cda = cdaList; cda != NULL; cda = cda->next)
        {
        int i;
        char cloneStrand = cdaCloneStrand(cda);
        if (sourceType != stOrfs || cloneStrand == feat->strand)
            {
            struct cdaBlock *blocks;
            int endBlock;

            endBlock = cda->blockCount-1;
            blocks = cda->blocks;
            /* Add to exon list. */
            for (i=1; i<endBlock; ++i)
                {
                struct cdaBlock *b = blocks+i;
                if (isIntronBetween(b-1, b) && isIntronBetween(b, b+1))
                    {
                    ex = newLiteFeature(b->hStart, b->hEnd, cloneStrand);
                    slAddHead(&exList, ex);
                    }
                }
            /* Add to intron list. */
            for (i=0; i<endBlock; ++i)
                {
                struct cdaBlock *b = blocks+i;
                struct cdaBlock *nb = b+1;
                if (isIntronBetween(b, nb))
                    {
                    in = newLiteFeature(b->hEnd, nb->hStart, cloneStrand);
                    slAddHead(&inList, in);
                    }
                }
            }
        } 
    slUniqify(&exList, cmpLiteFeatures, freeLiteFeature);
    slUniqify(&inList, cmpLiteFeatures, freeLiteFeature);
        
    /* Go through and check for introns that cover exons completely.
     * Any such exons are occassionally skipped */
    skippedExonList = filterLite(skippedExonList, exList, inList, feat->name, feat->chrom, 
        "skip_ex", encompassedFilter);
    
    freeLiteFeatureList(&exList);
    freeLiteFeatureList(&inList);
    cdaFreeAliList(&cdaList);
    }
slUniqify(&skippedExonList, cmpIdbFeatures, freeIdbFeature);
return skippedExonList;
}

struct idbFeature *skippedIntrons(struct idbFeature *featList, int sourceType)
/* Get all the exons that are sometimes spliced in and sometimes not. */
{
struct idbFeature *feat;
struct idbFeature *skippedIntronList = NULL;

for (feat = featList; feat != NULL; feat = feat->next)
    {
    struct cdaAli *cdaList, *cda;
    struct liteFeature *inList = NULL, *in;
    struct liteFeature *exList = NULL, *ex;

    cdaList = wormCdaAlisInRange(feat->chrom, feat->start, feat->end);
    cdaCoalesceFast(cdaList);
    /* Build up list of introns and exons.  */
    for (cda = cdaList; cda != NULL; cda = cda->next)
        {
        int i;
        char cloneStrand = cdaCloneStrand(cda);
        if (sourceType != stOrfs || cloneStrand == feat->strand)
            {
            struct cdaBlock *blocks;
            int endBlock;
            boolean nonGenomic = FALSE;

            /* Build up intron list. */
            endBlock = cda->blockCount-1;
            blocks = cda->blocks;
            for (i=0; i<endBlock; ++i)
                {
                struct cdaBlock *b = blocks+i;
                struct cdaBlock *nb = b+1;
                if (isIntronBetween(b, nb))
                    {
                    int start = b->hEnd, end = nb->hStart;
                    in = newLiteFeature(start, end, cloneStrand);
                    slAddHead(&inList, in);
                    nonGenomic = TRUE;
                    }
                }
            /* Build up exon list too. Only include cDNAs that have
             * an intron to avoid genomic contamination. */
            if (nonGenomic)
                {
                for (i=0; i<=endBlock; ++i)
                    {
                    struct cdaBlock *b = blocks+i;
                    ex = newLiteFeature(b->hStart, b->hEnd, cloneStrand);
                    slAddHead(&exList, ex);
                    }
                }
            }
        }    
    
    slUniqify(&inList, cmpLiteFeatures, freeLiteFeature);
    slUniqify(&exList, cmpLiteFeatures, freeLiteFeature);

    /* Go through and check for exons that cover introns completely.
     * Any such introns are occassionally skipped */
    skippedIntronList = filterLite(skippedIntronList, inList, exList, feat->name, feat->chrom, 
        "skip_in", encompassedFilter);
    
    freeLiteFeatureList(&inList);
    freeLiteFeatureList(&exList);
    cdaFreeAliList(&cdaList);
    }
slUniqify(&skippedIntronList, cmpIdbFeatures, freeIdbFeature);
return skippedIntronList;
}

boolean alt3Filter(struct liteFeature *a, struct liteFeature *b)
/* Return TRUE if 5' end is same but 3' end is different. */
{
if (a->strand == '+')
    return (b->start == a->start && intAbs(a->end - b->end) >= 3);
else
    return (b->end == a->end && intAbs(a->start - b->start) >= 3);
}

boolean alt5Filter(struct liteFeature *a, struct liteFeature *b)
/* Return TRUE if 3' end is same but 5' end is different. */
{
if (a->strand == '+')
    return (b->end == a->end && intAbs(a->start - b->start) >= 3);
else
    return (b->start == a->start && intAbs(a->end - b->end) >= 3);
}

struct idbFeature *altEnd(struct idbFeature *featList, int sourceType,
     boolean (*filter)(struct liteFeature *a, struct liteFeature *b), char *typeName)
/* Return introns that are alt3 or alt5 depending on filter passed in. */
{
struct idbFeature *feat;
struct idbFeature *altIntronList = NULL;

for (feat = featList; feat != NULL; feat = feat->next)
    {
    struct cdaAli *cdaList, *cda;
    struct liteFeature *inList = NULL, *in;

    cdaList = wormCdaAlisInRange(feat->chrom, feat->start, feat->end);
    cdaCoalesceFast(cdaList);

    /* Build up list of introns   */
    for (cda = cdaList; cda != NULL; cda = cda->next)
        {
        int i;
        char cloneStrand = cdaCloneStrand(cda);
        if (sourceType != stOrfs || cloneStrand == feat->strand)
            {
            struct cdaBlock *blocks;
            int endBlock;

            /* Build up intron list. */
            endBlock = cda->blockCount-1;
            blocks = cda->blocks;
            for (i=0; i<endBlock; ++i)
                {
                struct cdaBlock *b = blocks+i;
                struct cdaBlock *nb = b+1;
                if (isIntronBetween(b, nb))
                    {
                    int start = b->hEnd, end = nb->hStart;
                    in = newLiteFeature(start, end, cloneStrand);
                    slAddHead(&inList, in);
                    }
                }
            }
        }    
    slUniqify(&inList, cmpLiteFeatures, freeLiteFeature);

    altIntronList = filterLite(altIntronList, inList, inList, feat->name, feat->chrom, 
        typeName, filter);
    
    freeLiteFeatureList(&inList);
    cdaFreeAliList(&cdaList);
    }
slUniqify(&altIntronList, cmpIdbFeatures, freeIdbFeature);
return altIntronList;
}


struct idbFeature *filterRegions(struct idbFeature *featList, int featType, int sourceType)
/* Do region processing - which can be pretty wild. */
{
/* If they want it all just return current list. */
if (featType == rtAll)
    return featList;
switch (featType)
    {
    case rtAll:
        return featList;
    case rtExons:
        return breakIntoIntronsOrExons(featList, FALSE);
    case rtIntrons:
        return breakIntoIntronsOrExons(featList, TRUE);
    case rtIntergenic:
        return intergenicRegions(featList, sourceType);
    case rtSkippedExon:
        return skippedExons(featList, sourceType);
    case rtSkippedIntron:
        return skippedIntrons(featList, sourceType);
    case rtAlt3Intron:
        return altEnd(featList, sourceType, alt3Filter, "alt3");
    case rtAlt5Intron:
        return altEnd(featList, sourceType, alt5Filter, "alt5");
    default:
        errAbort("Sorry, that region type isn't implemented yet");
    }
}

struct idbFeature *offsetFeatures(struct idbFeature *oldList, int type, int start, int end)
/* Add offsets to features list. (Removes any empties it creates.) */
{
struct idbFeature *feat, *next;
struct idbFeature *newList = NULL;
if (start == 0 && end == 0)
    return oldList;
for (feat = oldList; feat != NULL; feat = next)
    {
    next = feat->next;
    if (feat->strand == '+')
        {
        switch(type)
            {
            case rttRegion:
                feat->start += start;
                feat->end += end;
                break;
            case rttStart:
                feat->end = feat->start + end;
                feat->start = feat->start + start;
                break;
            case rttEnd:
                feat->start = feat->end + start;
                feat->end = feat->end + end;
                break;
            }    
        }
    else
        {
        int fStart = feat->start;
        int fEnd = feat->end;
        int fSize;
        switch(type)
            {
            case rttRegion:
                feat->start -= end;
                feat->end -= start;                
                break;
            case rttStart:
                fSize = end - start;
                feat->start = fEnd - end;
                feat->end = feat->start + fSize;
                break;
            case rttEnd:
                fSize = end - start;
                feat->start = fStart - end;
                feat->end = feat->start + fSize;
                break;
            }    
        }
    if (feat->start < feat->end)
        {
        slAddHead(&newList, feat);
        }
    else
        {
        freeIdbFeature(feat);
        }
    }
slReverse(newList);
return newList;
}

boolean cdaIsEst(struct cdaAli *cda)
/* Returns TRUE if it's an EST type name ending in .3 or .5 */
{
char *s;
if ((s = strrchr(cda->name,'.')) == NULL)
    return FALSE;
return (s[1] == '3' || s[1] == '5');
}

void  setByCda(char *hits, int featStart, int featEnd, struct cdaAli *cda, boolean isIntron)
/* Set parts of hits corresponding to exons or introns depending if startOffset is 0 or 1. */
{
struct cdaBlock *blocks = cda->blocks;
int blockCount = cda->blockCount;
int i;

if (isIntron)
    blockCount -= 1;
for (i = 0; i<blockCount; ++i)
    {
    int bStart, bEnd;
    int start, end, size;
    struct cdaBlock *b = blocks+i;
    if (isIntron)
        {
        struct cdaBlock *nb = b+1;
        bStart = b->hEnd;
        bEnd = nb->hStart;
        }
    else
        {
        bStart = b->hStart;
        bEnd = b->hEnd;
        }
    start = max(featStart, bStart);
    end = min(featEnd, bEnd);
    size = end - start;
    if (size > 0)
        memset(hits + start - featStart, 0xff, size);
    }
}

struct liteFeature *makeLiteRuns(char *hits, int featStart, int featEnd)
/* Convert byte-by-byte hit discription to a liteList - where the runs that
 * are set will be on the list, and the runs of zeros between elements of 
 * the lite list. */
{
struct liteFeature *liteList = NULL, *lite;
int size = featEnd - featStart + 1;     /* We added a zero tag to hits. */
char lastHit = 0, hit;
int lastStart = 0;
int i;
for (i=0; i<size; ++i)
    {
    hit = hits[i];
    if (hit && !lastHit)
        lastStart = i;
    else if (!hit && lastHit)
        {
        lite = newLiteFeature(lastStart+featStart, i+featStart, '+');
        slAddHead(&liteList, lite);
        }
    lastHit = hit;
    }
slReverse(&liteList);
return liteList;
}

double hitRatio(char *hits, int size)
/* Return % of hits array that's set. */
{
int setCount = 0;
int i;

if (size <= 0)
    return 0.0;
for (i=0; i<size; ++i)
    {
    if (hits[i]) ++setCount;
    }
return (double)setCount/size;    
}

void addLiteIntersection(struct idbFeature **pIdbList, struct liteFeature *intersectList, 
    struct idbFeature *feature, char *nameBase)
/* Add intersections of intersectList with feature to IdbList */
{
char nameBuf[512];
int partCount = 0;
int start, end, size;
int fStart = feature->start;
int fEnd = feature->end;
struct liteFeature *intersect;

if (feature->strand == '-')
    slReverse(&intersectList);
for (intersect = intersectList; intersect != NULL; intersect = intersect->next)
    {
    start = max(fStart, intersect->start);
    end = min(fEnd, intersect->end);
    size = end - start;
    if (size > 0)
        {
        struct idbFeature *idb;
        sprintf(nameBuf, "%s_%s_%d", feature->name, nameBase, ++partCount);
        idb = newIdbFeature(nameBuf, feature->chrom, start, end, feature->strand);
        slAddHead(pIdbList, idb);
        }
    }
if (feature->strand == '-')
    slReverse(&intersectList);
}

void invertHits(char *hits, int size)
/* Invert logic of hit array. */
{
char *endHits = hits+size;
for ( ; hits < endHits; ++hits)
    {
    if (*hits)
        *hits = 0;
    else
        *hits = (char)0xFF;
    }
}

void addAnywhere(int whereType, char *hits, int logicType, struct idbFeature *feat, char *nameBase, struct idbFeature **pNewList)
/* Add in sections defined by set parts of hit list. */
{
int featSize = feat->end - feat->start;
if (logicType == logicNeg)
    invertHits(hits, featSize);
switch (whereType)
    {
    case cwAnywhere:
        if (hitRatio(hits, featSize) >= 0.00001)
            {
            slAddHead(pNewList, feat);
            }
        break;
    case cwMostly:
        if (hitRatio(hits, featSize) >= 0.50001)
            {
            slAddHead(pNewList, feat);
            }
        break;
    case cwThroughout:
        if (hitRatio(hits, featSize) >= 0.99999)
            {
            slAddHead(pNewList, feat);
            }
        break;
    case cwPiecemeal: /* break it up! */
        {
        struct liteFeature *liteList = makeLiteRuns(hits, feat->start, feat->end);
        addLiteIntersection(pNewList, liteList, feat, nameBase);
        freeLiteFeatureList(&liteList);
        }
    }
}

struct idbFeature *restrictOneCdnaType(struct idbFeature *oldList, 
    int cdnaHitType, int cdnaStageType, int cdnaWhereType, int logicType, int regionType)
/* Restrict features list to areas that have cDNA coverage. */
{
struct idbFeature *newList = NULL, *feat, *next;

for (feat = oldList; feat != NULL; feat = next)
    {
    struct cdaAli *cdaList;
    struct cdaAli *cda;
    int featStart = feat->start;
    int featEnd = feat->end;
    int featSize = featEnd - featStart;
    char *hits = needLargeMem(featSize+1); /* Extra zero at end will simplify things */

    memset(hits, 0, featSize+1);    
    next = feat->next;
    cdaList = wormCdaAlisInRange(feat->chrom, feat->start, feat->end);
    cdaCoalesceFast(cdaList);
    for (cda = cdaList; cda != NULL; cda = cda->next)
        {
        boolean isEmbryonic = cda->isEmbryonic;
        boolean isEst = cdaIsEst(cda);
        if ((cdnaStageType == cstEither || (cdnaStageType == cstMixed && !isEmbryonic) || (cdnaStageType == cstEmbryo && isEmbryonic))
         && (cdnaHitType == cdtEither || (cdnaHitType == cdtEst && isEst) || (cdnaHitType == cdtFullLength && !isEst) ) )
            {
            /* Here there's a relevant cDNA.  What we do with it depends on region type.
             * if looking at introns or exons go into details of cdna alignment.  Other-
             * wise just get whole thing. */
            switch (regionType)
                {
                case rtAll:
                case rtIntergenic:
                    {
                    int start, end, size;
                    start = max(featStart, cda->chromStart);
                    end = min(featEnd, cda->chromEnd);
                    size = end - start;
                    if (size > 0)
                        memset(hits + start - featStart, 0xff, size);
                    break;
                    }
                case rtExons:
                case rtSkippedExon:
                    {
                    setByCda(hits, featStart, featEnd, cda, FALSE);
                    break;
                    }
                case rtIntrons:
                case rtSkippedIntron:
                case rtAlt3Intron:
                case rtAlt5Intron:
                    {
                    setByCda(hits, featStart, featEnd, cda, TRUE);
                    break;
                    }
                default:
                    errAbort("Unknown region type %d\n", regionType);
                    break;
                }
            }
        }
    addAnywhere(cdnaWhereType, hits, logicType, feat, "cdna", &newList);
    freeMem(hits);
    cdaFreeAliList(&cdaList);
    }
slReverse(&newList);
return newList;
}

struct idbFeature *restrictCdnaDiff(struct idbFeature *oldList, 
    int cdnaHitType, int cdnaStageType, int cdnaWhereType, int logicType, int regionType)
/* Restrict features list to areas that have cDNA coverage in one stage but not another. */
{
struct idbFeature *newList = NULL, *feat, *next;

if (logicType == logicNeg)
    errAbort("Sorry, the Embryo Only and Mixed Only cDNA options don't work under inverted logic.");
if (cdnaWhereType != cwAnywhere)
    errAbort("Sorry, the Embryo Only and Mixed Only cDNA options only works 'Anywhere', "
             " not 'Mostly,' or 'Throughout' or 'Piecemeal'");
for (feat = oldList; feat != NULL; feat = next)
    {
    struct cdaAli *cdaList;
    struct cdaAli *cda;
    boolean gotAny = FALSE;
    boolean gotMixed = FALSE, gotEmbryo = FALSE, gotEst = FALSE, gotFullLength = FALSE;
    
    next = feat->next;
    cdaList = wormCdaAlisInRange(feat->chrom, feat->start, feat->end);

    for (cda = cdaList; cda != NULL; cda = cda->next)
        {
        gotAny = TRUE;
        if (cda->isEmbryonic)
            gotEmbryo = TRUE;
        else
            gotMixed = TRUE;
        if (cdaIsEst(cda))
            gotEst = TRUE;
        else
            gotFullLength = TRUE;
        }
    /* See if stage is appropriate. */
    if (((cdnaStageType == cstEither && gotAny) || 
         (cdnaStageType == cstMixedOnly && gotMixed && !gotEmbryo) ||
         (cdnaStageType == cstEmbryoOnly && gotEmbryo && !gotMixed) )
     && ((cdnaHitType == cdtEither && gotAny) ||
         (cdnaHitType == cdtEst && gotEst) ||
         (cdnaHitType == cdtFullLength && gotFullLength)))
        {
        slAddHead(&newList, feat);
        }
    else
        {
        freeIdbFeature(feat);
        }  
    cdaFreeAliList(&cdaList);
    }
slReverse(&newList);
return newList;
}

struct idbFeature *restrictToCdna(struct idbFeature *oldList, 
    int cdnaHitType, int cdnaStageType, int cdnaWhereType, int logicType, int regionType)
/* Restrict features list to areas that have cDNA coverage or complex/partial sort. */
{
if (cdnaHitType == cdtDontCare)
    return oldList;
if (cdnaStageType == cstEmbryoOnly || cdnaStageType == cstMixedOnly)
    return restrictCdnaDiff(oldList, cdnaHitType, cdnaStageType, cdnaWhereType, logicType, regionType);
else 
    return restrictOneCdnaType(oldList, cdnaHitType, cdnaStageType, cdnaWhereType, logicType, regionType);
}

struct liteFeature *liteXaList(struct xaAli *xaList, int featStart, int featEnd, int cbHomologyType)
/* Return a lite features list corresponding the the parts of xaList of
 * the right homology. */
{
struct liteFeature *liteList = NULL, *lite;
struct xaAli *xa;
char *hSym;
char *tSym;
int symCount;
int startNtIx;
int curNtIx;
int i;
char nt;
char ok, lastOk;
char okTable[256];

/* Here's a little state machine that puts something on the lite
 * list every time the hidden symbol goes from an ok one to a
 * not-ok one.  It's slightly complicated by having to keep track
 * of where in the target (C. elegans) sequence we are in spite
 * of insert characters. */
zeroBytes(okTable, sizeof(okTable));
okTable['1'] = okTable['2'] = okTable['3'] = okTable['H'] = TRUE;
if (cbHomologyType == cbtAny)
    okTable['L'] = TRUE;
for (xa = xaList; xa != NULL; xa = xa->next)
    {
    hSym = xa->hSym;
    tSym = xa->tSym;
    curNtIx = xa->tStart;
    symCount = xa->symCount;
    lastOk = FALSE;
    for (i=0; i<=symCount; ++i)  /* We depend on the zero tag! */
        {
        nt = tSym[i];
        if (nt != '-')
            {
            ok = okTable[hSym[i]];
            if (ok && !lastOk)
                {
                startNtIx = curNtIx;
                }
            else if (!ok && lastOk)
                {
                int start = max(startNtIx, featStart);
                int end = min(curNtIx, featEnd);
                if (start < end)
                    {
                    lite = newLiteFeature(startNtIx, curNtIx, '+');
                    slAddHead(&liteList, lite);
                    }
                }
            ++curNtIx;
            lastOk = ok;
            }
        }
    }
slUniqify(&liteList, cmpLiteFeatures, freeLiteFeature);
return liteList;
}

struct idbFeature *filterCbHomology(struct idbFeature *oldList, int cbHomologyType, int logicType, int cbWhereType)
/* Filter out all but selected level of homology. */
{
struct idbFeature *newList = NULL, *feat, *next;
struct xaAli *xaList;
struct liteFeature *xfList, *xf;
char *xDir;
char fileName[512];
FILE *data;
FILE **indexes;
FILE *index;
char *cb = "cbriggsae";
int chromIx;
int i;

/* If user doesn't care just return old list. */
if (cbHomologyType == cbtDontCare)
    return oldList;

indexes = needMem(chromCount * sizeof(indexes));
xDir = wormXenoDir();
sprintf(fileName, "%s%s/all%s", xDir, cb, xaAlignSuffix());
data = xaOpenVerify(fileName);

for (feat = oldList; feat != NULL; feat = next)
    {
    int featStart = feat->start;
    int featEnd = feat->end;
    int featSize = featEnd - featStart;
    char *hits = needLargeMem(featSize+1);
    memset(hits, 0, featSize+1);

    next = feat->next;
    chromIx = wormChromIx(feat->chrom);
    if ((index = indexes[chromIx]) == NULL)
        {
        sprintf(fileName, "%s%s/%s%s", xDir, cb, feat->chrom, xaChromIxSuffix());
        indexes[chromIx] = index = xaIxOpenVerify(fileName);
        }
    xaList = xaRdRange(index, data, featStart, featEnd, FALSE);
    xfList = liteXaList(xaList, featStart, featEnd, cbHomologyType);    
    for (xf = xfList; xf != NULL; xf = xf->next)
        {
        int start = max(xf->start, featStart);
        int end = min(xf->end, featEnd);
        int size = end-start;
        if (size > 0)
            memset(hits + start-featStart, 0xff, size);
        }
    addAnywhere(cbWhereType, hits, logicType, feat, "hom", &newList);
    freeLiteFeatureList(&xfList);
    xaAliFreeList(&xaList);
    freeMem(hits);
    }
slReverse(&newList);

/* Clean up time. */
fclose(data);
for (i=0; i<chromCount; ++i)
    carefulClose(&indexes[i]);
freeMem(indexes);

return newList;
}

struct idbFeature *filterGenePredictions(struct idbFeature *oldList, int gpType, int logicType, int gpWhere)
/* Filter based on gene predictions if any. */
{
char *gdfDir;
struct wormGdfCache *gdfCache;
struct idbFeature *newList = NULL, *feat, *nextFeat;

if (gpType == gpDontCare)
    return oldList;
findGdf(&gdfDir, &gdfCache);
for (feat = oldList; feat != NULL; feat = nextFeat)
    {
    struct wormFeature *geneList, *gene;
    int featStart = feat->start;
    int featEnd = feat->end;
    int featSize = featEnd - featStart;
    char *hits = needLargeMem(featSize+1); /* Extra zero at end will simplify things */

    memset(hits, 0, featSize+1);    
    nextFeat = feat->next;
    geneList = wormSomeGenesInRange(feat->chrom, feat->start, feat->end, gdfDir);
    for (gene = geneList; gene != NULL; gene = gene->next)
        {
        int start, end, size;
        switch (gpType)
            {
            case gpCds:
                {
                start = max(gene->start, featStart);
                end = min(gene->end, featEnd);
                size = end-start;
                if (size > 0)
                    memset(hits+start-featStart, 0xff, size);
                break;
                }
            case gpCoding:
                {
                struct gdfGene *gdf = wormGetSomeGdfGene(gene->name, gdfCache);
                struct gdfDataPoint *dp = gdf->dataPoints;
                int dpCount = gdf->dataCount;
                int i;

                for (i=0; i<dpCount; i += 2)
                    {
                    start = max(dp[i].start, featStart);
                    end = min(dp[i+1].start, featEnd);
                    size = end-start;
                    if (size > 0)
                        memset(hits+start-featStart, 0xff, size);
                    }
                gdfFreeGene(gdf);
                break;
                }
            }
        }
    addAnywhere(gpWhere, hits, logicType, feat, "gp", &newList);
    slFreeList(&geneList);
    freeMem(hits);
    }
slReverse(&newList);
return newList;
}


void dumpFeatList(char *name, struct idbFeature *featList)
{
struct idbFeature *feat;

printf("Feature List %s\n", name);
for (feat = featList; feat != NULL; feat = feat->next)
    {
    printf("  %s %s:%d-%d %c\n", feat->name, feat->chrom, feat->start, feat->end, feat->strand);
    }
}

void crashAbort()
{
char *pt = NULL;
*pt = 0;
}

void doMiddle()
/* Write out the main part of the .html */
{
int sourceType, offsetType, regionType;
int cdnaHitType, cdnaWhereType,  cdnaStageType;
int cbHomologyType, cbWhereType;
int logicType;
int gpType, gpWhere;
struct idbFeature *featList;
char *rawNames;
int startOffset, endOffset;
long startTime, endTime;
boolean reportTime = FALSE;

//pushAbortHandler(crashAbort);
printf("<TT><PRE>");
wormChromNames(&chromNames, &chromCount);

/* Get the source sequence names and original ranges. */
sourceType = cgiOneChoice("source", sourceChoices, ArraySize(sourceChoices));
rawNames = cgiString("seqNames");
startTime = clock1000();
featList = getSequenceNames(sourceType, rawNames);
endTime = clock1000();
if (reportTime) printf("%4.3f seconds to getSequenceNames\n", 0.001*(endTime-startTime) );

//dumpFeatList("After source", featList);

/* Break up and filter features based on region. */
startTime = clock1000();
regionType = cgiOneChoice("region", regionChoices, ArraySize(regionChoices));
featList = filterRegions(featList, regionType, sourceType);
endTime = clock1000();
if (reportTime) printf("%4.3f seconds to filterRegions\n", 0.001*(endTime-startTime) );

/* Offset features. */
startTime = clock1000();
offsetType = cgiOneChoice("relativeTo", relativeToChoices, ArraySize(relativeToChoices));
startOffset = cgiOptionalInt("startOffset", 0);
endOffset = cgiOptionalInt("endOffset", 0);
featList = offsetFeatures(featList, offsetType, startOffset, endOffset);
endTime = clock1000();
if (reportTime) printf("%4.3f seconds to offsetFeatures\n", 0.001*(endTime-startTime) );

/* Restrict to C. briggsae homologous regions. */
startTime = clock1000();
cbWhereType = cgiOneChoice("cbWhere", cWhereChoices, ArraySize(cWhereChoices));
cbHomologyType = cgiOneChoice("cbHomology", cbHomologyChoices, ArraySize(cbHomologyChoices));
logicType = cgiOneChoice("notCb", logicChoices, ArraySize(logicChoices));
featList = filterCbHomology(featList, cbHomologyType, logicType, cbWhereType);
endTime = clock1000();
if (reportTime) printf("%4.3f seconds to filterCbHomology\n", 0.001*(endTime-startTime) );

/* Restrict to cDNA hits. */
startTime = clock1000();
cdnaWhereType = cgiOneChoice("cdnaWhere", cWhereChoices, ArraySize(cWhereChoices));
cdnaHitType = cgiOneChoice("cdnaHits", cdnaHitsChoices, ArraySize(cdnaHitsChoices));
cdnaStageType = cgiOneChoice("cdnaStage", cdnaStageChoices, ArraySize(cdnaStageChoices));
logicType = cgiOneChoice("notCdna", logicChoices, ArraySize(logicChoices));
featList = restrictToCdna(featList, cdnaHitType, cdnaStageType, cdnaWhereType, logicType, regionType);
endTime = clock1000();
if (reportTime) printf("%4.3f seconds to restrictToCdna\n", 0.001*(endTime-startTime) );

/* Restrict to gene prediction hits. */
startTime = clock1000();
gpWhere = cgiOneChoice("gpWhere", cWhereChoices, ArraySize(cWhereChoices));
gpType = cgiOneChoice("gpType", gpChoices, ArraySize(gpChoices));
logicType = cgiOneChoice("notGp", logicChoices, ArraySize(logicChoices));
featList = filterGenePredictions(featList, gpType, logicType, gpWhere);
endTime = clock1000();
if (reportTime) printf("%4.3f seconds to filterGenePredictions\n", 0.001*(endTime-startTime) );

startTime = clock1000();
outputList(featList, cgiOneChoice("output", outputChoices, ArraySize(outputChoices)),
    cgiOptionalInt("lineSize",0), cgiOptionalInt("wordSize",0), 
    cgiBoolean("lineNumbers"), cgiOneChoice("capType", capChoices, ArraySize(capChoices)));
endTime = clock1000();
if (reportTime) printf("%4.3f seconds to outputList\n", 0.001*(endTime-startTime) );
}

int main(int argv, char *argc[])
{
dnaUtilOpen();
localMem = lmInit(0);
htmShell("Relative Sequence Data", doMiddle, NULL);
return 0;
}
