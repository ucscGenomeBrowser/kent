/* qaToQa - convert from compressed to uncompressed 
 * quality score format. */
#include "common.h"
#include "linefile.h"
#include "qaSeq.h"
#include "options.h"

static char const rcsid[] = "$Id: qacToQa.c,v 1.4 2004/01/28 02:28:59 kate Exp $";

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

while ((qa = qacReadNext(in, isSwapped)) != NULL)
    {
    if (name != NULL)
        if (!sameString(qa->name, name))
            continue;
    qaWriteNext(out, qa);
    qaSeqFree(&qa);
    }
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
