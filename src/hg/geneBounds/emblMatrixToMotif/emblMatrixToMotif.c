/* emblMatrixToMotif - Convert transfac matrix in EMBL format to dnaMotif. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "dnaMotif.h"
#include "dnaMotifSql.h"
#include "emblParse.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "emblMatrixToMotif - Convert transfac matrix in EMBL format to dnaMotif\n"
  "usage:\n"
  "   emblMatrixToMotif in.embl out.tab\n"
  "options:\n"
  "   -org=human   get only human (or mouse or whatever).\n"
  );
}

char *organism = NULL;

struct dnaMotif *emblToMotif(char *name, struct hash *hash)
/* Convert hash of embl record to motif. */
{
#define maxCount 99
static int counts[maxCount][4];
int startReal=0, endReal=-1, realSize=0, i, j, total;
char num[3];
char *line, *row[4];
struct dnaMotif *motif = NULL;
float *cols[4];

/* Collect all counts. */
memset(counts, 0, sizeof(counts));
for (i=0; i<maxCount; ++i)
    {
    snprintf(num, sizeof(num), "%02d", i+1);
    if ((line = hashFindVal(hash, num)) == NULL)
        break;
    if (chopLine(line, row) != 4)
        errAbort("Expecting 4 words in motif %s line %s", name, num);
    total = 0;
    for (j=0; j<4; ++j)
       {
       int val = atoi(row[j]);
       total += val;
       counts[i][j] = val;
       }
    if (total > 0)
       {
       if (endReal == -1)
	   startReal = i;
       endReal = i;
       }
    }
if (endReal == -1)
    errAbort("No motif matrix for %s", name);


/* Allocate dnaMotif structure that can fit counts. */
realSize = endReal - startReal + 1;
AllocVar(motif);
motif->name = cloneString(name);
motif->columnCount = realSize;
cols[0] = AllocArray(motif->aProb, realSize);
cols[1] = AllocArray(motif->cProb, realSize);
cols[2] = AllocArray(motif->gProb, realSize);
cols[3] = AllocArray(motif->tProb, realSize);

/* Fill in dnaMotif with counts converted to
 * probabilities. */
for (i=startReal; i<=endReal; ++i)
    {
    double scale;
    total = 0;
    for (j=0; j<4; ++j)
        total += counts[i][j];
    scale = 1.0 / total;
    for (j=0; j<4; ++j)
	cols[j][i-startReal] = counts[i][j] * scale;
    }

return motif;
#undef maxCount
}

boolean orgFits(struct hash *hash)
/* Return TRUE if it's an organism that passes filter. */
{
char *bf;
if (organism == NULL)
    return TRUE;
bf = hashFindVal(hash, "BF");
if (bf == NULL)
    return FALSE;
return stringIn(organism, bf) != NULL;
}

void emblMatrixToMotif(char *inName, char *outName)
/* emblMatrixToMotif - Convert transfac matrix in EMBL format to dnaMotif. */
{
struct hash *hash = NULL;
struct lineFile *lf = emblOpen(inName, NULL);
FILE *f = mustOpen(outName, "w");
struct dnaMotif *motif;

while ((hash = emblRecord(lf)) != NULL)
    {
    char *ac = hashFindVal(hash, "AC");
    char *po = hashFindVal(hash, "P0");
    if (ac != NULL && po != NULL && orgFits(hash))
        {
	motif = emblToMotif(ac, hash);
	dnaMotifTabOut(motif, f);
	dnaMotifFree(&motif);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
organism = optionVal("org", NULL);
emblMatrixToMotif(argv[1], argv[2]);
return 0;
}
