/* bac2bac - align one bac against another (or the contigs of one
 * back against themselves. */

#include "common.h"
#include "portable.h"
#include "dnautil.h"
#include "fa.h"
#include "dnaseq.h"
#include "patSpace.h"
#include "fuzzyFind.h"
#include "supStitch.h"
#include "cheapcgi.h"
#include "htmshell.h"

#define UCSC

#ifdef JKHOME
char *unfinDir = "/d/biodata/human/unfin/fa";
char *finDir = "/d/biodata/human/fin/fa";
char *oocPlace = "/d/biodata/human/10.ooc";
#endif

#ifdef UCSC
char *finDir = "/projects/compbio/experiments/hg/testSets/chkGlue/fin";
char *unfinDir = "/projects/compbio/experiments/hg/testSets/chkGlue/unfin";
char *oocPlace = "/projects/compbio/experiments/hg/h/10.ooc";
#endif

boolean makeHtml = FALSE;

struct dnaSeq *readBacContigs(char *acc)
/* Read in contigs of bac of given accession. */
{
char fileName[512];

sprintf(fileName, "%s/%s.fa", unfinDir, acc);
if (fileExists(fileName))
    return faReadAllDna(fileName);
sprintf(fileName, "%s/%s.fa", finDir, acc);
return faReadAllDna(fileName);
}

void printGoodBundles(struct ssBundle *bunList, boolean isSelf,
    int contigIx, boolean isRc)
/* Print out alignments in bundle list that look good enough
 * according to some criteria, and are not self alignments. */
{
struct ssBundle *bun;
struct ssFfItem *sfi;
for (bun = bunList; bun != NULL; bun = bun->next)
    {
    if (!isSelf || contigIx < bun->genoContigIx)
	{
	struct dnaSeq *needle = bun->qSeq;
	struct dnaSeq *hay = bun->genoSeq;
	for (sfi = bun->ffList; sfi != NULL; sfi = sfi->next)
	    {
	    struct ffAli *left = sfi->ff;
	    struct ffAli *right = ffRightmost(left);
	    int score = ffScoreCdna(left);
	    int milliScore = 1000*score/(right->nEnd-left->nStart);
	    if (makeHtml)
		{
		char b1Name[64], b2Name[64];
		strcpy(b1Name, needle->name);
		chopSuffix(b1Name);
		strcpy(b2Name, hay->name);
		chopSuffix(b2Name);
		printf("<A HREF=\"../cgi-bin/bac2bac?bac1=%s&c1=%d&s1=%d&e1=%d&bac2=%s&c2=%d&s2=%d&e2=%d%s\">",
		    b1Name, contigIx, left->nStart-needle->dna, right->nEnd-needle->dna,
		    b2Name, bun->genoContigIx, left->hStart-hay->dna, right->hEnd-hay->dna,
		    (isRc ? "&rc1=on" : ""));
		}

	    printf("%d.%d%% %s and %s (%d) %d-%d of %d and %d-%d of %d %c",
	    	milliScore/10, milliScore%10, needle->name, hay->name, score, 
	    	left->nStart-needle->dna, right->nEnd-needle->dna, needle->size, 
	    	left->hStart-hay->dna, right->hEnd-hay->dna, hay->size, 
		(isRc ? '-' : '+'));
	    if (makeHtml)
		{
		printf("</A>\n");
		}
	    else
		printf("\n");
	    }
	}
    }
}

