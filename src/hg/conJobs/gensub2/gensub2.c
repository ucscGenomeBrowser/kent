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
  "This will substitute each file in the file lists for $(file1) and $(file2)\n"
  "in the template between #LOOP and #ENDLOOP, and write the results to\n"
  "the output.");

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
char *lineT, *line1, *line2;
int lineSize;
struct slName *loopList = NULL, *loopEl;
bool gotLoop = FALSE, gotEndLoop = FALSE;
struct sub *subList = NULL, *sub;

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
while (lineFileNext(lf1, &line1, &lineSize))
    {
    struct lineFile *lf2 = lineFileOpen(list2Name, TRUE);
    line1 = trimSpaces(line1);
    while (lineFileNext(lf2, &line2, &lineSize))
        {
	line2 = trimSpaces(line2);
	sub = subNew("$(file1)", line1);
	slAddHead(&subList, sub);
	sub = subNew("$(file2)", line2);
	slAddHead(&subList, sub);
	for (loopEl = loopList; loopEl != NULL; loopEl = loopEl->next)
	    writeSubbed(loopEl->name, subList, f);
	slFreeList(&subList);
	}
    lineFileClose(&lf2);
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
