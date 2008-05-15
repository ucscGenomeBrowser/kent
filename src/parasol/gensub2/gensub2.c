/* gensub2 - Generate job submission file from template and two file lists. */

#include "paraCommon.h"
#include "linefile.h"
#include "obscure.h"
#include "options.h"
#include "subText.h"
#include "dystring.h"

char *version = PARA_VERSION;   /* Version number. */

#define ARBITRARY_MAX_NUM_DIRS 30

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gensub2 - version %s\n"
  "Generate condor submission file from template and two file lists.\n"
  "Usage:\n"
  "   gensub2 <file list 1> <file list 2> <template file> <output file>\n"
  "This will substitute each file in the file lists for $(path1) and $(path2)\n"
  "in the template between #LOOP and #ENDLOOP, and write the results to\n"
  "the output.  Other substitution variables are:\n"
  "       $(path1)  - Full path name of first file.\n"
  "       $(path2)  - Full path name of second file.\n"
  "       $(dir1)   - First directory. Includes trailing slash if any.\n"
  "       $(dir2)   - Second directory.\n"
  "       $(lastDir1) - The last directory in the first path. Includes trailing slash if any.\n"
  "       $(lastDir2) - The last directory in the second path. Includes trailing slash if any.\n"
  "       $(lastDirs1=<n>) - The last n directories in the first path.\n"
  "       $(lastDirs2=<n>) - The last n directories in the second path.\n"
  "       $(root1)  - First file name without directory or extension.\n"
  "       $(root2)  - Second file name without directory or extension.\n"
  "       $(ext1)   - First file extension.\n"
  "       $(ext2)   - Second file extension.\n"
  "       $(file1)  - Name without dir of first file.\n"
  "       $(file2)  - Name without dir of second file.\n"
  "       $(num1)   - Index of first file in list.\n"
  "       $(num2)   - Index of second file in list.\n"
  "The <file list 2> parameter can be 'single' if there is only \n"
  "one file list.  By default the order is diagonal, meaning if \n"
  "the first list is ABC and the secon list is abc the combined \n"
  "order is Aa Ba Ab Ca Bb Ac  Cb Bc Cc.  This tends to put the \n"
  "largest jobs first if the file lists are both sorted by size. \n"
  "The following options can change this:\n"
  "    -group1 - write elements in order Aa Ab Ac Ba Bb Bc Ca Cb Cc\n"
  "    -group2 - write elements in order Aa Ba Ca Ab Bb Cb Ac Bc Cc\n"
  , version
  );
	      
}

struct slName *getLastDirs(char *pathToParse)
/* Return a linked list of the dir names. */
{
struct slName *retList = NULL;
char *slash = strchr(pathToParse,'/');
char *dirNames[ARBITRARY_MAX_NUM_DIRS];
char *copy = cloneString(pathToParse);
int got, i;

if (NULL == slash)   /** Do a little coping with MS-DOS style paths. */
    {
    char *dosSlash = strchr(pathToParse, '\\');
    if (dosSlash != NULL)
        {
        subChar(pathToParse, '\\', '/');
        slash = dosSlash;
        }
    }
got = chopByChar(copy, '/', dirNames, ArraySize(dirNames));
for (i = 0; i < got; i++) 
    {
    int j;
    struct dyString *ds = newDyString(64);
    for (j = i; j < got; j++)
        {
	dyStringAppend(ds, dirNames[j]);
	if (j < got-1)
	    dyStringAppendC(ds, '/');
	}
    slNameAddHead(&retList, ds->string);
    dyStringFree(&ds);
    }
freeMem(copy);
return retList;
}

