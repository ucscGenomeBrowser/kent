/* faSomeRecords - Extract multiple fa records. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "fa.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faSomeRecords - Extract multiple fa records\n"
  "usage:\n"
  "   faSomeRecords in.fa listFile out.fa\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct hash *hashLines(char *fileName)
/* Read all lines in file and put them in a hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[1];
struct hash *hash = newHash(0);
while (lineFileRow(lf, row))
    hashAdd(hash, row[0], NULL);
lineFileClose(&lf);
return hash;
}

void faSomeRecords(char *faIn, char *listName, char *faOut)
/* faSomeRecords - Extract multiple fa records. */
{
struct hash *hash = hashLines(listName);
char *line, *word;
struct lineFile *lf = lineFileOpen(faIn, TRUE);
FILE *f = mustOpen(faOut, "w");
boolean passMe = FALSE;

while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '>')
	{
	char *d = cloneString(line);
	passMe = FALSE;
	line += 1;
	word = nextWord(&line);
	if (word != NULL)
	    {
	    if (hashLookup(hash, word))
		{
		fprintf(f, "%s\n", d);
		passMe = TRUE;
		}
	    }
	freeMem(d);
	}
    else if (passMe)
	{
	fprintf(f, "%s\n", line);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
faSomeRecords(argv[1], argv[2], argv[3]);
return 0;
}
