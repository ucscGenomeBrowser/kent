/* Undo word wrapping and some other general munging of map file. */
#include "common.h"
#include "linefile.h"


boolean okCenter(char *s)
/* Is center ok for here... */
{
return sameString(s, "NA") || sameString(s, "WUGSC")
	|| sameString(s, "WIBR") || sameString(s, "SC")
	|| sameString(s, "JAPAN") || sameString(s, "BCM");
}

void unwrap(char *inName, char *outName)
/* Filter and unwrap map file. */
{
struct lineFile *in = lineFileOpen(inName, TRUE);
FILE *out = mustOpen(outName, "w");
int lineSize;
char *line;
char *words[20];
int wordCount;
boolean inside = FALSE;
int i;
char pieceBuf[512];
int bogusCtgCount = 2;	/* 2 added by hand. */
char bogusCtgBuf[16];

while (lineFileNext(in, &line, &lineSize))
    {
    if (startsWith("start human SUPERLINK", line))
	{
	inside = TRUE;
	fprintf(out, "%s\n", line);
	}
    else if (startsWith("end human SUPERLINK", line))
	{
	inside = FALSE;
	fprintf(out, "%s\n", line);
	}
    else if (inside)
	{
	if (line[0] == '@' || line[0] == '*' || startsWith("COMMITTED", line))
	    fprintf(out, "%s\n", line);
	else
	    {
	    int writtenWordCount = 0;
	    int newWordCount;
	    char *pieces = pieceBuf;
	    char *nextPieces;
	    for (;;)
		{
		strcpy(pieces, line);
		nextPieces = pieces + strlen(pieces) + 1;
		if (sizeof(pieceBuf) - (nextPieces - pieceBuf) < 100)
		    errAbort("Line continuation too long line %d of %s", in->lineIx, in->fileName);
		wordCount = chopByWhite(pieces, words+writtenWordCount, 
			ArraySize(words)-writtenWordCount);
		pieces = nextPieces;
		newWordCount = writtenWordCount + wordCount;
		if ((writtenWordCount > 0 && newWordCount > 7 ) || 
		    line[0] == '@' || line[0] == '*' ||
		    startsWith("COMMITTED", line))
		    {
		    lineFileReuse(in);
		    break;
		    }
		writtenWordCount = newWordCount;
		if (writtenWordCount >= 7)
		    {
		    break;
		    }
		lineFileNext(in, &line, &lineSize);
		}
	    if (writtenWordCount == 6)
		{
		if (startsWith("ctg", words[2]))
		    {
		    warn("Fixing missing center line %d of %s", in->lineIx, in->fileName);
		    for (i=6; i>2; --i)
			{
			words[i] = words[i-1];
			}
		    words[2] = "UNKNOWN";
		    writtenWordCount = 7;
		    }
		else
		    {
		    errAbort("Stange 6 word line %d of %s", in->lineIx, in->fileName);
		    }
		}
	    else if (writtenWordCount == 5)
		{
		if (!startsWith("ctg", words[3]))
		    {
		    warn("Fixing missing ctg line %d of %s", in->lineIx, in->fileName);
		    sprintf(bogusCtgBuf, "ctg_unknown_%d", ++bogusCtgCount);
		    words[6] = words[4];
		    words[5] = words[3];
		    words[4] = "10";
		    words[3] = bogusCtgBuf;
		    writtenWordCount = 7;
		    }
		else if (!okCenter(words[2]))
		    {
		    errAbort("Stange 5 word line %d of %s", in->lineIx, in->fileName);
		    }
		}
	    else if (writtenWordCount != 7)
		{
		errAbort("Stange %d word line %d of %s", writtenWordCount, in->lineIx, in->fileName);
		}
	    if (!startsWith("ctg", words[3]))
		errAbort("Strange line %d of %s", in->lineIx, in->fileName);
	    for (i=0; i<writtenWordCount; ++i)
		fprintf(out, "%s\t", words[i]);
	    fprintf(out, "\n");

	    if (writtenWordCount != 7 && writtenWordCount != 5)
		warn("Odd word count %d line %d of %s", writtenWordCount,
			in->lineIx, in->fileName);
	    }
	}
    }
lineFileClose(&in);
fclose(out);
}

void usage()
/* Print usage and exit. */
{
errAbort("unwrap in out");
}

main(int argc, char *argv[])
{
if (argc != 3)
    usage();
unwrap(argv[1], argv[2]);
}
