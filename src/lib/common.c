/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* Commonly used routines in a wide range of applications.
 * Strings, singly-linked lists, and a little file i/o. */
#include "common.h"

/* removes the '\r' character from a string */
/* the source and destination strings can be the same, if there is no threads */
void removeReturns(char* dest, char* src) {
    int i = 0;
    int j = 0;

    // until the end of the string
    while(1) {
        // skip the returns
        while(src[j] == '\r')
            j++;

        // copy the characters
        dest[i] = src[j];

        // check to see if done
        if(src[j] == '\0')
            break;

        // advance the counters
        i++;
        j++;
    }
}

void *cloneMem(void *pt, size_t size)
/* Allocate a new buffer of given size, and copy pt to it. */
{
void *newPt = needLargeMem(size);
memcpy(newPt, pt, size);
return newPt;
}

char *cloneStringZ(char *s, int size)
/* Make a zero terminated copy of string in memory */
{
char *d = needMem(size+1);
memcpy(d, s, size);
d[size] = 0;
return d;
}

char *cloneString(char *s)
/* Make copy of string in dynamic memory */
{
if (s == NULL)
    return NULL;
else
    return cloneStringZ(s, strlen(s));
}

/* fill a specified area of memory with zeroes */
void zeroBytes(void *vpt, int count)
{
char *pt = (char*)vpt;
while (--count>=0)
    *pt++=0;
}

/* Reverse the order of the bytes. */
void reverseBytes(char *bytes, long length)
{
long halfLen = (length>>1);
char *end = bytes+length;
char c;
while (--halfLen >= 0)
    {
    c = *bytes;
    *bytes++ = *--end;
    *end = c;
    }
}

void reverseInts(int *a, int length)
/* Reverse the order of the integer array. */
{
int halfLen = (length>>1);
int *end = a+length;
int c;
while (--halfLen >= 0)
    {
    c = *a;
    *a++ = *--end;
    *end = c;
    }
}

void reverseUnsigned(unsigned *a, int length)
/* Reverse the order of the unsigned array. */
{
int halfLen = (length>>1);
unsigned *end = a+length;
unsigned c;
while (--halfLen >= 0)
    {
    c = *a;
    *a++ = *--end;
    *end = c;
    }
}



/* Swap buffers a and b. */
void swapBytes(char *a, char *b, int length)
{
char c;
int i;

for (i=0; i<length; ++i)
    {
    c = a[i];
    a[i] = b[i];
    b[i] = c;
    }
}


/** List managing routines. */

/* Count up elements in list. */
int slCount(void *list)
{
struct slList *pt = (struct slList *)list;
int len = 0;

while (pt != NULL)
    {
    len += 1;
    pt = pt->next;
    }
return len;
}

void *slElementFromIx(void *list, int ix)
/* Return the ix'th element in list.  Returns NULL
 * if no such element. */
{
struct slList *pt = (struct slList *)list;
int i;
for (i=0;i<ix;i++)
    {
    if (pt == NULL) return NULL;
    pt = pt->next;
    }
return pt;
}

int slIxFromElement(void *list, void *el)
/* Return index of el in list.  Returns -1 if not on list. */
{
struct slList *pt;
int ix = 0;

for (pt = list, ix=0; pt != NULL; pt = pt->next, ++ix)
    if (el == (void*)pt)
	return ix;
return -1;
}

void *slLastEl(void *list)
/* Returns last element in list or NULL if none. */
{
struct slList *next, *el;
if ((el = list) == NULL)
    return NULL;
while ((next = el->next) != NULL)
    el = next;
return el;
}

#ifdef OLD
   Now use a macro for this. 
/* Add new node to start of list.
 * Usage:
 *    slAddHead(&list, node);
 * where list and nodes are both pointers to structure
 * that begin with a next pointer. 
 */
void slAddHead(void *listPt, void *node)
{
struct slList **ppt = (struct slList **)listPt;
struct slList *n = (struct slList *)node;

n->next = *ppt;
*ppt = n;
}
#endif /* OLD */

