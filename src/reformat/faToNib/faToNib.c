#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "sig.h"
#include "fa.h"

int main(int argc, char *argv[])
{
char *inName, *outName;
struct dnaSeq *seq;
FILE *outFile;

if (argc != 3)
    {
    errAbort("faToNib - a program to convert .fa files to .nib files\n"
             "Usage: faToNib in.fa out.nib");
    }
dnaUtilOpen();
inName = argv[1];
outName = argv[2];
seq = faReadDna(inName);
nibWrite(seq, outName);
return 0;
}
