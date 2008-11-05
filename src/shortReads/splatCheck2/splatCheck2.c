/* splatCheck2 - Check splat output against a MAQ-generated test set.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: splatCheck2.c,v 1.1 2008/11/05 00:02:09 kent Exp $";

int offset = 0;
boolean bed = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "splatCheck2 - Check splat output against a MAQ-generated test set.\n"
  "usage:\n"
  "   splatCheck2 in.fa in.splat out.miss out.bad\n"
  "options:\n"
  "   -offset=N - add offset.\n"
  "   -bed - treat input as bed rather than splat file\n"
  );
}

static struct optionSpec options[] = {
   {"offset", OPTION_INT},
   {"bed", OPTION_BOOLEAN},
   {NULL, 0},
};

struct hash *hashFaNames(char *fileName)
/* Return a hash full of the names (but not sequence) of all 
 * records in fa file. */
{
struct hash *hash = hashNew(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    line = skipLeadingSpaces(line);
    if (line[0] == '>')
        {
	line += 1;
	char *name = firstWordInLine(line);
	if (name == NULL || name[0] == 0)
	    errAbort("Empty name in fasta file line %d of %s", lf->lineIx, lf->fileName);
	if (hashLookup(hash, name))
	    errAbort("Duplicate name %s in fasta file line %d of %s",
	    	name, lf->lineIx, lf->fileName);
	hashAdd(hash, name, NULL);
	}
    }
lineFileClose(&lf);
return hash;
}

void parseNameNeighborhood(char *name, char **retChrom, int *retStart, int *retEnd)
/* Parse chromosome and start/end out of name field.   This is expected to look something
 * like:  chr22_382900_383094_3c/1  or chr22_20M_382900_383094_3c/1 */
{
char *words[6];
int wordCount = chopByChar(name, '_', words, ArraySize(words));
if (wordCount < 4 || wordCount > 5)
    errAbort("Don't understand name field");
*retChrom = words[0];
*retStart = sqlUnsigned(words[wordCount-3]);
*retEnd = sqlUnsigned(words[wordCount-2]);
}

int splatCheck2(char *inFa, char *inSplat, char *outMiss, char *outWrong)
/* splatCheck2 - Check splat output against a MAQ-generated test set.. */
{
int nameCol = (bed ? 3 : 6);
struct lineFile *lf = lineFileOpen(inSplat, TRUE);
FILE *missF = mustOpen(outMiss, "w");
FILE *badF = mustOpen(outWrong, "w");
char *row[7];
struct hash *allHash = hashFaNames(inFa);
struct hash *mappedHash = hashNew(0);	/* Keep track of reads we've seen here. */
struct hash *goodHash = hashNew(0);	/* Keep track of good reads here. */
while (lineFileNextRow(lf, row, nameCol+1))
    {
    /* Read in line and parse it, track it. */
    char *chrom = row[0];
    int chromStart = sqlUnsigned(row[1]) - offset;
    int chromEnd = sqlUnsigned(row[2]) - offset;
    char *name = row[nameCol];
    char *origName = hashStore(mappedHash, name)->name;
    char *sourceChrom;
    int sourceStart, sourceEnd;

    /* Parse out name field to figure out where we expect it to map. */
    parseNameNeighborhood(name, &sourceChrom, &sourceStart, &sourceEnd);
    if (sameString(sourceChrom, chrom) && rangeIntersection(chromStart, chromEnd, sourceStart, sourceEnd) > 0)
        {
	hashStore(goodHash, origName);
	}
    }
struct hashEl *hel, *helList = hashElListHash(allHash);
int allCount = allHash->elCount;
int missCount = 0, badCount = 0;
for (hel = helList; hel != NULL; hel = hel->next)
    {
    char *name = hel->name;
    if (!hashLookup(mappedHash, name))
	{
        fprintf(missF, "%s\n", name);
	++missCount;
	}
    else
        {
	if (!hashLookup(goodHash, hel->name))
	    {
	    fprintf(badF, "%s\n", hel->name);
	    ++badCount;
	    }
	}
     
    }
carefulClose(&badF);
carefulClose(&missF);
verbose(1, "Total reads %d\n", allCount);
verbose(1, "Unmapped %d (%5.2f%%)\n", missCount, 100.0*missCount/allCount);
verbose(1, "Mapped wrong %d (%5.2f%%)\n", badCount, 100.0*badCount/allCount);
return -(missCount + badCount);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
offset = optionInt("offset", offset);
bed = optionExists("bed");
if (argc != 5)
    usage();
return splatCheck2(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
