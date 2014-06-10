/* freen - My Pet Freen. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "htmlPage.h"
#include "sqlList.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};


void freen(char *input)
/* Do something, who knows what really */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
char *line;
int maxCount = 4;
while (lineFileNext(lf, &line, NULL))
    {
    uglyf("line: %s\n", line);
    char *word = nextWord(&line);
    uglyf("word = %s\n", word);
    if (--maxCount == 0)
        break;
    }
lineFileSeek(lf, 0, SEEK_SET);
while (lineFileNext(lf, &line, NULL))
    {
    uglyf("line: %s\n", line);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
