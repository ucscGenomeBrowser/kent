/* testXap - Initial test harness for Xap XML Parser. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "xp.h"
#include "xap.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testXap - Initial test harness for Xap XML Parser\n"
  "usage:\n"
  "   testXap file.xml\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct testData
    {
    struct testData *next;
    FILE *f;
    int depth;
    };

void indent(FILE *f, int count)
/* Indent the given count. */
{
int i;
for (i=0; i<count; ++i)
    {
    fputc(' ', f);
    fputc(' ', f);
    }
}

void testStart(void *userData, char *name, char **atts)
{
struct testData *td = userData;
int i;
indent(stdout, td->depth);
printf("%s", name);
for (i=0; ; i += 2)
    {
    char *name, *val;
    if ((name = atts[i]) == NULL)
        break;
    val = atts[i+1];
    printf(" %s='%s'", name, val);
    }
printf("\n");
td->depth += 1;
}

void testEnd(void *userData, char *name, char *text)
{
struct testData *td = userData;
td->depth -= 1;
indent(stdout, td->depth);
printf("/%s %d\n", name, strlen(text));
}

void simpleStart(void *userData, char *name, char **atts)
{
struct testData *td = userData;
indent(stdout, td->depth);
printf("%s\n", name);
++td->depth;
}

void simpleEnd(void *userData, char *name, char *text)
{
struct testData *td = userData;
--td->depth;
}

int testRead(void *userData, char *buf, int bufSize)
{
struct testData *td = userData;
return fread(buf, 1, bufSize, td->f);
}

void testXap(char *xmlFile)
/* testXap - Initial test harness for Xap XML Parser. */
{
struct xp *xp;
static struct testData td;
td.f = mustOpen(xmlFile, "r");
td.depth = 0;
xp = xpNew(&td, simpleStart, simpleEnd, testRead, xmlFile);
xpParse(xp);
xpFree(&xp);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
testXap(argv[1]);
return 0;
}
