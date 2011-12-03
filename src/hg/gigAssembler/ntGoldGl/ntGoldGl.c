/* ntGoldGl - Update gold.NN and gl.NN files to unpack NT contig info.. */
#include "common.h"
#include "hash.h"
#include "portable.h"
#include "linefile.h"
#include "agpFrag.h"
#include "agpGap.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "ntGoldGl - Update gold.NN.noNt and gl.NN.noNt files to unpack NT contig info.\n"
  "Put updates in gold.NN and gl.NN"
  "usage:\n"
  "   ntGoldGl cloneSizes nt.agp NN ctgDir(s)\n");
}

struct clone
/* Info on a clone. */
    {
    struct clone *next;
    char *name;		/* Clone name. */
    int size;		/* Clone size. */
    char *startFrag;	/* First frag. */
    char *endFrag;	/* Last frag. */
    int ntOrientation;	/* Orientation in NT contig. */
    int ntStart;	/* Start/end in NT contig. */
    int ntEnd;		/* Start/end in NT contig. */
    int goldStart;	/* Start/end of part used in NT contig. */
    int goldEnd;	/* Start/end of part used in NT contig. */
    char seqType;	/* F for finished, D for draft, etc. */
    char *fragName;	/* Name including version_frag. */
    struct ntContig *nt;/* Associated NT contig if any. */
    int ntOrder;	/* Position in NT contig. */
    };

struct cloneRef
/* Reference to a clone. */
    {
    struct cloneRef *next;
    struct clone *ref;
    };

struct ntContig
/* Info on an NT contig. */
    {
    struct ntContig *next;
    char *name;			/* Name of contig. */
    struct cloneRef *cloneList;	/* List of clones. */
    int size;			/* Size. */
    int sumSize;		/* Sum of sizes from fragments. */
    boolean problem;		/* Is problematic. */
    };

