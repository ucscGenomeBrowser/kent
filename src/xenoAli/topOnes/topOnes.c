/* topOnes - Creates an html file with pointers to the areas with C. briggsae
 * homologs sorted with best ones first. */
#include "common.h"
#include "dnautil.h"
#include "xa.h"
#include "wormdna.h"
#include "htmshell.h"

int cmpXaScore(const void *va, const void *vb)
/* Compare function to sort hit array by ascending
 * target offset followed by ascending probe offset. */
{
const struct xaAli *a = *((struct xaAli **)va);
const struct xaAli *b = *((struct xaAli **)vb);
return b->milliScore - a->milliScore;
}

int cmpXaQuery(const void *va, const void *vb)
/* Compare function to sort hit array by ascending
 * target offset followed by ascending probe offset. */
{
const struct xaAli *a = *((struct xaAli **)va);
const struct xaAli *b = *((struct xaAli **)vb);
int diff;
diff = strcmp(a->query, b->query);
if (diff == 0)
    diff = a->qStart - b->qStart;
if (diff == 0)
    diff = a->qEnd - b->qEnd;
return diff;
}

int cmpXaTarget(const void *va, const void *vb)
/* Compare function to sort hit array by ascending
 * target offset followed by ascending probe offset. */
{
const struct xaAli *a = *((struct xaAli **)va);
const struct xaAli *b = *((struct xaAli **)vb);
int diff;
diff = strcmp(a->target, b->target);
if (diff == 0)
    diff = a->tStart - b->tStart;
if (diff == 0)
    diff = a->tEnd - b->tEnd;
return diff;
}

void usage()
/* Explain usage and exit. */
{
errAbort("topOnes - creates an .html file of C. briggsae homologs.\n"
         "usage:\n"
         "    topOnes score output.html\n"
         "    topOnes briggsae output.html\n"
         "    topOnes elegans output.html\n"
         );
}

int main(int argc, char *argv[])
{
char *outName;
char xaFileName[512];
char region[64];
FILE *xaFile, *out;
struct xaAli *xaList = NULL, *xa;
char *sortBy;
char *subtitle;
int (*cmp)(const void *va, const void *vb);

if (argc != 3)
    {
    usage();
    }
sortBy = argv[1];
outName = argv[2];

if (sameWord(sortBy, "score"))
    {
    cmp = cmpXaScore;
    subtitle = "(sorted by alignment score)";
    }
else if (sameWord(sortBy, "briggsae"))
    {
    cmp = cmpXaQuery;
    subtitle = "(sorted by <I>C. briggsae</I> region)";
    }
else if (sameWord(sortBy, "elegans"))
    {
    cmp = cmpXaTarget;
    subtitle = "(sorted by <I>C. elegans</I> region)";
    }
else
    usage();

/* Read in alignment file. */
sprintf(xaFileName, "%s%s/all%s", wormXenoDir(), "cbriggsae", 
    xaAlignSuffix());
printf("Scanning %s\n", xaFileName);
xaFile = xaOpenVerify(xaFileName);
while ((xa = xaReadNext(xaFile, FALSE)) != NULL)
    {
    xa->milliScore = round(0.001 * xa->milliScore * (xa->tEnd - xa->tStart));
    freeMem(xa->qSym);
    freeMem(xa->tSym);
    freeMem(xa->hSym);
    slAddHead(&xaList, xa);
    }

/* Sort by score. */
printf("Sorting...");
slSort(&xaList, cmp);
printf(" best score %d\n", xaList->milliScore);

/* Write out .html */
printf("Writing %s\n", outName);
out = mustOpen(outName, "w");
htmStart(out, "C. briggsae/C. elegans Homologies");
fprintf(out, "<H2>Regions with Sequenced <I>C. briggsae</I> Homologs</H2>\n");
fprintf(out, "<H3>%s</H3>\n", subtitle);
fprintf(out, "<TT><PRE><B>");
fprintf(out, "Score  <I>C. elegans Region</I>     <I>C. briggsae</I> Region </B>\n");
fprintf(out, "--------------------------------------------------------\n");
for (xa = xaList; xa != NULL; xa = xa->next)
    {
    fprintf(out, "%6d ", xa->milliScore);
    sprintf(region, "%s:%d-%d", xa->target, xa->tStart, xa->tEnd);
    fprintf(out, "<A HREF=\"../cgi-bin/tracks.exe?where=%s\">%21s</A> %s:%d-%d %c", 
        region, region, xa->query, xa->qStart, xa->qEnd, xa->qStrand);
    fprintf(out, "\n");
    }
htmEnd(out);
return 0;
}