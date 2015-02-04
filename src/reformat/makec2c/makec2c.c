#include "common.h"
#include "portable.h"
#include "linefile.h"
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

#if defined(GZ_FILENAMES)
char *inNames[] = {
    "CHROMOSOME_I.gff.gz",
    "CHROMOSOME_II.gff.gz",
    "CHROMOSOME_III.gff.gz",
    "CHROMOSOME_IV.gff.gz",
    "CHROMOSOME_V.gff.gz",
    "CHROMOSOME_X.gff.gz",
};
#else
char *inNames[] = {
    "CHROMOSOME_I.gff",
    "CHROMOSOME_II.gff",
    "CHROMOSOME_III.gff",
    "CHROMOSOME_IV.gff",
    "CHROMOSOME_V.gff",
    "CHROMOSOME_X.gff",
};
#endif

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
return ((char *)NULL);
}

char *findSmallName(char *smallName)
/* Return small name in array (so don't have to alloc it
 * over and over) */
{
int i;
for (i=0; i<ArraySize(smallChromNames); ++i)
    {
    if (sameString(smallChromNames[i], smallName))
        return smallChromNames[i];
    }
errAbort("%s isn't a chromosome", smallName);
return ((char *)NULL);
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

struct hash *readOldC2c(char *fileName)
/* Read old c2c file. */
{
struct clonePos *pos;
struct hash *hash = newHash(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[3];
char *parts[4];
int partCount;
int count = 0;

while (lineFileRow(lf, row))
     {
     if ((partCount = chopString(row[0], ":-", parts, ArraySize(parts))) != 3)
         errAbort("Bad line %d of %s", lf->lineIx, lf->fileName);
     AllocVar(pos);
     hashAddSaveName(hash, row[2], pos, &pos->cloneName);
     pos->chromName = findSmallName(parts[0]);
     pos->start = atoi(parts[1]);
     pos->end = atoi(parts[2]);
     ++count;
     }
printf("Read %d old clones in %s\n", count, fileName);
return hash;
}

#if defined(GZ_FILENAMES)
static FILE *
pipeOpen(char * fileName)
{
char cmd[1024];
FILE * fh;

strcpy(cmd, "gzip -dc ");  /* use "gzip -c" for zwrite */
strncat(cmd, fileName, sizeof(cmd)-strlen(cmd));
fh = popen(cmd, "r");  /* use "w" for zwrite */
if ( (FILE *) NULL == fh )
    errAbort("makec2c: - can not gzip open: %s\n", fileName );
return fh;
}
#endif	/*	GZ_FILENAMES	*/

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
char *gffType;
bool gotBoth, gotLeft, gotRight;
int cloneCount;
struct hash *oldCloneHash = NULL;

if (argc != 4)
    {
    errAbort("makec2c - creates a cosmid chromosome offset index file from Sanger .gffs\n"
           "usage:\n"
           "      makec2c gffDir c2c oldC2c\n"
           "This will create the file c2c with the index. You can use\n"
	   "/dev/null for the oldC2c.  It's just used to fill in missing clone sizes\n");
    }
gffDir = argv[1];
outName = argv[2];
oldCloneHash = readOldC2c(argv[3]);
c2c = mustOpen(outName, "w");
setCurrentDir(gffDir);
for (i=0; i<ArraySize(inNames); ++i)
    {
    inName = inNames[i];
    printf("Processing %s\n", inName);
#if defined(GZ_FILENAMES)
    in = pipeOpen(inName);
#else
    in = mustOpen(inName, "r");
#endif
    lineCount = 0;
    while (fgets(line, sizeof(line), in))
        {
        ++lineCount;
        if (strncmp(line, "CHROMOSOME", 10) == 0)
            {
            strcpy(origLine, line);
            wordCount = chopTabs(line, words);
            if (wordCount < 9)
		continue;
	    gffType = words[2];
	    gotLeft = sameString("Clone_left_end", gffType);
	    gotRight = sameString("Clone_right_end", gffType);
	    gotBoth = (sameString("Genomic_canonical", words[1]) && sameString("Sequence", gffType));
	    if (gotLeft || gotRight || gotBoth)
	        {
		char *cloneName = trimSpaces(words[8]);
		char *header;
		
		if (gotBoth)
		    header = "Sequence ";
		else
		    header = "Clone ";
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
		    clone->start = atoi(words[3]) - 1;
		    clone->gotStart = TRUE;
		    }
		else if (gotRight)
		    {
		    clone->end = atoi(words[4]);
		    clone->gotEnd = TRUE;
		    }
		else if (gotBoth)
		    {
		    clone->start = atoi(words[3]) - 1;
		    clone->gotStart = TRUE;
		    clone->end = atoi(words[4]);
		    clone->gotEnd = TRUE;
		    }
		}
            }
        }
    fclose(in);
    slReverse(&cloneList);
    cloneCount = slCount(cloneList);
    if (cloneCount == 0)
        errAbort("No clones in %s", inName);
    printf("Got %d clones in %s\n", cloneCount, inName);
    for (clone = cloneList; clone != NULL; clone = clone->next)
        {
	if (!clone->gotStart)
	    {
	    struct clonePos *old = hashFindVal(oldCloneHash, clone->cloneName);
	    if (old != NULL)
		{
	        clone->gotStart = clone->gotEnd - (old->gotEnd - old->gotStart);
		warn("Filling in %s start from old size", clone->cloneName);
		clone->gotStart = TRUE;
		}
	    }
	if (!clone->gotEnd)
	    {
	    struct clonePos *old = hashFindVal(oldCloneHash, clone->cloneName);
	    if (old != NULL)
		{
	        clone->gotEnd = clone->gotStart + (old->gotEnd - old->gotStart);
		warn("Filling in %s end from old size", clone->cloneName);
		clone->gotEnd = TRUE;
		}
	    }
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
    cloneHash = newHash(0);
    }
return 0;
}
