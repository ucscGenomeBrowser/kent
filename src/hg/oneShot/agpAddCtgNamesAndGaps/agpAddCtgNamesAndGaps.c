/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

/* agpAddCtgNamesAndGaps - Add component names to AGP where there are multiple 
* components contributing to one accession with gaps getween them. e.g.
* Zv7_scaffold2543        228414  260893  29      D       BX649254.13     1 
* 32480   +
* Zv7_scaffold2543        260894  260993  30      N       100     fragment no
* Zv7_scaffold2543        260994  326741  31      D       BX649254.13     32581
* 98328   +
* Zv7_scaffold2543        326742  326841  32      N       100     fragment no
* The BX649254.13 accession is changed to the name given for this fragment in 
* the contigs FASTA file. Also, add gaps between scaffolds. This was written 
* to process the randoms contigs agp file for danRer5. 
* Input: file of fragment parts' names and positions (relative to accession),
* agp file, output file.
* Example of line from fragment parts' file (from contigs FASTA headers):
* BX649254.13_01364 zK228P6.01364 BX649254 1 32480
*/

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "dystring.h"

#define FRAGLINE 9
#define GAPLINE 8

char *assembly = "Zv7";
int gap = 50000;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "agpAddCtgNamesAndGaps - Add component names to AGP where there are\n" 
  "multiple components contributing to one accession with gaps getween them.\n"
  "usage:\n"
  "   agpAddCtgNamesAndGaps <fragment parts and positions> <agp file>"
  " <output file>\n"
  "   where fragment parts' file is of format: \n"
  "   <accession part ID> <Sanger name> <accession> <start> <end>\n" 
  "   (start and end are relative to accession).\n"
  "   BX649254.13_01364 zK228P6.01364 BX649254 1 32480\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct ctgInfo 
    {
    struct ctgInfo *next; /* Next in singly linked list */
    char *acc;  /* accession for scaffold */
    char *ctgName; /* name of contig */
    unsigned int start; /* contig start position */
    unsigned int end;   /* contig end position */
    };

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
    char line[512]; /* rest of line of agp */
    };

char *createCtgInfoKey(char *acc, int start, int end)
/* Create a unique hash key for a ctgInfo using the contig accession and the
   scaffold start and end positions */
{
struct dyString *dy = dyStringNew(256);

dyStringPrintf(dy, "%s_%d_%d", acc, start, end);
return dyStringCannibalize(&dy); 
}

struct ctgInfo *ctgInfoLoadOne(struct lineFile *lf)
/* Load one line of ctgInfo from a whitespaces-separated file.
   Dispose of this with ctgInfo(). */
{
struct ctgInfo *el = NULL;
char *row[5];
char *acc = NULL;

if (lineFileRow(lf, row))
    {
    AllocVar(el);
    el->ctgName = cloneString(row[0]);
    /* remove suffix to get accession */
    acc = cloneString(row[0]);
    chopSuffixAt(acc, '_');
    el->acc = acc;
    el->start = sqlUnsigned(row[3]);
    el->end = sqlUnsigned(row[4]);  
    }
return el;
}

void ctgInfoFree(struct ctgInfo **pEl)
/*  Free a single dynamically allocated ctgInfo such as created
 * with ctgInfoLoad(). */
{
struct ctgInfo *el;
if ((el = *pEl) == NULL) return;
freeMem(el->ctgName);
freeMem(el->acc);
freez(pEl);
}

void ctgInfoFreeList(struct ctgInfo **pList)
/* Free a list of dynamically allocated ctgInfos */
{
struct ctgInfo *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ctgInfoFree(&el);
    }
*pList = NULL;
}