void getLastDir(char *retDir, char *pathToParse)
/*
Function to get the last directory in a path

param retDir - The directory buffer in which to return the last directory
in the path.
param pathToParse - The path to parse the last directory out of. This function
assumes that the end of the directory path ends with a '/' character.
That is, if there is no filename specified, then the path will end with a slash.
The algorithm drops all content after the last slash it finds.
return - The last directory in the path with all slashes removed.
 */
{
char *lastSlash = strrchr(pathToParse, '/');
char *slash = strchr(pathToParse,'/');
char *result = pathToParse;

if (NULL == slash)   /** Do a little coping with MS-DOS style paths. */
    {
    char *dosSlash = strchr(pathToParse, '\\');
    if (dosSlash != NULL)
        {
        subChar(pathToParse, '\\', '/');
        slash = dosSlash;
        }
    }

if (NULL == lastSlash)
    {
    strcpy(retDir, pathToParse);
    return;
    }

lastSlash = strrchr(pathToParse,'/');
while (slash != lastSlash)
    {
    result = slash;
    slash = strchr(result + 1, '/');
    }

/* If result has a leading slash, move the string pointer past it */
if (strchr(result, '/') == result ) 
    {
    result += 1;
    }

/* Null terminate before any trailing slash */
lastSlash = strrchr(result, '/');
if (NULL != lastSlash)
    {
    lastSlash[0] = 0;
    }

strcpy(retDir, result);
return;
}

void writeSubbed(char *string, struct subText *subList, FILE *f)
/* Perform substitutions on (copy of) string and write to file. */
{
static char subBuf[1024*16];
subTextStatic(subList, string, subBuf, sizeof(subBuf));
fprintf(f, "%s\n", subBuf);
}

void readLines(char *fileName, int *retCount, char ***retLines)
/* Read all lines of file into an array. */
{
struct slName *el, *list = readAllLines(fileName);
int i=0, count = slCount(list);
char **lines;

AllocArray(lines, count);
for (el = list; el != NULL; el = el->next)
    lines[i++] = trimSpaces(el->name);
*retCount = count;
*retLines = lines;
}

void outputOneSub(char *path1, char *path2, int i, int j, struct slName *loopText, FILE *f)
/* Output all text in loop with one set of substitutions. */
{
char numBuf1[16], numBuf2[16];
char dir1[256], root1[128], ext1[64], file1[265];
char dir2[256], root2[128], ext2[64], file2[265];
char lastDir1[128];
char lastDir2[128];
struct subText *subList = NULL, *sub;
struct slName *loopEl;
struct slName *lastDirs1 = NULL, *lastDirs2 = NULL, *lastDirsEl;
int k;

/* Collect info for substitutions on first list. */
splitPath(path1, dir1, root1, ext1);
sprintf(file1, "%s%s", root1, ext1);
getLastDir(lastDir1, dir1);
lastDirs1 = getLastDirs(dir1);
lastDirsEl = lastDirs1;
for (k = 1; k <= ARBITRARY_MAX_NUM_DIRS; k++)
    {
    char targetText[24];
    struct subText *tmpSub = NULL;
    if (!lastDirsEl)
	break;
    safef(targetText, 24, "$(lastDirs1=%d)", k);
    tmpSub = subTextNew(targetText, lastDirsEl->name);
    slAddHead(&subList, tmpSub);
    lastDirsEl = lastDirsEl->next;
    }
sub = subTextNew("$(path1)", path1);
slAddHead(&subList, sub);
sub = subTextNew("$(dir1)", dir1);
slAddHead(&subList, sub);
sub = subTextNew("$(lastDir1)", lastDir1);
slAddHead(&subList, sub);
sub = subTextNew("$(root1)", root1);
slAddHead(&subList, sub);
sub = subTextNew("$(ext1)", ext1);
slAddHead(&subList, sub);
sub = subTextNew("$(file1)", file1);
slAddHead(&subList, sub);
sprintf(numBuf1, "%d", i);
sub = subTextNew("$(num1)", numBuf1);
slAddHead(&subList, sub);

/* Collect info for substitutions on second list. */
splitPath(path2, dir2, root2, ext2);
sprintf(file2, "%s%s", root2, ext2);
getLastDir(lastDir2, dir2);
lastDirs2 = getLastDirs(dir2);
lastDirsEl = lastDirs2;
for (k = 1; k <= ARBITRARY_MAX_NUM_DIRS; k++)
    {
    char targetText[24];
    struct subText *tmpSub = NULL;
    if (!lastDirsEl)
	break;
    safef(targetText, 24, "$(lastDirs1=%d)", k);    
    tmpSub = subTextNew(targetText, lastDirsEl->name);
    slAddHead(&subList, tmpSub);
    lastDirsEl = lastDirsEl->next;
    }
sub = subTextNew("$(path2)", path2);
slAddHead(&subList, sub);
sub = subTextNew("$(dir2)", dir2);
slAddHead(&subList, sub);
sub = subTextNew("$(lastDir2)", lastDir2);
slAddHead(&subList, sub);
sub = subTextNew("$(root2)", root2);
slAddHead(&subList, sub);
sub = subTextNew("$(ext2)", ext2);
slAddHead(&subList, sub);
sub = subTextNew("$(file2)", file2);
slAddHead(&subList, sub);
sprintf(numBuf2, "%d", j);
sub = subTextNew("$(num2)", numBuf2);
slAddHead(&subList, sub);

/* Write out substitutions. */
for (loopEl = loopText; loopEl != NULL; loopEl = loopEl->next)
    writeSubbed(loopEl->name, subList, f);

/* Clean up. */
slFreeList(&subList);
}