void readCloneSizes(char *fileName, struct clone **retCloneList, struct hash **retCloneHash)
/* Read in clone sizes from file into list/hash of them.  Some clone fields
 * filled in later. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[4];
struct clone *cloneList = NULL, *clone;
struct hash *cloneHash = newHash(16);

while (lineFileRow(lf, row))
    {
    AllocVar(clone);
    slAddHead(&cloneList, clone);
    hashAddSaveName(cloneHash, row[0], clone, &clone->name);
    clone->size = atoi(row[1]);
    clone->startFrag = cloneString(row[2]);
    clone->endFrag = cloneString(row[3]);
    }
lineFileClose(&lf);
slReverse(&cloneList);
*retCloneList = cloneList;
*retCloneHash = cloneHash;
}

void readPatch(char *fileName, struct hash *cloneHash, 
	struct ntContig **retNtList, struct hash **retNtHash)
/* Read nt.agp file into clone/hash.  */
{
struct ntContig *ntList = NULL, *nt = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[9];
struct agpFrag frag;
struct clone *clone, *ntClone, *lastClone = NULL;
struct cloneRef *ref;
struct hash *ntHash = newHash(0);
char cloneName[128];
char fragName[128];
char c;
int ntOrder = 0;

while (lineFileRow(lf, row))
    {
    agpFragStaticLoad(row, &frag);
    // file is 1-based but agpFragLoad() now assumes 0-based:
    frag.chromStart -= 1;
    frag.fragStart  -= 1;
    if (nt == NULL || !sameString(frag.chrom, nt->name))
        {
	AllocVar(nt);
	slAddHead(&ntList, nt);
	if (hashLookup(ntHash, frag.chrom) != NULL)
	    errAbort("NT contig %s repeated line %d of %s", row[0], lf->lineIx, lf->fileName);
	hashAddSaveName(ntHash, frag.chrom, nt, &nt->name);
	lastClone = NULL;
	ntOrder = 0;
	}
    strcpy(cloneName, frag.frag);
    chopSuffix(cloneName);
    clone = hashMustFindVal(cloneHash, cloneName);
    clone->ntStart = frag.chromStart;
    clone->ntEnd = frag.chromEnd;
    if (clone->nt != NULL)
	{
        warn("Clone %s trying to be in two NT contigs (%s and %s) line %d of %s",
		clone->name, clone->nt->name, nt->name, lf->lineIx, lf->fileName);
	nt->problem = TRUE;
	}
    clone->nt = nt;
    c = frag.strand[0];
    if (c == '-')
	clone->ntOrientation = -1;
    else if (c == '+')
	clone->ntOrientation = +1;
    else
	errAbort("Expecting +1 or -1 field 5, line %d, file %s", lf->lineIx, lf->fileName);
    c = frag.type[0];
    if (c == 'F' || c == 'D' || c == 'P')
	clone->seqType =  c;
    else
	errAbort("Expecting F, D, or P  field 6, line %d, file %s", lf->lineIx, lf->fileName);
    sprintf(fragName, "%s_1", frag.frag);
    clone->fragName = cloneString(fragName);
    clone->goldStart = frag.fragStart;
    clone->goldEnd = frag.fragEnd;
    clone->ntOrder = ntOrder++;

    /* Add ref to NT. */
    AllocVar(ref);
    ref->ref = clone;
    slAddTail(&nt->cloneList, ref);

    /* Do a few tests. */
    if (clone->goldStart >= clone->goldEnd)
	{
	warn("Clone %s end before start (%d before %d) line %d of %s", 
		clone->name, clone->goldStart, clone->goldEnd, lf->lineIx, lf->fileName);
	nt->problem = TRUE;
	}
    if (clone->ntStart >= clone->ntEnd)
	{
	warn("Clone %s NT end before NT start line %d of %s", 
		clone->name, lf->lineIx, lf->fileName);
	nt->problem = TRUE;
	}
    if (clone->goldEnd > clone->size)
	{
	if (sameString(clone->startFrag, clone->endFrag))
	    {
	    warn("Clone %s end position %d, clone size %d, line %d of %s", 
		clone->name, clone->goldEnd, clone->size, lf->lineIx, lf->fileName);
	    nt->problem = TRUE;
	    }
	}
    if (clone->ntEnd - clone->ntStart != clone->goldEnd - clone->goldStart)
        {
	warn("Size not the same in NT contig as in clone %s (%d vs %d) line %d of %s",
		clone->name,
		clone->ntEnd - clone->ntStart, clone->goldEnd-clone->goldStart,
		lf->lineIx, lf->fileName);
	nt->problem = TRUE;
	}
    nt->sumSize += clone->goldEnd - clone->goldStart;
    ntClone = hashFindVal(cloneHash, nt->name);
    if (ntClone != NULL && clone->ntEnd > ntClone->size)
	{
	warn("Clone %s NT end position %d, NT size %d, line %d of %s", 
	    clone->name, clone->ntEnd, ntClone->size, lf->lineIx, lf->fileName);
	nt->problem = TRUE;
	}
    if (ntClone != NULL)
	nt->size = ntClone->size;
    else
        nt->size = clone->size;		/* This happens for single-clone NT contigs only. */
    if (lastClone != NULL)
        {
	if (lastClone->ntEnd != clone->ntStart)
	    {
	    warn("last clone (%s)'s end doesn't match with current clone (%s)'s start line %d of %s",
	    	lastClone->name, clone->name, lf->lineIx, lf->fileName);
	    }
	}
    lastClone = clone;
    }

lineFileClose(&lf);
slReverse(&ntList);

for (nt = ntList; nt != NULL; nt = nt->next)
    {
    if (nt->sumSize != nt->size)
        {
	warn("Sum of fragments of %s is %d, but size is supposed to be %d",
		nt->name, nt->sumSize, nt->size);
	nt->problem = TRUE;
	}
    }
*retNtList = ntList;
*retNtHash = ntHash;
}

