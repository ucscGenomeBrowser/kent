/* Split MAF file based on positions in a bed file */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "portable.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "maf.h"
#include "bed.h"
#include "dlist.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafBreak - calculate distance to chrom breaks for each bed for each species\n"
  "usage:\n"
  "    mafBreak regions.bed species.lst outStats *.maf\n"
  );
}

static struct optionSpec options[] = {
    {"outDir", OPTION_BOOLEAN},
    {"ignoreDups", OPTION_BOOLEAN},
    {NULL, 0},
};

/* a structure to keep the lists of bed's for each chromosome */
struct bedHead
{
    struct bed *head;
};

struct breakPoint 
{
int refAddress;
int nestLevel;
int type;
}; 

struct speciesBreaks
{
    int currentNestLevel;
    int numBreaks;
    int allocedBreaks;
    struct breakPoint *breaks;
};

boolean outDir = FALSE;
char *dir = NULL;
char *scoring = NULL;
boolean ignoreDups = FALSE;

void checkBeds(struct bed *bed)
{
char *chrom = NULL;
struct bed *prev = NULL;

for(; bed; prev = bed, bed = bed->next)
    {
    if ((chrom == NULL) || !sameString(chrom, bed->chrom))
	chrom = bed->chrom;
    else
	{
	if ((bed->chromStart == prev->chromStart) && (bed->chromEnd == prev->chromEnd))
	    {
	    if (ignoreDups)
		{
		prev->next = bed->next;
		bed = prev;
		}
	    else
		errAbort("dup'ed bed starting at %d end at %d on chrom %s with name %s",
		    bed->chromStart, bed->chromEnd, bed->chrom, bed->name);
	    }
	else if (bed->chromStart < prev->chromEnd)
	    {
	    errAbort("bed starting at %d overlaps previous end at %d on chrom %s",
	    	bed->chromStart, prev->chromEnd, chrom);
	    }
	}
    }
}


struct hash *loadSpecies(char *file)
{
struct lineFile *lf = lineFileOpen(file, TRUE);
char *row[1];
struct hash *cHash = newHash(0);
while (lineFileRow(lf, row))
    {
    struct speciesBreaks *sb;
    char *name = row[0];
    //struct breakPoint *aBreak;
    AllocVar(sb);
    sb->allocedBreaks = 0;

    hashAdd(cHash, name, sb);
    }
lineFileClose(&lf);
return cHash;
}

struct hash *loadRegions(char *file)
/* load regions into a hash of lists by chrom */
{
struct bed *bed = NULL,  *nextBed = NULL;
struct hash *regionHash = newHash(6);
struct bed *regions;
char *chrom = NULL;
struct bedHead *bHead = NULL;

regions = bedLoadNAll(file, 4);
/* order by chrom, start */
slSort(&regions, bedCmp);
verbose(2, "found %d regions\n", slCount(regions));

/* make sure the bed regions are in the right order */
slSort(regions, bedCmp);
checkBeds(regions);

for (bed = regions; bed != NULL; bed = nextBed)
    {
    verbose(3, "region %s:%d-%d\n", bed->chrom, bed->chromStart+1, bed->chromEnd);
    nextBed = bed->next;
    if ((chrom == NULL) || (!sameString(chrom, bed->chrom)))
	{
	if ((bHead = hashFindVal(regionHash, bed->chrom)) != NULL)
	    errAbort("bed file not sorted correctly");
	
	AllocVar(bHead);
	verbose(3, "adding %s to hash\n", bed->chrom);
	hashAdd(regionHash, bed->chrom, bHead);
	chrom = bed->chrom;
        }

    slAddHead(&bHead->head, bed);
    }

if (bHead)
    slReverse(&bHead->head);

return regionHash;
}

char *chromFromSrc(char *src)
/* get chrom name from <db>.<chrom> */
{
char *p;
if ((p = strchr(src, '.')) == NULL)
    errAbort("Can't find chrom in MAF component src: %s\n", src);
return ++p;
}

FILE *startOutFile(char *outFile)
{
static FILE *f = NULL;

f = mustOpen(outFile, "w");
verbose(3, "creating %s\n", outFile);
return f;
}

