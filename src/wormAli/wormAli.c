/* Test.c */
#include "common.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "nt4.h"
#include "wormdna.h"
#include "cda.h"
#include "crudeali.h"
#include "fuzzyFind.h"
#include "htmshell.h"
#include "cheapcgi.h"

int chromCount;
char **chromNames;
struct nt4Seq **chrom;

void alignToWorm(char *rawSeq, int tileSize)
{
int probeSize = dnaFilteredSize(rawSeq);
DNA *probeDna = needMem(probeSize+1);
struct crudeAli *caList, *crudeAli;


dnaFilter(rawSeq, probeDna);
wormChromNames(&chromNames, &chromCount);
wormLoadNt4Genome(&chrom, &chromCount);
caList = crudeAliFind(probeDna, probeSize, chrom, chromCount,tileSize,4);
if (caList == NULL)
    {
    errAbort("No alignments, sorry");
    }
else
    {
    struct tempName faTn, cdaTn;
    struct cdaAli *cda = NULL;
    struct ffAli *ffAli = NULL;
    boolean isRc;
    char *chromName;
    DNA *chromDna;
    int chromDnaSize;
    int start, end;
    boolean alignedOne = FALSE;

    printf("Click on the chromosome position(s) below for tracks display of alignment.\n");
    htmlHorizontalLine();
    for (crudeAli = caList; crudeAli != NULL; crudeAli = crudeAli->next)
        {
        makeTempName(&faTn, "dyn", ".fa");
        faWrite(faTn.forCgi, "WebSeq", probeDna, probeSize);
	chmod(faTn.forCgi, 0666);

        chromName = chromNames[crudeAli->chromIx];
        start = crudeAli->start - 1000;
        end = crudeAli->end + 1000;
        wormClipRangeToChrom(chromName, &start, &end);

        chromDnaSize = end - start;
        chromDna = wormChromPart(chromName, start, chromDnaSize);

        if (crudeAli->strand == '-')
            {
            reverseComplement(chromDna, chromDnaSize);
            isRc = TRUE;
            }
        else
            isRc = FALSE;
        ffAli = ffFind(probeDna, probeDna + probeSize, chromDna, chromDna + chromDnaSize, ffCdna);
        if (ffAli != NULL)
            {
            if (isRc)
                reverseComplement(chromDna, chromDnaSize);
            alignedOne = TRUE;
            cda = cdaAliFromFfAli(ffAli, probeDna, probeSize, chromDna-start, chromDnaSize, isRc);
            cda->chromIx = crudeAli->chromIx;
            cda->name = "Pasted";
            makeTempName(&cdaTn, "dyn", ".cda");
            cdaWrite(cdaTn.forCgi, cda);
	    chmod(cdaTn.forCgi, 0666);

            printf("<A HREF=\"../cgi-bin/tracks.exe?where=%s:%d-%d&dynFa=%s&dynCda=%s\">",
                chromName, start, end, 
                cgiEncode(faTn.forCgi), cgiEncode(cdaTn.forCgi));
            printf("%s:%d-%d</A> strand %c score %d<BR>\n", chromName, 
                crudeAli->start, crudeAli->end, crudeAli->strand, crudeAli->score);
            }
        }
    if (!alignedOne)  /* Nobody aligned. */
        errAbort("Couldn't align, sorry");
    }
}

void doMiddle()
{
char *sequence = cgiString("sequence");
int tileSize = 8;
char *source = cgiString("source");

if (sameWord(source, "C. elegans"))
    tileSize = 16;
else
    tileSize = 8;

alignToWorm(sequence, tileSize);
}

int main(int argc, char *argv[])
{
dnaUtilOpen();
if (argc == 2 && sameWord(argv[1], "test") )
    {
    alignToWorm(
    "TC CAGGAGTCCC 18900"
    "ACACTCCCAC ACCAAGCCAT AC", 16);
    }
else
    {
    htmShell("Worm Align Results", doMiddle, "POST");
    }
return 0;
}