void patchOneGold(char *inName, struct hash *ntHash, char *outName)
/* Make patched copy of inName in outName. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
int lineOutIx = 0;
int i;
char *words[16];
int wordCount, lineSize;
char choppedLine[512], *line;
struct ntContig *nt;
int startOffset = 0, endOffset;


while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
        continue;
    strcpy(choppedLine, line);
    wordCount = chopLine(choppedLine, words);
    if (wordCount == 0)
        continue;
    if (wordCount != 8 && wordCount != 9)
        errAbort("Expecting 8 or 9 words line %d of %s", lf->lineIx, lf->fileName);
    if (sameString(words[4], "N") || !startsWith("NT_", words[5]))
        {
	/* Only thing that might change here is the order index. */
	for (i=0; i<wordCount; ++i)
	    {
	    if (i == 3)
		fprintf(f, "%d\t", ++lineOutIx);
	    else
		{
		fputs(words[i], f);
		if (i == wordCount-1)
		    fputc('\n', f);
		else
		    fputc('\t', f);
		}
	    }
	}
    else  /* It's an NT contig */
        {
	char ntName[128];
	strcpy(ntName, words[5]);
	chopSuffix(ntName);
	startOffset = atoi(words[1]) - 1;
	endOffset = atoi(words[2]);
	nt = hashFindVal(ntHash, ntName);
	if (nt == NULL)	/* Just leave a gap - no good info here. */
	    {
	    fprintf(f, "%s\t%s\t%s\t%d\tN\t%d\tbadNt\tnull\n",
	    	words[0], words[1], words[2], ++lineOutIx, endOffset - startOffset);
	    }
	else    /* Unpack the contig. */
	    {
	    struct cloneRef *ref;
	    struct clone *clone;
	    int asmStart, asmEnd, cloneStart, cloneEnd;	/* Start/end in assembly and clone coordinates. */
	    int uStart, uEnd;		/* Start/end of NT contig used. */
	    int skipStart, skipEnd;	/* Amount of a fragment of NT contig to skip. */
	    boolean isRev = (words[8][0] == '-');
	    int orientation;
	    
	    uStart = atoi(words[6]) - 1;
	    uEnd = atoi(words[7]);
	    asmStart = startOffset;
	    if (isRev)
	        slReverse(&nt->cloneList);
	    for (ref = nt->cloneList; ref != NULL; ref = ref->next)
	        {
		clone = ref->ref;
		orientation = clone->ntOrientation;
		skipStart = uStart - clone->ntStart;
		if (skipStart < 0) skipStart = 0;
		skipEnd = clone->ntEnd - uEnd;
		if (skipEnd < 0) skipEnd = 0;
		if (!isRev)
		    {
		    if (orientation > 0)
		        {
			cloneStart = clone->goldStart + skipStart;
			cloneEnd = clone->goldEnd - skipEnd;
			}
		    else
		        {
			cloneStart = clone->goldStart + skipEnd;
			cloneEnd = clone->goldEnd - skipStart;
			}
		    }
		else	/* NT contig as a whole is on minus strand. */
		    {
		    if (orientation > 0)
		        {
			cloneStart = clone->goldStart + skipStart;
			cloneEnd = clone->goldEnd - skipEnd;
			}
		    else
		        {
			cloneStart = clone->goldStart + skipEnd;
			cloneEnd = clone->goldEnd - skipStart;
			}
		    orientation = -orientation;
		    }
			
		if (cloneStart < cloneEnd)
		    {
		    asmEnd = asmStart + cloneEnd - cloneStart;
		    fprintf(f, "%s\t%d\t%d\t%d\t%c\t%s\t%d\t%d\t%c\n",
			    words[0], asmStart+1, asmEnd,
			    ++lineOutIx, clone->seqType, clone->fragName,  
			    cloneStart+1, cloneEnd,
			    (orientation < 0 ? '-' : '+') );
		    asmStart = asmEnd;
		    }
		}
	    if (isRev)
	        slReverse(&nt->cloneList);
	    }
	}
    }
lineFileClose(&lf);
carefulClose(&f);
}


