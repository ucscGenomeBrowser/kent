/* dynAlign.c - align using dynamic programming. */
#include "common.h"
#include "memalloc.h"
#include "portable.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "wormdna.h"
#include "crudeali.h"
#include "xenalign.h"

boolean isOnWeb;  /* True if run from Web rather than command line. */

void dustData(char *in,char *out)
/* Clean up data by getting rid non-alphabet stuff. */
{
char c;
while ((c = *in++) != 0)
    if (isalpha(c)) *out++ = tolower(c);
*out++ = 0;
}

void doMiddle()
{
char *query = cgiString("query");
char *target = cgiString("target");
int qSize, tSize;
int maxQ = 10000 * 2, maxT = 100000;
long startTime, endTime;
double estTime;
long clock1000();

dustData(query, query);
dustData(target, target);
printf("<PRE><TT>");
qSize = strlen(query);
tSize = strlen(target);
printf("query size %d\ntarget size %d\n", qSize, tSize);
if ( qSize <= maxQ || tSize <= maxT)
    {
    if (qSize < 2000 && tSize < 4000)
        {
        estTime = 0.1 + qSize * tSize * 0.25 / 1000000.0;
        }
    else
        {
        estTime = (double)qSize*15.0/10000.0 + 0.4;
        }
    printf("This will take about %4.1f minutes to align on a vintage 1999 server\n",
        estTime);
    fflush(stdout);
    startTime = clock1000();
    xenAlignBig(query,  qSize, target, tSize, stdout, TRUE);
    endTime = clock1000();
    printf("%4.2f minutes to align\n", (double)(endTime-startTime)/(60*1000));
    }
else
    errAbort("%d bases in query, %d bases in target - too big to handle.\n"
             "Maximum is %d bases in query, %d bases in target.",
             qSize, tSize, maxQ, maxT);
}


void  xenAlignFa(char *queryFile, char *targetFile, FILE *out)
/* Align query with target fa files, putting result in out. */
{
struct dnaSeq *q, *t;

q = faReadDna(queryFile);
t = faReadDna(targetFile);
printf("Aligning %s (%d bases) with %s (%d bases)\n",
    q->name, q->size, t->name, t->size);
xenAlignBig(q->dna, q->size, t->dna, t->size, out, TRUE);
freeDnaSeq(&t);
freeDnaSeq(&q);
}

void usage()
/* Describe usage of program and terminate. */
{
errAbort("dynAlign - used dynamic programming to find alignment between two nucleotide sequence\n"
         "usage:\n"
         "    dynAlign test string1 string2\n"
         "aligns two short sequences in command line.\n"
         "    dynAlign two query.fa target.fa outfile\n"
         "aligns two sequences in fa files\n"
         "    dynAlign worms bwana.out outFile\n"
         "aligns C. briggsae genomic fragments against full C. elegans genome.\n"
         "cbali.out is the output of the cbAli program and outFile is the file\n"
         "to produce.\n"
         "    dynAlign starting cbali.out outFile startIx\n"
         "same as dynAlign worms, but starts part way through cbali.out\n"
         "    dynAlign range cbali.out outFile startIx endIx\n"
         "same as dynAlign worms, but specifies start and end areas to work on\n");
}

void doublePrint(FILE *f, char *format, ...)
/* Print both to file and to standard output. */
{
va_list args;
va_start(args, format);
vfprintf(f, format, args);
vfprintf(stdout, format, args);
va_end(args);
}

struct hitCluster
/* Merge a couple of hits into same cluster if it's easy via this. */
    {
    struct hitCluster *next;
    char *probe;
    int pStart, pEnd;
    char *target;
    int tStart, tEnd;
    boolean isRc;
    };

int innerTargetSize = 3000;  /* How big a cluster can get. */
int outerTargetSize = 5000;  /* 1000 bases on either side of cluster. */

