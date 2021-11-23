/* hgBedSources - Split a bed file containing a comma-separate list of sources
        (currently, just bed5Sources)
        into two files suitable for loading as tables:
            * idName fomat: unique alpha-sorted list of sources with auto-inc ids
            * bedNSources format: BED N plus source count and comma-sep list of ids
*/

/* Copyright (C) 2011 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "hgRelate.h"
#include "verbose.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "\nhgBedSources -  Split a bed file containing a comma-separated list of sources\n"
  "        into two files suitable for loading as tables:\n"
  "             * idName fomat: unique alpha-sorted list of sources with auto-inc ids\n"
  "             * bedNSources format: BED N plus source count and comma-sep list of ids\n"
  "        Currently, only bed5Sources format is supported.\n"
  "usage:\n"
  "       hgBedSources in.bed outRoot\n\n"
  "This will create out.bed and outSources.tab.\n"
  "Input file should be tab-separated.\n"
    );
}

struct hash *makeSourceFile(char *inFile, char *sourceFile, int *sourceCount)
/* Create the source file with ids and names.
   Return a hash of sourceId's, keyed by name */
{
struct hash *prepSourceHash = newHash(0);
struct hash *sourceHash = newHash(0);
struct slName *name, *sourceNames = NULL;

// Extract sources from input file
struct lineFile *lf = lineFileOpen(inFile, TRUE);
char *row[7];
while (lineFileRow(lf, row))
    {
    sourceNames = slNameListFromComma(row[6]);
    for (name = sourceNames; name != NULL; name = name->next)
        hashStore(prepSourceHash, name->name);
    }
lineFileClose(&lf);

// Alpha sort sources by name, add ids, and write to sources file
struct hashEl *el, *elList = hashElListHash(prepSourceHash);
slSort(&elList, hashElCmp);

FILE *f = hgCreateTabFile(".", sourceFile);
int sourceId = 0;
for (el = elList; el != NULL; el = el->next)
    {
    hashAddInt(sourceHash, el->name, sourceId);
    fprintf(f, "%d\t%s\n", sourceId, el->name);
    sourceId++;
    }
verbose(2, "%d sources\n", sourceId);
carefulClose(&f);

if (sourceCount)
    *sourceCount = sourceId;
hashFree(&prepSourceHash);
return sourceHash;
}

void makeBedFile(char *inFile, char *bedFile, struct hash *sourceHash, int allSourceCount)
/* Create the bedNSources file */
{
char *line, *lastTab;
struct dyString *ds;
struct slName *source, *sourceNames;
int sourceId = 0;
int sourceCount;

struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(bedFile, "w");

while (lineFileNextReal(lf, &line))
    { 
    lastTab = strrchr(line, '\t');
    if (!lastTab)
        errAbort("Line %d of %s is not tab-separated", lf->lineIx, lf->fileName);
    *lastTab = 0;
    sourceNames = slNameListFromComma(++lastTab);
    ds = dyStringNew(0);
    sourceCount = 0;
    for (source = sourceNames; source != NULL; source = source->next)
        {
        sourceId = hashIntVal(sourceHash, source->name);
        if (sourceId < 0 || sourceId > allSourceCount-1)
            errAbort("Invalid source ID %d for %s, line %d of %s",
                     sourceId, source->name, lf->lineIx, lf->fileName);
        dyStringPrintf(ds, "%d,", sourceId);
        sourceCount++;
        }
    fprintf(f, "%s\t%u\t%s\n", line, sourceCount, dyStringCannibalize(&ds));
    //freeMem(lastTab);
    }
lineFileClose(&lf);
carefulClose(&f);
}

void hgBedSources(char *inFile, char *outRoot)
/* Main function */
{
char sourceFile[256], outFile[256];
int sourceCount;
struct hash *sourceHash;

/* Create the sources file and source hash with unique ids */
safef(sourceFile, ArraySize(sourceFile), "%sSources", outRoot);
sourceHash = makeSourceFile(inFile, sourceFile, &sourceCount);

/* Create the bedSources file */
safef(outFile, ArraySize(outFile), "%s.bed", outRoot);
makeBedFile(inFile, outFile, sourceHash, sourceCount);

/* Cleanup */
//freeHash(&sourceHash);
//bed5SourcesFreeList(&bedList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
hgBedSources(argv[1], argv[2]);
return 0;
}