void bac2bac(char *b1Name, char *b2Name, char *oocName)
/* See if two bacs are the same or not, and call the
 * appropriate aligner. */
{
boolean isSelf = sameWord(b1Name, b2Name);
struct dnaSeq *c1List, *c2List, *c1, *c2;
struct patSpace *ps;
int contigIx;

c1List = readBacContigs(b1Name);
if (isSelf)
    c2List = c1List;
else
    c2List = readBacContigs(b2Name);
ps = makePatSpace(&c2List, 1, oocName, 3, 256);

if (makeHtml)
    {
    printf("<H1>%s (%d contigs) vs. %s (%d contigs) </H1>\n", 
    	b1Name, slCount(c1List), b2Name, slCount(c2List));
    printf("<PRE><TT>");
    }

for (c1 = c1List,contigIx=0; c1 != NULL; c1 = c1->next,++contigIx)
    {
    boolean isRc;
    for (isRc = FALSE; isRc <= TRUE; isRc += (TRUE-FALSE))
	{
	struct ssBundle *bunList = ssFindBundles(ps, c1, c1->name, ffTight);
	printGoodBundles(bunList, isSelf, contigIx, isRc);
	ssBundleFreeList(&bunList);
	reverseComplement(c1->dna, c1->size);
	}
    }

freePatSpace(&ps);
freeDnaSeq(&c1List);
if (!isSelf)
    freeDnaSeq(&c2List);
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bac2bac - align one bac against another, or the contigs of \n"
  "one bac against themselves\n"
  "usage:\n"
  "   bac2bac bac1.fa bac2.fa file.ooc \n"
  "If the .html flag is present the result will be written in .html\n"
  "format, and this program can be used as a cgi script");
}

void contig2contig(char *bac1,int s1,int e1,int c1,char *bac2,int s2,int e2,int c2, boolean rc1)
/* Compare two contigs. */
{
struct dnaSeq *clist1 = readBacContigs(bac1);
struct dnaSeq *clist2 = readBacContigs(bac2);
struct dnaSeq *contig1 = slElementFromIx(clist1, c1);
struct dnaSeq *contig2 = slElementFromIx(clist2, c2);
struct ffAli *ff;
DNA *needle, *hay;

s1 -= 100;
if (s1 < 0) s1 = 0;
s2 -= 100;
if (s2 < 0) s2 = 0;
e1 += 100;
if (e1 > contig1->size)
    e1 = contig1->size;
e2 += 100;
if (e2 > contig2->size)
    e2 = contig2->size;

printf("<H2>%s contig %d:%d-%d of %d vs. %s contig %d:%d-%d of %d strand %c</H2>\n",
    bac1, c1, s1, e1, contig1->size, bac2, c2, s2, e2, contig2->size, (rc1 ? '-' : '+'));
needle = contig1->dna + s1;
hay = contig2->dna + s2;

if (rc1)
    reverseComplement(contig1->dna, contig1->size);
ff = ffFind(needle, contig1->dna+e1, hay, contig2->dna+e2, ffTight);

contig1->dna[e1] = 0;
contig2->dna[e2] = 0;
ffShowAli(ff, bac1, needle, s1, bac2, hay, s2, FALSE);

freeDnaSeq(&clist1);
freeDnaSeq(&clist2);
}

void webMiddle()
/* Generate middle of web page. */
{
char *bac1 = cgiString("bac1");
char *bac2 = cgiString("bac2");
if (cgiVarExists("s1"))
    {
    int s1 = cgiInt("s1");
    int s2 = cgiInt("s2");
    int e1 = cgiInt("e1");
    int e2 = cgiInt("e2");
    int c1 = cgiInt("c1");
    int c2 = cgiInt("c2");
    boolean rc1 = cgiBoolean("rc1");
    contig2contig(bac1,s1,e1,c1,bac2,s2,e2,c2,rc1);
    }
else
    {
    bac2bac(bac1,bac2, oocPlace);
    }
}

int main(int argc, char *argv[])
/* Process command line, setting global switches as need be.
 * Call aligner. */
{
if (argc == 1)	/* testing/CGI. */
    {
    if (cgiIsOnWeb())
	{
	makeHtml = TRUE;
	htmShell("Bac 2 Bac results", webMiddle, NULL);
	}
    else
	{
	bac2bac("AL050333", "AL050333", oocPlace);
	}
    return 0;
    }
if (argc != 4)
    usage();
bac2bac(argv[1], argv[2], argv[3]);
}
