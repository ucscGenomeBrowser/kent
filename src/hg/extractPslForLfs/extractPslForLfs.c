/*
  File: extractPslForLfs.c
  Author: Rachel Harte
  Description: Takes a PSL file and a lfs file as input. Extracts the 
  relevant PSL rows for the items in the lfs file based on name, chrom
  and position.

*/

#include "common.h"
#include "linefile.h"
#include "memalloc.h"
#include "errabort.h"
#include "hash.h"
#include "psl.h"
#include "lfs.h"
#include "bed.h"
#include "options.h"

#define MAXFILES 100

static char const rcsid[] = "$Id: extractPslForLfs.c,v 1.2 2006/08/09 18:55:08 hartera Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"verbose", OPTION_STRING},
    {NULL, 0}
};

char *createKey (char *name, char *chrom, int start, int end)
/* creates a hash key with the name. chrom, chromStart and chromEnd */
{
char key[255], *key2;

safef(key, sizeof(key), "%s_%s_%d_%d", name, chrom, start, end);
key2 = cloneString(key);
return key2;
}

boolean existsInHash (struct hash *hash, char *key)
/* check if key exists in hash already */
{
if ((hashFindVal(hash, key)) != NULL)
    return TRUE;
else
    return FALSE;
}

void readPslFile (struct lineFile *pf, struct hash **hash)
/* Read in psl file and store contents in a hash keyed by qName */
{
struct hash *pslHash = *hash;
struct psl *psl = pslNext(pf);
char *key = NULL;

while (psl != NULL)
    {
    /* add to hash */
    if (psl != NULL)
        {
        key = createKey(psl->qName, psl->tName, psl->tStart, psl->tEnd);
        /* check if this key exists already, if not then add to hash */
        if (!existsInHash(pslHash, key))    
            hashAdd(pslHash, key, psl);
        }    
    psl = pslNext(pf);
    }
}

struct bed *addLfToList(struct lfs *el, struct bed *list)
{
struct bed *lfBed;
int i;

for (i = 0; i < el->lfCount; i++)
    {
    AllocVar(lfBed);
    lfBed->chrom = cloneString(el->chrom);
    lfBed->chromStart = el->lfStarts[i];
    lfBed->chromEnd = el->lfStarts[i] + el->lfSizes[i];
    lfBed->name = el->lfNames[i];
    slSafeAddHead(&list, lfBed);
    }
return list;
}
 
struct bed *convertLfsToBed (struct lineFile *lf)
/* Read in lfs file contents. Convert to bed struct and store as a list */
{
struct lfs *el = NULL;
char *row[11];
struct bed *lfBedList = NULL;
char *chrom, *name;
unsigned int chromStart, chromEnd;
int i;

while (lineFileRow(lf, row))
    {
    el = lfsLoad(row);
    if (el != NULL)
      /* add each linked feature to list */
      lfBedList = addLfToList(el, lfBedList);
    }
if (lfBedList == NULL)
    errAbort("Bed list is null.\n");
return lfBedList;
}

struct psl *getPslList(struct hash *pHash, struct bed *bList)
/* Get psls for alignments in bed file */
{
struct bed *bedEl;
struct psl *psl = NULL, *pslNext = NULL, *pslList = NULL;
struct hashEl *hEl;
char *key = NULL;

for (bedEl = bList; bedEl != NULL; bedEl = bedEl->next)
    {
    /* create key for hash look up */
    key = createKey(bedEl->name, bedEl->chrom, bedEl->chromStart, bedEl->chromEnd);
    hEl = hashLookup(pHash, key);
    if (hEl != NULL)
        {
        /* Get psl and then add to list */
        psl = hEl->val;
        if (psl != NULL)
            {
            slSafeAddHead(&pslList, psl);
            /* Remove from hash */
            hashRemove(pHash, key);
            }
        }
    }
return pslList;
}

void extractPslForLfs(char *pslFile, char *lfsFile, char *out)
/* Extract psl alignments for alignments in lfs bed file */
{
struct lineFile *pf, *bf;
FILE *pslOut;
char errorFile[256] = "error.log";
struct hash *pslHash = newHash(20);
struct bed *bedList = NULL;
struct psl *pslList = NULL;

fprintf(stdout,"NOTE: Program assumes that chrom, chromStart and chromEnd \n");
fprintf(stdout, "are unique for each item in the lfs file so only one PSL \n"); 
fprintf(stdout, "alignment will be picked for each specific item and \n");
fprintf(stdout, "genomic location.\n\n");

fprintf(stdout, "Opening files ...\n");
bf = lineFileOpen(lfsFile, TRUE);
pslOut = mustOpen(out, "w");
stderr = mustOpen(errorFile, "w");

/* Open psl alignments file */
pf = pslFileOpen(pslFile);
/* Read in psl file of alignments */
verbose(1, "Reading psl and storing alignments file ...\n");
readPslFile(pf, &pslHash);

/* Read in lfs BED file of alignments */
verbose(1, "Reading and storing lfs alignments file ...\n");
bedList = convertLfsToBed(bf);
verbose(1, "Extracting psl alignments ... \n");
pslList = getPslList(pslHash, bedList);
/* print out the psl file without the header */
pslWriteAll(pslList, out, FALSE);

/* close files */
lineFileClose(&bf);
lineFileClose(&pf);
carefulClose(&pslOut);
carefulClose(&stderr);
}

int main(int argc, char *argv[])
/* Process command line */
{
int verb = 0;

verboseSetLevel(0);
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    {
fprintf(stdout, "USAGE: extractPslForLfs [-verbose=<level>] psl lfsBed outputPsl\n");
    return 1;
    }
verb = optionInt("verbose", 0);
verboseSetLevel(verb);
extractPslForLfs(argv[1], argv[2], argv[3]);
return (0);
}