void gensub2(char *list1Name, char *list2Name, char *templateName, char *conName)
/* gensub2 - Generate condor submission file from template and two file lists. */
{
struct lineFile *lfT = lineFileOpen(templateName, TRUE);
FILE *f = mustOpen(conName, "w");
char *lineT;
int i, j;
struct slName *loopText = NULL, *loopEl;
bool gotLoop = FALSE, gotEndLoop = FALSE;
boolean twoLists = !sameWord(list2Name, "single");
char **lines1 = NULL, **lines2 = NULL;
int count1, count2;

/* Read input list files. */
readLines(list1Name, &count1, &lines1);
if (twoLists)
    readLines(list2Name, &count2, &lines2);
else
    {
    AllocArray(lines2, 1);
    lines2[0] = "single";
    count2 = 1;
    }


/* Print up to #LOOP. */
while (lineFileNext(lfT, &lineT, NULL))
    {
    if (startsWith("#LOOP", lineT))
	{
	gotLoop = TRUE;
        break;
	}
    fprintf(f, "%s\n", lineT);
    }
if (!gotLoop)
    errAbort("No #LOOP statement in %s", templateName);

/* Read until #ENDLOOP into list. */
while (lineFileNext(lfT, &lineT, NULL))
    {
    if (startsWith("#ENDLOOP", lineT))
         {
	 gotEndLoop = TRUE;
	 break;
	 }
    loopEl = newSlName(lineT);
    slAddHead(&loopText, loopEl);
    }
if (!gotEndLoop)
    errAbort("No #ENDLOOP statement in %s", templateName);
slReverse(&loopText);

if (optionExists("group1"))
    {
    for (i=0; i<count1; ++i)
	for (j=0; j<count2; ++j)
	    outputOneSub(lines1[i], lines2[j], i, j, loopText, f);
    }
else if (optionExists("group2"))
    {
    for (j=0; j<count2; ++j)
	for (i=0; i<count1; ++i)
	    outputOneSub(lines1[i], lines2[j], i, j, loopText, f);
    }
else
    {
    /* This loop is constructed to output stuff on the diagonal.
     * That is if list 1 contains A,B,C and list 2 a,b,c the
     * order of the output will be Aa Ba Ab Ca Bb Ac Cb Bc Cc
     * This is so that if both lists are sorted by size, then
     * the largest sized jobs will tend to be output first. */
    int bothCount = count1 + count2, bothIx;
    bothCount = count1 + count2;
    for (bothIx = 0; bothIx < bothCount; ++bothIx)
	{
	i = bothIx;
	j = 0;
	while (i >= 0)
	    {
	    if (i < count1 && j < count2)
		outputOneSub(lines1[i], lines2[j], i, j, loopText, f);
	    ++j;
	    --i;
	    }
	}
    }

/* Write after #ENDLOOP */
while (lineFileNext(lfT, &lineT, NULL))
    fprintf(f, "%s\n", lineT);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
gensub2(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
