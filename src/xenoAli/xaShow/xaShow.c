/* xaShow.c - Display a section of a cross-species alignment. */
#include "common.h"
#include "xa.h"
#include "wormdna.h"
#include "snof.h"
#include "cheapcgi.h"
#include "htmshell.h"

struct xaAli *getOneXaAli(char *organism, char *xaName)
/* Return a single named xaAli for organism. */
{
char ixFileName[512];
char dataFileName[512];
char *xDir;
struct snof *snof;
long offset;
FILE *f;
struct xaAli *xa;

xDir = wormXenoDir();
sprintf(ixFileName, "%s%s/all", xDir, organism); 
sprintf(dataFileName, "%s%s/all%s", xDir, organism, xaAlignSuffix());

snof = snofMustOpen(ixFileName);
if (!snofFindOffset(snof, xaName, &offset))
    errAbort("Couldn't find %s", xaName);
snofClose(&snof);
uglyf("offset is %ld<BR>\n", offset);
f = xaOpenVerify(dataFileName);
uglyf("xa %s file offset %ld<BR>\n", xaName, offset);
fseek(f, offset, SEEK_SET);
xa = xaReadNext(f, FALSE);
fclose(f);
return xa;
}

int countNonDash(char *s, int size)
/* Count non-dash characters. */
{
int count = 0;
int i;
for (i=0; i<size; ++i)
    if (s[i] != '-') ++count;
return count;
}

int lenWithDashes(char *s, int sizeNoDashes)
/* Figure out length of s need to show to display # of characters
 * that don't have a dash. */
{
int count = 0;
int noDashCount = 0;
char c;
while ((c = *s++) != 0)
    {
    if (noDashCount >= sizeNoDashes)
        break;
    if (c != '-')
        ++noDashCount;
    ++count;
    }
return count;
}

void showTargetRange(struct xaAli *xa, int tOff, int tLen, char strand, boolean showSym)
/* Display a range of xa, indexed by target. */
{
char *hSym = xa->hSym, *qSym = xa->qSym, *tSym = xa->tSym;
int symCount = xa->symCount;
int tPos = 0;
int i = 0;
int j;
int maxLineLen = 50;
int lineLen;
int startIx;
int fullLen;
int endIx;

/* Figure out starting and ending positions taking inserts in target
 * into account. */
startIx = lenWithDashes(tSym, tOff);
fullLen = lenWithDashes(tSym+startIx, tLen);
endIx = startIx + fullLen;
if (strand == '-')
    {
    reverseComplement(qSym+startIx, fullLen);
    reverseComplement(tSym+startIx, fullLen);
    reverseBytes(hSym+startIx, fullLen);
    }
for (i=startIx; i<endIx; i += lineLen)
    {
    lineLen = endIx-i;
    if (lineLen > maxLineLen)
        lineLen = maxLineLen;
    mustWrite(stdout, qSym+i, lineLen);
    fputc('\n', stdout);
    for (j=0; j<lineLen; ++j)
        {
        char c = (toupper(qSym[i+j]) == toupper(tSym[i+j]) ? '|' : ' ');
        fputc(c, stdout);
        }
    fputc('\n', stdout);
    mustWrite(stdout, tSym+i, lineLen);
    fputc('\n', stdout);
    //if (showSym)
        {
        mustWrite(stdout, hSym+i, lineLen);
        fputc('\n', stdout);
        }
    fputc('\n', stdout);
    }
if (strand == '-')
    {
    reverseComplement(qSym+startIx, fullLen);
    reverseComplement(tSym+startIx, fullLen);
    reverseBytes(hSym+startIx, fullLen);
    }
}

void upcCorresponding(DNA *aSeq, int aSize, DNA *bSeq, int bOffset)
/* Put corresponding regions of B that correspond with upper cased regions of A
 * in upper case.  A has no gaps.  B may have gaps. */
{
char ca, cb;
DNA *a = aSeq, *b = bSeq;
b += lenWithDashes(b, bOffset);
while ((cb = *b) != 0)
    {
    if (cb != '-')
        {
        if ((ca = *a) == 0)
            break;
        if (isupper(ca))
            *b = toupper(cb);
        if (*a != *b)
            {
            warn("<TT><PRE>Warning: corresponding regions not the same:\n%s\n%s",
                 a, b);
            break;
            }
        ++a;
        }
    ++b; 
    }
}

void doMiddle()
/* Write middle part of .html. */
{
DNA *targetDna;
char *chrom;
int tStart, tEnd;
struct xaAli *xa;
int bothStart, bothEnd;
char cbCosmidName[256];
char *s;

/* Get input variables from CGI. */
char *qOrganism = cgiString("qOrganism");
char *tOrganism = cgiString("tOrganism");
char *query = cgiString("query");
char *target = cgiString("target");
char *strandString = cgiString("strand");
char strand = strandString[0];
boolean showSym = cgiBoolean("symbols");
boolean gotClickPos = cgiVarExists("clickPos");
double clickPos;
if (gotClickPos) clickPos = cgiDouble("clickPos");

strcpy(cbCosmidName, query);
if ((s = strrchr(cbCosmidName, '.')) != NULL)
    *s = 0;


/* Get xaAli. */
xa = getOneXaAli(qOrganism, query);

printf("<H2>Alignment of <I>C. briggsae</I> %s:%d-%d and <I>C. elegans</I> %s</H2>\n",
    cbCosmidName, xa->qStart, xa->qEnd, target);

uglyf("qOrganism %s, tOrganism %s, query %s, target %s, strand %c<BR>\n",
	qOrganism, tOrganism, query, target, strand);

htmlParagraph("<I>C. briggsae</I> appears on top. Coding regions in <I>C. elegans</I> are in upper case.");


uglyf("Ok1<BR>\n");
/* Get display window. */
if (!wormParseChromRange(target, &chrom, &tStart, &tEnd))
    errAbort("Target %s isn't formatted correctly", target);

/* Figure out intersection of display window and xeno-alignment */
uglyf("Ok2<BR>\n");
bothStart = max(xa->tStart, tStart);
bothEnd = min(xa->tEnd, tEnd);
uglyf("xa->tStart %d, xa->tEnd %d, tStart %d, tEnd %d, bothStart %d, bothEnd %d<BR>\n",
	xa->tStart, xa->tEnd, tStart, tEnd, bothStart, bothEnd);

/* Get upper-cased-exon target DNA. */
targetDna = wormChromPartExonsUpper(chrom, bothStart, bothEnd - bothStart);
uglyf("Ok3<BR>\n");
upcCorresponding(targetDna, bothEnd - bothStart, xa->tSym, bothStart - xa->tStart);
uglyf("Ok4<BR>\n");


printf("<TT><PRE>");
showTargetRange(xa, bothStart - xa->tStart, bothEnd-bothStart, strand, showSym);
uglyf("Ok5<BR>\n");
printf("</TT></PRE>");
}

int main(int argc, char *argv[])
{
htmShell("xaShow", doMiddle, NULL);
return 1;
}
