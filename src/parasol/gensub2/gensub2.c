/* gensub2 - Generate job submission file from template and two file lists. */

#include "common.h"
#include "linefile.h"
#include "obscure.h"
#include "options.h"
#include "subText.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gensub2 - Generate condor submission file from template and two file lists\n"
  "usage:\n"
  "   gensub2 <file list 1> <file list 2> <template file> <output file>\n"
  "This will substitute each file in the file lists for $(path1) and $(path2)\n"
  "in the template between #LOOP and #ENDLOOP, and write the results to\n"
  "the output.  Other substitution variables are:\n"
  "       $(path1)  - full path name of first file\n"
  "       $(path1)  - full path name of second file\n"
  "       $(dir1)   - first directory. Includes trailing slash if any.\n"
  "       $(dir2)   - second directory\n"
  "       $(lastDir1) - The last directory in the first path. Includes trailing slash if any.\n"
  "       $(lastDir2) - The last directory in the second path. Includes trailing slash if any.\n"
  "       $(root1)  - first file name without directory or extension\n"
  "       $(root2)  - second file name without directory or extension\n"
  "       $(ext1)   - first file extension\n"
  "       $(ext2)   - second file extension\n"
  "       $(file1)  - name without dir of first file\n"
  "       $(file2)  - name without dir of second file\n"
  "       $(num1)   - index of first file in list\n"
  "       $(num2)   - index of second file in list\n"
  "The <file list 2> parameter can be 'single' if there is only one\n"
  "file list.\n"
  "By default the order is Aa Ba aB Bb if the first list is AB and the\n"
  "second list is ab.  This tends to put the largest jobs first if the\n"
  "file lists are both sorted by size. The following options can change this\n"
  "Options:\n"
  "       -group1 - write elements in order Aa Ab Ba Bb\n"
  "       -group2 - write elements in order Aa Ba Ab Bb\n");
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

/* Collect info for substitutions on first list. */
splitPath(path1, dir1, root1, ext1);
sprintf(file1, "%s%s", root1, ext1);
getLastDir(lastDir1, dir1);
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
char *lineT, *path1, *path2;
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