void patchOneGl(char *inName, struct hash *ntHash, char *outName)
/* Make patched copy of inName in outName. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
int lineOutIx = 0;
int i;
char *row[4];
int lineSize;
char choppedLine[512], *line;
struct ntContig *nt;
int startOffset = 0, endOffset;


while (lineFileRow(lf, row))
    {
    if (startsWith("NT_", row[0]))
        {
	char ntName[128];
	strcpy(ntName, row[0]);
	chopSuffix(ntName);
	startOffset = atoi(row[1]);
	endOffset = atoi(row[2]);
	nt = hashFindVal(ntHash, ntName);
	if (nt == NULL)	/* Just leave a gap - no good info here. */
	    {
	    warn("Couldn't find %s", ntName);
	    }
	else    /* Unpack the contig. */
	    {
	    struct cloneRef *ref;
	    struct clone *clone;
	    boolean isRev = (row[3][0] == '-');
	    
	    if (isRev)
	        {
		int start, end, cloneEnd;
	        slReverse(&nt->cloneList);
		for (ref = nt->cloneList; ref != NULL; ref = ref->next)
		    {
		    clone = ref->ref;
		    cloneEnd = clone->ntEnd;
		    if (clone->ntOrientation > 0)
		        cloneEnd += clone->size - clone->goldEnd;
		    else
		        cloneEnd += clone->goldStart;
		    start = startOffset + (nt->size - cloneEnd);
		    end = start + clone->size;
		    fprintf(f, "%s %d %d %c\n",
		        clone->fragName, 
			start, end,
			(clone->ntOrientation < 1 ? '+' : '-'));
		    }
	        slReverse(&nt->cloneList);
		}
	    else
	        {
		for (ref = nt->cloneList; ref != NULL; ref = ref->next)
		    {
		    int start, end;

		    clone = ref->ref;
		    start = startOffset + clone->ntStart;
		    if (clone->ntOrientation > 0)
		        start -= clone->goldStart;
		    else
		        start -= clone->size - clone->goldEnd;
		    end = start + clone->size;
		    fprintf(f, "%s %d %d %c\n",
		        clone->fragName, start, end, 
			(clone->ntOrientation < 1 ? '-' : '+'));
		    }
		}
	    }
	}
    else
        {
	fprintf(f, "%s %s %s %s\n", row[0], row[1], row[2], row[3]);
	}
    }
lineFileClose(&lf);
carefulClose(&f);
}


void ntGoldGl(char *cloneSizes, char *ntAgp, char *oogVersion, int ctgCount, char *ctgNames[])
/* ntGoldGl - Update gold.NN and gl.NN files to unpack NT contig info.. */
{
struct clone *cloneList = NULL, *clone;
struct ntContig *ntList = NULL, *nt;
struct hash *cloneHash = NULL, *ntHash = NULL;
char oldName[512], newName[512], *contig;
int i;

readCloneSizes(cloneSizes, &cloneList, &cloneHash);
printf("Read %d clones in %s\n", slCount(cloneList), cloneSizes);

readPatch(ntAgp, cloneHash, &ntList, &ntHash);
printf("Read %d nt contigs in %s\n", slCount(ntList), ntAgp);

printf("Working on %d contigs\n", ctgCount);
for (i=0; i<ctgCount; ++i)
    {
    contig = ctgNames[i];
    printf(" %s\n", contig);
    sprintf(oldName, "%s/gold.%s.noNt", contig, oogVersion);
    if (!fileExists(oldName))
        {
	warn("%s doesn't exist", oldName);
	continue;
	}
    sprintf(newName, "%s/gold.%s", contig, oogVersion);
    patchOneGold(oldName, ntHash, newName);
    sprintf(oldName, "%s/ooGreedy.%s.gl.noNt", contig, oogVersion);
    sprintf(newName, "%s/ooGreedy.%s.gl", contig, oogVersion);
    patchOneGl(oldName, ntHash, newName);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 5)
    usage();
ntGoldGl(argv[1], argv[2], argv[3], argc-4, argv+4);
return 0;
}
