/* pslCoverage - estimate coverage from alignments.
 */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "psl.h"


void usage()
/* Print usage instructions and exit. */
{
errAbort(
    "pslCoverage - estimate coverage from alignments."
    "usage:\n"
    "    pslCoverage in.sizes in.psl minPercentId endTrim out.cov misAsm.out");
}

struct probe
/* Keep track of input sequence. */
    {
    struct probe *next;/* Next in list. */
    char *name;        /* Name of seq - not allocated here. */
    int size;          /* Size of seq. */
    int aliSize;       /* Size of aligning part of sequence. */
    boolean disordered; /* TRUE if exons not in order in best alignment. */
    struct psl *bestPsl; /* Points to best PSL if any. */
    };

void readProbeList(char *fileName, struct probe **retList, struct hash **retHash)
/* Read in sequence list from file.  (Set aliSize field to zero). */
{
struct hash *hash = newHash(0);
struct hashEl *hel;
struct probe *list = NULL, *el;
char *words[4];
int wordCount;
struct lineFile *lf = lineFileOpen(fileName, TRUE);

while ((wordCount = lineFileChop(lf, words)) > 0)
    {
    lineFileExpectWords(lf, 2, wordCount);
    AllocVar(el);
    hel = hashAdd(hash, words[0], el);
    el->name = hel->name;
    el->size = atoi(words[1]);
    slAddHead(&list, el);
    }
slReverse(&list);
lineFileClose(&lf);
*retList = list;
*retHash = hash;
}

int pslAliArea(struct psl *psl)
/* Return sum of all block sizes. */
{
int i, blockCount = psl->blockCount;
unsigned *blockSizes = psl->blockSizes;
int total = 0;

for (i=0; i<blockCount; ++i)
   total += blockSizes[i];
return total;
}

int calcMilliScore(struct psl *psl)
/* Figure out percentage score. */
{
int milliScore = psl->match + psl->repMatch - psl->misMatch - 
	2*psl->qNumInsert;

milliScore *= 1000;
milliScore /= (pslAliArea(psl));
// uglyf("%s %d mismatch, %d tInsert, %d match, %d milliScore\n",
	// psl->qName, psl->misMatch, psl->qNumInsert, psl->match + psl->repMatch, milliScore);
return milliScore;
}



int setScoreTrack(int *scoreTrack, int qSize, int milliScore, struct psl *psl)
/* Set bases in psl to score.  Return number of bases in psl that are in aligning
 * blocks. */
{
int bases = 0;
int blockIx;
int i, start, end, size;

for (blockIx = 0; blockIx < psl->blockCount; ++blockIx)
    {
    size = psl->blockSizes[blockIx];
    start = psl->qStarts[blockIx];
    end = start+size;
    if (psl->strand[0] == '-')
	{
	int s, e;
	s = psl->qSize - end;
	e = psl->qSize - start;
	start = s;
	end = e;
	}
    if (start < 0 || end > qSize)
	{
	warn("Odd: qName %s tName %s qSize %d psl->qSize %d start %d end %d",
	    psl->qName, psl->tName, qSize, psl->qSize, start, end);
	if (start < 0)
	    start = 0;
	if (end > qSize)
	    end = qSize;
	}
    bases += end - start;
    for (i=start; i<end; ++i)
	{
	if (milliScore > scoreTrack[i])
	    scoreTrack[i] = milliScore;
	}
    }
return bases;
}

int notFoundCount = 0;

void doOneAcc(char *acc, struct psl *pslList, int threshold, int trimSize, 
	struct hash *probeHash, FILE *misAsm)
/* Process alignments of one piece of mRNA. */
{
struct psl *psl;
int qSize;
int *scoreTrack = NULL;
int milliScore;
int goodAliCount = 0;
int i, start, end, size;
struct probe *probe;
struct psl *longestPsl = NULL;
int longestBases = 0;
int bases;
boolean disordered = FALSE;
int blockIx;

if (pslList == NULL)
    return;
qSize = pslList->qSize;
AllocArray(scoreTrack, qSize+1);

for (psl = pslList; psl != NULL; psl = psl->next)
    {
    bases = 0;
    milliScore = calcMilliScore(psl);
    if (milliScore >= threshold)
	{
	++goodAliCount;
	bases = setScoreTrack(scoreTrack, qSize, milliScore, psl);
	}
    bases *= milliScore;
    if (bases > longestBases)
        {
	longestBases = bases;
	longestPsl = psl;
	}
    }

/* Calculate size of trimmed aligning parts. */
start = trimSize;
end = qSize - trimSize;
size = 0;
for (i=start; i<end; ++i)
    {
    if (scoreTrack[i] > 0)
        ++size;
    }
probe = hashFindVal(probeHash, acc);
if (probe != NULL)
    {
    probe->aliSize = size;

    /* Figure out if have any reversals. */
    if (longestPsl != NULL)
	{
	int exonTrim = 10;     /* Amount to trim from side of exons. */
	probe->bestPsl = longestPsl;
	zeroBytes(scoreTrack, (qSize+1) * sizeof(scoreTrack[0]));
	setScoreTrack(scoreTrack, qSize, 1, longestPsl);
	for (psl = pslList; psl != NULL; psl = psl->next)
	    {
	    if (psl != longestPsl)
		{
		milliScore = calcMilliScore(psl);
		if (milliScore >= threshold)
		    {
		    int missCount = 0;
		    for (blockIx = 0; blockIx < psl->blockCount; ++blockIx)
			{
			size = psl->blockSizes[blockIx];
			start = psl->qStarts[blockIx];
			end = start+size;
			if (psl->strand[0] == '-')
			    {
			    int s, e;
			    s = psl->qSize - end;
			    e = psl->qSize - start;
			    start = s;
			    end = e;
			    }
			start += exonTrim;
			end -= exonTrim;
			if (start < end)
			    {
			    for (i=start; i<end; ++i)
				{
				if (!scoreTrack[i])
				    {
				    ++missCount;
				    }
				}
			    }
			}
		    if (missCount > 0)
			{
			if (!disordered)
			    {
			    fprintf(misAsm, "%s %s:%d-%d(%d%s%d)", psl->qName,
			    	longestPsl->tName, longestPsl->tStart, longestPsl->tEnd,
				longestPsl->qStart, longestPsl->strand, longestPsl->qEnd);
			    disordered = TRUE;
			    }
			fprintf(misAsm, " %s:%d-%d(%d%s%d)", 
			    psl->tName, psl->tStart, psl->tEnd, 
			    psl->qStart, psl->strand, psl->qEnd);
			}
		    }
		}
	    }
	if (disordered)
	    fprintf(misAsm, "\n");
	}
    probe->disordered = disordered;
    }
else
    {
    warn("%s not found in in.lst file", acc);
    notFoundCount += 1;
    }
freeMem(scoreTrack);
}

