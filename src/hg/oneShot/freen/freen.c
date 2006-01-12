/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "xap.h"

static char const rcsid[] = "$Id: freen.c,v 1.63 2005/12/20 17:52:26 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file tag\n");
}

int level = 0;

void *atStart(struct xap *xap, char *name, char **atts)
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
return name;
}

void atEnd(struct xap *xap, char *name)
{
char *text = trimSpaces(xap->stack->text->string);
if (text[0] != 0)
    {
    spaceOut(stdout, level*2);
    printf("%s\n", trimSpaces(text));
    }
level -= 1;
spaceOut(stdout, level*2);
printf("</%s>\n", name);
}

void freen(char *fileName, char *tag)
/* Test some hair-brained thing. */
{
struct xap *xap = xapOpen(fileName, atStart, atEnd);
char *s;
while ((s = xapNext(xap, tag)) != NULL)
    ;
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
   usage();
freen(argv[1], argv[2]);
return 0;
}