/* Add new node to tail of list.
 * Usage:
 *    slAddTail(&list, node);
 * where list and nodes are both pointers to structure
 * that begin with a next pointer. 
 */
void slAddTail(void *listPt, void *node)
{
struct slList **ppt = (struct slList **)listPt;
struct slList *n = (struct slList *)node;

while (*ppt != NULL)
    {
    ppt = &((*ppt)->next);
    }
n->next = NULL;	
*ppt = n;
}

/* Add new node to start of list.
 * Usage:
 *    slAddHead(&list, node);
 * where list and nodes are both pointers to structure
 * that begin with a next pointer. 
 */
void slSafeAddHead(void *listPt, void *node)
{
struct slList **ppt = (struct slList **)listPt;
struct slList *n = (struct slList *)node;

n->next = *ppt; 
*ppt = n; 
}


void *slPopHead(void *vListPt)
/* Return head of list and remove it from list. (Fast) */
{
struct slList **listPt = (struct slList **)vListPt;
struct slList *el = *listPt;
if (el != NULL)
    *listPt = el->next;
return el;
}

void *slPopTail(void *vListPt)
/* Return tail of list and remove it from list. (Not so fast) */
{
struct slList **listPt = (struct slList **)vListPt;
struct slList *el = *listPt;
if (el != NULL)
    {
    for (;;)
        {
        if (el->next == NULL)
            {
            *listPt = NULL;
            break;
            }
        listPt = &el->next;
        el = el->next;
        }
    }
return el;
}



void *slCat(void *va, void *vb)
/* Return concatenation of lists a and b.
 * Example Usage:
 *   struct slName *a = getNames("a");
 *   struct slName *b = getNames("b");
 *   struct slName *ab = slCat(a,b)
 */
{
struct slList *a = va;
struct slList *b = vb;
struct slList *end;
if (a == NULL)
    return b;
for (end = a; end->next != NULL; end = end->next)
    ;
end->next = b;
return a;
}

void slReverse(void *listPt)
/* Reverse order of a list.
 * Usage:
 *    slReverse(&list);
 */
{
struct slList **ppt = (struct slList **)listPt;
struct slList *newList = NULL;
struct slList *el, *next;

next = *ppt;
while (next != NULL)
    {
    el = next;
    next = el->next;
    el->next = newList;
    newList = el;
    }
*ppt = newList;
}

/* Free list */
void slFreeList(void *listPt)
{
struct slList **ppt = (struct slList**)listPt;
struct slList *next = *ppt;
struct slList *el;

while (next != NULL)
    {
    el = next;
    next = el->next;
    freeMem((char*)el);
    }
*ppt = NULL;
}

void slSort(void *pList, int (*compare )(const void *elem1,  const void *elem2))
/* Sort a singly linked list with Qsort and a temporary array. */
{
struct slList **pL = (struct slList **)pList;
struct slList *list = *pL;
int count;
count = slCount(list);
if (count > 1)
    {
    struct slList *el;
    struct slList **array;
    int i;
    array = needLargeMem(count * sizeof(*array));
    for (el = list, i=0; el != NULL; el = el->next, i++)
        array[i] = el;
    qsort(array, count, sizeof(array[0]), compare);
    list = NULL;
    for (i=0; i<count; ++i)
        {
        array[i]->next = list;
        list = array[i];
        }
    freeMem(array);
    slReverse(&list);
    *pL = list;       
    }
}

void slUniqify(void *pList, int (*compare )(const void *elem1,  const void *elem2), void (*free)())
/* Return sorted list with duplicates removed. 
 * Compare should be same type of function as slSort's compare (taking
 * pointers to pointers to elements.  Free should take a simple
 * pointer to dispose of duplicate element, and can be NULL. */
{
struct slList **pSlList = (struct slList **)pList;
struct slList *oldList = *pSlList;
struct slList *newList = NULL, *el, *next;
int origSize = 0, newSize = 0;  /* Keep these counts for debugging. */

slSort(&oldList, compare);
for (el = oldList; el != NULL; el = next)
    {
    ++origSize;
    next = el->next;
    if (newList == NULL || compare(&newList, &el) != 0)
        {
        slAddHead(&newList, el);
        ++newSize;
        }
    else if (free != NULL)
        free(el);
    }
slReverse(&newList);
*pSlList = newList;
}

