/* wgEncodeRegTfbsDedupeHelp - Help remove dupes from list of TFBS efforts. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "wgEncodeRegTfbsDedupeHelp - Help remove dupes from list of TFBS efforts\n"
  "usage:\n"
  "   wgEncodeRegTfbsDedupeHelp in.tab picks.tab out.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void wgEncodeRegTfbsDedupeHelp(char *inTab, char *picksTab, char *outTab)
/* wgEncodeRegTfbsDedupeHelp - Help remove dupes from list of TFBS efforts. */
{
char key[256];
struct hash *hash = hashNew(0);

/* Build hash of picks. */
    {
    struct lineFile *lf = lineFileOpen(picksTab, TRUE);
    char *row[3];
    while (lineFileRow(lf, row))
        {
	char *factor = row[0];
	char *source = row[1];
	char *id = row[2];
	safef(key, sizeof(key), "%s & %s", factor, source);
	hashAdd(hash, key, cloneString(id));
	}
    }

struct lineFile *lf = lineFileOpen(inTab, TRUE);
FILE *f = mustOpen(outTab, "w");
char *line;
while (lineFileNextReal(lf, &line))
    {
    char *dupe = cloneString(line);
    char *row[11];
    chopLine(dupe, row);
    char *source = row[7];
    char *factor = row[8];
    safef(key, sizeof(key), "%s & %s", factor, source);
    char *pick = hashFindVal(hash, key);
    if (pick == NULL || stringIn(pick, line))
        fprintf(f, "%s\n", line);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
wgEncodeRegTfbsDedupeHelp(argv[1], argv[2], argv[3]);
return 0;
}