void summarizeProbeList(struct probe *probeList, int trimSize, double aliRatio, FILE *out,
   FILE *misAsm)
/* Summarize probe list alignments. */
{
int totalBases = 0;
int totalProbes = 0;
int trimmedBases = 0;
int trimmedProbes = 0;
int aliBases = 0;
int aliProbes = 0;
int orderedProbes = 0;
struct probe *probe;
int start,end,size,tSize;

for (probe = probeList; probe != NULL; probe = probe->next)
    {
    size = probe->size;
    totalBases += size;
    totalProbes += 1;
    start = trimSize;
    end = size - trimSize;
    tSize = end - start;
    if (tSize > 0)
        {
	trimmedBases += tSize;
	trimmedProbes += 1;
	if (probe->aliSize > 0)
	    {
	    aliBases += probe->aliSize;
	    aliProbes += 1;
	    if (!probe->disordered)
	       ++orderedProbes;
	    }
	}
    }
printf("%5.2f%% bases align (%d of %d). %5.2f%% probes align (%d of %d).\n",
	100.0*aliBases/(double)trimmedBases, aliBases, trimmedBases,
	100.0*aliProbes/(double)trimmedProbes, aliProbes, trimmedProbes);
printf("%5.2f%% probes survived trimming (%d of %d)\n",
        100.0*trimmedProbes/(double)totalProbes, trimmedProbes, totalProbes);
fprintf(out, "%5.1f%%     %5.2f%%     %5.2f%%    %5.2f%%    %d\n",
	100.0*aliRatio, 
	100.0*aliBases/(double)trimmedBases, 
	100.0*aliProbes/(double)trimmedProbes,
	100.0 - 100.0*orderedProbes/(double)aliProbes, aliProbes);
printf("Not found count %d vs. %d\n", notFoundCount, totalProbes);
#ifdef SOON
#endif /* SOON */
}

void finishList(struct psl **pList)
/* Empty list.  Don't free it since there are references elsewhere. */
{
*pList = NULL;
}

void pslCoverage(char *inLst, char *inPsl, double aliRatio, int trimSize, char *outName,
	char *misAsmName)
/* Analyse inName and put best alignments for eacmRNA in estAliName.
 * Put repeat info in repName. */
{
struct lineFile *in = pslFileOpen(inPsl);
FILE *out = mustOpen(outName, "a");
FILE *misAsm = mustOpen(misAsmName, "w");
struct psl *pslList = NULL, *psl;
char lastName[256];
int threshold = round((1.0 - (1.0 - aliRatio)*2)*1000);
struct hash *probeHash;
struct probe *probeList;

readProbeList(inLst, &probeList, &probeHash);
printf("Found %d probes in %s\n", slCount(probeList), inLst);
printf("Processing %s percent ID %f%% threshold %d\n", inPsl, aliRatio*100, threshold);
strcpy(lastName, "");
while ((psl = pslNext(in)) != NULL)
    {
    if (!sameString(lastName, psl->qName))
	{
	doOneAcc(lastName, pslList, threshold, trimSize, probeHash, misAsm);
	finishList(&pslList);
	strcpy(lastName, psl->qName);
	}
    slAddHead(&pslList, psl);
    }
doOneAcc(lastName, pslList, threshold, trimSize, probeHash, misAsm);
finishList(&pslList);
lineFileClose(&in);

summarizeProbeList(probeList, trimSize, aliRatio, out, misAsm);
fclose(out);
}

int expectInt(char *s)
/* Return integer representation of s.  Squawk and die
 * if can't. */
{
if (s[0] == '-')
  ++s;
if (!isdigit(s[0]))
  errAbort("Expecting integer got %s", s);
return atoi(s);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 7)
    usage();
expectInt(argv[3]);
pslCoverage(argv[1], argv[2], atof(argv[3])*0.01, expectInt(argv[4]), argv[5], argv[6]);
return 0;
}