void alignAll(struct hitCluster *hcList, int hcListSize, int starting, int ending, char *outName)
/* Do all alignments in list. */
{
FILE *out = mustOpen(outName, "w");
char *probeName = NULL;
struct dnaSeq *probe = NULL;
struct hitCluster *hc;
DNA *targetDna = NULL;
char *chrom;
int tStart, tEnd, tMid, tSize;
int pSize;
int score;
int hcIx = 1;

for (hc = hcList;hc != NULL; hc = hc->next)
    {
    if (hc->target != NULL && hcIx >= starting && hcIx <= ending)
        {
        /* Get new probe if necessary. */
        if (hc->probe != probeName)
            {
            probeName = hc->probe;
            freeDnaSeq(&probe);
            probe = faReadDna(probeName);
            }
        /* Get 5000 base pairs around target. */
        chrom = hc->target;
        tMid = (hc->tStart + hc->tEnd)/2;
        tStart = tMid - outerTargetSize/2;
        tEnd = tStart + outerTargetSize;
        wormClipRangeToChrom(chrom, &tStart, &tEnd);
        tSize = tEnd - tStart;
        targetDna = wormChromPart(chrom, tStart, tSize);
    
        pSize = hc->pEnd - hc->pStart;
        if (hc->isRc)
            reverseComplement(probe->dna + hc->pStart, pSize);
    
        /* Write preliminary info to file and screen. */
        doublePrint(out, "Aligning %s:%d-%d %c to %s:%d-%d + \n(%d of %d)\n",
            probeName, hc->pStart, hc->pEnd, (hc->isRc ? '-' : '+'),
            chrom, tStart, tEnd,
            hcIx, hcListSize);

        score = xenAlignSmall(probe->dna + hc->pStart, pSize,
            targetDna, tSize, out, FALSE);

        if (hc->isRc)
            reverseComplement(probe->dna + hc->pStart, pSize);

        doublePrint(out, "best score %d\n\n", score);

        freez(&targetDna);
        fflush(out);
        }
    ++hcIx;
    }
freeDnaSeq(&probe);
fclose(out);
}

boolean addToCluster(struct hitCluster *hc, char *chrom, int start, int end, boolean isRc, int count)
/* If can add chromosome range to cluster in first count members of hcList
 * without making cluster too big do it, otherwise return FALSE. */
{
int s, e;
int pStart = hc->pStart;
int i;

for (i=0; i<count; ++i)
    {
    assert(hc->pStart == pStart);
    if (hc->tStart == 0 && hc->tEnd == 0) 
        {
        /* We're the first member of this cluster. */
        hc->target = wormOfficialChromName(chrom);
        hc->tStart = start;
        hc->tEnd = end;
        hc->isRc = isRc;
        return TRUE;
        }
    if (!sameString(chrom, hc->target) || isRc != hc->isRc)
        continue;
    s = min(hc->tStart, start);
    e = max(hc->tEnd, end);
    if (e - s > innerTargetSize)
        continue;
    else
        {
        hc->tStart = s;
        hc->tEnd = e;
        return TRUE;
        }
    if ((hc = hc->next) == NULL)
        break;
    }
return FALSE;
}

