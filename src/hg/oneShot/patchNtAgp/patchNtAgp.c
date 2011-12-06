/* patchNtAgp - Patch in NT contigs into AGP files.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "hCommon.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "patchNtAgp - Patch in NT contigs into AGP files.\n"
  "usage:\n"
  "   patchNtAgp cloneSizes patchFile okContigs ctg_coords outputDir inFile1.agp ... inFileN.agp\n");
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
    char *verName;	/* Name including version. */
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

void readPatch(char *fileName, struct hash *cloneHash, struct hash *okHash,
	struct ntContig **retNtList, struct hash **retNtHash)
/* Read patch file into clone/hash.  */
{
struct ntContig *ntList = NULL, *nt = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[9];
struct clone *clone, *ntClone, *lastClone = NULL;
struct cloneRef *ref;
struct hash *ntHash = newHash(0);
char cloneName[128];
char c;
int ntOrder = 0;

while (lineFileRow(lf, row))
    {
    if (nt == NULL || !sameString(row[0], nt->name))
        {
	AllocVar(nt);
	slAddHead(&ntList, nt);
	if (hashLookup(ntHash, row[0]) != NULL)
	    errAbort("NT contig %s repeated line %d of %s", row[0], lf->lineIx, lf->fileName);
	hashAddSaveName(ntHash, row[0], nt, &nt->name);
	lastClone = NULL;
	ntOrder = 0;
	}
    strcpy(cloneName, row[6]);
    chopSuffix(cloneName);
    clone = hashMustFindVal(cloneHash, cloneName);
    clone->ntStart = atoi(row[2]) - 1;
    clone->ntEnd = atoi(row[3]);
    if (clone->nt != NULL && !hashLookup(okHash, nt->name))
	{
        warn("Clone %s trying to be in two NT contigs (%s and %s) line %d of %s",
		clone->name, clone->nt->name, nt->name, lf->lineIx, lf->fileName);
	nt->problem = TRUE;
	}
    clone->nt = nt;
    c = row[4][0];
    if (c == '-')
	clone->ntOrientation = -1;
    else if (c == '+')
	clone->ntOrientation = +1;
    else
	errAbort("Expecting +1 or -1 field 5, line %d, file %s", lf->lineIx, lf->fileName);
    c = row[5][0];
    if (c == 'F' || c == 'D' || c == 'P')
	clone->seqType =  c;
    else
	errAbort("Expecting F, D, or P  field 6, line %d, file %s", lf->lineIx, lf->fileName);
    clone->verName = cloneString(row[6]);
    clone->goldStart = atoi(row[7]) - 1;
    if (row[8][0] == '(')
        {
	clone->goldEnd = clone->goldStart + clone->ntEnd - clone->ntStart;
	}
    else
	clone->goldEnd = atoi(row[8]);
    clone->ntOrder = ntOrder++;

#ifdef OLD
    /* Flip if need be. */
    if (clone->ntOrientation < 0)
	{
	int s, e;
	s = clone->size - clone->goldEnd;
	e = clone->size - clone->goldStart;
	clone->goldStart = s;
	clone->goldEnd = e;
	}
#endif /* OLD */

    /* Add ref to NT. */
    AllocVar(ref);
    ref->ref = clone;
    slAddTail(&nt->cloneList, ref);

    /* Do a few tests. */
    if (clone->goldStart >= clone->goldEnd && !hashLookup(okHash, nt->name))
	{
	warn("Clone %s end before start (%d before %d) line %d of %s", 
		clone->name, clone->goldStart, clone->goldEnd, lf->lineIx, lf->fileName);
	nt->problem = TRUE;
	}
    if (clone->ntStart >= clone->ntEnd && !hashLookup(okHash, nt->name))
	{
	warn("Clone %s NT end before NT start line %d of %s", 
		clone->name, lf->lineIx, lf->fileName);
	nt->problem = TRUE;
	}
    if (clone->goldEnd > clone->size && !hashLookup(okHash, nt->name))
	{
	if (sameString(clone->startFrag, clone->endFrag))
	    {
	    warn("Clone %s end position %d, clone size %d, line %d of %s", 
		clone->name, clone->goldEnd, clone->size, lf->lineIx, lf->fileName);
	    nt->problem = TRUE;
	    }
	}
    if (clone->ntEnd - clone->ntStart != clone->goldEnd - clone->goldStart && !hashLookup(okHash, nt->name))
        {
	warn("Size not the same in NT contig as in clone %s (%d vs %d) line %d of %s",
		clone->name,
		clone->ntEnd - clone->ntStart, clone->goldEnd-clone->goldStart,
		lf->lineIx, lf->fileName);
	nt->problem = TRUE;
	}
    nt->sumSize += clone->goldEnd - clone->goldStart;
    ntClone = hashFindVal(cloneHash, nt->name);
    if (ntClone != NULL && clone->ntEnd > ntClone->size && !hashLookup(okHash, nt->name))
	{
	warn("Clone %s NT end position %d, NT size %d, line %d of %s", 
	    clone->name, clone->goldEnd, clone->size, lf->lineIx, lf->fileName);
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
    if (nt->sumSize != nt->size && !hashLookup(okHash, nt->name))
        {
	warn("Sum of fragments of %s is %d, but size is supposed to be %d",
		nt->name, nt->sumSize, nt->size);
	nt->problem = TRUE;
	}
    }
*retNtList = ntList;
*retNtHash = ntHash;
}

void checkCtgCoors(char *fileName, struct hash *cloneHash, struct hash *ntHash,
	struct hash *okHash)
/* Check match against ctgCoords. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[8];
struct ntContig *nt, *lastNt = NULL;
struct clone *clone;
char cloneName[128];
int orientation, ntOrder;

while (lineFileRow(lf, row))
    {
    nt = hashFindVal(ntHash, row[0]);
    if (nt == NULL)
        {
	warn("%s is in ctg_coords but not patch file", row[0]);
	AllocVar(nt);
	hashAddSaveName(ntHash, row[0], nt, &nt->name);
	nt->problem = TRUE;
	}
    if (lastNt == NULL || nt != lastNt)
        ntOrder = 0;
    if (!nt->problem)
        {
	strcpy(cloneName, row[6]);
	chopSuffix(cloneName);
	clone = hashMustFindVal(cloneHash, cloneName);
	orientation = atoi(row[4]);
	if (orientation != clone->ntOrientation && !hashLookup(okHash, nt->name))
	    {
	    warn("Clone %s in %s has opposite orientations in ctg_coords and patch file",
	    	clone->name, nt->name);
	    // nt->problem = TRUE;
	    }
	if (ntOrder != clone->ntOrder && !hashLookup(okHash, nt->name))
	    {
	    warn("Clone %s in %s is in different order between ctg_coords and patch file",
	    	clone->name, nt->name);
	    // nt->problem = TRUE;
	    }
	if (clone->verName == NULL || !sameString(clone->verName, row[6]))
	    {
	    warn("Version %s in ctg_coords, %s in patch file", row[6], clone->verName);
	    clone->verName = cloneString(row[6]);
	    }
	}
    ++ntOrder;
    lastNt = nt;
    }
lineFileClose(&lf);
}

void patchOne(char *inName, struct hash *ntHash, struct hash *cloneHash, char *outName)
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
			    ++lineOutIx, clone->seqType, clone->verName,  
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
}

void hashFirstWords(char *fileName, struct hash **retHash)
/* Read in lines with one word into hash. */
{
struct hash *hash = newHash(8);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[1];

while (lineFileRow(lf, row))
    {
    hashAdd(hash, row[0], NULL);
    }
lineFileClose(&lf);
*retHash = hash;
}

void patchNtAgp(char *cloneSizes, char *patchFile, char *okFile, char *ctgCoors,
	char *newDir, int agpCount, char *agpFiles[])
/* patchNtAgp - Patch in NT contigs into AGP files.. */
{
struct hash *cloneHash = NULL, *ntHash = NULL;
struct hash *okHash = NULL;
struct clone *cloneList = NULL, *clone;
struct ntContig *ntList = NULL, *nt;
int i;
char *agpFile;
char sDir[256], sFile[128], sExt[64];

/* Read in patch data and other data and make sure it's
 * consistent. */
makeDir(newDir);
readCloneSizes(cloneSizes, &cloneList, &cloneHash);
printf("Read %d clones in %s\n", slCount(cloneList), cloneSizes);
hashFirstWords(okFile, &okHash);
readPatch(patchFile, cloneHash, okHash, &ntList, &ntHash);
printf("Read %d nt contigs in %s\n", slCount(ntList), patchFile);
checkCtgCoors(ctgCoors, cloneHash, ntHash, okHash);
printf("Finished check against %s\n", ctgCoors);

/* clear problems that we override as ok. */
for (nt = ntList; nt != NULL; nt = nt->next)
    {
    if (nt->problem && hashLookup(okHash, nt->name) != NULL)
        nt->problem = FALSE;
    }

/* Patch each AGP file. */
for (i=0; i<agpCount; ++i)
    {
    char newFile[512];
    char *agpFile = agpFiles[i];
    splitPath(agpFile, sDir, sFile, sExt);
    sprintf(newFile, "%s/%s.agp", newDir, sFile);
    patchOne(agpFile, ntHash, cloneHash, newFile);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 6)
    usage();
patchNtAgp(argv[1], argv[2], argv[3], argv[4], argv[5], argc-6, argv+6);
return 0;
}
