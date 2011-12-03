/* spanRaft - combine James Gilbert's phrap derived
 * .span files with ooGreedy raft files ease comparison. */

#include "common.h"
#include "hash.h"
#include "linefile.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
"spanRaft - combine James Gilbert's phrap derived\n"
".span files with ooGreedy raft files ease comparison.\n"
"usage\n"
"    spanRaft spanFile raftFile outFile");
}

struct raft
/* Info on an ooGreedy raft. */
    {
    struct raft *next;	/* Next in list. */
    char *name;
    int baseCount;
    int fragCount;
    int defaultPos;				
    struct raftFrag *rfList;	/* List of frags. */
    };

struct raftFrag
/* Info on a fragment inside of a raft. */
    {
    struct raftFrag *next;
    char *name;
    int pos;
    int size;
    char strand;
    struct spanFrag *sf;
    struct raft *raft;
    };
	
struct raft *readRaftFile(char *fileName)
/* Read in a raft file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct raft *raftList = NULL, *raft;
struct raftFrag *rf;
int lineSize, wordCount;
char *line, *words[16];

if (!lineFileNext(lf, &line, &lineSize))
	errAbort("%s is empty", fileName);
if (!startsWith("ooGreedy version", line))
	errAbort("%s isn't a raft file", fileName);

while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    if (wordCount == 0)
	    {
	    raft = NULL;
	    continue;
	    }
    if (wordCount >= 9 && sameString(words[1], "raft"))
	    {
	    AllocVar(raft);
	    raft->name = cloneString(words[0]);
	    raft->baseCount = atoi(words[2]);
	    raft->fragCount = atoi(words[4]);
	    raft->defaultPos = atoi(words[6]);
	    slAddHead(&raftList, raft);
	    }
    else if (wordCount == 4 || wordCount == 6)
	    {
	    AllocVar(rf);
	    rf->name = cloneString(words[2]);
	    rf->pos = atoi(words[0]);
	    rf->strand = words[1][0];
	    rf->size = atoi(words[3]);
	    rf->raft = raft;
	    slAddTail(&raft->rfList, rf);
	    }
    else
	    errAbort("Bad line %d of %s", lf->lineIx, lf->fileName);
    }
lineFileClose(&lf);
slReverse(&raftList);
return raftList;
}


struct span
/* Info on one span (phrapp contig) */
    {
    struct span *next;
    char *name;
    struct spanFrag *sfList;
    int fragCount;
    };

struct spanFrag
/* Info on one span fragment. */
    {
    struct spanFrag *next;
    int fragIx;
    int spanStart;
    int spanEnd;
    char strand;
    char *name;
    int fragStart;
    int fragEnd;
    struct span *span;
    struct raftFrag *rf;
    };

struct span *readSpanFile(char *fileName)
/* Read in a .span file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct span *spanList = NULL, *span;
struct spanFrag *sf;
int lineSize, wordCount;
char *line, *words[16];

while (lineFileNext(lf, &line, &lineSize))
	{
	wordCount = chopLine(line, words);
	if (wordCount == 0)
		{
		span = NULL;
		continue;
		}
	if (wordCount == 1)
		{
		AllocVar(span);
		span->name = cloneString(words[0]);
		slAddHead(&spanList, span);
		}
	else if (wordCount == 6)
		{
		AllocVar(sf);
		sf->fragIx = ++span->fragCount;
		sf->spanStart = atoi(words[0])-1;
		sf->spanEnd = atoi(words[1]);
		sf->strand = words[2][0];
		sf->name = cloneString(words[3]);
		sf->fragStart = atoi(words[4])-1;
		sf->fragEnd = atoi(words[5]);
		sf->span = span;
		slAddTail(&span->sfList, sf);
		}
	else
		errAbort("Bad line %d of %s", lf->lineIx, lf->fileName);
	}
lineFileClose(&lf);
slReverse(&spanList);
return spanList;
}

int multiRaftCount(struct raft *raftList)
/* Count up non-singleton rafts. */
{
int count = 0;
struct raft *raft;

for (raft = raftList; raft != NULL; raft = raft->next)
	if (raft->fragCount > 1)
		++count;
return count;
}

void matchFrags(struct raft *raftList, struct span *spanList, 
     struct hash *fragHash)
/* Make hash of raftFrags and fill in raftFrag->span elements. */
{
struct raft *raft;
struct raftFrag *rf;
struct span *span;
struct spanFrag *sf;

for (raft = raftList; raft != NULL; raft = raft->next)
    for (rf = raft->rfList; rf != NULL; rf = rf->next)
	hashAdd(fragHash, rf->name, rf);
for (span = spanList; span != NULL; span = span->next)
    {
    for (sf = span->sfList; sf != NULL; sf = sf->next)
	{
	rf = hashMustFindVal(fragHash, sf->name);
	rf->sf = sf;
	sf->rf = rf;
	}
    }
}

