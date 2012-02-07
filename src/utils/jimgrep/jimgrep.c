/* Jim grep - crude approximation of grep. */
#include "common.h"
#include <io.h>


_inline toUp(char c)
{
if (isalpha(c))
    c |= 0x20;
return c;
}

char *findInString(const char *string, const char *pattern)
/* Return first occurrence of pattern in string.
 * (Ignores case) */
{
char c1 = toUp(*pattern++);
int patSize = strlen(pattern);
char c;

while ((c = *string++) != 0)
    {
    if (c1 == toUp(c))
        {
        int i;
        for (i=0; i<patSize; ++i)
            {
            if (toUp(string[i]) != toUp(pattern[i]))
                break;
            }
        if (i == patSize)
            return (char*)(string-1);
        }
    }
return NULL;
}

char *findBefore(char *bufStart, char *pos, char c)
/* Scan backwards from pos to bufStart looking for first
 * occurence of character.  Return position right after
 * that.  */
{
for (;;)
    {
    if (--pos == bufStart)
        return bufStart;
    if (*pos == c)
        return pos+1;
    }
}

char *findAfter(char *pos, char *bufEnd, char c)
/* Scan forwards from pos to bufEnd looking for 
 * last occurence of character. Return position 
 * just before that. */
{
for (;;)
    {
    if (++pos == bufEnd)
        return bufEnd;
    if (*pos == c)
        return pos;
    }
}

void fgrepOne(char *pattern, char *fileName, boolean ignoreCase, boolean singleFile)
/* Gnarly, speed tweaked, unbuffered i/o grep. Why? Because it's
 * 4x faster, which matters on those huge GenBank files! */
{
char buf[8*1024+1];
int bufRead;
int remainder = 0;
int fd;
char *s;
int patSize = strlen(pattern);
char *(*searcher)(const char *string, const char *pat); 

searcher = (ignoreCase ? findInString : strstr);

fd = open(fileName, _O_RDONLY|_O_BINARY);
if (fd < 0)
    errAbort("Couldn't open %s\n", fileName);

for (;;)
    {
    bufRead = read(fd, buf + remainder, sizeof(buf)-1-remainder) + remainder;
    if (bufRead == 0)
        break;
    buf[bufRead] = 0;
    s = buf;
    while ((s = searcher(s, pattern)) != NULL)
        {
        char *lineStart = findBefore(buf, s, '\n');
        char *lineAfter = findAfter(s, buf+bufRead, '\n');
        if (lineAfter[-1] == '\r')
            --lineAfter;
        if (!singleFile)
            printf("%s: ", fileName);
        mustWrite(stdout, lineStart, lineAfter-lineStart);
        putchar('\n');
        s += patSize;
        }
    s = findBefore(buf, buf+bufRead, '\n');
    if (s == buf)
        remainder = 0;
    else
        {
        remainder = buf + bufRead - s;
        memmove(buf, s, remainder);
        }
    if (bufRead < sizeof(buf)-1)
        break;
    }
close(fd);
}

int main(int argc, char *argv[])
{
char *fileName, *pattern;
int i;
boolean ignoreCase = FALSE;
int startFileIx = 2;
boolean singleFile = FALSE;

if (argc < 3 || (argv[1][0] == '-' && argc < 4))
    {
    errAbort("grep - print lines of file that include pattern\n"
             "usage:\n"
             "    grep pattern file(s)\n"
             "or\n"
             "    grep -i pattern file(s)\n"
             "The -i makes the program ignore case.\n");
    }

pattern = argv[1];
if (sameString(pattern, "-i"))
    {
    startFileIx = 3;
    pattern = argv[2];
    ignoreCase = TRUE;
    }
singleFile = (startFileIx+1 == argc);
for (i=startFileIx; i<argc; ++i)
    {
    fileName = argv[i];
    fgrepOne(pattern, fileName, ignoreCase, singleFile);
    }
return 0;
}