/* scanIntrons.c */
#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "wormdna.h"
#include "cheapcgi.h"
#include "htmshell.h"


#define hisSize 15
/* Histogram size. */

static int preHis[hisSize][4];
static int startHis[hisSize][4];
static int endHis[hisSize][4];
static int postHis[hisSize][4];

void addToHis(DNA *dna, int his[hisSize][4])
{
int i;
int val;

for (i=0; i<hisSize; ++i)
    {
    if ((val = ntVal[dna[i]]) >= 0)
        his[i][val] += 1;
    }
}

void addIntronToHistogram(DNA *pre, DNA *start, DNA *end, DNA *post)
{
    addToHis(pre-hisSize, preHis);
    addToHis(start, startHis);
    addToHis(end-hisSize, endHis);
    addToHis(post, postHis);
}

int sum4(int *a)
{
return a[0]+a[1]+a[2]+a[3];
}

void printLineOfHis(int his[hisSize][4], DNA base)
{
int baseVal = ntVal[base];
int i;
long millis;

printf("%c ", base);
for (i=0; i<hisSize; ++i)
    {
    millis = his[i][baseVal] * 1000 / sum4(his[i]);
    if (millis >= 10)
	printf("%3d%% ", (millis+5)/10);
    else
	printf("%d.%d%% ", millis/10, millis%10);
    }
printf("<BR>\n");
}


void printHis(int his[hisSize][4])
{
printf("<P>");
printLineOfHis(his, 'a');
printLineOfHis(his, 'c');
printLineOfHis(his, 'g');
printLineOfHis(his, 't');
printf("</P>");
}

void printAllHistograms()
{
htmlParagraph("Frequency Counts of 5' Ends of Exons:");
printHis(preHis);
htmlParagraph("Frequency Counts of Intron Starts:");
printHis(startHis);
htmlParagraph("Frequency Counts of Intron Ends:");
printHis(endHis);
htmlParagraph("Frequency Counts of 3' Ends of Exons:");
printHis(postHis);
}

void printLineOfProbs(int his[hisSize][4], DNA base)
{
int baseVal = ntVal[base];
int i;
double ratio;

printf("%c[] = { ", base);
for (i=0; i<hisSize; ++i)
    {
    ratio = (double) his[i][baseVal] / sum4(his[i]);
    printf("%4.3f, ", ratio);
    }
printf("};\n");
}


void printProbs(int his[hisSize][4])
{
printf("<P>");
printLineOfProbs(his, 'a');
printLineOfProbs(his, 'c');
printLineOfProbs(his, 'g');
printLineOfProbs(his, 't');
printf("</P>");
}

void printAllProbs()
/* Print histogram in probability format. */
{
puts("<TT><PRE>");
htmlParagraph("Frequency Counts of 5' Ends of Exons:");
printProbs(preHis);
htmlParagraph("Frequency Counts of Intron Starts:");
printProbs(startHis);
htmlParagraph("Frequency Counts of Intron Ends:");
printProbs(endHis);
htmlParagraph("Frequency Counts of 3' Ends of Exons:");
printProbs(postHis);
}

boolean patMatch(DNA *queryString, DNA *targetString, int querySize)
/* Does as match with bs except for the N's in as? */
{
DNA q, t, lastQ;
int i;

lastQ = 0;
for (i=0; i<querySize; ++i)
    {
    q = *queryString++;
    if (q != '!')
        {
        t = *targetString++;
        if (lastQ == '!')
            {
            if (q == t)
                return FALSE;
            }
        else
            {
            if (!(q == 'n' || q == t))
                return FALSE;
            }        
        }
    lastQ = q;
    }
return TRUE;
}

int countSpecial(DNA *query)
/* Count special chars in query. */
{
int count = 0;
DNA q;
while ((q = *query++) != 0)
    {
    if (q == '!')
        ++count;
    }
return count;
}

char *spaces(int count)
{
int i;
static char buf[80];
if (count < 0) count = 0;
assert(count < sizeof(buf));
for (i=0; i<count; ++i)
    buf[i] = ' ';
buf[count] = 0;
return buf;
}

struct nameOff
    {
    struct nameOff *next;
    char *name;
    char *chrom;
    int start, end;
    int cdnaCount;
    DNA startI[3];
    DNA endI[3];
    struct slName *cdnaNames;
    };

int cmpNameOff(const void *va, const void *vb)
/* Compare two slNames. */
{
const struct nameOff *a = *((struct nameOff **)va);
const struct nameOff *b = *((struct nameOff **)vb);
return strcmp(a->name, b->name);
}

int cmpCounts(const void *va, const void *vb)
/* Compare two slNames. */
{
const struct nameOff *a = *((struct nameOff **)va);
const struct nameOff *b = *((struct nameOff **)vb);
int dif = b->cdnaCount - a->cdnaCount;
if (dif != 0)
    return dif;
return strcmp(a->name, b->name);
}


