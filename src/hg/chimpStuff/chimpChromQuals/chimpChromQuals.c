/* chimpChromQuals - Map chimp quality scores from contig to supercontig.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "qaSeq.h"
#include "rle.h"
#include "agpFrag.h"

static char const rcsid[] = "$Id: chimpChromQuals.c,v 1.1 2004/01/31 01:57:23 kate Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chimpChromQuals - Map chimp quality scores from scaffold to chromosome.\n"
  "usage:\n"
  "   chimpChromQuals scaffoldToChrom.agp scaffolds.qac chrom.qac\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct qac
/* A run-length-compressed set of quality scores. */
    {
    int uncSize;		/* Uncompressed size. */
    int compSize;		/* Compressed size. */
    signed char data[1];	/* Compressed data. */
    };

struct hash *qacReadToHash(char *fileName)
/* Read in a qac file into a hash of qacs keyed by name. */
/* TODO: share code with chimpSuperQuals */
{
boolean isSwapped;
FILE *f = qacOpenVerify(fileName, &isSwapped);
bits32 compSize, uncSize;
struct qac *qac;
char *name;
struct hash *hash = newHash(18);
int count = 0;

for (;;)
    {
    name = readString(f);
    if (name == NULL)
       break;
    mustReadOne(f, uncSize);
    if (isSwapped)
	uncSize = byteSwap32(uncSize);
    mustReadOne(f, compSize);
    if (isSwapped)
	compSize = byteSwap32(compSize);
    qac = needHugeMem(sizeof(*qac) + compSize - 1);
    qac->uncSize = uncSize;
    qac->compSize = compSize;
    mustRead(f, qac->data, compSize);
    hashAdd(hash, name, qac);
    ++count;
    }
carefulClose(&f);
printf("Read %d qacs from %s\n", count, fileName);
return hash;
}

struct chrom
/* A list of agpFrags. */
    {
    struct chrom *next;
    int size;			/* Max size of chrom. */
    struct agpFrag *list;	/* List of fragments in chrom. */
    };

struct chrom *readChromScaffoldsFromAgp(char *fileName)
/* Read in agp file and return as list of chroms. */
{
struct hash *chromHash = newHash(17);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[9];
int wordCount;
struct chrom *chromList = NULL, *chrom;
struct agpFrag *frag;
int size;

for (;;)
    {
    wordCount = lineFileChop(lf, row);
    if (wordCount <= 0)
        break;
    if (wordCount < 8)
	lineFileShort(lf);
    if (row[4][0] == 'N')
        continue;
    if (wordCount < 9)
        lineFileShort(lf);
    frag = agpFragLoad(row);
    frag->chromStart -= 1;
    frag->fragStart -= 1;
    size = frag->fragEnd - frag->fragStart;
    if (size != frag->chromEnd - frag->chromStart)
        errAbort("chrom/scaffold size mismatch line %d of %s",
                                  lf->lineIx, lf->fileName);
    /* Ignore for now
    if (frag->strand[0] != '+')
        errAbort("Strand not + line %d of %s", lf->lineIx, lf->fileName);
        */
    chrom = hashFindVal(chromHash, frag->chrom);
    if (chrom == NULL)
        {
	AllocVar(chrom);
	hashAdd(chromHash, frag->chrom, chrom);
	slAddHead(&chromList, chrom);
	}
    slAddHead(&chrom->list, frag);
    if (frag->chromEnd > chrom->size)
        chrom->size = frag->chromEnd;
    }
slReverse(&chromList);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    slReverse(&chrom->list);
printf("Got %d chrom in %s\n", slCount(chrom), lf->fileName);
lineFileClose(&lf);
hashFree(&chromHash);
return chromList;
}

void chimpChromQuals(char *agpFile, char *qacInName, char *qacOutName)
/* chimpChromQuals - Map chimp quality scores from scaffold to chrom.. */
{
struct hash *qacHash = qacReadToHash(qacInName);
struct chrom *chrom, *chromList = readChromScaffoldsFromAgp(agpFile);
FILE *f = mustOpen(qacOutName, "w");
struct qaSeq qa;
struct agpFrag *frag;
struct qac *qac;
int bufSize = 0;
UBYTE *buf = NULL;
int qaMaxSize = 0;
int fragSize;
int count = 0;

qacWriteHead(f);
ZeroVar(&qa);

for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    /* Set up qa to hold uncompressed quals for whole chrom. */
    qa.name = chrom->list->chrom;
    qa.size = chrom->size;
    if (qaMaxSize < qa.size)
	{
	freez(&qa.qa);
	qa.qa = needHugeZeroedMem(qa.size);
	qaMaxSize = qa.size;
	}

    /* Uncompress contig quality scores and copy into chrom's quality buffer. */
    for (frag = chrom->list; frag != NULL; frag = frag->next)
        {
	qac = hashMustFindVal(qacHash, frag->frag);
	if (bufSize < qac->uncSize)
	    {
	    freez(&buf);
	    bufSize = qac->uncSize;
	    buf = needMem(bufSize);
	    }
	rleUncompress(qac->data, qac->compSize, buf, qac->uncSize);
	fragSize = frag->fragEnd - frag->fragStart;
	memcpy(qa.qa + frag->chromStart, buf + frag->fragStart, fragSize);
	}

    /* Compress and write it out. */
    qacWriteNext(f, &qa);
    ++count;
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
chimpChromQuals(argv[1], argv[2], argv[3]);
return 0;
}
