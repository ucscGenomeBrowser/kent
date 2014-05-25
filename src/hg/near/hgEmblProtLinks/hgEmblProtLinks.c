/* hgEmblProtLinks - Parse EMBL flat file into protein link table. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "emblParse.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgEmblProtLinks - Parse EMBL flat file into protein link table\n"
  "usage:\n"
  "   hgEmblProtLinks in.embl out.tab\n"
  "Note, this is just an experiment at the moment.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

char *org = "Caenorhabditis elegans";

static struct optionSpec options[] = {
   {NULL, 0},
};

boolean emblLineGroup(struct lineFile *lf, char type[16], struct dyString *val);
/* Read next line of embl file.  Read line after that too if it
 * starts with the same type field. Return FALSE at EOF. */

char *findOrEmpty(struct hash *hash, char *name)
/* Return value if it exists in hash or "". */
{
char *s = hashFindVal(hash, name);
if (s == NULL)
    s = "";
return s;
}

void hgEmblProtLinks(char *inName, char *outName)
/* hgEmblProtLinks - Parse EMBL flat file into protein link table. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct hash *hash;

while ((hash = emblRecord(lf)) != NULL)
    {
    char *os = hashFindVal(hash, "OS");
    if (os != NULL && startsWith(org, os))
        {
	char *gn = findOrEmpty(hash, "GN");
	char *de = findOrEmpty(hash, "DE");
	char *ac = findOrEmpty(hash, "AC");
	fprintf(f, "%s\t%s\t%s\n", gn, de, ac);
	}
    freeHashAndVals(&hash);
    }
lineFileClose(&lf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hgEmblProtLinks(argv[1], argv[2]);
return 0;
}