struct nameOff *scanIntronFile(char *preIntronQ, char *startIntronQ, 
    char *endIntronQ, char *postIntronQ, boolean invert)
{
char intronFileName[600];
FILE *f;
char lineBuf[4*1024];
char *words[4*128];
int wordCount;
int lineCount = 0;
int preLenQ = strlen(preIntronQ);
int startLenQ = strlen(startIntronQ);
int endLenQ = strlen(endIntronQ);
int postLenQ = strlen(postIntronQ);
char *preIntronF, *startIntronF, *endIntronF, *postIntronF;
int preLenF, startLenF, endLenF, postLenF;
int preIx = 6, startIx = 7, endIx =8, postIx = 9;
struct nameOff *list = NULL, *el;
boolean addIt;
int i;

if (preLenQ > 25 || postLenQ > 25 || startLenQ > 40 || endLenQ > 40)
    {
    errAbort("Can only handle queries up to 25 bases on either side of the intron "
             "and 40 bases inside the intron.");
    }
sprintf(intronFileName, "%s%s", wormCdnaDir(), "introns.txt");
f = mustOpen(intronFileName, "r");
while (fgets(lineBuf, sizeof(lineBuf), f) != NULL)
    {
    ++lineCount;
    wordCount = chopByWhite(lineBuf, words, ArraySize(words));
    if (wordCount == ArraySize(words))
        {
        warn("May have truncated end of line %d of %s",
            lineCount, intronFileName);
        }
    if (wordCount == 0)
        continue;
    if (wordCount < 11)
        errAbort("Unexpected short line %d of %s", lineCount, intronFileName);
    preIntronF = words[preIx];
    startIntronF = words[startIx];
    endIntronF = words[endIx];
    postIntronF = words[postIx];
    preLenF = strlen(preIntronF);
    startLenF = strlen(startIntronF);
    endLenF = strlen(endIntronF);
    postLenF = strlen(postIntronF);
    addIt = FALSE;
    if (   (  preLenQ == 0 || patMatch(preIntronQ, preIntronF+preLenF-preLenQ+countSpecial(preIntronQ), preLenQ))
        && (startLenQ == 0 || patMatch(startIntronQ, startIntronF, startLenQ))
        && (  endLenQ == 0 || patMatch(endIntronQ, endIntronF+endLenF-endLenQ+countSpecial(endIntronQ), endLenQ))
        && ( postLenQ == 0 || patMatch(postIntronQ, postIntronF, postLenQ)) )
        {
        addIt = TRUE;
        }
    if (invert)
        addIt = !addIt;
    if (addIt)
        {
        addIntronToHistogram(preIntronF+preLenF, startIntronF, endIntronF+endLenF, postIntronF);
        AllocVar(el);
        el->chrom = cloneString(words[1]);
        el->name = cloneString(words[5]);
        el->start = atoi(words[2]);
        el->end = atoi(words[3]);        
        el->cdnaCount = atoi(words[0]);
        memcpy(el->startI, startIntronF, 2);
        memcpy(el->endI, endIntronF + endLenF - 2, 2);
        assert(wordCount == el->cdnaCount + 10);
        for (i=10; i<wordCount; ++i)
            {
            struct slName *name = newSlName(words[i]);
            slAddHead(&el->cdnaNames, name);
            }
        slReverse(&el->cdnaNames);
        assert(slCount(el->cdnaNames) == el->cdnaCount);
        slAddHead(&list, el);
        }
    }
fclose(f);
slSort(&list, cmpCounts);
return list;
}

void showMatches(struct nameOff *matches)
{
int nameWidth = 13;

printf("<TT><PRE>");
printf("  ORF       cDNA intron   supporting\n");
printf("  name     count  ends    cDNA\n");
printf("--------------------------------------\n");
for (;matches != NULL; matches = matches->next)
    {
    struct slName *name;
    printf("<A HREF=\"../cgi-bin/tracks.exe?where=%s:%d-%d&title=Intron+Near+%s&hilite=%d-%d\">%s</A> ", 
            matches->chrom, matches->start-1000, matches->end+1000, matches->name, matches->start, matches->end, matches->name);
    printf("%s", spaces(nameWidth - strlen(matches->name) ) );
    printf("%2d %s...%s", matches->cdnaCount, matches->startI, matches->endI);
    for (name = matches->cdnaNames; name != NULL; name = name->next)
        {
        printf(" %s", name->name);
        }
    printf("\n");
    }
printf("</PRE></TT>\n");
}


void doMiddle()
{
char *preIntron, *startIntron, *endIntron, *postIntron;
int count = 0;
int matchCount = 0;
int maxCount;
struct nameOff *matchList;
boolean invert = cgiVarExists("Invert");

preIntron = cgiString("preIntron");
startIntron = cgiString("startIntron");
endIntron = cgiString("endIntron");
postIntron = cgiString("postIntron");

/* Just for debugging cut search short if matches special magic */
maxCount = atoi(preIntron);
if (maxCount <= 0)
    maxCount = 0x7ffffff;


eraseWhiteSpace(preIntron);
eraseWhiteSpace(startIntron);
eraseWhiteSpace(endIntron);
eraseWhiteSpace(postIntron);

tolowers(preIntron);
tolowers(startIntron);
tolowers(endIntron);
tolowers(postIntron);

matchList = scanIntronFile(preIntron, startIntron, endIntron, postIntron, invert);
if (matchList == NULL)
    errAbort("Sorry, no matches to %s%s %s %s %s", (invert ? "inverted " : ""), preIntron, startIntron, endIntron, postIntron);
printf("<P>%d introns matching %s%s(%s %s)%s. ", slCount(matchList),
    (invert ? "inverted " : ""), preIntron, startIntron, endIntron, postIntron);
printf("Shortcut to <A HREF=\"#1\">frequency counts.</A></P>");

htmlHorizontalLine();
showMatches(matchList);
htmlHorizontalLine();
printf("<TT><PRE>");
printf("<A NAME=\"1\"></A>");
printAllHistograms();
printf("</TT></PRE>");
}

int main(int argc, char *argv[])
{
dnaUtilOpen();
/* If run from the command line with the right number of arguments,
 * set up QUERY_STRING so we can pretend we're being run by web server
 * instead. */
if (argc == 5)
    {
    char buf[512];
    sprintf(buf, "QUERY_STRING=preIntron=%s&startIntron=%s&endIntron=%s&postIntron=%s", argv[1], argv[2], argv[3], argv[4]);
    putenv(buf);
    }
htmShell("ScanIntron Output", doMiddle, "QUERY");
return 0;
}
