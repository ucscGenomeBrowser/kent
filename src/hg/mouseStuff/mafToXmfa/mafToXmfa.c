/* mafToXmfa - Convert from MAF to XMFA format. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "maf.h"
#include "hdb.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafToXmfa - Convert from MAF to XMFA format\n"
  "usage:\n"
  "   xmfaToMaf in.maf out.xmfa db1=org1 db1=org1 db2=org2 ... dbN=orgN\n"
  );
}

void mafToXmfa(char *in, char *out)
/* mafToXmfa - Convert from maf to xmfa format. */
{
struct mafAli* ali;
struct mafFile* maf;
FILE* f;

f = fopen(out, "w");
if(out == NULL) {
    errAbort("Cannot open output file %s\n", out);
}

maf = mafOpen(in);

while((ali = mafNext(maf)) != NULL) {
    struct mafComp *comp;
    
    for (comp = ali->components; comp != NULL; comp = comp->next) {
        char database[64];
        char chrom[64];
        int start;
        int end;

        if(sscanf(comp->src, "%[^.].%[^.]", database, chrom) != 2) {
            errAbort("Could not parse '%s' into db.chr format\n", comp->src);
        }
        
        start = mafPlusStart(comp);
        end   = start + comp->size;

        start = start + 1; /* convert to one based */

        fprintf(f, ">%s %s:%d-%d %c\n", optionVal(database, database), chrom, start, end, comp->strand);
        writeSeqWithBreaks(f, comp->text, strlen(comp->text), 60);
    }


    if(sameString(maf->scoring, "mlagan"))
        fprintf(f, "= score=%.1f\n", ali->score);
    else
        fprintf(f, "= score=%.1f %s\n", ali->score, maf->scoring);
    
    mafAliFree(&ali);
}

fclose(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc < 3)
    usage();
mafToXmfa(argv[1],argv[2]);
return 0;
}
