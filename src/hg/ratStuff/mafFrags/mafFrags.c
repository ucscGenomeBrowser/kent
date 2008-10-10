/* mafFrags - Collect MAFs from regions specified in a 6 column bed file. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "maf.h"
#include "hdb.h"
#include "hgMaf.h"


static char const rcsid[] = "$Id: mafFrags.c,v 1.8 2008/10/10 06:02:24 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafFrags - Collect MAFs from regions specified in a 6 column bed file\n"
  "usage:\n"
  "   mafFrags database track in.bed out.maf\n"
  "options:\n"
  "   -orgs=org.txt - File with list of databases/organisms in order\n"
  "   -bed12 - If set, in.bed is a bed 12 file, including exons\n"
  "   -thickOnly - Only extract subset between thickStart/thickEnd\n"
  "   -meFirst - Put native sequence first in maf\n"
  "   -txStarts - Add MAF txstart region definitions ('r' lines) using BED name\n"
  "    and output actual reference genome coordinates in MAF.\n"
  );
}

static struct optionSpec options[] = {
   {"orgs", OPTION_STRING},
   {"bed12", OPTION_BOOLEAN},
   {"thickOnly", OPTION_BOOLEAN},
   {"meFirst", OPTION_BOOLEAN},
   {"txStarts", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean bed12 = FALSE;
boolean thickOnly = FALSE;
boolean meFirst = FALSE;
boolean txStarts = FALSE;

struct mafAli *mafFromBed12(char *database, char *track, struct bed *bed, 
	struct slName *orgList)
/* Construct a maf out of exons in bed. */
{
/* Loop through all block in bed, collecting a list of mafs, one
 * for each block.  While we're at make a hash of all species seen. */
struct hash *speciesHash = hashNew(0);
struct mafAli *mafList = NULL, *maf, *bigMaf;
struct mafComp *comp, *bigComp;
int totalTextSize = 0;
int i;
for (i=0; i<bed->blockCount; ++i)
    {
    int start = bed->chromStart + bed->chromStarts[i];
    int end = start + bed->blockSizes[i];
    if (thickOnly)
        {
	start = max(start, bed->thickStart);
	end = min(end, bed->thickEnd);
	}
    if (start < end)
        {
	maf = hgMafFrag(database, track, bed->chrom, start, end, '+',
	   database, NULL);
	slAddHead(&mafList, maf);
	for (comp = maf->components; comp != NULL; comp = comp->next)
	    hashStore(speciesHash, comp->src);
	totalTextSize += maf->textSize; 
	}
    }
slReverse(&mafList);

/* Add species in order list too */
struct slName *org;
for (org = orgList; org != NULL; org = org->next)
    hashStore(speciesHash, org->name);

/* Allocate memory for return maf that contains all blocks concatenated together. 
 * Also fill in components with any species seen at all. */
AllocVar(bigMaf);
bigMaf->textSize = totalTextSize;
struct hashCookie it = hashFirst(speciesHash);
struct hashEl *hel;
while ((hel = hashNext(&it)) != NULL)
    {
    AllocVar(bigComp);
    bigComp->src = cloneString(hel->name);
    bigComp->text = needLargeMem(totalTextSize + 1);
    memset(bigComp->text, '.', totalTextSize);
    bigComp->text[totalTextSize] = 0;
    bigComp->strand = '+';
    bigComp->srcSize = totalTextSize;	/* It's safe if a bit of a lie. */
    hel->val = bigComp;
    slAddHead(&bigMaf->components, bigComp);
    }

/* Loop through maf list copying in data. */
int textOffset = 0;
for (maf = mafList; maf != NULL; maf = maf->next)
    {
    for (comp = maf->components; comp != NULL; comp = comp->next)
        {
	bigComp = hashMustFindVal(speciesHash, comp->src);
	memcpy(bigComp->text + textOffset, comp->text, maf->textSize);
	bigComp->size += comp->size;
	}
    textOffset += maf->textSize;
    }

/* Cope with strand of darkness. */
if (bed->strand[0] == '-')
    {
    for (comp = bigMaf->components; comp != NULL; comp = comp->next)
	reverseComplement(comp->text, bigMaf->textSize);
    }

/* If got an order list then reorder components according to it. */
if (orgList != NULL)
    {
    struct mafComp *newList = NULL;
    for (org = orgList; org != NULL; org = org->next)
        {
	comp = hashMustFindVal(speciesHash, org->name);
	slAddHead(&newList, comp);
	}
    slReverse(&newList);
    bigMaf->components = newList;
    }

/* Rename our own component to bed name */
comp = hashMustFindVal(speciesHash, database);
freeMem(comp->src);
comp->src = cloneString(bed->name);


/* Clean up and go home. */
hashFree(&speciesHash);
mafAliFreeList(&mafList);
return bigMaf;
}

