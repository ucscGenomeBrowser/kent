#include "common.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "wormdna.h"

static char const rcsid[] = "$Id: getgene.c,v 1.2 2003/05/06 07:22:33 kate Exp $";

struct dfm
/* Output formatter. */
    {
    int wordLen, lineLen;
    int inWord, inLine;
    boolean lineNumbers;
    boolean hiliteRange;
    long startRange;
    long endRange;
    long charCount;
    FILE *out;
    };

void initDfm(struct dfm *dfm, int wordLen, int lineLen,
	boolean lineNumbers,
	boolean hiliteRange, long startRange, long endRange,
	FILE *out)
/* Set up formatting. */
{
dfm->inWord = dfm->inLine = dfm->charCount = 0;
dfm->wordLen = wordLen;
dfm->lineLen = lineLen;
dfm->lineNumbers = lineNumbers;
dfm->hiliteRange = hiliteRange;
dfm->startRange = startRange;
dfm->endRange = endRange;
dfm->out = out;
}

void dfmOut(struct dfm *dfm, char c)
/* Write out a byte, and depending on formatting extras
 */
{
if (dfm->hiliteRange && dfm->charCount == dfm->startRange)
    {
    fprintf(dfm->out, "<A NAME=\"CLICKED\"></A><span style='color:#0033FF;'>");
    }
++dfm->charCount;
fputc(c, dfm->out);
if (dfm->hiliteRange && dfm->charCount == dfm->endRange)
    {
    fprintf(dfm->out, "</span>");
    }
if (dfm->wordLen)
    {
    if (++dfm->inWord >= dfm->wordLen)
	{
	fputc(' ', dfm->out);
	dfm->inWord = 0;
	}
    }
if (dfm->lineLen)
    {
    if (++dfm->inLine >= dfm->lineLen)
	{
	if (dfm->lineNumbers)
	    fprintf(dfm->out, " %ld", dfm->charCount);
	fprintf(dfm->out, "<BR>\n");
	dfm->inLine = 0;
	}
    }
}

boolean findLineInFile(char *fileName, char *start,
    char *lineBuf, int lineBufSize)
/* Loop through each line in named file until come to one whose
 * first word (deliminated by a space) is start.  Put the resulting
 * line in lineBuf. */
{
FILE *f;
int startLen = strlen(start);
boolean foundIt = FALSE;
f = mustOpen(fileName, "r");
for (;;)
    {
    if ((fgets(lineBuf, lineBufSize, f)) == NULL)
	break;
    if (strncmp(start, lineBuf, startLen) == 0 && lineBuf[startLen] == ' ')
	{
	foundIt = TRUE;
	break;
	}
    }
fclose(f);
return foundIt;
}

char *chopOutSecondWord(char *lineBuf)
/* Return pointer to second word in line. Zero terminate this word. */
{
char *s;
int wordLen;

s = skipLeadingSpaces(lineBuf);
wordLen = strcspn(s,whiteSpaceChopper);
s += wordLen;
s = skipLeadingSpaces(s);
wordLen = strcspn(s,whiteSpaceChopper);
if (wordLen <= 0)
    return NULL;
s[wordLen] = 0;
return s;
}

void outputSeq(DNA *dna, int dnaSize,
	boolean hiliteRange, long startRange, long endRange,
	FILE *out)
/* Write out sequence. */
{
struct dfm dfm;
int i;
char *seq = dna;
int size = dnaSize;

if (cgiBoolean("translate"))
    {
    int utr5 = 0;
    int maxProtSize = (dnaSize+2)/3;
    char *prot = needMem(maxProtSize + 1);
    if (cgiVarExists("utr5"))
        utr5 = cgiInt("utr5")-1;
    startRange -= utr5;
    endRange -= utr5;
    startRange /= 3;
    endRange /= 3;
    dna += utr5;
    seq = prot;
    for (size = 0; size < maxProtSize; ++size)
        {
        if ((*prot++ = lookupCodon(dna)) == 0)
            break;
        dna += 3;
        }
    *prot = 0;
    }
initDfm(&dfm, 10, 50, TRUE, hiliteRange, startRange, endRange, out);
for (i=0; i<size; ++i)
    dfmOut(&dfm, seq[i]);
}

int countUpper(char *s)
/* Count upper case chars */
{
char c;
int count = 0;
while (c = *s++)
    if (isupper(c)) ++count;
return count;
}

char *cloneUpperOnly(char *s)
/* Return string that is only upper case bits of s. */
{
char c;
char *d;
char *upper;
int upSize = countUpper(s);
upper = d = needMem(upSize+1);
while (c = *s++)
    {
    if (isupper(c))
        *d++ = c;
    }
*d = 0;
return upper;
}