void endOutFile(FILE *f)
{
if (f && outDir)
    {
    mafWriteEnd(f);
    carefulClose(&f);
    }
}

void clearBreaks(struct hash *speciesHash)
{
struct hashCookie cookie = hashFirst(speciesHash);
struct hashEl *hel;

while ((hel = hashNext(&cookie)) != NULL)
    {
    struct speciesBreaks *sb = hel->val;

    //printf("clearing %s\n",hel->name);
    sb->numBreaks = 0;
    sb->currentNestLevel = 0;
    freez(&sb->breaks);
    sb->allocedBreaks = 0;
    }
}

struct breakPoint *getBreak(struct speciesBreaks *sb)
{
struct breakPoint *aBreak;

if (sb->numBreaks == sb->allocedBreaks)
    {
    int oldSize = sb->allocedBreaks * sizeof(struct breakPoint);

    //printf("allocating more \n");
    sb->allocedBreaks += 1000;
    if (sb->breaks == NULL)
	sb->breaks = needMem(sb->allocedBreaks * sizeof(struct breakPoint));
    else
	sb->breaks = needMoreMem(sb->breaks, oldSize, 
		sb->allocedBreaks * sizeof(struct breakPoint));
    }

aBreak = &sb->breaks[sb->numBreaks++];

return aBreak;
}

void recordBreaks(struct hash *speciesHash, struct mafAli *maf)
{
struct hashCookie cookie = hashFirst(speciesHash);
struct hashEl *hel;
struct mafComp *masterMc = maf->components;

while ((hel = hashNext(&cookie)) != NULL)
    {
    struct speciesBreaks *sb = hel->val;
    struct mafComp *mc = mafMayFindCompSpecies(maf, hel->name, '.');

    if (mc != NULL)
	{
	struct breakPoint *aBreak;

	/* check for leftStatus */
	if ((mc->leftStatus == 'N') || (mc->leftStatus == 'n'))
	    {
	    int numDashes = 0;
	    char *ptr = mc->text;

	    if (mc->text)
		while (*ptr++ == '-')
		    numDashes++;

	    aBreak = getBreak(sb);

	    if (mc->leftStatus == 'n')
		{
		aBreak->type = 1;
		sb->currentNestLevel++;
		}
	    aBreak->refAddress = masterMc->start + numDashes;
	    aBreak->nestLevel = sb->currentNestLevel;

	    //printf("adding break %s %d %d\n",hel->name,aBreak->refAddress, aBreak->nestLevel);
	    }

	if ((mc->rightStatus == 'N') || (mc->rightStatus == 'n'))
	    {
	    int numDashes = 0;
	    char *ptr = &mc->text[maf->textSize - 1];

	    if (mc->text)
		while (*ptr-- == '-')
		    numDashes++;

	    aBreak = getBreak(sb);
	    if (mc->rightStatus == 'n')
		{
		aBreak->type = -1;
		sb->currentNestLevel--;
		}
	    aBreak->refAddress = masterMc->start + masterMc->size - numDashes;
	    aBreak->nestLevel = sb->currentNestLevel;

	    //printf("adding break %s %d %d\n",hel->name,aBreak->refAddress, aBreak->nestLevel);
	    if (mc->rightStatus == 'N')
		assert(sb->currentNestLevel== 0);
	    }
	}
    }
}


