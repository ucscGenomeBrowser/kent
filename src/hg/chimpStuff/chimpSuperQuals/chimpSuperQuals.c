/* chimpSuperQuals - Map chimp quality scores from contig to supercontig.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "qaSeq.h"
#include "rle.h"
#include "agpFrag.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "chimpSuperQuals - Map chimp quality scores from contig to supercontig.\n"
  "usage:\n"
  "   chimpSuperQuals contigToSupercontig.agp contigs.qac  supercontigs.qac\n"
  "options:\n"
  "   -xxx=XXX\n"
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

struct scaffold
/* A list of agpFrags. */
    {
    struct scaffold *next;
    int size;			/* Max size of scaffold. */
    struct agpFrag *list;	/* List of fragments in scaffold. */
    };

struct scaffold *readScaffoldsFromAgp(char *fileName)
/* Read in agp file and return as list of scaffolds. */
{
struct hash *scaffoldHash = newHash(17);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[9];
int wordCount;
struct scaffold *scaffoldList = NULL, *scaffold;
struct agpFrag *frag;
int size;

for (;;)
    {
    wordCount = lineFileChop(lf, row);
    if (wordCount <= 0)
        break;
    if (wordCount < 8)
	lineFileShort(lf);
    if (row[4][0] == 'N' || row[4][0] == 'U')
        continue;
    if (wordCount < 9)
        lineFileShort(lf);
    frag = agpFragLoad(row);
    frag->chromStart -= 1;
    frag->fragStart -= 1;
    size = frag->fragEnd - frag->fragStart;
    if (size != frag->chromEnd - frag->chromStart)
        errAbort("scaffold/contig size mismatch line %d of %s", lf->lineIx, lf->fileName);
    if (frag->strand[0] != '+')
        errAbort("Strand not + line %d of %s", lf->lineIx, lf->fileName);
    scaffold = hashFindVal(scaffoldHash, frag->chrom);
    if (scaffold == NULL)
        {
	AllocVar(scaffold);
	hashAdd(scaffoldHash, frag->chrom, scaffold);
	slAddHead(&scaffoldList, scaffold);
	}
    slAddHead(&scaffold->list, frag);
    if (frag->chromEnd > scaffold->size)
        scaffold->size = frag->chromEnd;
    }
slReverse(&scaffoldList);
for (scaffold = scaffoldList; scaffold != NULL; scaffold = scaffold->next)
    slReverse(&scaffold->list);
printf("Got %d scaffolds in %s\n", slCount(scaffoldList), lf->fileName);
lineFileClose(&lf);
hashFree(&scaffoldHash);
return scaffoldList;
}

void chimpSuperQuals(char *agpFile, char *qacInName, char *qacOutName)
/* chimpSuperQuals - Map chimp quality scores from contig to supercontig.. */
{
struct hash *qacHash = qacReadToHash(qacInName);
struct scaffold *scaffold, *scaffoldList = readScaffoldsFromAgp(agpFile);
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

for (scaffold = scaffoldList; scaffold != NULL; scaffold = scaffold->next)
    {
    /* Set up qa to hold uncompressed quals for whole scaffold. */
    qa.name = scaffold->list->chrom;
    qa.size = scaffold->size;
    if (qaMaxSize < qa.size)
	{
	freez(&qa.qa);
	qa.qa = needHugeZeroedMem(qa.size);
	qaMaxSize = qa.size;
	}

    /* Uncompress contig quality scores and copy into scaffold's quality buffer. */
    for (frag = scaffold->list; frag != NULL; frag = frag->next)
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
chimpSuperQuals(argv[1], argv[2], argv[3]);
return 0;
}
