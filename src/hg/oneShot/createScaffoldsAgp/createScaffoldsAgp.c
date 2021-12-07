/* createScaffoldsAgp - Takes an AGP of contigs mapped to chromosomes and 
an AGP of contigs mapped to scaffolds and creates an AGP file of scaffolds 
mapped to chromosomes. This program was written specifically to handle
AGP files for the zebrafish Zv7 assembly (danRer5). */

/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "dystring.h"

#define FRAGLINE 9
#define GAPLINE 8

void usage()
/* Explain usage and exit. */
{
errAbort(
  "createScaffoldsAgp - Takes an AGP of contigs mapped to chromosomes and\n" 
  "an AGP of contigs mapped to scaffolds and creates an AGP file of\n" 
  "scaffolds mapped to chromosomes.\n"
  "usage:\n"
  "   createScaffoldsAgp <contigs to scaffolds AGP> <contigs to chroms AGP>\n"
  "   <output file for scaffolds to chroms AGP>\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct agpInfo
    {
    /* fields below are common to all lines of AGP including gap lines */
    struct agpInfo *next; /* Next in singly linked list */
    char *name;  /* scaffold, contig or chrom ID for object */
    unsigned int start; /* object start position */
    unsigned int end;   /* object end position */
    unsigned int partNum; /* part number of object */
    char type[2]; /* component type, sequencing status of component */
    char *acc; /* accession or contig ID */
    unsigned int ctgStart; /* start on contig, char for making hash key */
    unsigned int ctgEnd;   /* end on contig, char for making hash key */
    char *gapType; /* type of gap e.g. fragment or contig */
    char line[512]; /* rest of line of agp */
    };

boolean lineFileNextCharRow2(struct lineFile *lf, char sep, char *words[], int wordCount)
/* Return next non-blank line that doesn't start with '#' chopped into words
 * delimited by sep. Returns FALSE at EOF.  Aborts on error if words in line
 * are not the same as wordCount or wordCount-1. */
{
int wordsRead;
wordsRead = lineFileChopCharNext(lf, sep, words, wordCount);
if (wordsRead == 0)
    return FALSE;
if ((wordsRead > wordCount) || (wordsRead < wordCount-1))
    errAbort("Expecting %d or %d words line %d of %s got %d",
            wordCount, wordCount-1, lf->lineIx, lf->fileName, wordsRead);

return TRUE;
}

struct agpInfo *agpInfoLoadAllByChar(char *fileName, char chopper)
/* Load all agpInfo from a chopper separated file.
   Dispose of this with agpInfoFreeList(). */
{
struct agpInfo *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[FRAGLINE];

while (lineFileNextCharRow2(lf, chopper, row, ArraySize(row)))
    {
    AllocVar(el);
    el->name = cloneString(row[0]);
    el->start = sqlUnsigned(row[1]);
    el->end = sqlUnsigned(row[2]);  
    el->partNum = sqlUnsigned(row[3]);
    strcpy(el->type, row[4]);
    
    if (sameWord(el->type, "N"))
        {
        el->acc = NULL;
        el->ctgStart = 0;
        el->ctgEnd = 0;
        el->gapType = cloneString(row[6]);
        /* partial row saved for printing at the end */
        safef(el->line, sizeof(el->line), "%s\t%s\t%s\n", row[5], row[6], row[7]);
        }
    else
        {
        el->acc = cloneString(row[5]);
        el->ctgStart = sqlUnsigned(row[6]);
        el->ctgEnd = sqlUnsigned(row[7]);
        el->gapType = NULL;
        /* partial row saved for printing at the end */
        safef(el->line, sizeof(el->line), "%s\t%s\t%s\n", row[6], row[7], row[8]);
        }
       
    slAddHead(&list, el); 
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

#define agpInfoLoadAllByTab(a) agpInfoLoadAllByChar(a, '\t')
/* Load all agpInfo from tab separated file. 
   Dispose of this using agpInfoFreeList(). */

void agpInfoFree(struct agpInfo **pEl)
/*  Free a single dynamically allocated agpInfo such as created
 * with agpInfoLoad(). */
{
struct agpInfo *el;

if ((el = *pEl) == NULL) return;
freeMem(el->name);
if (el->acc != NULL)
    freeMem(el->acc);
if (el->gapType != NULL)
    freeMem(el->gapType);
freez(pEl);
}

void agpInfoFreeList(struct agpInfo **pList)
/* Free a list of dynamically allocated agpInfos */
{
struct agpInfo *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    agpInfoFree(&el);
    }
*pList = NULL;
}

struct agpInfo *readAgpFile(char *agpFile)
/* read AGP file and store information in a list */
{
struct agpInfo *list = NULL;

list = agpInfoLoadAllByTab(agpFile);
return list;
}

char *createCtgKey(char *acc, int start, int end)
/* Create a unique hash key of the contig with its end-start diff */
{
struct dyString *dy = dyStringNew(256);

dyStringPrintf(dy, "%s_%d_%d", acc, start, end);
return dyStringCannibalize(&dy); 
}

void storeCtgsOfScafs(struct agpInfo *list, struct hash **first, struct hash **last)
{
/* store scaffolds in hashes keyed by the contig and end-start diff. There
   is one hash for the first contig of a scaffold and one for the last contig
   of a scaffold */
struct hash *firstHash = *first;
struct hash *lastHash = *last;
struct agpInfo *agpEl = NULL, *prevEl = NULL;
char *key = NULL;

while ((agpEl = (struct agpInfo*)slPopHead(&list)) != NULL)
    {
    /* if part number is 1 then this is the first contig for a scaffold */
    if (agpEl->partNum == 1)
        {
        key = createCtgKey(agpEl->acc, agpEl->ctgStart, agpEl->ctgEnd);
        hashAdd(firstHash, key, agpEl->name);
        /* if not at the beginning of the file, then previous element is the 
           last for the previous scaffold */
        if (prevEl != NULL)
            { 
            key = createCtgKey(prevEl->acc, prevEl->ctgStart, prevEl->ctgEnd);
            hashAdd(lastHash, key, prevEl->name);
            }
        }
    /* set the current agp element to be the previous element */
    prevEl = agpEl;
    }
/* at end of list need to add the last one to the hash */
if (prevEl != NULL)
    {
    key = createCtgKey(prevEl->acc, prevEl->ctgStart, prevEl->ctgEnd);
    hashAdd(lastHash, key, prevEl->name);
    }
}

boolean findScaffold(struct hash *hash, char *name, char **scaffold)
/* check for name in the hash of first contigs and return scaffold name */
{
char *scaf = NULL;

if ((scaf = (char *)hashFindVal(hash, name)) != NULL) 
    {
    *scaffold = cloneString(scaf); 
    return TRUE;
    }
else 
    return FALSE;
}

struct agpInfo *processContigGap (struct agpInfo *agpEl, int partNum)
/* adds a contig gap line to new list */
{
struct agpInfo *gapEl = NULL;
/* then process the currently read struct which is the contig gap.
   This agpInfo does not change except for the partNum */

if ((agpEl->gapType != NULL) && sameWord(agpEl->gapType, "contig"))
     {
     AllocVar(gapEl); 
     gapEl->name = cloneString(agpEl->name);
     gapEl->start = agpEl->start;
     gapEl->end = agpEl->end;
     partNum++;
     gapEl->partNum = partNum;
     strcpy(gapEl->type, agpEl->type);
     gapEl->acc = NULL;
     gapEl->ctgStart = agpEl->ctgStart;
     gapEl->ctgEnd = agpEl->ctgEnd;
     gapEl->gapType = cloneString(agpEl->gapType);
     strcpy(gapEl->line, agpEl->line);
     } 
return gapEl;
}

struct agpInfo* makeNewElement(struct agpInfo *el, struct agpInfo *newEl, int partNum, char *scaf, boolean first)
/* put together new AGP element */
{

if (first)
    {
    AllocVar(newEl);
    newEl->name = cloneString(el->name);
    newEl->start = el->start;
    newEl->partNum = partNum;
    strcpy(newEl->type, "W");
    /* add scaffold name */
    newEl->acc = cloneString(scaf);
    }

else
    {
    if ((newEl->acc != NULL) && !sameWord(scaf, newEl->acc))
             errAbort("Expecting the scaffold to be %s, but it is %s", 
                          newEl->acc, scaf);
    /* finish adding to new element struct */
    newEl->end = el->end;
    int diff = newEl->end - newEl->start;
    newEl->ctgStart = 1;
    newEl->ctgEnd = newEl->ctgStart + diff;
    /* then add rest of line which is just the strand */
    safef(newEl->line, sizeof(newEl->line), "+\n");
    }
return newEl;
}

struct agpInfo *makeScaffoldToChromAgp(struct agpInfo *list, struct hash *firstHash, struct hash *lastHash)
/* Takes the contigs to chrom AGP list and prints out the scaffold to 
   chrom list.
 */
{
struct agpInfo *agpEl = NULL, *prevEl = NULL, *newEl = NULL; 
struct agpInfo *newList = NULL, *firstEl = NULL, *newEl2 = NULL, *gapEl = NULL;
char *scaf = NULL;
boolean first = FALSE, last = FALSE, contigGap = FALSE, firstFound = FALSE;
int count = 0, partNum = 0;
char *ctgKey = NULL;

while ((agpEl = (struct agpInfo*)slPopHead(&list)) != NULL)
    {
    /* reset partNum if this is a new chromosome, tracked by prevEl */
    if (prevEl != NULL && (!sameWord(prevEl->name, agpEl->name)))
        partNum = 0;
    count++;
    first = FALSE;
    last = FALSE;
    contigGap = FALSE;
     /* look for contig only if type is not N i.e. a gap */
     /* if the contig is found in the first hash then store the coordinates */
    if (agpEl->acc != NULL)
         {
         ctgKey = createCtgKey(agpEl->acc, agpEl->ctgStart, agpEl->ctgEnd);
         first = findScaffold(firstHash, ctgKey, &scaf);
         }

    /* if it is a contig gap line then set flag */
    if (agpEl->gapType != NULL && (sameWord(agpEl->gapType, "contig")))
        contigGap = TRUE;
  
    if (first)
        { 
        partNum++;
        firstEl = newEl;
        /* add scaffold name */
        newEl = makeNewElement(agpEl, newEl, partNum, scaf, first);
        } 
   
    if (first && partNum == 1 && count!= 1)
       {
       if (prevEl != NULL && prevEl->acc != NULL)
           {
        if (firstEl != NULL)
           ctgKey = createCtgKey(prevEl->acc, prevEl->ctgStart, prevEl->ctgEnd);
           last = findScaffold(lastHash, ctgKey, &scaf);
           if (last)
                {
                if (firstEl != NULL)
                    {
                    newEl2 = makeNewElement(prevEl, firstEl, partNum, scaf, FALSE);
                    slAddHead(&newList, newEl2);
                    firstFound = FALSE;
                    }
                }
           } 
       }
    if (contigGap)
         {
         /* found a contig gap, add this line to the list, these 
            gaps are between scaffolds. */
         if (prevEl != NULL && prevEl->acc != NULL)
             {
              ctgKey = createCtgKey(prevEl->acc, prevEl->ctgStart, prevEl->ctgEnd);
              last = findScaffold(lastHash, ctgKey, &scaf);
              if (last)
                   {
                   if (newEl != NULL)
                        {
                        newEl2 = makeNewElement(prevEl, newEl, partNum, scaf, FALSE);
                        slAddHead(&newList, newEl2);
                        }
                    }
              }
         /* then add contig gap */
         gapEl = processContigGap(agpEl, partNum);
         slAddHead(&newList, gapEl);
         }
    prevEl = agpEl;
    }
/* print last item in list */
if (prevEl != NULL && prevEl->acc != NULL)
     {
     last = FALSE;
     ctgKey = createCtgKey(prevEl->acc, prevEl->ctgStart, prevEl->ctgEnd);
     last = findScaffold(lastHash, ctgKey, &scaf);
    /* check that the scaffold is the same as that for 
       the first part */
    if (last)
        {
        if (firstEl != NULL)
            {
            newEl2 = makeNewElement(prevEl, newEl, partNum, scaf, FALSE);
            slAddHead(&newList, newEl2);
            }
        }
     }
slReverse(&newList);
return newList;
}

void printNewAgpList(FILE *out, struct agpInfo *agpList)
/* prints out the contents of the AGP file mapping scaffolds to chroms */
{
struct agpInfo *agpEl = NULL;

while((agpEl = (struct agpInfo*)slPopHead(&agpList)) != NULL)
    {
    fprintf(out, "%s\t%d\t%d\t%d\t%s", agpEl->name, agpEl->start, agpEl->end, agpEl->partNum, agpEl->type);
    /* if it is not a gap line */
    if (agpEl->type != NULL && !sameString(agpEl->type, "N"))
        {
        /* print rest of line */
        fprintf(out, "\t%s\t%d\t%d\t%s", agpEl->acc, agpEl->ctgStart, agpEl->ctgEnd, agpEl->line);
        }
    else 
        fprintf(out, "\t%s", agpEl->line);
    }
}

void createScaffoldsAgp(char *ctgToScafAgpFile, char *ctgToChromAgpFile, char *outFile)
/* createScaffoldsAgp - Takes an AGP of contigs mapped to chromosomes and 
an AGP of contigs mapped to scaffolds and creates an AGP file of scaffolds 
mapped to chromosomes. */
{
FILE *out = mustOpen(outFile, "w");
struct hash *firstCtgHash = newHash(0);
struct hash *lastCtgHash = newHash(0);
struct agpInfo *agpScafsList = NULL, *agpChromsList = NULL, *newAgpList = NULL;

/* Read in AGP file that maps contigs to scaffolds */
agpScafsList = readAgpFile(ctgToScafAgpFile);

/* Read in AGP file that maps contigs to chroms */
agpChromsList = readAgpFile(ctgToChromAgpFile);

/* store the first and last contig names for each scaffold */
storeCtgsOfScafs(agpScafsList, &firstCtgHash, &lastCtgHash);

/* go through contigs to chroms AGP and rewrite as scaffolds to chroms AGP */
newAgpList = makeScaffoldToChromAgp(agpChromsList, firstCtgHash, lastCtgHash);

/* then write out the contents of newAgpList */
printNewAgpList(out, newAgpList);

/* Frees all the values in the agp Lists and also the values that were 
   used for the hashes */
agpInfoFreeList(&agpScafsList);
agpInfoFreeList(&agpChromsList);
agpInfoFreeList(&newAgpList);
freeHash(&firstCtgHash);
freeHash(&lastCtgHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
createScaffoldsAgp(argv[1], argv[2], argv[3]);
return 0;
}
/* Read in contigs to scaffolds AGP file and store the names of the first and
   last contigs for each scaffold in hashes - one for first and one for last.
   Hashes are keyed by contig name and values are the scaffold names */
