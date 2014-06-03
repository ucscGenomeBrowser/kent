/* mafOrtholog - find orthlogs in other species based on maf alignment and reference genePred */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "maf.h"
#include "hdb.h"
#include "hgMaf.h"
#include "genePred.h"
#include "genePredReader.h"



char *nibDir;
char *geneTable = "refGene";
char *altTable = "knownGene";
struct hash *geneListHash = NULL ; /* hash of list of genes in each species */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafOrtholog - find orthlogs in other species based on maf alignment and reference genePred \n"
  "usage:\n"
  "   mafOrtholog database mafTable in.gp out.tab\n"
  "options:\n"
  "   -table=table - name of table to find orthlogs in other species [default: refGene]\n"
  "   -alt=table - name of alternate table to find orthlogs in other species if table above does not exist [default: knownGene]\n"
  "   -orgs=org.txt - File with list of databases/organisms in order\n"
  "   -thickOnly - Only extract subset between thickStart/thickEnd\n"
  "   -meFirst - Put native sequence first in maf\n"
  );
}

static struct optionSpec options[] = {
   {"orgs", OPTION_STRING},
   {"nibDir", OPTION_STRING},
   {"table", OPTION_STRING},
   {"alt", OPTION_STRING},
   {"thickOnly", OPTION_BOOLEAN},
   {"meFirst", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean thickOnly = FALSE;
boolean meFirst = FALSE;

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

struct genePred *getOverlappingGeneDb(struct genePred **list, char *table, char *chrom, int cStart, int cEnd, char *name, int *retOverlap, char *db)
{
/* read all genes from a table find the gene with the biggest overlap. 
   Cache the list of genes to so we only read it once */

struct genePred *el = NULL, *bestMatch = NULL, *gp = NULL;
int overlap = 0 , bestOverlap = 0, i;
int *eFrames;

if (list == NULL)
    return NULL;

if (*list == NULL)
    {
    struct genePred *gpList = NULL;
    struct sqlConnection *conn = sqlConnect(db);
    struct genePredReader *gpr = NULL;
    if (!hTableExistsDb(db,table))
        table = altTable;
    if (!hTableExistsDb(db,table))
        {
        verbose(2,"no table %s in %s\n",table, db);
        return NULL;
        }
    gpr = genePredReaderQuery(conn, table, NULL);
    verbose(1,"Loading Predictions from %s in %s\n",table, db);
    gpList = genePredReaderAll(gpr);
    if (gpList != NULL)
        {
        hashAdd(geneListHash, db, gpList);
        *list = gpList;
        }
    sqlDisconnect(&conn);
    }
for (el = *list; el != NULL; el = el->next)
    {
    if (chrom != NULL && el->chrom != NULL)
        {
        overlap = 0;
        if ( sameString(chrom, el->chrom))
            {
            for (i = 0 ; i<(el->exonCount); i++)
                {
                overlap += positiveRangeIntersection(cStart,cEnd, el->exonStarts[i], el->exonEnds[i]) ;
                }
            if (overlap > 20 && sameString(name, el->name))
                {
                bestMatch = el;
                bestOverlap = overlap;
                *retOverlap = bestOverlap;
                }
            if (overlap > bestOverlap)
                {
                bestMatch = el;
                bestOverlap = overlap;
                *retOverlap = bestOverlap;
                }
            }
        }
    }
if (bestMatch != NULL)
    {
    /* Allocate genePred and fill in values. */
    AllocVar(gp);
    gp->name = cloneString(bestMatch->name);
    gp->chrom = cloneString(bestMatch->chrom);
    gp->strand[1] = bestMatch->strand[1];
    gp->strand[0] = bestMatch->strand[0];
    gp->txStart = bestMatch->txStart;
    gp->txEnd = bestMatch->txEnd;
    gp->cdsStart = bestMatch->cdsStart;
    gp->cdsEnd = bestMatch->cdsEnd;
    gp->exonCount = bestMatch->exonCount;
    AllocArray(gp->exonStarts, bestMatch->exonCount);
    AllocArray(gp->exonEnds, bestMatch->exonCount);
    for (i=0; i<bestMatch->exonCount; ++i)
        {
        gp->exonStarts[i] = bestMatch->exonStarts[i] ;
        gp->exonEnds[i] = bestMatch->exonEnds[i] ;
        }
    gp->optFields = bestMatch->optFields;
    gp->id = bestMatch->id;

    if (bestMatch->optFields & genePredName2Fld)
        gp->name2 = cloneString(bestMatch->name2);
    else
        gp->name2 = NULL;
    if (bestMatch->optFields & genePredCdsStatFld)
        {
        gp->cdsStartStat = bestMatch->cdsStartStat;
        gp->cdsEndStat = bestMatch->cdsEndStat;
        }
    if (bestMatch->optFields & genePredExonFramesFld)
        {
        gp->exonFrames = AllocArray(eFrames, bestMatch->exonCount);
        for (i = 0; i < bestMatch->exonCount; i++)
            gp->exonFrames[i] = bestMatch->exonFrames[i];
        }
    eFrames = gp->exonFrames;
    }

return gp;
}
void printOrthologs(FILE *f, struct mafAli *maf, struct genePred *gp)
{
struct mafComp *c;

fprintf(f, "%s", gp->name);
if (maf == NULL || maf->components == NULL)
    {
    fprintf(f,"\n");
    return;
    }

if (maf->components->next == NULL)
    {
    fprintf(f,"\n");
    return;
    }

for (c = maf->components->next; c != NULL ; c=c->next)
    {
    int end = c->start + c->size;
    int geneOverlap = 0;
    char *src[20];
    struct genePred *og = NULL;
    int wordCount;
    struct genePred *geneList = NULL;
    wordCount = chopString(c->src, ".", src, ArraySize(src));
    verbose(3, "searching in %s at chrom %s %d %d, %s\n",src[0], src[1], c->start, end, gp->name);
    assert(wordCount == 2);
    geneList = (struct genePred *)hashFindVal(geneListHash, src[0]);
    og = getOverlappingGeneDb(&geneList, geneTable, src[1], c->start, 
        end , gp->name, &geneOverlap, src[0]);
    if (og != NULL)
        fprintf(f,"\t%s\t%s\t%s\t%d\t%d",c->src, og->name, og->chrom, og->txStart, og->txEnd);
    }
    fprintf(f,"\n");
}

void mafOrtholog(char *database, char *track, char *genePredFile, char *outFile)
/* mafOrtholog - find orthlogs in other species based on maf alignment and reference genePred */
{
struct slName *orgList = NULL;
FILE *f = mustOpen(outFile, "w");
struct genePredReader *gpr = genePredReaderFile(genePredFile, NULL);
struct genePred *gpList = genePredReaderAll(gpr), *gp = NULL;
struct sqlConnection *conn = hAllocConn();

if (optionExists("nibDir"))
    nibDir = optionVal("nibDir", NULL);
if (optionExists("orgs"))
    {
    char *orgFile = optionVal("orgs", NULL);
    char *buf;
    readInGulp(orgFile, &buf, NULL);
    orgList = stringToSlNames(buf);
    }

for (gp = gpList ; gp != NULL ; gp=gp->next)
    {
    struct mafAli *maf = NULL;
    if (thickOnly)
        maf = mafLoadInRegion(conn, track,
        gp->chrom, gp->cdsStart, gp->cdsEnd);
    else
        maf = mafLoadInRegion(conn, track,
        gp->chrom, gp->txStart, gp->txEnd);
    if (meFirst)
        moveMeToFirst(maf, database);
    printOrthologs(f, maf, gp);
    mafAliFree(&maf);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
hSetDb(argv[1]);
geneListHash = hashNew(0);
geneTable = optionVal("table", geneTable);
altTable = optionVal("alt", altTable);
thickOnly = optionExists("thickOnly");
meFirst = optionExists("meFirst");
mafOrtholog(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