void outItems(FILE *f, struct bedHead *bHead, struct hash *speciesHash)
{
struct bed *bed;

bed = bHead->head;
for(; bed;  bed = bed->next)
    {
    struct hashCookie cookie = hashFirst(speciesHash);
    struct hashEl *hel;
    int nestLevel;

    fprintf(f, ">%s\n",bed->name);
    while ((hel = hashNext(&cookie)) != NULL)
	{
	struct speciesBreaks *sb = hel->val;
	struct breakPoint *aBreak;

	fprintf(f, "%s\t",hel->name);

	/* find break before bed chromStart */
	for(aBreak = sb->breaks; aBreak < &sb->breaks[sb->numBreaks] ;  aBreak++)
	    {
	    if (bed->chromStart < aBreak->refAddress)
		{
		struct breakPoint *nestBreak = aBreak - 1;
		int countNestLevel = 0;
		nestLevel = -1;

		for (; nestBreak >= sb->breaks; nestBreak--)
		    {
		    if (nestBreak->type == -1)
			countNestLevel++;
		    else if (nestBreak->type == 1)
			{
			if (countNestLevel == 0)
			    {
			    nestLevel = nestBreak->nestLevel;
			    break;
			    }
			countNestLevel--;
			}
		    else if (nestBreak->type == 0)
			{
			assert(countNestLevel == 0);
			nestLevel = 0;
			break;
			}
		    }

		if (nestLevel == -1)
		    fprintf(f, "%d\t", -1);
		else
		    fprintf(f, "%d\t", bed->chromStart - nestBreak->refAddress);
		break;
		}
	    }
	assert(aBreak < &sb->breaks[sb->numBreaks]);
	nestLevel = -1000000000;
	for(; aBreak < &sb->breaks[sb->numBreaks] ;  aBreak++)
	    {
	    int countNestLevel = 0;

	    if ((aBreak->refAddress >= bed->chromStart) &&
		(aBreak->refAddress < bed->chromEnd))
		{
		nestLevel = aBreak->nestLevel;
	//	fprintf(f, "-1\n");
	//	nestLevel = -2;
		break;
		}
	    if (bed->chromEnd < aBreak->refAddress)
		{
		if (aBreak->type == 1)
		    countNestLevel++;
		else if (aBreak->type == -1)
		    {
		    if (countNestLevel == 0)
			{
			nestLevel = aBreak->nestLevel;
			break;
			}
		    countNestLevel--;
		    }
		else if (aBreak->type == 0)
		    {
		    assert(countNestLevel == 0);
		    nestLevel = 0;
		    break;
		    }
		}
	    }

	if (nestLevel == -1000000000)
	    errAbort("didn't find after nest level %s",hel->name);

	//if (nestLevel != -2)
	    fprintf(f, "%d\n",  aBreak->refAddress - bed->chromEnd);
	}
    }
}

void extractMafs(char *file, FILE *f, 
	struct hash *regionHash, struct hash *speciesHash, struct hash *statsHash)
/* extract MAFs in a file from regions specified in hash */
{
char *chrom = NULL, *thisChrom;
struct bedHead *bHead = NULL;
struct mafFile *mf = mafOpen(file);
struct mafAli *maf = NULL;

verbose(1, "extracting from %s\n", file);
for(maf = mafNext(mf); maf; mafAliFree(&maf), maf = mafNext(mf) )
    {
    struct mafComp *mc = maf->components;

    thisChrom = chromFromSrc(mc->src);
    if ((chrom == NULL) || (!sameString(chrom, thisChrom)))
	{
	if (chrom)
	    {
	    if (bHead)
		outItems(f, bHead, speciesHash);
	    clearBreaks(speciesHash);
	    }
	bHead = hashFindVal(regionHash, thisChrom);
	chrom = cloneString(thisChrom);
	}

    if (bHead)
	recordBreaks(speciesHash, maf);
    }

if (bHead)
    outItems(f, bHead, speciesHash);

clearBreaks(speciesHash);
mafFileFree(&mf);
}

void mafBreak(char *regionFile, char *speciesFile, char *out, int mafCount, char *mafFiles[])
/* Extract MAFs in regions listed in regin file */
{
int i = 0;
struct hash *bedHash = NULL;
struct hash *speciesHash = NULL;
struct hash *statsHash = newHash(5);
FILE *f = NULL;

verbose(1, "Extracting from %d files to %s\n", mafCount, out);
bedHash = loadRegions(regionFile);
speciesHash = loadSpecies(speciesFile);

f = startOutFile(out);

for (i = 0; i < mafCount; i++)
    extractMafs(mafFiles[i], f, bedHash, speciesHash, statsHash);

endOutFile(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
outDir = optionExists("outDir");
ignoreDups = optionExists("ignoreDups");
if (argc < 5)
    usage();
mafBreak(argv[1], argv[2], argv[3], argc - 4, &argv[4]);
return 0;
}