void slRemoveEl(void *vpList, void *vToRemove)
/* Remove element from doubly linked list.  Usage:
 *    slRemove(&list, el);  */
{
struct slList **pList = vpList;
struct slList *toRemove = vToRemove;
struct slList *el, *next, *newList = NULL;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    if (el != toRemove)
	{
	slAddHead(&newList, el);
	}
    }
slReverse(&newList);
*pList = newList;
}



struct slName *newSlName(char *name)
/* Return a new name. */
{
int len = strlen(name);
struct slName *sn = needMem(sizeof(*sn)+len);
strcpy(sn->name, name);
return sn;
}


int slNameCmp(const void *va, const void *vb)
/* Compare two slNames. */
{
const struct slName *a = *((struct slName **)va);
const struct slName *b = *((struct slName **)vb);
return strcmp(a->name, b->name);
}

void slNameSort(struct slName **pList)
/* Sort slName list. */
{
slSort(pList, slNameCmp);
}

boolean slNameInList(struct slName *list, char *string)
/* Return true if string is in name list */
{
struct slName *el;
for (el = list; el != NULL; el = el->next)
    if (sameWord(string, el->name))
        return TRUE;
return FALSE;
}

char *slNameStore(struct slName **pList, char *string)
/* Put string into list if it's not there already.  
 * Return the version of string stored in list. */
{
struct slName *el;
for (el = *pList; el != NULL; el = el->next)
    {
    if (sameString(string, el->name))
	return el->name;
    }
el = newSlName(string);
slAddHead(pList, el);
return el->name;
}

struct slRef *refOnList(struct slRef *refList, void *val)
/* Return ref if val is already on list, otherwise NULL. */
{
struct slRef *ref;
for (ref = refList; ref != NULL; ref = ref->next)
    if (ref->val == val)
        return ref;
return NULL;
}

void refAdd(struct slRef **pRefList, void *val)
/* Add reference to list. */
{
struct slRef *ref;
AllocVar(ref);
ref->val = val;
slAddHead(pRefList, ref);
}

void refAddUnique(struct slRef **pRefList, void *val)
/* Add reference to list if not already on list. */
{
if (refOnList(*pRefList, val) == NULL)
    {
    refAdd(pRefList, val);
    }
}


void gentleFree(void *pt)
{
if (pt != NULL) freeMem((char*)pt);
}

int differentWord(char *s1, char *s2)
/* strcmp ignoring case - returns zero if strings are
 * the same (ignoring case) otherwise returns difference
 * between first non-matching characters. */
{
	char c1, c2;
	for (;;)
	{
		c1 = toupper(*s1++);
		c2 = toupper(*s2++);
		if (c1 != c2)
			return c2-c1;
		if (c1 == 0)
			return 0;
	}
}

boolean startsWith(char *start,char *string)
/* Returns TRUE if string begins with start. */
{
char c;
int i;

for (i=0; ;i += 1)
    {
    if ((c = start[i]) == 0)
        return TRUE;
    if (string[i] != c)
        return FALSE;
    }
}

boolean endsWith(char *string, char *end)
/* Returns TRUE if string ends with end. */
{
int sLen, eLen, offset;
sLen = strlen(string);
eLen = strlen(end);
offset = sLen - eLen;
if (offset < 0)
    return FALSE;
return sameString(string+offset, end);
}

char lastChar(char *s)
/* Return last character in string. */
{
if (s == NULL || s[0] == 0)
    return 0;
return s[strlen(s)-1];
}

char *memMatch(char *needle, int nLen, char *haystack, int hLen)
/* Returns first place where needle (of nLen chars) matches
 * haystack (of hLen chars) */
{
char c = *needle++;
hLen -= nLen;
nLen -= 1;
while (--hLen >= 0)
    {   
    if (*haystack++ == c && memcmp(needle, haystack, nLen) == 0)
        {
        return haystack-1;
        }
    }
return NULL;
}

