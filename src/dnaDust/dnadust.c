#include "common.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "dnautil.h"

/* Return first non-alpha char */
char *skipAlNum(char *s)
{
for (;;)
    {
    if (!isalnum(*s)) return s;
    s += 1;
    }
}

/* return first alpha char */
char *skipToAlNum(char *s)
{
char c;
for (;;)
    {
    c = *s;
    if (c == 0 || isalnum(c))
	return s;
    ++s;
    }
}

/* Return TRUE if all chars are DNA type */
boolean allNt(char *s,int size)
{
while (--size >= 0)
    if (ntChars[*s++] == 0) return FALSE;
return TRUE;
}

void dustDna(char *in,char *out)
{
char *wordEnd;
int wordLen;
while (*in != 0)
    {
    in = skipToAlNum(in);
    wordEnd = skipAlNum(in);
    wordLen = wordEnd - in;
    if (allNt(in, wordLen))
	{
	memcpy(out, in, wordLen);
	out += wordLen;
	}
    in = wordEnd;
    }
*out++ = 0;
}

int noneOrNumber(char *s)
{
return atoi(s);
}

void formatOutput(char *output, int spacer, int liner, 
	boolean showNumbers, char *caseChoice)
{
int countSpace = 0;
int countLine = 0;
int baseCount = 0;
char c;

if (differentWord(caseChoice, "upper") == 0)
    touppers(output);
else if (differentWord(caseChoice, "lower") == 0)
    tolowers(output);
printf("<TT>\n");
for (;;)
    {
    c = *output++;
    if (c == 0)
	break;
    ++baseCount;
    putc(c, stdout);
    if (spacer != 0 && ++countSpace == spacer)
	{
	putc(' ', stdout);
	countSpace = 0;
	}
    if (liner != 0 && ++countLine == liner)
	{
	if (showNumbers)
	   printf(" %d", baseCount);
	printf("<BR>\n");
	countLine = 0;
	}
    }
if (countLine != 0)
    {
    if (showNumbers)
       printf(" %d", baseCount);
    printf("<BR>\n");
    }
printf("</TT>\n");
}

void eraseLowerCase(char *s)
{
char *in, *out;
char c;

in = out = s;
for (;;)
    {
    c = *in++;
    if (c == 0) 
	break;
    if (isupper(c))
	*out++ = c;
    }
*out++ = 0;
}

char *customTranslate(char *dna, boolean searchAug)
{
int dnaLength;
int protLength;
char *protein;
char aa;
int i;

tolowers(dna);
toRna(dna);
if (searchAug)
    {
    dna = strstr(dna, "aug");
    if (dna == NULL)
	errAbort("No start codon in input.");
    }
dnaLength = strlen(dna);
protLength = dnaLength/3;
protein = (char *)needMem(protLength+1);
for (i=0; i<protLength; i += 1)
    {
    aa = lookupCodon(dna);
    dna += 3;
    protein[i] = aa;
    if (aa == 0)
	break;
    }
protein[protLength] = 0;
return protein;
}

void complementDna(DNA *dna)
{
DNA b;
while ((b = *dna) != 0)
    *dna++ = ntCompTable[b];
}

void doMiddle()
{
char *input, *dustedDna;
long len;
char *linesEvery, *spacesEvery;
boolean showNumbers;
char *caseChoice;
boolean asProtein;
boolean searchAug;
boolean ignoreLower;
char *strand;

input = cgiString("TextArea");
linesEvery = cgiString("linesEvery");
spacesEvery = cgiString("spacesEvery");
showNumbers = cgiBoolean("showNumbers");
caseChoice = cgiString("caseChoice");
asProtein = cgiBoolean("asProtein");
searchAug = cgiBoolean("searchAug");
ignoreLower = cgiBoolean("ignoreLower");
strand = cgiString("strand");

len = strlen(input);
if ((dustedDna = malloc(len+1)) == NULL)
    errAbort("Out of memory");
dustDna(input, dustedDna);
if (ignoreLower)
    {
    eraseLowerCase(dustedDna);
    if (strlen(dustedDna) == 0)
	{
	errAbort("Nothing left after removing introns. Perhaps you should go back and uncheck the 'lower case is intron' button");
	}
    }

if (!differentWord("reverse complement", strand))
    reverseComplement(dustedDna, strlen(dustedDna));
else if (!differentWord("reverse", strand))
    reverseBytes(dustedDna, strlen(dustedDna));
else if (!differentWord("complement", strand))
    complementDna(dustedDna);

if (asProtein)
    {
    char *prot = customTranslate(dustedDna, searchAug);
    formatOutput(prot,
	    noneOrNumber(spacesEvery), noneOrNumber(linesEvery),
	    showNumbers, caseChoice);
    }
else
    {
    formatOutput(dustedDna, 
	    noneOrNumber(spacesEvery), noneOrNumber(linesEvery),
	    showNumbers, caseChoice);
    }
}

int main(int argc, char *argv[])
{
dnaUtilOpen();
htmShell("DNA Duster Output", doMiddle, "POST");
return 0;
}
