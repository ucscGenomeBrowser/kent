/* goldToAgp - convert from ooGreedy "gold" format to the official
 * ".agp" format for golden paths. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "portable.h"



void usage()
/* Print usage instructions and exit */
{
errAbort(
  "goldToAgp - convert from ooGreedy 'gold' format to the official\n"
  "'.agp' format for golden paths through a contig. \n"
  "usage:\n"
  "  goldToAgp gsDir version chromDir(s)\n"
  "This will look for gsDir/fin/trans gsDir/draft/trans and gsdir/predraft/trans\n"
  "to define name translations.  It will then translate in chromDir/ctg*/gold.version\n"
  "into chromDir/ctg*/ctg*.agp\n");
}

struct fragData 
/* Info on fragment */
    {
    char *ucscName;	/* Name on UCSC side. */
    char *ffaName;      /* Greg Schuler name. */
    char *accVer;       /* Genbank accession.version. */
    int subStart;	/* Start in Genbank submission coordinates. (1 based) */
    int subEnd;         /* End in Genbank submission coordinates. (inclusive) */
    char *type;     /* Phase in agp sense "F" "D" or "P" */
    };

struct fragData *parseFragData(char *words[], char *phase, int whereIx)
/* Parse frag data from a line. */
{
struct fragData *fd;
char *s, *e;

AllocVar(fd);
fd->ucscName = cloneString(words[0]);
fd->ffaName = cloneString(words[1]);
s = words[whereIx];
if (s[0] != '(')
    errAbort("Format error 1 in trans file: %s\n", s);
s += 1;
if ((e = strchr(s, ':')) == NULL)
    errAbort("Format error 2 in trans file: %s\n", s);
*e++ = 0;
fd->accVer = cloneString(s);
s = e;
if ((e = strchr(s, '.')) == NULL || e[1] != '.' || !isdigit(s[0]))
    errAbort("Format error 3 in trans file: %s\n", s);
*e++ = 0;
fd->subStart = atoi(s);
s = e+1;
if (!isdigit(s[0]))
    errAbort("Format error 4 in trans file: %s\n", s);
fd->subEnd = atoi(s);
fd->type = phase;
return fd;
}

void hashTrans(char *dir, char *subDir, char *type, struct hash *hash)
/* Read trans file in dir/subdir/trans into hash. */
{
char fileName[512];
struct lineFile *lf;
int lineSize;
char *line;
int wordCount;
char *words[16];
struct fragData *fd;

sprintf(fileName, "%s/%s/trans", dir, subDir);
printf("Reading in %s\n", fileName);
lf = lineFileOpen(fileName, TRUE);
while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    if (wordCount != 7 && wordCount != 3)
	errAbort("Bad line %d of %s\n", lf->lineIx, lf->fileName);
    fd = parseFragData(words, type, wordCount-1);
    hashAdd(hash, fd->ucscName, fd);
    }
lineFileClose(&lf);
}

void translateOne(char *goldName, char *agpName, char *contig, struct hash *hash)
/* Translate one gold to agp. */
{
struct lineFile *lf = lineFileOpen(goldName, TRUE);
FILE *f = mustOpen(agpName, "w");
int lineSize, wordCount;
char *line, *words[16];

printf("Translating %s to %s\n", goldName, agpName);
while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    if (wordCount != 8 && wordCount != 9)
	errAbort("Bad line %d of %s\n", lf->lineIx, lf->fileName);
    fprintf(f, "%s\t%s\t%s\t%s\t", contig, words[1], words[2], words[3]);
    if (wordCount == 8)
	{
	if (!sameWord(words[4], "N"))
	    errAbort("Bad N line %d of %s\n", lf->lineIx, lf->fileName);
	fprintf(f, "N\t%s\t%s\t%s\n", words[5], words[6], words[7]);
	}
    else
	{
	struct fragData *fd;
	char *frag = words[5];
	int fragStartInClone;

	if (startsWith("NT_", frag))
	    {
	    chopSuffix(frag);
	    fprintf(f, "%s\t%s.0\t%s\t%s\t\%s\n", "F", 
	        frag, 	words[6], words[7], words[8]);
	    }
	else
	    {
	    fd = hashFindVal(hash, frag);
	    if (fd == NULL)
		errAbort("%s isn't in trans files\n", frag);
	    fragStartInClone = fd->subStart - 1;
	    fprintf(f, "%s\t%s\t%d\t%d\t%s\n",
		    fd->type, fd->accVer,
		    fragStartInClone + atoi(words[6]),
		    fragStartInClone + atoi(words[7]),
		    words[8]);
	    }
	}
    }
lineFileClose(&lf);
fclose(f);
}

void goldToAgp(char *gsDir, int version, char *chromDirs[], int chromCount)
/* goldToAgp - convert from ooGreedy "gold" format to the official
 * ".agp" format for golden paths. */
{
struct hash *seqHash = newHash(18);
int i;
char *chrom;
char goldFileName[512];
char agpFileName[512];
char contigName[256];
char  *ctgName;
struct slName *dirList, *dirEl;
char chromRoot[128], chromSuperDir[256], chromExt[64];

hashTrans(gsDir, "fin", "F", seqHash);
hashTrans(gsDir, "draft", "D", seqHash);
hashTrans(gsDir, "predraft", "P", seqHash);
hashTrans(gsDir, "extras", "D", seqHash);
// hashTrans(gsDir, "missing", "O", seqHash);
for (i=0; i<chromCount; ++i)
    {
    chrom = chromDirs[i];
    dirList = listDir(chrom, "ctg*");
    splitPath(chrom, chromSuperDir, chromRoot, chromExt);
    for (dirEl = dirList; dirEl != NULL; dirEl = dirEl->next)
	{
	ctgName = dirEl->name;
	sprintf(contigName, "%s/%s", chromRoot, ctgName);
	sprintf(goldFileName, "%s/%s/gold.%d", chrom, ctgName, version);
	sprintf(agpFileName, "%s/%s/%s.agp", chrom, ctgName, ctgName);
	if (fileExists(goldFileName))
	    {
	    translateOne(goldFileName, agpFileName, contigName, seqHash);
	    }
	else
	    {
	    warn("%s doesn't exist\n", goldFileName);
	    }
	}
    slFreeList(&dirList);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 4 || !isdigit(argv[2][0]))
    usage();
goldToAgp(argv[1], atoi(argv[2]), argv+3, argc-3);
return 0;
}

