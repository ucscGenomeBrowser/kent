/* gensub2 - Generate condor submission file from template and two file lists. */
#include "common.h"
#include "linefile.h"

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
  "       $(root1)  - first file name without directory or extension\n"
  "       $(root2)  - second file name without directory or extension\n"
  "       $(ext1)   - first file extension\n"
  "       $(ext2)   - second file extension\n"
  "       $(file1)  - name without dir of first file\n"
  "       $(file2)  - name without dir of second file\n"
  "       $(num1)   - index of first file in list\n"
  "       $(num2)   - index of second file in list\n");
}

struct sub
/** Structure that holds data for a single substitution to be made */
	{
	struct sub *next;		/* pointer to next substitution */
	char *in;				/* source side of substitution */
	char *out;				/* dest side of substitution */
	int inSize;				/* length of in string */
	int outSize;			/* length of out string */
	};

struct sub *subNew(char *in, char *out)
/* Make new substitution structure. */
{
struct sub *sub;
AllocVar(sub);
sub->in = in;
sub->out = out;
sub->inSize = strlen(in);
sub->outSize = strlen(out);
return sub;
}

boolean firstSame(char *s, char *t, int len)
/* Return TRUE if  the  first len characters of the strings s and t
 * are the same. */
{
while (--len >= 0)
    {
    if (*s++ != *t++)
	return FALSE;
    }
return TRUE;
}

struct sub *firstInList(struct sub *l, char *name)
/* Return first element in Sub list who's in string matches the
 * first part of name. */
{
while (l != NULL)
    {
    if (firstSame(l->in, name, l->inSize))
	return l;
    l = l->next;
    }
return NULL;
}

void subString(char *in, char *out, struct sub *subList)
/* Do substitution while copying from in to out.  This
 * cheap little routine doesn't check that out is big enough.... */
{
struct sub *sub;
char *s, *d;

s = in;
d = out;
while (*s)
    {
    if ((sub = firstInList(subList, s)) != NULL)
	{
	s += sub->inSize;
	memcpy(d, sub->out, sub->outSize);
	d += strlen(sub->out);
	}
    else
	{
	*d++ = *s++;
	}
    }
*d++ = 0;
}

void writeSubbed(char *string, struct sub *subList, FILE *f)
/* Perform substitutions on (copy of) string and write to file. */
{
static char subBuf[2048];

subString(string, subBuf, subList);
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
struct sub *subList = NULL, *sub;
char numBuf1[16], numBuf2[16];
char dir1[256], root1[128], ext1[64], file1[265];
char dir2[256], root2[128], ext2[64], file2[265];

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
    struct lineFile *lf2 = lineFileOpen(list2Name, TRUE);
    path1 = trimSpaces(path1);
    j=1;
    while (lineFileNext(lf2, &path2, &lineSize))
        {
	path2 = trimSpaces(path2);

	splitPath(path1, dir1, root1, ext1);
	sprintf(file1, "%s%s", root1, ext1);
	sub = subNew("$(path1)", path1);
	slAddHead(&subList, sub);
	sub = subNew("$(dir1)", dir1);
	slAddHead(&subList, sub);
	sub = subNew("$(root1)", root1);
	slAddHead(&subList, sub);
	sub = subNew("$(ext1)", ext1);
	slAddHead(&subList, sub);
	sub = subNew("$(file1)", file1);
	slAddHead(&subList, sub);
	sprintf(numBuf1, "%d", i);
	sub = subNew("$(num1)", numBuf1);
	slAddHead(&subList, sub);

	splitPath(path2, dir2, root2, ext2);
	sprintf(file2, "%s%s", root2, ext2);
	sub = subNew("$(path2)", path2);
	slAddHead(&subList, sub);
	sub = subNew("$(dir2)", dir2);
	slAddHead(&subList, sub);
	sub = subNew("$(root2)", root2);
	slAddHead(&subList, sub);
	sub = subNew("$(ext2)", ext2);
	slAddHead(&subList, sub);
	sub = subNew("$(file2)", file2);
	slAddHead(&subList, sub);
	sprintf(numBuf2, "%d", j);
	sub = subNew("$(num2)", numBuf2);
	slAddHead(&subList, sub);

	for (loopEl = loopList; loopEl != NULL; loopEl = loopEl->next)
	    writeSubbed(loopEl->name, subList, f);
	slFreeList(&subList);
	++j;
	}
    lineFileClose(&lf2);
    ++i;
    }

/* Write after #ENDLOOP */
while (lineFileNext(lfT, &lineT, &lineSize))
    {
    fprintf(f, "%s\n", lineT);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 5)
    usage();
gensub2(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