void toUpperN(char *s, int n)
/* Convert a section of memory to upper case. */
{
while (--n >= 0)
    *s++ = toupper(*s);
}

void toLowerN(char *s, int n)
/* Convert a section of memory to upper case. */
{
while (--n >= 0)
    *s++ = tolower(*s);
}

void touppers(char *s)
/* Convert entire string to upper case. */
{
char c;
for (;;)
    {
    if ((c = *s) == 0) break;
    *s++ = toupper(c);
    }
}

void tolowers(char *s)
/* Convert entire string to lower case */
{
char c;
for (;;)
    {
    if ((c = *s) == 0) break;
    *s++ = tolower(c);
    }
}

void subChar(char *s, char oldChar, char newChar)
/* Substitute newChar for oldChar throughout string s. */
{
char c;
for (;;)
    {
    c = *s;
    if (c == 0)
	break;
    if (c == oldChar)
	*s = newChar;
    ++s;
    }
}

void stripChar(char *s, char c)
/* Remove all occurences of c from s. */
{
char *in = s, *out = s;
char b;

for (;;)
    {
    b = *out = *in++;
    if (b == 0)
       break;
    if (b != c)
       ++out;
    }
}

/* int chopString(in, sep, outArray, outSize); */
/* This chops up the input string (cannabilizing it)
 * into an array of zero terminated strings in
 * outArray.  It returns the number of strings. 
 * If you pass in NULL for outArray, it will just
 * return the number of strings that it *would*
 * chop. */
int chopString(char *in, char *sep, char *outArray[], int outSize)
{
int recordCount = 0;

for (;;)
    {
    if (outArray != NULL && recordCount >= outSize)
	break;
    /* Skip initial separators. */
    in += strspn(in, sep);
    if (*in == 0)
	break;
    if (outArray != NULL)
	outArray[recordCount] = in;
    recordCount += 1;
    in += strcspn(in, sep);
    if (*in == 0)
	break;
    if (outArray != NULL)
	*in = 0;
    in += 1;
    }
return recordCount;
}

int chopByWhite(char *in, char *outArray[], int outSize)
/* Like chopString, but specialized for white space separators. */
{
int recordCount = 0;
char c;
for (;;)
    {
    if (outArray != NULL && recordCount >= outSize)
	break;

    /* Skip initial separators. */
    while (isspace(*in)) ++in;
    if (*in == 0)
	break;
    
    /* Store start of word and look for end of word. */    
    if (outArray != NULL)
	outArray[recordCount] = in;
    recordCount += 1;
    for (;;)
        {
        if ((c = *in) == 0)
            break;
        if (isspace(c))
            break;
        ++in;
        }
    if (*in == 0)
	break;
 
    /* Tag end of word with zero. */
    if (outArray != NULL)
	*in = 0;
    /* And skip over the zero. */
    in += 1;
    }
return recordCount;
}

int chopByChar(char *in, char chopper, char *outArray[], int outSize)
/* Chop based on a single character. */
{
int i;
char c;
if (*in == 0)
    return 0;
for (i=0; i<outSize; ++i)
    {
    outArray[i] = in;
    for (;;)
	{
	if ((c = *in++) == 0)
	    return i+1;
	else if (c == chopper)
	    {
	    in[-1] = 0;
	    break;
	    }
	}
    }
return i;
}

char crLfChopper[] = "\n\r";
char whiteSpaceChopper[] = " \t\n\r";


/* Return first non-white space. */
char *skipLeadingSpaces(char *s)
{
char c;
if (s == NULL) return NULL;
for (;;)
    {
    c = *s;
    if (!isspace(c))
	return s;
    ++s;
    }
}

/* Return first white space or NULL if none.. */
char *skipToSpaces(char *s)
{
char c;
for (;;)
    {
    c = *s;
    if (c == 0)
        return NULL;
    if (isspace(c))
	return s;
    ++s;
    }
}