void wormSecondPassAlign(char *inName, char *outName, int starting, int ending)
/* Do second pass of 3 stage alignment.  Take homologous chromosome
 * position info from BWANA, run seven-state-aligner, and make
 * output for stitcher program to put together. */
{
FILE *in = mustOpen(inName, "r");
char line[512];
char *words[64];
int lineCount = 0, wordCount;
struct dnaSeq *probe = NULL;
char *probeFileName = "";
int aliCount = 0;
int totalAliCount;
int numToDo = 25;
struct hitCluster *hcList = NULL, *hc = NULL;

/* First just scan through input file - parse it, make sure
 * it looks good, and figure out how long it is. */
printf("Scanning input file\n");
while (fgets(line, sizeof(line), in))
    {
    ++lineCount;
    wordCount = chopLine(line, words);
    if (wordCount < 1)
        continue;   /* Skip blank lines. */
    if (sameWord("Processing", words[0]))
        {
        assert(wordCount >= 4);
        probeFileName = cloneString(words[1]);
        }
    else if (sameWord("Aligning", words[0]))
        {
        assert(wordCount >= 7);
        assert(sameString(probeFileName, words[6]));
        assert(isdigit(words[2][0]) && isdigit(words[4][0]));
        AllocVar(hc);
        hc->probe = probeFileName;
        hc->pStart = atoi(words[2]);
        hc->pEnd = atoi(words[4]);
        slAddHead(&hcList, hc);
        aliCount = 1;
        }
    else if (sameWord("Got", words[0]))
        {
        }
    else if (sameWord("No", words[0]))
        {
        assert(wordCount >= 2 && sameWord("alignment", words[1]));
        freeMem(slPopHead(&hcList));
        }
    else if (isdigit(words[0][0]))
        {
        char *chrom;
        int start, end;
        boolean isRc;
        char strand;
        int score = atoi(words[0]);

        if (score >= 45)
            {
            assert(hc != NULL);
            if (wordCount < 3 || !wormParseChromRange(words[1], &chrom, &start, &end))
                errAbort("bad line %d of %s\n", lineCount, inName);
            strand = words[2][0];
            if (strand == '+')
                isRc = FALSE;
            else if (strand == '-')
                isRc = TRUE;
            else
                errAbort("Expecting + or - line %d of %s", lineCount, inName);
            if (!addToCluster(hc, chrom, start, end, isRc, aliCount))
                {
                if (aliCount < numToDo)
                    {
                    ++aliCount;
                    AllocVar(hc);
                    hc->probe = probeFileName;
                    hc->pStart = hcList->pStart;
                    hc->pEnd = hcList->pEnd;
                    hc->target = wormOfficialChromName(chrom);
                    hc->tStart = start;
                    hc->tEnd = end;
                    hc->isRc = isRc;
                    slAddHead(&hcList, hc);
                    }
                }
            }
        }
    }
slReverse(&hcList);

totalAliCount = slCount(hcList);
fclose(in);
printf("Proceeding on to %d alignments\n", totalAliCount);
alignAll(hcList, totalAliCount, starting, ending, outName);
}
 
int main(int argc, char *argv[])
{
//pushCarefulMemHandler();

dnaUtilOpen();
if ((isOnWeb = cgiIsOnWeb()) == TRUE)
    {
    htmShell("DynaAlign Results", doMiddle, NULL);
    }
else
    {
    char *command;

    if (argc < 2)
        usage();
    command = argv[1];
    if (sameWord(command, "test"))
        {
        DNA *query, *target;
        if (argc < 4)
            usage();
        query = (DNA*)argv[2];
        target = (DNA*)argv[3];
        xenAlignSmall(query, strlen(query), target, strlen(target), stdout, TRUE);
        }
    else if (sameWord(command, "two"))
        {
        FILE *out;
        if (argc < 5)
            usage();
        out = mustOpen(argv[4], "w");
        xenAlignFa(argv[2], argv[3], out);
        }
    else if (sameWord(command, "worms"))
        {
        char *cbName = argv[2];
        char *outFileName = argv[3];
        if (argc < 4)
            usage();
        wormSecondPassAlign(cbName, outFileName, 0, 0x7fffffff);
        }
    else if (sameWord(command, "starting"))
        {
        char *cbName = argv[2];
        char *outFileName = argv[3];
        if (argc < 5 || !isdigit(argv[4][0]))
            usage();
        wormSecondPassAlign(cbName, outFileName, atoi(argv[4]), 0x7fffffff);
        }
    else if (sameWord(command, "range"))
        {
        char *cbName = argv[2];
        char *outFileName = argv[3];
        if (argc < 6 || !isdigit(argv[4][0]) || !isdigit(argv[5][0]))
            usage();
        wormSecondPassAlign(cbName, outFileName, atoi(argv[4]), atoi(argv[5]));
        }
    else
        usage();
    }
return 0;
}
