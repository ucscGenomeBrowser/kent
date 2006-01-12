/* test - Test something. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "xp.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "test - Test something\n"
  "usage:\n"
  "   test a \n"
  "options:\n"
  );
}

int level = 0;

void atStart(void *userData, char *name, char **atts)
{
char *att;
spaceOut(stdout, level*2);
printf("<%s", name);
while ((att = *atts++) != NULL)
     {
     printf(" %s=\"", att);
     att = *atts++;
     printf("%s\"", att);
     }
printf(">\n");
level += 1;
}

void atEnd(void *userData, char *name, char *text)
{
text = trimSpaces(text);
if (text[0] != 0)
    {
    spaceOut(stdout, level*2);
    printf("%s\n", trimSpaces(text));
    }
level -= 1;
spaceOut(stdout, level*2);
printf("</%s>\n", name);
}


void test(char *in)
/* test - Test something. */
{
FILE *f = mustOpen(in, "r");
struct xp *xp = xpNew(f, atStart, atEnd, xpReadFromFile, in);
while (xpParseNext(xp, "interest"))
    ;
}

int main(int argc, char *argv[], char *env[])
/* Process command line. */
{
int i;
if (argc != 2)
   usage();
test(argv[1]);
return 0;
}
