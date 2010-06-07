/* ixali.c - Index ali file (binary cdna alignment file, often "good.ali"
 * by name of the cdna. */
#include "common.h"
#include "sig.h"
#include "cda.h"
#include "snof.h"
#include "snofmake.h"

static int recCount = 0;
static boolean aliNextRecord(FILE *f, void *data, char **rName, int *rNameLen)
/* Reads next record of ali file and returns the name from it. */
{
struct cdaAli *cda;

if ((cda = cdaLoadOne(f)) == NULL)
    return FALSE;
if (++recCount % 10000 == 0)
    printf("Reading %d\n", recCount);
*rName = cda->name;
*rNameLen = strlen(cda->name);
//cdaFreeAli(cda);
return TRUE;
}

int main(int argc, char *argv[])
{
char *inName, *outName;
FILE *in;
bits32 sig;

if (argc != 3)
    {
    errAbort("This program makes a name index file for an .ali file\n"
             "usage: ixali file.ali file.ix\n");
    }
inName = argv[1];
outName = argv[2];
in = mustOpen(inName, "rb");
mustReadOne(in, sig);
if (sig !=  aliSig)
    {
    fprintf(stderr, "%s isn't a good ali file expected %x got %x\n", 
	inName, aliSig, sig);
    exit(-3);
    }
if (!snofMakeIndex(in, outName, aliNextRecord, NULL))
    {
    exit(-4);
    }
return 0;
}
