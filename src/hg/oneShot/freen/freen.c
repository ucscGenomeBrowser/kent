/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "jksql.h"
#include "genePred.h"
#include "xAli.h"

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen in out");
}

void genePredOffset(struct genePred *gp, int offset)
/* Add offset to gene prediction. */
{
int i;
gp->txStart += offset;
gp->txEnd += offset;
gp->cdsStart += offset;
gp->cdsEnd += offset;
for (i=0; i<gp->exonCount; ++i)
    {
    gp->exonStarts[i] += offset;
    gp->exonEnds[i] += offset;
    }
}

void oldFreen(char *input, char *output)
{
FILE *f = mustOpen(output, "w");
struct lineFile *lf = NULL;
char *row[50];
struct genePred *gp;

lf = lineFileOpen(input, TRUE);
while (lineFileChop(lf, row))
    {
    gp = genePredLoad(row);
    genePredOffset(gp, 100000);
    genePredTabOut(gp, f);
    }
}

void freen(char *root)
{
char buf[256];
char *hello = "hello";
int fd;
struct xAli *xa = NULL;

snprintf(buf, sizeof(buf), "%sXXXXXX", root);
mkstemp(buf);
fd = mkstemp(buf);
printf("%d\n", fd);
if (fd > 0)
   write(fd, hello, strlen(hello));
else
   perror("mkstemp error");
xAliFree(&xa);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
}
