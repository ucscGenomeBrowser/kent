/* qaToQa - convert from compressed to uncompressed 
 * quality score format. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "qaSeq.h"
#include "options.h"
#include "verbose.h"


static struct optionSpec optionSpecs[] = {
        {"name", OPTION_STRING},
        {NULL, 0}
};

static char *name = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
"qacToQa - convert from compressed to uncompressed\n"
"quality score format.\n"
"usage:\n"
"   qacToQa in.qac out.qa\n"
   "\t-name=name  restrict output to just this sequence name\n"
    );

}

void qacToQa(char *inName, char *outName)
/* qacToQa - convert from compressed to uncompressed 
 * quality score format. */
{
boolean isSwapped;
FILE *in = qacOpenVerify(inName, &isSwapped);
FILE *out = mustOpen(outName, "wb");
struct qaSeq *qa;
int size = 0;

while ((qa = qacReadNext(in, isSwapped)) != NULL)
    {
    if (name != NULL)
        if (!sameString(qa->name, name))
            continue;
    size += qa->size;
    qaWriteNext(out, qa);
    qaSeqFree(&qa);
    }
verbose(3, "qa total size: %d\n", size);
fclose(in);
fclose(out);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);   
if (argc < 2)
    usage();
name = optionVal("name", NULL);
qacToQa(argv[1], argv[2]);
return 0;
}
