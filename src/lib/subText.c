/* subText - Stuff to do text substitutions. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "subText.h"


struct subText *subTextNew(char *in, char *out)
/* Make new substitution structure. */
{
struct subText *sub;
AllocVar(sub);
sub->in = cloneString(in);
sub->out = cloneString(out);
sub->inSize = strlen(in);
sub->outSize = strlen(out);
return sub;
}

void subTextFree(struct subText **pSub)
/* Free a subText. */
{
struct subText *sub = *pSub;
if (sub != NULL)
    {
    freeMem(sub->in);
    freeMem(sub->out);
    freez(pSub);
    }
}

void subTextFreeList(struct subText **pList)
/* Free a list of dynamically allocated subText's */
{
struct subText *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    subTextFree(&el);
    }
*pList = NULL;
}

#if 0 /* unused */
static boolean firstSame(char *s, char *t, int len)
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
#endif

static struct subText *firstInList(struct subText *l, char *name)
/* Return first element in Sub list who's in string matches the
 * first part of name. */
{
struct subText *text = l;
char *start;
char *end;
char *cmp;

for(; text; text = text->next)
    {
    start = text->in;
    end = &start[text->inSize];
    cmp = name;
    for(;(start < end) && (*start == *cmp); start++, cmp++)
	;
    if (start == end)
    	return text;
    }
return NULL;
}


int subTextSizeAfter(struct subText *subList, char *in)
/* Return size string will be after substitutions. */
{
struct subText *sub;
char *s;
int size = 0;

s = in;
while (*s)
    {
    if ((sub = firstInList(subList, s)) != NULL)
	{
	s += sub->inSize;
	size += sub->outSize;
	}
    else
	{
	size += 1;
	s += 1;
	}
    }
return size;
}


static void doSub(char *in, char *buf, struct subText *subList)
/* Do substitutions in list while copying from in to buf.  This
 * cheap little routine doesn't check that out is big enough.... */
{
struct subText *sub;
char *s, *d;

s = in;
d = buf;
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

void subTextStatic(struct subText *subList, char *in, char *out, int outMaxSize)
/* Do substition to output buffer of given size.  Complain
 * and die if not big enough. */
{
int newSize = subTextSizeAfter(subList, in);
if (newSize >= outMaxSize)
    errAbort("%s would expand to more than %d bytes", in, outMaxSize);
doSub(in, out, subList);
}

char *subTextString(struct subText *subList, char *in)
/* Return string with substitutions in list performed.  freeMem
 * this string when done. */
{
int size = subTextSizeAfter(subList, in);
char *out = needMem(size+1);
doSub(in, out, subList);
return out;
}

