/* joinToPatch - Convert Elia's parsed GenBank joins into an equivalent .agpPatch file.. */
#include "common.h"
#include "linefile.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "joinToPatch - Convert Elia's parsed GenBank joins into an equivalent .agpPatch file.\n"
  "usage:\n"
  "   joinToPatch inFile out.agpPatch\n");
}

void joinToPatch(char *inName, char *outName)
/* joinToPatch - Convert Elia's parsed GenBank joins into an equivalent .patchAgp file.. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
char *ntRow[2];
char *line, *s;
static char *commaSep[1024], *colonSep[3], *dotSep[3];
int commaCount, colonCount, dotCount, lineSize, i, len;
char ntName[256], cloneName[256];
boolean breakJoin, isComplement;
int start, end, pos, size;
FILE *f = mustOpen(outName, "w");

for (;;)
    {
    if (!lineFileRow(lf, ntRow))
         break;
    strcpy(ntName, ntRow[0]);
    if (!startsWith("NT_", ntName))
        errAbort("Expecting NT_ line %d of %s", lf->lineIx, lf->fileName);
    breakJoin = FALSE;
    pos = 0;
    while (!breakJoin)
        {
	if (!lineFileNext(lf, &line, &lineSize))
	    errAbort("Missing join line at end of %s", lf->fileName);
	line = trimSpaces(line);
	lineSize = strlen(line);
	breakJoin = (line[lineSize-1] != ',');
	if (startsWith("join(", line))
	    line += strlen("join(");
	commaCount = chopString(line, ",", commaSep, ArraySize(commaSep));
	for (i=0; i<commaCount; ++i)
	    {
	    s = commaSep[i];
	    if (startsWith("complement(", s))
		{
		isComplement = TRUE;
		s += strlen("complement(");
		len = strlen(s);
		if (s[len-1] != ')')
		    errAbort("Missing paren in complement line %d of %s", 
		    	lf->lineIx, lf->fileName);
		s[len-1] = 0;
		}
	    else
	        isComplement = FALSE;
	    colonCount = chopString(s, ":", colonSep, ArraySize(colonSep));
	    if (colonCount != 2)
	        errAbort("Expecting 1 colon line %d of %s, got %d", 
			lf->lineIx, lf->fileName, colonCount-1);
	    strcpy(cloneName, colonSep[0]);
	    dotCount = chopByChar(colonSep[1], '.', dotSep, ArraySize(dotSep));
	    if (dotCount != 3)
	        errAbort("Expecting start..end line %d of %s", lf->lineIx, lf->fileName);
	    if (!isdigit(dotSep[0][0]) || !isdigit(dotSep[2][0]))
	        errAbort("Expecting start..end numbers %d of %s", lf->lineIx, lf->fileName);
	    start = atoi(dotSep[0])-1;
	    end = atoi(dotSep[2]);
	    size = end - start;
	    fprintf(f, "%s\txxx\t%d\t%d\t%c1\tF\t%s\t%d\t%d\n",
	    	ntName, pos+1, pos+size,
		(isComplement ? '-' : '+'),
		cloneName, start+1, end);
	    pos += size;
	    }
	}
    }
lineFileClose(&lf);
fclose(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
joinToPatch(argv[1], argv[2]);
return 0;
}