void ctgInfoFreeHash(struct hash **hash)
/* Free a hash of dynamically allocated ctgInfo's */
{
struct ctgInfo *el;

struct hashCookie cookie = hashFirst(*hash);
struct hashEl *hashEl;

while ((hashEl = hashNext(&cookie)) != NULL)
   {
   el = (struct ctgInfo *) hashEl->val;
   ctgInfoFree(&el);
/*
   freez(&hashEl->name); */
   }
freeHash(hash);
}

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
        /* partial row saved for printing at the end */
        safef(el->line, sizeof(el->line), "%s\t%s\t%s\n", row[5], row[6], row[7]);
        }
    else
        {
        el->acc = cloneString(row[5]);
        el->ctgStart = sqlUnsigned(row[6]);
        el->ctgEnd = sqlUnsigned(row[7]);
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

struct hash *readCtgsFile(char *ctgsFile)
/* read file of contigs headers with contigs information and store in a hash */
{
struct ctgInfo *ctg = NULL;
struct hash *hash = newHash(0);
char *hashKey;

struct lineFile *lf = lineFileOpen(ctgsFile, TRUE);
while ((ctg = ctgInfoLoadOne(lf)) != NULL)
    {
    /* Add this to a hash keyed by the accession */
    hashKey = createCtgInfoKey(ctg->acc, ctg->start, ctg->end);
    hashAdd(hash, hashKey, ctg);
    }
lineFileClose(&lf);
return hash;
}

struct agpInfo *readAgpFile(char *agpFile)
/* read AGP file and store information in a list */
{
struct agpInfo *list = NULL;

list = agpInfoLoadAllByTab(agpFile);
return list;
}

void createRandomsAgp(struct hash *ctgsHash, struct agpInfo *agpList, FILE *out)
/* print out the randoms AGP file to the output file. Add the contig names to 
 * match those in the FASTA file and add a gap of specified number of Ns to 
 * the AGP between scaffolds. 
 */
{
struct agpInfo *agpEl = NULL;
struct ctgInfo *ctgEl = NULL;
char NARandom[256];
char UnRandom[256];
char chrom[256] = "";
char *prevChrom = NULL;
int lineNum = 0;
boolean endOfScaffold = FALSE;
int prevEnd = 0, newStart = 0, newEnd = 0, size = 0; 

safef(NARandom, sizeof(NARandom), "%s_NA", assembly);
safef(UnRandom, sizeof(UnRandom), "%s_scaffold", assembly);
while((agpEl = (struct agpInfo*)slPopHead(&agpList)) != NULL)
    {
    prevChrom = cloneString(chrom);
    /* create correct chromosome name */
    if (startsWith(NARandom, agpEl->name))
        safef(chrom, sizeof(chrom), "chrNA_random");
    else if (startsWith(UnRandom, agpEl->name))
        safef(chrom, sizeof(chrom), "chrUn_random");
    else 
        errAbort("Expecting %s_NA or %s_scaffold in contig name\n", assembly, assembly);
    /* if the new chrom is not the same as the one from the previous line
     * then reset lineNum to 0 as this is tracking the part number for the 
     * chromosome. This will also prevent an extra gap line being added.  */
    if (!sameWord(chrom, prevChrom))
        lineNum = 0;
    lineNum++;
    /* if the part number is 1 then this is the start of a new
     * scaffold so insert 50000 Ns here */
    if ((lineNum != 1) && (agpEl->partNum == 1))   
        endOfScaffold = TRUE;
    else
        endOfScaffold = FALSE;
    if (endOfScaffold)
        {
        newStart = prevEnd + 1;
        newEnd = (newStart + 50000) - 1;
        fprintf(out, "%s\t%d\t%d\t%d\tN\t50000\tcontig\tno\n", chrom, newStart, newEnd, lineNum);
        /* add 1 to the line number */
        lineNum++;
        prevEnd = newEnd;
        }
    /* then process this AGP line */
    newStart = prevEnd + 1;
    size = agpEl->end - agpEl->start;
    newEnd = newStart + size;
    fprintf(out, "%s\t%d\t%d\t%d\t%s", chrom, newStart, newEnd, lineNum, agpEl->type);     
    if (!startsWith(agpEl->type, "N"))
        {
        /* if it is not a gap line and the contig or accession is in the 
         * contigs hash, then add the correct contig name to the agp */
        char *ctgKey = createCtgInfoKey(agpEl->acc, agpEl->ctgStart, agpEl->ctgEnd);
        if ((ctgEl = hashFindVal(ctgsHash, ctgKey)) != NULL)
            {
            /* add contig name from FASTA to agp */
            fprintf(out, "\t%s", ctgEl->ctgName); 
            }
        else
            fprintf(out, "\t%s", agpEl->acc);
        freez(&ctgKey);
        }
    /* print rest of the line */
    fprintf(out, "\t%s", agpEl->line);
    /* set previous end to the current end */
    prevEnd = newEnd;
    freez(&prevChrom);
    }
}

void agpAddCtgNamesAndGaps(char *ctgsFile, char *agpFile, char *outFile)
/* agpAddCtgNamesAndGaps - Add contig names to AGP and add gaps between 
   scaffolds. */
{
/* file handle for output file */
FILE *out = mustOpen(outFile, "w");
/* hash of contig information */
struct hash *ctgsHash = NULL;
struct agpInfo *agpList = NULL;

ctgsHash = readCtgsFile(ctgsFile);

/* Then read in agp file and add to a list to keep in order */
agpList = readAgpFile(agpFile);
createRandomsAgp(ctgsHash, agpList, out);

/* free hashes and associated dynamically allocated variables */
ctgInfoFreeHash(&ctgsHash); 
agpInfoFreeList(&agpList);
/* close output file */
carefulClose(&out);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
agpAddCtgNamesAndGaps(argv[1], argv[2], argv[3]);
return 0;
}