/* Replace trailing white space with zeroes. */
void eraseTrailingSpaces(char *s)
{
int len = strlen(s);
int i;
char c;

for (i=len-1; i>=0; --i)
    {
    c = s[i];
    if (isspace(c))
	s[i] = 0;
    else
	break;
    }
}

/* Remove white space from a string */
void eraseWhiteSpace(char *s)
{
char *in, *out;
char c;

in = out = s;
for (;;)
    {
    c = *in++;
    if (c == 0) 
	break;
    if (!isspace(c))
	*out++ = c;
    }
*out++ = 0;
}

char *trimSpaces(char *s)
/* Remove leading and trailing white space. */
{
    s = skipLeadingSpaces(s);
    eraseTrailingSpaces(s);
    if (s[0] == 0) return NULL;
    return s;
}

char *firstWordInLine(char *line)
/* Returns first word in line if any (white space separated).
 * Puts 0 in place of white space after word. */
{
char *e;
line = skipLeadingSpaces(line);
if ((e = skipToSpaces(line)) != NULL)
    *e = 0;
return line;
}

char *nextWord(char **pLine)
/* Return next word in *pLine and advance *pLine to next
 * word. */
{
char *s = *pLine, *e;
if (s == NULL || s[0] == 0)
    return NULL;
s = skipLeadingSpaces(s);
if (s[0] == 0)
    return NULL;
e = skipToSpaces(s);
if (e != NULL)
    *e++ = 0;
*pLine = e;
return s;
}

int stringArrayIx(char *string, char *array[], int arraySize)
/* Return index of string in array or -1 if not there. */
{
int i;
for (i=0; i<arraySize; ++i)
    {
    if (!differentWord(array[i], string))
        return i;
    }
return -1;
}

int ptArrayIx(void *pt, void *array, int arraySize)
/* Return index of pt in array or -1 if not there. */
{
int i;
void **a = array;
for (i=0; i<arraySize; ++i)
    {
    if (pt == a[i])
        return i;
    }
return -1;
}



FILE *mustOpen(char *fileName, char *mode)
/* Open a file - or squawk and die. */
{
FILE *f;

if (sameString(fileName, "stdin"))
    return stdin;
if (sameString(fileName, "stdout"))
    return stdout;
if ((f = fopen(fileName, mode)) == NULL)
    {
    char *modeName = "";
    if (mode)
        {
        if (mode[0] == 'r')
            modeName = " to read";
        else if (mode[0] == 'w')
            modeName = " to write";
        else if (mode[0] == 'a')
            modeName = " to append";
        }
    errAbort("Can't open %s%s: %s", fileName, modeName, strerror(errno));
    }
return f;
}

void mustWrite(FILE *file, void *buf, size_t size)
/* Write to a file or squawk and die. */
{
if (size != 0 && fwrite(buf, size, 1, file) != 1)
    {
    errAbort("Error writing %d bytes: %s\n", size, strerror(ferror(file)));
    }
}


void mustRead(FILE *file, void *buf, size_t size)
/* Read from a file or squawk and die. */
{
if (size != 0 && fread(buf, size, 1, file) != 1)
    errAbort("Error reading %d bytes: %s", size, strerror(ferror(file)));
}

void writeString(FILE *f, char *s)
/* Write a 255 or less character string to a file.
 * This will write the length of the string in the first
 * byte then the string itself. */
{
UBYTE bLen;
int len = strlen(s);

if (len > 255)
    {
    warn("String too long in writeString (%d chars):\n%s", len, s);
    len = 255;
    }
bLen = len;
writeOne(f, bLen);
mustWrite(f, s, len);
}

char *readString(FILE *f)
/* Read a string (written with writeString) into
 * memory.  freeMem the result when done. */
{
UBYTE bLen;
int len;
char *s;

if (!readOne(f, bLen))
    return NULL;
len = bLen;
s = needMem(len+1);
if (len > 0)
    mustRead(f, s, len);
return s;
}

