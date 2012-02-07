/* faSomeRecords - Extract multiple fa records. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "fa.h"
#include "options.h"


/* command line options */
static struct optionSpec optionSpecs[] =
{
    {"exclude", OPTION_BOOLEAN},
    {NULL, 0}
};
static boolean gExclude = FALSE;  // output sequences not in list file

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faSomeRecords - Extract multiple fa records\n"
  "usage:\n"
  "   faSomeRecords in.fa listFile out.fa\n"
  "options:\n"
  "   -exclude - output sequences not in the list file.\n"
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
	passMe = gExclude;
	line += 1;
	word = nextWord(&line);
	if (word != NULL)
	    {
	    if (hashLookup(hash, word))
		passMe = !gExclude;
            if (passMe)
                fprintf(f, "%s\n", d);
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
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage();
gExclude = optionExists("exclude");
faSomeRecords(argv[1], argv[2], argv[3]);
return 0;
}