void writeRaftFrags(struct raft *raft, FILE *f, char *prefix)
/* Write out frags for one raft. */
{
struct raftFrag *rf;
struct spanFrag *sf;

fprintf(f, "%s%s raft, %d bases, %d frags, %d default pos\n",
    prefix, raft->name, raft->baseCount, 
    raft->fragCount, raft->defaultPos);
for (rf = raft->rfList; rf != NULL; rf = rf->next)
    {
    fprintf(f, "%6d %c %16s %6d",
	rf->pos, rf->strand, rf->name, rf->size);
    if ((sf = rf->sf) != NULL)
	{
	fprintf(f, "%18s %c %3d", sf->span->name, 
	     sf->strand, sf->fragIx);
	}
    fprintf(f, "\n");
    }
fprintf(f, "\n");
}

int cmpRfStart(const void *va, const void *vb)
/* Compare two raftFrag. */
{
const struct raftFrag *a = *((struct raftFrag **)va);
const struct raftFrag *b = *((struct raftFrag **)vb);
return a->pos - b->pos;
}

void reverseRaft(struct raft *raft)
/* Reverse complement raft. */
{
struct raftFrag *rf, *next;

for (rf = raft->rfList; rf != NULL; rf = next)
    {
    next = rf->next;
    rf->pos = raft->baseCount - (rf->pos + rf->size);
    rf->strand = (rf->strand == '+' ? '-' : '+');
    }
slSort(&raft->rfList, cmpRfStart);
}

boolean looksReversed(struct raft *raft)
/* Return TRUE if orientation of raft with respect to span looks
 * reversed. */
{
struct raftFrag *rf;
struct spanFrag *sf;
int orientation = 0;

for (rf = raft->rfList; rf != NULL; rf = rf->next)
    {
    if ((sf = rf->sf) != NULL)
	{
	if (sf->strand == rf->strand)
	    ++orientation;
	else
	    --orientation;
	}
    }
return orientation < 0;
}

void countContainments(struct span *span, int *retCont, boolean *retSingle)
/* Count how many rafts a span is split between. */
{
struct hash *hash = newHash(8);
int count = 0;
boolean isSingle = TRUE;
struct spanFrag *sf;
struct raft *raft;

for (sf = span->sfList; sf != NULL; sf = sf->next)
    {
    raft = sf->rf->raft;
    if (raft->fragCount > 1)
	isSingle = FALSE;
    if (!hashLookup(hash, raft->name))
	{
	hashAdd(hash, raft->name, raft);
	++count;
	}
    }
freeHash(&hash);
*retCont = count;
*retSingle = isSingle;
}

void writeContainments(FILE *f, struct span *spanList)
/* Write out info about how many rafts a contig is split
 * between. */
{
struct spanFrag *sf;
struct span *span;
static int containments[6];
int cont;
boolean isSingle;
int i;

for (span = spanList; span != NULL; span = span->next)
    {
    countContainments(span, &cont, &isSingle);
    if (isSingle && cont == 1)
	cont = 0;
    if (cont >= ArraySize(containments))
	cont = ArraySize(containments)-1;
    ++containments[cont];
    }
fprintf(f, "Containments: ");
for (i = 0; i<ArraySize(containments); ++i)
    fprintf(f, " %2d", containments[i]);
fprintf(f, "\n");
}

void writeFrags(struct raft *raftList, struct span *spanList, char *fileName)
/* Write out rafts to file. */
{
FILE *f = mustOpen(fileName, "w");
struct raft *raft;
struct raftFrag *rf;
char *spanName;
struct spanFrag *sf;
struct span *span;

fprintf(f, "Got %d spans\n", slCount(spanList));
fprintf(f, "Got %d rafts\n", slCount(raftList));
fprintf(f, "Got %d non-singleton rafts\n", multiRaftCount(raftList));
fprintf(f, "\n");
writeContainments(f, spanList);
for (raft = raftList; raft != NULL; raft = raft->next)
    {
    if (looksReversed(raft))
	{
	reverseRaft(raft);
	writeRaftFrags(raft, f, "r");
	reverseRaft(raft);
	}
    else
	writeRaftFrags(raft, f, "f");
    fprintf(f, "\n");
    }
fclose(f);
}

void spanRaft(char *spanFile, char *raftFile, char *outFile)
/* spanRaft - combine James Gilbert's phrap derived
 * .span files with ooGreedy raft files ease comparison. */
{
struct span *spanList = NULL, *span;
struct spanFrag *sf;
struct raft *raftList = NULL, *raft;
struct raftFrag *rf;
struct hash *fragHash = newHash(0);

spanList = readSpanFile(spanFile);
printf("Got %d spans\n", slCount(spanList));
raftList = readRaftFile(raftFile);
printf("Got %d rafts\n", slCount(raftList));
printf("Got %d non-singleton rafts\n", multiRaftCount(raftList));
matchFrags(raftList, spanList, fragHash);
uglyf("Matched frags\n");
writeFrags(raftList, spanList, outFile);
uglyf("All done\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
spanRaft(argv[1], argv[2], argv[3]);
return 0;
}

