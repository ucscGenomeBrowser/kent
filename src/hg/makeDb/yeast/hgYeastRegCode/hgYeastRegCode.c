/* hgYeastRegCode - Load files from the regulatory code paper 
 * (large scale CHIP-CHIP on yeast) into database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "jksql.h"
#include "hgRelate.h"

static char const rcsid[] = "$Id: hgYeastRegCode.c,v 1.1 2004/09/13 02:45:54 kent Exp $";

char *tmpDir = ".";
char *tableName = "transRegCode";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgYeastRegCode - Load files from the regulatory code paper (large scale \n"
  "CHIP-CHIP on yeast) into database\n"
  "usage:\n"
  "   hgYeastRegCode database gffDir output.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void hgYeastRegCode(char *database, char *gffDir, char *outBed)
/* hgYeastRegCode - Load files from the regulatory code paper 
 * (large scale CHIP-CHIP on yeast) into database. */
{
static char *consLevelPath[3] = {"3", "2", "0"};
static char *consLevelBed[3] = {"2", "1", "0"};
static char *pLevelPath[3] = {"p001b", "p005b", "nobind"};
static char *pLevelBed[3] = {"good", "weak", "none"};
int cIx, pIx;
FILE *f = mustOpen(outBed, "w");

for (cIx=0; cIx<3; ++cIx)
   {
   for (pIx=0; pIx<3; ++pIx)
       {
       struct lineFile *lf;
       char *line, *row[10];
       char fileName[PATH_LEN];
       int score = 1000 / (cIx + pIx + 1);
       char *pLevel = pLevelBed[pIx];
       char *consLevel = consLevelBed[cIx];
       safef(fileName, sizeof(fileName), "%s/IGR_v24.%s.%s.GFF",
       	   gffDir, consLevelPath[cIx], pLevelPath[pIx]);
       lf = lineFileOpen(fileName, TRUE);
       while (lineFileRow(lf, row))
            {
	    char *name = row[9];
	    char *e;
	    if (!sameWord(row[8], "Site"))
	        errAbort("Expecting 'Site' line %d of %s", lf->lineIx, lf->fileName);
	    e = strchr(name, ';');
	    if (e == NULL)
	        errAbort("Expecting semicolon line %d of %s", lf->lineIx, lf->fileName);
	    *e = 0;
	    fprintf(f, "chr%s\t", row[0]);
	    fprintf(f, "%d\t", lineFileNeedNum(lf, row, 1) - 1);
	    fprintf(f, "%d\t", lineFileNeedNum(lf, row, 2));
	    fprintf(f, "%s\t", name);
	    fprintf(f, "%s\t", pLevel);
	    fprintf(f, "%s\n", consLevel);
	    }
       }
   }
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hgYeastRegCode(argv[1], argv[2], argv[3]);
return 0;
}
