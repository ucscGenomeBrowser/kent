#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "sig.h"
#include "fa.h"
#include "nib.h"

int main(int argc, char *argv[])
{
char *inName, *outName;
struct dnaSeq *seq;
FILE *outFile;

int options = 0;
optionHash(&argc, argv);
if (optionExists("masked"))
    options = NIB_MASK_MIXED;

if (argc != 3)
    {
    errAbort(
        "faToNib - a program to convert .fa files to .nib files\n"
        "Usage: faToNib in.fa out.nib"
        "options:\n"
        "   -masked - use lower-case characters masked-out based\n"
        );
    }
dnaUtilOpen();
inName = argv[1];
outName = argv[2];
seq = faReadAllMixed(inName);
nibWriteMasked(options, seq, outName);
return 0;
}
