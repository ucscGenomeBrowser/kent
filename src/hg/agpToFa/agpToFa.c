/* agpToFa - Convert a .agp file to a .fa file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "fa.h"
#include "agpFrag.h"
#include "agpGap.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "agpToFa - Convert a .agp file to a .fa file\n"
  "usage:\n"
  "   agpToFa in.agp agpSeq out.fa seqDir\n"
  "Where agpSeq matches a sequence name in in.agp\n"
  "and seqDirs are where program looks for sequence\n"
  );
}

void agpToFa(char *agpFile, char *agpSeq, char *faOut, char *seqDir)
/* agpToFa - Convert a .agp file to a .fa file. */
{
struct lineFile *lf = lineFileOpen(agpFile, TRUE);
char *line, *words[16];
int lineSize, wordCount;
int lastPos = 0;
struct agpFrag *agpList = NULL, *agp;
DNA *dna;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#' || line[0] == '\n')
        continue;
    wordCount = chopLine(line, words);
    if (wordCount < 5)
        errAbort("Bad line %d of %s\n", lf->lineIx, lf->fileName);
    if (words[4][0] == 'N')
        errAbort("Sorry, can't handle gap lines yet (line %d of %s)\n", lf->lineIx, lf->fileName);
    lineFileExpectWords(lf, 9, wordCount);
    if (sameWord(words[0], agpSeq))
	{
	agp = agpFragLoad(words);
	if (agp->chromStart != lastPos)
	    errAbort("Start doesn't match previous end line %d of %s\n", lf->lineIx, lf->fileName);
	if (agp->chromEnd - agp->chromStart != agp->fragEnd - agp->fragStart)
	    errAbort("Sizes don't match in %s and %s line %d of %s\n",
	    	agp->chrom, agp->frag, lf->lineIx, lf->fileName);
	slAddHead(&agpList, agp);
	lastPos = agp->chromEnd;
	}
    }
slReverse(&agpList);
if (lastPos == 0)
    errAbort("%s not found in %s\n", agpSeq, agpFile);
dna = needLargeMem(lastPos+1);
memset(dna, 'n', lastPos);
dna[lastPos] = 0;
for (agp = agpList; agp != NULL; agp = agp->next)
    {
    char clone[128];
    char path[512];
    struct dnaSeq *seq;
    int size;
    strcpy(clone, agp->frag);
    chopSuffix(clone);
    sprintf(path, "%s/%s.fa", seqDir, clone);
    seq = faReadAllDna(path);
    if (slCount(seq) != 1)
        errAbort("Can only handle exactly one clone in %s.", path);
    size = agp->fragEnd - agp->fragStart;
    if (agp->strand[0] == '-')
	reverseComplement(seq->dna + agp->fragStart, size);
    memcpy(dna + agp->chromStart, seq->dna + agp->fragStart, size);
    freeDnaSeq(&seq);
    }
printf("Writing %d bases to %s\n", lastPos, faOut);
faWrite(faOut, agpSeq, dna, lastPos);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 5)
    usage();
agpToFa(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
