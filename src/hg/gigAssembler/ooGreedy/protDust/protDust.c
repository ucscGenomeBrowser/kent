#include "common.h"
#include "cheapcgi.h"
#include "htmshell.h"

int noneOrNumber(char *s)
{
return atoi(s);
}

void dustProt(char *in,char *out)
{
char c;
while ((c = *in++) != 0)
    {
    if (isalpha(c)) *out++ = c;
    }
*out++ = 0;
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

void doMiddle()
{
char *input, *dustedProt;
long len;
char *linesEvery, *spacesEvery;
boolean showNumbers;
char *caseChoice;

input = cgiString("TextArea");
linesEvery = cgiString("linesEvery");
spacesEvery = cgiString("spacesEvery");
showNumbers = cgiBoolean("showNumbers");
caseChoice = cgiString("caseChoice");

len = strlen(input);
if ((dustedProt = malloc(len+1)) == NULL)
    errAbort("Out of memory");
dustProt(input, dustedProt);

formatOutput(dustedProt, 
	    noneOrNumber(spacesEvery), noneOrNumber(linesEvery),
	    showNumbers, caseChoice);
}

int main()
{
htmShell("Protein Duster Output", doMiddle, "POST");
return 0;
}
