/* raToTab - Convert ra file to table.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "ra.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "raToTab - Convert ra file to table.\n"
  "usage:\n"
  "   raToTab in.ra out.tab\n"
  "options:\n"
  "   -cols=a,b,c - List columns in order to output in table\n"
  "                 Only these columns will be output.  If you\n"
  "                 Don't give this option, all columns are output\n"
  "                 in alphabetical order\n"
  "   -head - Put column names in header\n"
  );
}

struct slName *colList;

static struct optionSpec options[] = {
   {"cols", OPTION_STRING},
   {"head", OPTION_BOOLEAN},
   {NULL, 0},
};

struct hash *raReadList(char *fileName)
/* Read file full or ra's and return as a list of hashes */
{
struct hash *ra, *raList = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
while ((ra = raNextRecord(lf)) != NULL)
    {
    slAddHead(&raList, ra);
    }
lineFileClose(&lf);
slReverse(&raList);
return raList;
}

struct slName *hashListAllKeys(struct hash *hashList)
/* Return list of all keys in all hashes in list. */
{
struct hash *uniqHash = hashNew(18);
struct hash *hash;
for (hash = hashList; hash != NULL; hash = hash->next)
    {
    struct hashCookie cookie = hashFirst(hash);
    struct hashEl *el;
    while ((el = hashNext(&cookie)) != NULL)
        hashStore(uniqHash, el->name);
    }
struct slName *list = NULL;
struct hashCookie cookie = hashFirst(uniqHash);
struct hashEl *el;
while ((el = hashNext(&cookie)) != NULL)
    slNameAddHead(&list, el->name);
hashFree(&uniqHash);
slSort(&list, slNameCmp);
return list;
}

void raToTab(char *inRa, char *outTab)
/* raToTab - Convert ra file to table.. */
{
struct hash *ra, *raList = raReadList(inRa);
FILE *f = mustOpen(outTab, "w");
struct slName *col;
if (colList == NULL)
    colList = hashListAllKeys(raList);
if (optionExists("head"))
    {
    if (colList == NULL)
        fprintf(f, "#\n");
    else
	{
	fprintf(f, "#%s", colList->name);
	for (col = colList->next; col != NULL; col = col->next)
	    fprintf(f, "\t%s", col->name);
	fprintf(f, "\n");
	}
    }
for (ra = raList; ra != NULL; ra = ra->next)
    {
    if (colList == NULL)
        fprintf(f, "\n");
    else
        {
	fprintf(f, "%s", emptyForNull(hashFindVal(ra, colList->name)));
	for (col = colList->next; col != NULL; col = col->next)
	    fprintf(f, "\t%s", emptyForNull(hashFindVal(ra, col->name)));
        fprintf(f, "\n");
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
char *colText = optionVal("cols", NULL);
if (colText != NULL)
     colList = commaSepToSlNames(colText);
raToTab(argv[1], argv[2]);
return 0;
}