char *mustReadString(FILE *f)
/* Read a string.  Squawk and die at EOF or if any problem. */
{
char *s = readString(f);
if (s == NULL)
    errAbort("Couldn't read string");
return s;
}

 
boolean fastReadString(FILE *f, char buf[256])
/* Read a string into buffer, which must be long enough
 * to hold it.  String is in 'writeString' format. */
{
UBYTE bLen;
int len;
if (!readOne(f, bLen))
    return FALSE;
if ((len = bLen)> 0)
    mustRead(f, buf, len);
buf[len] = 0;
return TRUE;
} 


void splitPath(char *path, char dir[256], char name[128], char extension[64])
/* Split a full path into components.  The dir component will include the
 * trailing / if any.  The extension component will include the starting
 * . if any.   Pass in NULL for dir, name, or extension if you don't care about
 * that part. */
{
char *dirStart, *nameStart, *extStart, *extEnd;
int dirSize, nameSize, extSize;

dirStart = path;
nameStart = strrchr(path,'/');
if (nameStart == NULL)	/* Do a little coping with MS-DOS style paths. */
    {
    nameStart = strrchr(path, '\\');
    if (nameStart != NULL)
	subChar(path, '\\', '/');
    }
if (nameStart == NULL)
    nameStart = path;
else
    nameStart += 1;
extStart = strrchr(nameStart, '.');
if (extStart == NULL)
    extStart = nameStart + strlen(nameStart);
extEnd = extStart + strlen(extStart);
if ((dirSize = (nameStart - dirStart)) >= 256)
    errAbort("Directory too long in %s", path);
if ((nameSize = (extStart - nameStart)) >= 128)
    errAbort("Name too long in %s", path);
if ((extSize = (extEnd - extStart)) >= 64)
    errAbort("Extension too long in %s", path);
if (dir != NULL)
    {
    memcpy(dir, dirStart, dirSize);
    dir[dirSize] = 0;
    }
if (name != NULL)
    {
    memcpy(name, nameStart, nameSize);
    name[nameSize] = 0;
    }
if (extension != NULL)
    {
    memcpy(extension, extStart, extSize);
    extension[extSize] = 0;
    }
}

void chopSuffix(char *s)
/* Remove suffix (last . in string and beyond) if any. */
{
char *e = strrchr(s, '.');
if (e != NULL)
    *e = 0;
}
    
void chopSuffixAt(char *s, char c)
/* Remove end of string from first occurrence of char c. 
 * chopSuffixAt(s, '.') is equivalent to regular chopSuffix. */
{
char *e = strrchr(s, c);
if (e != NULL)
    *e = 0;
}
    

void carefulClose(FILE **pFile)
/* Close file if open and null out handle to it. */
{
FILE *f;
if ((f = *pFile) != NULL)
    {
    if (f != stdin && f != stdout)
	fclose(f);
    *pFile = NULL;
    }
}
	
char *firstWordInFile(char *fileName, char *wordBuf, int wordBufSize)
/* Read the first word in file into wordBuf. */
{
FILE *f = mustOpen(fileName, "r");
fgets(wordBuf, wordBufSize, f);
fclose(f);
return trimSpaces(wordBuf);
}

int roundingScale(int a, int p, int q)
/* returns rounded a*p/q */
{
if (a > 100000 || p > 100000)
    {
    double x = a;
    x *= p;
    x /= q;
    return round(x);
    }
else
    return (a*p + q/2)/q;
}

int intAbs(int a)
/* Return integer absolute value */
{
return (a >= 0 ? a : -a);
}

int  rangeIntersection(int start1, int end1, int start2, int end2)
/* Return amount of bases two ranges intersect over, 0 or negative if no
 * intersection. */
{
int s = max(start1,start2);
int e = min(end1,end2);
return e-s;
}

bits32 byteSwap32(bits32 a)
/* Return byte-swapped version of a */
{
union {bits32 whole; UBYTE bytes[4];} u,v;
u.whole = a;
v.bytes[0] = u.bytes[3];
v.bytes[1] = u.bytes[2];
v.bytes[2] = u.bytes[1];
v.bytes[3] = u.bytes[0];
return v.whole;
}