void moveMeToFirst(struct mafAli *maf, char *myName)
/* Find component matching myName, and move it to first. */
{
struct mafComp *comp;
for (comp = maf->components; comp != NULL; comp = comp->next)
    if (sameString(comp->src, myName))
        break;
assert(comp != NULL);
slRemoveEl(&maf->components, comp);
slAddHead(&maf->components, comp);
}

static void processBed6(char *database, char *track, FILE *f, struct bed *bed,
                        struct slName *orgList)
/* generate MAF alignment for a bed6 */
{
struct mafAli *maf; 
if (txStarts)
    {
    maf = hgMafFrag(database, track, 
                    bed->chrom, bed->chromStart, bed->chromEnd, bed->strand[0],
                    NULL, orgList);
    maf->regDef = mafRegDefNew(mafRegDefTxUpstream,
                               bed->chromEnd-bed->chromStart,
                               bed->name);
    if (meFirst)
        moveMeToFirst(maf, database);
    }
else
    {
    maf = hgMafFrag(database, track, 
                    bed->chrom, bed->chromStart, bed->chromEnd, bed->strand[0],
                    bed->name, orgList);
    if (meFirst)
        moveMeToFirst(maf, bed->name);
    }
mafWrite(f, maf);
mafAliFree(&maf);
} 

void mafFrags(char *database, char *track, char *bedFile, char *mafFile)
/* mafFrags - Collect MAFs from regions specified in a 6 column bed file. */
{
struct slName *orgList = NULL;
struct lineFile *lf = lineFileOpen(bedFile, TRUE);
FILE *f = mustOpen(mafFile, "w");

if (optionExists("orgs"))
    {
    char *orgFile = optionVal("orgs", NULL);
    char *buf;
    readInGulp(orgFile, &buf, NULL);
    orgList = stringToSlNames(buf);

    /* Ensure that org list starts with database. */
    struct slName *me = slNameFind(orgList, database);
    if (me == NULL)
        errAbort("Need to have reference database '%s' in %s", database, orgFile);
    if (me != orgList)
        {
	slRemoveEl(&orgList, me);
	slAddHead(&orgList, me);
	}
    }
mafWriteStart(f, "zero");

if (bed12)
    {
    char *row[12];
    while (lineFileRow(lf, row))
	{
	struct bed *bed = bedLoadN(row, ArraySize(row));
	struct mafAli *maf = mafFromBed12(database, track, bed, orgList);
	if (meFirst)
	    moveMeToFirst(maf, bed->name);
	mafWrite(f, maf);
	mafAliFree(&maf);
	bedFree(&bed);
	}
    }
else
    {
    char *row[6];
    while (lineFileRow(lf, row))
	{
	struct bed *bed = bedLoadN(row, ArraySize(row));
        processBed6(database, track, f, bed, orgList);
	bedFree(&bed);
	}
    }
mafWriteEnd(f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
bed12 = optionExists("bed12");
thickOnly = optionExists("thickOnly");
meFirst = optionExists("meFirst");
txStarts = optionExists("txStarts");
mafFrags(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
