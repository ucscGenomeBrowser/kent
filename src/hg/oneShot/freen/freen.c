/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fa.h"
#include "twoBit.h"

static char const rcsid[] = "$Id: freen.c,v 1.42 2004/02/24 22:04:13 kent Exp $";

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen something");
}

void freen(char *in)
/* Test some hair-brained thing. */
{
if (twoBitIsFile(in))
    printf("%s is a twoBit file\n", in);
else if (twoBitIsRange(in))
    {
    char *file, *seq;
    int start, end;
    printf("%s is a range in a twoBit file\n", in);
    twoBitParseRange(in, &file, &seq, &start, &end);
    printf("file %s, seq %s, start %d, end %d\n", file, seq, start, end);
    }
else
    {
    printf("%s is not twoBit\n", in);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
