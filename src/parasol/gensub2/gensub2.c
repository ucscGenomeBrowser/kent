/* gensub2 - Generate job submission file from template and two file lists. */

#include "common.h"
#include "linefile.h"
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
  "file list.\n");
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

printf("Path to parse = %s\n", pathToParse);

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
printf("RETURNING: %s\n", retDir);
return;
}

void writeSubbed(char *string, struct subText *subList, FILE *f)
/* Perform substitutions on (copy of) string and write to file. */
{
static char subBuf[2048];
subTextStatic(subList, string, subBuf, sizeof(subBuf));
fprintf(f, "%s\n", subBuf);
}

void gensub2(char *list1Name, char *list2Name, char *templateName, char *conName)
/* gensub2 - Generate condor submission file from template and two file lists. */
{
struct lineFile *lfT = lineFileOpen(templateName, TRUE);
struct lineFile *lf1 = lineFileOpen(list1Name, TRUE);
FILE *f = mustOpen(conName, "w");
char *lineT, *path1, *path2;
int lineSize, i, j;
struct slName *loopList = NULL, *loopEl;
bool gotLoop = FALSE, gotEndLoop = FALSE;
struct subText *subList = NULL, *sub;
char numBuf1[16], numBuf2[16];
char dir1[256], root1[128], ext1[64], file1[265];
char dir2[256], root2[128], ext2[64], file2[265];
char lastDir1[128];
char lastDir2[128];
boolean twoLists = !sameWord(list2Name, "single");

/* Print up to #LOOP. */
while (lineFileNext(lfT, &lineT, &lineSize))
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
while (lineFileNext(lfT, &lineT, &lineSize))
    {
    if (startsWith("#ENDLOOP", lineT))
         {
	 gotEndLoop = TRUE;
	 break;
	 }
    loopEl = newSlName(lineT);
    slAddHead(&loopList, loopEl);
    }
if (!gotEndLoop)
    errAbort("No #ENDLOOP statement in %s", templateName);
slReverse(&loopList);

/* Substitute $(file1) and $(file2) in each line of loop for
 * each pair of files in two lists. */
i=1;
while (lineFileNext(lf1, &path1, &lineSize))
    {
    struct lineFile *lf2 = NULL;
    path1 = trimSpaces(path1);
    splitPath(path1, dir1, root1, ext1);
    sprintf(file1, "%s%s", root1, ext1);
    getLastDir(lastDir1, dir1);

    j=1;
    if (twoLists)
	lf2 = lineFileOpen(list2Name, TRUE);
    for (;;)
        {
	if (twoLists)
	    {
	    if (!lineFileNext(lf2, &path2, &lineSize))
	        break;
	    path2 = trimSpaces(path2);
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

	for (loopEl = loopList; loopEl != NULL; loopEl = loopEl->next)
	    writeSubbed(loopEl->name, subList, f);
	slFreeList(&subList);
	++j;
	if (!twoLists)
	    break;
	}
    lineFileClose(&lf2);
    ++i;
    }

/* Write after #ENDLOOP */
while (lineFileNext(lfT, &lineT, &lineSize))
    fprintf(f, "%s\n", lineT);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 5)
    usage();
gensub2(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
