#include "common.h"
#include "portable.h"
#include "hash.h"

struct clonePos
/* Info on clone position. */
    {
    struct clonePos *next;
    char *cloneName;	/* Name (allocated in hash) */
    char *chromName;	/* Chromosome name (small, not allocated here. */
    int start;		/* Start coordinate. */
    int end;		/* End coordinate. */
    bool gotStart;	/* Got start coordinate? */
    bool gotEnd;	/* Got end coordinate? */
    };

char *inNames[] = {
    "CHROMOSOME_I.gff",
    "CHROMOSOME_II.gff",
    "CHROMOSOME_III.gff",
    "CHROMOSOME_IV.gff",
    "CHROMOSOME_V.gff",
    "CHROMOSOME_X.gff",
};

char *bigChromNames[] = {
    "CHROMOSOME_I",
    "CHROMOSOME_II",
    "CHROMOSOME_III",
    "CHROMOSOME_IV",
    "CHROMOSOME_V",
    "CHROMOSOME_X",
};

char *smallChromNames[] = {
    "I",
    "II",
    "III",
    "IV",
    "V",
    "X",
};

char *smallName(char *bigName)
{
int i;
for (i=0; i<ArraySize(bigChromNames); ++i)
    {
    if (sameString(bigChromNames[i], bigName))
        return smallChromNames[i];
    }
errAbort("%s isn't a chromosome", bigName);
}

char *unquote(char *s)
/* Remove opening and closing quotes from string s. */
{
int len =strlen(s);
if (s[0] != '"')
    errAbort("Expecting begin quote on %s\n", s);
if (s[len-1] != '"')
    errAbort("Expecting end quote on %s\n", s);
s[len-1] = 0;
return s+1;
}


int main(int argc, char *argv[])
{
char *outName;
char *gffDir;
char *inName;
int i;
FILE *in, *c2c;
char line[4*1024];
char origLine[4*1024];
int lineCount = 0;
char *words[4*256];
int wordCount;
struct hash *cloneHash = newHash(0);
struct clonePos *cloneList = NULL, *clone;
char *gffSource;
bool gotLeft, gotRight;

if (argc != 3)
    {
    errAbort("makec2c - creates a cosmid chromosome offset index file from Sanger .gffs\n"
           "usage:\n"
           "      makec2c gffDir c2c\n"
           "This will create the file c2c with the index.\n");
    }
gffDir = argv[1];
outName = argv[2];
c2c = mustOpen(outName, "w");
if (!setCurrentDir(gffDir))
    errAbort("Couldn't cd to %s", gffDir);
for (i=0; i<ArraySize(inNames); ++i)
    {
    inName = inNames[i];
    printf("Processing %s\n", inName);
    in = mustOpen(inName, "r");
    while (fgets(line, sizeof(line), in))
        {
        ++lineCount;
        if (strncmp(line, "CHROMOSOME", 10) == 0)
            {
            strcpy(origLine, line);
            wordCount = chopLine(line, words);
            if (wordCount < 8)
		continue;
	    gffSource = words[1];
	    gotLeft = sameString("Clone_left_end", gffSource);
	    gotRight = sameString("Clone_right_end", gffSource);
	    if (gotLeft || gotRight)
	        {
		char *cloneName = words[7];
		char *header = "Clone ";
		if (!startsWith(header,  cloneName))
		    errAbort("Clone end without %s line %d of %s", header, lineCount, inName);
                cloneName = unquote(cloneName + strlen(header));
		if ((clone = hashFindVal(cloneHash, cloneName)) == NULL)
		    {
		    AllocVar(clone);
		    slAddHead(&cloneList, clone);
		    hashAddSaveName(cloneHash, cloneName, clone, &clone->cloneName);
		    clone->chromName = smallName(words[0]);
		    }
		if (gotLeft)
		    {
		    clone->start = atoi(words[2]) - 1;
		    clone->gotStart = TRUE;
		    }
		else if (gotRight)
		    {
		    clone->end = atoi(words[2]) - 1;
		    clone->gotEnd = TRUE;
		    }
		}
            }
        }
    fclose(in);
    slReverse(&cloneList);
    for (clone = cloneList; clone != NULL; clone = clone->next)
        {
	if (clone->gotStart && clone->gotEnd)
	    {
	    fprintf(c2c, "%s:%d-%d + %s\n", clone->chromName, clone->start, clone->end, clone->cloneName);
	    }
	else
	    {
	    warn("Only got one end of %s", clone->cloneName);
	    }
	}
    slFreeList(&cloneList);
    }
return 0;
}
