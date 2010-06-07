/* checkLayout - help check layout by displaying where contigs
 * go against glued and finished versions of same sequence. */
#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "patSpace.h"
#include "fuzzyFind.h"
#include "supStitch.h"
#include "memgfx.h"
#include "cheapcgi.h"
#include "htmshell.h"

#ifdef Crunx
#define testDir "/d/biodata/human/testSets/testset02"
char *oocFile = "/d/biodata/human/10.ooc";
#else
#define testDir "/projects/compbio/experiments/hg/testSets/testset02"
char *oocFile = "/projects/compbio/experiments/hg/h/10.ooc";
#endif

char *indexDir = testDir "/index";
char *faDir = testDir "/fa";

int trackHeight;
int trackWidth;
int border = 2;
MgFont *font;

Color blockColors[8];
struct rgbColor blockRgbColors[8] =
    {
        {0,0,0},
        {128, 0, 0},
        {0, 96, 0},
        {0, 0, 150},
        {80, 80, 80},
        {0, 80, 80},
        {100, 0, 100},
        {80, 80, 0},
    };

void makeBlockColors(struct memGfx *mg)
/* Get some defined colors to play with. */
{
int i;
struct rgbColor *c;
for (i=0; i<ArraySize(blockColors); ++i)
    {
    c = blockRgbColors+i;
    blockColors[i] = mgFindColor(mg, c->r, c->g, c->b);
    }
}

struct aliList
/* A list of scored alignments. */
    {
    struct aliList *next;
    struct ffAli *ali;
    boolean isRc;
    int score;
    };

void showAllNts()
/* Creat a form that lets user select an NT
 * layout to examine. */
{
struct slName *ntList, *ntEl;
printf("<H1>Check Layout</H1>\n");
printf("<H3>Click on a link below to see finished vs. unfinished alignments</H3>\n");
printf("<PRE>");
ntList = listDir(indexDir, "NT*.index");
for (ntEl = ntList; ntEl != NULL; ntEl = ntEl->next)
    {
    char *root = ntEl->name;
    char *s = strrchr(root, '.');
    *s = 0;
    printf("<A HREF=\"../cgi-bin/checkLayout?NT=%s\">%s</A>\n", root, root);
    }
}

void showBundle(struct ssBundle *bun, boolean isRc)
/* Display a bundle for user. */
{
struct ssFfItem *ffi;

for (ffi = bun->ffList; ffi != NULL; ffi = ffi->next)
    {
    struct ffAli *left, *right;
    int score;
    DNA *needle = bun->qSeq->dna;
    DNA *hay = bun->genoSeq->dna;
    left = ffi->ff;
    right = ffRightmost(left);
    score = ffScore(left, ffTight);
    printf("%s:%d-%d of %d %s:%d-%d of %d strand %c score %d\n",
	bun->genoSeq->name, left->hStart - hay, right->hEnd - hay, bun->genoSeq->size,
	bun->qSeq->name, left->nStart - needle, right->nEnd - needle, bun->qSeq->size,	
	(isRc ? '-' : '+'), score);
    }
}


void alignNt(char *nt)
/* Do alignments of draft bacs against one NT. */
{
char indexFileName[512];
char ntFaName[512];
struct lineFile *indexLf;
int lineSize;
char *line;
char *words[3];
int wordCount;
struct patSpace *ps;
struct dnaSeq *ntSeq;

printf("<H1>Check Layout of %s</H1>\n", nt);
printf("<PRE>");
sprintf(ntFaName, "%s/p%s.fa", faDir, nt);
ntSeq = faReadAllDna(ntFaName);
ps = makePatSpace(&ntSeq, 1, oocFile, 10, 500);
sprintf(indexFileName, "%s/%s.index", indexDir, nt);
uglyf("Checking out %s and %s\n", indexFileName, ntFaName);
indexLf = lineFileOpen(indexFileName, TRUE);
while (lineFileNext(indexLf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    if (wordCount > 0)
	{
	char bacFaName[512];
	struct dnaSeq *contigList, *contig;
	char *bacAcc = words[0];
	char *s = strrchr(bacAcc, '.');
	if (s != NULL)
	    *s = 0;
	uglyf("%s\n", bacAcc);
	sprintf(bacFaName, "%s/%s.fa", faDir, bacAcc);
	contigList = faReadAllDna(bacFaName);
	for (contig = contigList; contig != NULL; contig = contig->next)
	    {
	    boolean isRc;
	    uglyf(" %s\n", contig->name);
	    for (isRc = FALSE; isRc <= TRUE; isRc += 1)
		{
		struct ssBundle *bunList, *bun;
		bunList = ssFindBundles(ps, contig, contig->name, ffTight);
		for (bun = bunList; bun != NULL; bun = bun->next)
		    {
		    showBundle(bun, isRc);
		    }
		ssBundleFreeList(&bunList);
		reverseComplement(contig->dna, contig->size);
		}
	    }
	freeDnaSeqList(&contigList);
	}
    }
lineFileClose(&indexLf);
freeDnaSeqList(&ntSeq);
}


void doMiddle()
/* Create middle of html form. */
{
char *nt = cgiOptionalString("NT");
if (nt != NULL)
    alignNt(nt);
else
    showAllNts();
}

int main(int argc, char *argv[])
{
if (!cgiIsOnWeb())
    {
    if (argc == 2)
	{
	static char buf[128];
	sprintf(buf, "QUERY_STRING=NT=%s", argv[1]);
	putenv(buf);
	}
    else
	{
	putenv("QUERY_STRING=commandLine=on");
	}
    }
htmShell("Check Layout", doMiddle, "get");
return 0;
}
