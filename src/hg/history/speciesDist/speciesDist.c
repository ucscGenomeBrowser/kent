#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"

static char const rcsid[] = "$Id: speciesDist.c,v 1.5 2006/01/23 20:43:10 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "speciesDist - given element lists and distance matrices, find distances\n"
  "     that represent the species tree.  Ouputs distance matrix with species names in rows \n"
  "     suitable for input to neighbor joining algorithm.\n"
  "usage:\n"
  "   speciesDist genomes distances outDist\n"
  "arguments:\n"
  "   genomes        file with element lists\n"
  "   distances      file with distance matrices\n"
  "   outDist        file to output species distance matrix\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int compar(const void *one, const void *two)
{
char *s1 = *(char **)one;
char *s2 = *(char **)two;

return strcmp(s1, s2);
}


void speciesDist(char *genomeFileName, char *distanceFileName, char *outFileName)
{
FILE *f = mustOpen(outFileName, "w");
struct distance *distanceList;
struct hash *elementHash;
struct genome *genomes, *genome;
struct distance *d;
int numSpecies = 0;
struct hash *genomeHash = newHash(5);
struct hash *genomeNameHash = newHash(5);
struct hash *genomeNumHash = newHash(5);
char **genomeNames;
int ii;
int numExpect;
double *distanceArray;
struct hash *distHash = NULL;
struct hash *distElemHash = NULL;

genomes = readGenomes(genomeFileName);
//printGenomes(genomes);

for(genome=genomes; genome; genome=genome->next)
    numSpecies++;

genomeNames = (char **)needMem(numSpecies * sizeof(char *));
for(genome=genomes, ii=0; genome; ii++,genome=genome->next)
    {
    hashAdd(genomeHash, genome->name, (void *)genome);
    genomeNames[ii] = genome->name;
    }

qsort(genomeNames, ii, sizeof(char *), compar);

for(ii = 0; ii < numSpecies; ii++)
    hashAdd(genomeNumHash, genomeNames[ii], (void *)ii);

distanceList = readDistances(distanceFileName, genomeHash, &distHash, &distElemHash);

numExpect = (numSpecies * numSpecies - numSpecies ) /2;
distanceArray = (double *)needMem(sizeof(double) * numSpecies * numSpecies);

for(ii = 0; ii < numSpecies; ii++)
    distanceArray[ii * numSpecies + ii] = 0.0;

for(d=distanceList; numExpect && d; d = d->next)
    {
    char buffer[512];
    char *s1 = d->e1->species;
    char *s2 = d->e2->species;

    if (sameString(s1, s2))
	continue;

    if (strcmp(s1, s2) < 0)
	{
	s2 = d->e1->species;
	s1 = d->e2->species;
	}
    safef(buffer, sizeof(buffer), "%s-%s",s1,s2);

    if (!hashLookup(genomeNameHash, buffer))
	{
	int row, col;

	numExpect--;
	hashStoreName(genomeNameHash, buffer);

	row = (int)hashMustFindVal(genomeNumHash, s1);
	col = (int)hashMustFindVal(genomeNumHash, s2);

	distanceArray[row * numSpecies + col] = d->distance;
	distanceArray[col * numSpecies + row] = d->distance;

	//printf(" %g : %s.%s.%d -> %s.%s.%d\n",d->distance,d->e1->species,d->e1->name,d->e1->version,
	 //   d->e2->species,d->e2->name,d->e2->version);
	}

    }

fprintf(f, "%d\n", numSpecies);

for(ii=0; ii < numSpecies; ii++)
    {
    int jj;

    fprintf(f, "%-20s ",genomeNames[ii]);
    for(jj=0; jj < numSpecies; jj++)
	fprintf(f, "%g ",distanceArray[ii * numSpecies + jj]);
    fprintf(f, "\n");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();

//verboseSetLevel(2);
speciesDist(argv[1],argv[2], argv[3]);
return 0;
}