void doMiddle()
{
char *seqName;
boolean intronsLowerCase = TRUE;
boolean intronsParenthesized = FALSE;
boolean hiliteNear = FALSE;
int startRange = 0;
int endRange = 0;
boolean gotRange = FALSE;
struct dnaSeq *cdnaSeq;
boolean isChromRange = FALSE;
DNA *dna;
char *translation = NULL;
boolean showTitle = FALSE;

seqName = cgiString("geneName");
seqName = trimSpaces(seqName);
if (cgiVarExists("intronsLowerCase"))
    intronsLowerCase = cgiBoolean("intronsLowerCase");
if (cgiVarExists("intronsParenthesized"))
    intronsParenthesized = cgiBoolean("intronsParenthesized");
if (cgiVarExists("startRange") && cgiVarExists("endRange" ))
    {
    startRange = cgiInt("startRange");
    endRange = cgiInt("endRange");
    gotRange = TRUE;
    }
if (cgiVarExists("hiliteNear"))
    {
    hiliteNear = TRUE;
    }
fprintf(stdout, "<P><TT>\n");

/* The logic here is a little complex to optimize speed.
 * If we can decide what type of thing the name refers to by
 * simply looking at the name we do.  Otherwise we have to
 * search the database in various ways until we get a hit. */
if (wormIsNamelessCluster(seqName))
    {
    isChromRange = TRUE;
    }
else if (wormIsChromRange(seqName))
    {
    isChromRange = TRUE;
    }
else if (getWormGeneDna(seqName, &dna, TRUE))
    {
    if (cgiBoolean("litLink"))
        {
        char nameBuf[64];
        int nameLen;
        char *geneName = NULL;
        char *productName = NULL;
        char *coding;
        int transSize;
        struct wormCdnaInfo info;

        printf("<H3>Information and Links for %s</H3>\n", seqName);
        if (wormInfoForGene(seqName, &info))
            {
            if (info.description)
                printf("<P>%s</P>\n", info.description);
            geneName = info.gene;
            productName = info.product;
            }
        else
            {
            if (wormIsGeneName(seqName))
                geneName = seqName;
            else if (wormGeneForOrf(seqName, nameBuf, sizeof(nameBuf)))
                geneName = nameBuf;
            }
        coding = cloneUpperOnly(dna);
        transSize = 1 + (strlen(coding)+2)/3;
        translation = needMem(1+strlen(coding)/3);
        dnaTranslateSome(coding, translation, transSize);
        freez(&coding);

        if (geneName)
            {
            printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/htbin-post/Entrez/query?form=4&db=m&term=C+elegans+%s&dispmax=50&relentrezdate=No+Limit\">",
                geneName);
            printf("PubMed search on gene: </A>%s<BR>\n", geneName);
            }
        if (productName)
            {
            char *encoded = cgiEncode(productName);
            printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/htbin-post/Entrez/query?form=4&db=m&term=%s&dispmax=50&relentrezdate=No+Limit\">",
                encoded);
            printf("PubMed search on product:</A> %s<BR>\n", productName);
            freeMem(encoded);
            }
        /* Process name to get rid of isoform letter for Proteome. */
        if (geneName)
            strcpy(nameBuf, geneName);
        else
            {
            strcpy(nameBuf, seqName);
#ifdef NEVER
            /* Sometimes Proteome requires the letter after the orf name
             * in alt-spliced cases, sometimes it can't handle it.... */
            nameLen = strlen(nameBuf);
            if (wormIsOrfName(nameBuf) && isalpha(nameBuf[nameLen-1]))
                {
                char *dotPos = strrchr(nameBuf, '.');
                if (dotPos != NULL && isdigit(dotPos[1]))
                    nameBuf[nameLen-1] = 0;
                }
#endif /* NEVER */
            }
        printf("<A HREF=\"http://www.proteome.com/databases/WormPD/reports/%s.html\">", nameBuf);
        printf("Proteome link on:</A> %s<BR>\n<BR>\n", nameBuf);


        printf("<A HREF=#DNA>Genomic DNA Sequence</A><BR>\n");
        if (hiliteNear)
            printf("<A HREF=\"#CLICKED\">Shortcut to where you clicked in gene</A><BR>");
        printf("<A HREF=#protein>Translated Protein Sequence</A><BR>\n");
        htmlHorizontalLine();
	printf("<A NAME=DNA></A>");
        printf("<H3>%s Genomic DNA sequence</H3>", seqName);
        }
    if (!intronsLowerCase)
        tolowers(dna);
    if (hiliteNear)
	{
	if (!gotRange)
	    {
	    double nearPos = cgiDouble("hiliteNear");
	    int rad = 5;
	    int dnaSize = strlen(dna);
	    long mid = (int)(dnaSize * nearPos);
	    startRange = mid - rad;
	    if (startRange < 0) startRange = 0;
	    endRange = mid + rad;
	    if (endRange >= dnaSize) endRange = dnaSize - 1;
	    }
	}
    outputSeq(dna, strlen(dna), hiliteNear, startRange, endRange, stdout);
    freez(&dna);
    }
else if (wormCdnaSeq(seqName, &cdnaSeq, NULL))
    {
    outputSeq(cdnaSeq->dna, cdnaSeq->size, FALSE, 0, 0, stdout);
    }
else
    {
    isChromRange = TRUE;
    }
if (isChromRange)
    {
    char *chromId;
    int start, end;
    char strand = '+';
    int size;

    if (!wormGeneRange(seqName, &chromId, &strand, &start, &end))
        errAbort("Can't find %s",seqName);
    size = end - start;
    if (intronsLowerCase)
        dna = wormChromPartExonsUpper(chromId, start, size);
    else
        {
        dna = wormChromPart(chromId, start, size);
        touppers(dna);
        }
    if (cgiVarExists("strand"))
        strand = cgiString("strand")[0];
    if (strand == '-')
        reverseComplement(dna, size);
    outputSeq(dna, size, FALSE, 0, 0, stdout);
    }
if (translation != NULL)
    {
    htmlHorizontalLine();
    printf("<A NAME=protein></A>");
    printf("<H3>Translated Protein of %s</H3>\n", seqName);
    outputSeq(translation, strlen(translation), FALSE, 0, 0, stdout);
    freez(&translation);
    }
fprintf(stdout, "</TT></P>\n");

}


int main(int argc, char *argv[])
{
char *geneName;
char title[256];

if (argc == 2 && sameWord(argv[1], "test"))
    putenv("QUERY_STRING=geneName=I:4000-5500&hiliteNear=0.917112&intronsLowerCase=On");
geneName = cgiString("geneName");
sprintf(title, "%s DNA Sequence", geneName);
dnaUtilOpen();
htmShell(title, doMiddle, "QUERY");
return 0;
}
