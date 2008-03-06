/* segInfo - Get info about a segment file. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "options.h"
#include "seg.h"

static char const rcsid[] = "$Id: segInfo.c,v 1.1 2008/03/06 17:10:55 rico Exp $";

static struct optionSpec options[] = {
	{"noChrom", OPTION_BOOLEAN},
	{"ref", OPTION_STRING},
	{"species", OPTION_BOOLEAN},
	{NULL, 0},
};

static boolean noChrom  = FALSE;
static char    *ref     = NULL;
static boolean species  = FALSE;

static struct hash   *exists  = NULL;
static struct slName *keyList = NULL;


void usage()
/* Explain usage and exit. */
{
errAbort(
  "segInfo - Get info from segment file(s)\n"
  "usage:\n"
  "   segInfo in.seg ...\n"
  "options:\n"
  "   -ref=<S>  Use <S> as the reference species.  For each ref.chrom,\n"
  "             output a unique list of species seen with that ref.chrom.\n"
  "             (default = first species seen in the segment files(s))\n"
  "   -species  Output unique list of species seen in the segment file(s)\n"
  "   -noChrom  Output a unique list of species seen with ref.\n"
  );
}

static void setRef(struct segBlock *sb)
/* Set ref to the species of the first component. */
{
char *p;

if (sb->components == NULL)
	errAbort("ref is not set and the first segment block has no components.");

/* Temporarily split src into species and chrom. */
if ((p = strchr(sb->components->src, '.')) != NULL)
	*p = '\0';

ref = cloneString(sb->components->src);

/* Restore src. */
if (p != NULL)
	*p = '.';
}

static void addSpeciesData(struct segBlock *sb)
/* Keep a list of uniqe species we've seen. */
{
struct segComp *sc;
struct hashEl *hel;
char *p;

for (sc = sb->components; sc != NULL; sc = sc->next)
	{
	if ((p = strchr(sc->src, '.')) != NULL)
		*p = '\0';

	/* If we haven't seen this species yet, add it to the list. */
	if ((hel = hashLookup(exists, sc->src)) == NULL)
		{
		hashAdd(exists, sc->src, (void *) NULL);
		slNameAddHead(&keyList, sc->src);
		}

	if (p != NULL)
		*p = '.';
	}
}

static void addData(struct segBlock *sb, struct segComp *refComp, char *key)
/* Keep a list of unique keys. For each key, maintain a list of uniq
   species seen with that key. */
{
struct segComp *sc;
struct hashEl *hel;
struct slName *oldList;
char *p;

/* Get the list of species currently associated with this key. */
if ((hel = hashLookup(exists, key)) == NULL)
	{
	hel = hashAdd(exists, key, (void *) NULL);
	slNameAddHead(&keyList, key);
	}
oldList = (struct slName *) hel->val;

/* Add species to this list for this key if we haven't seen them yet. */
for (sc = sb->components; sc != NULL; sc = sc->next)
	{
	if (sc == refComp)
		continue;

	if ((p = strchr(sc->src, '.')) != NULL)
		*p = '\0';

	slNameStore(&oldList, sc->src);

	if (p != NULL)
		*p = '.';
	}

/* Store the updated list for this key. */
hel->val = (void *) oldList;
}

void segInfo(char *inSeg)
/* segInfo - Get info about a segment file. */
{
struct segFile *sf = segOpen(inSeg);
struct segBlock *sb;
struct segComp *refComp;

while ((sb = segNext(sf)) != NULL)
	{
	if (species)
		addSpeciesData(sb);
	else
		{
		if (ref == NULL)
			setRef(sb);

		refComp = segFindCompSpecies(sb, ref, '.');

		if (noChrom)
			addData(sb, refComp, ref);
		else
			addData(sb, refComp, refComp->src);
		}
	segBlockFree(&sb);
	}
segFileFree(&sf);
}

static void outputData(void)
/* Print out the data we collected. */
{
struct slName *key, *list, *species = NULL;
struct hashEl *hel;

for (key = keyList; key != NULL; key = key->next)
	{
	printf("%s", key->name);

	if (species)
		printf("\n");
	else
		{
		printf(" ");

		if ((hel = hashLookup(exists, key->name)) == NULL)
			errAbort("Can't find %s in exists hash!", key->name);

		list = (struct slName *) hel->val;
		slReverse(&list);

		for (species = list; species != NULL; species = species->next)
			{
			printf("%s ", species->name);
			}
		printf("\n");
		}
	}
}

int main(int argc, char *argv[])
/* Process command line. */
{
int i;

optionInit(&argc, argv, options);

if (argc < 2)
    usage();

noChrom = optionExists("noChrom");
ref     = optionVal("ref", ref);
species = optionExists("species");

exists = newHash(5);
for (i = 1; i < argc; i++)
	segInfo(argv[i]);

slReverse(&keyList);
outputData();

hashFreeWithVals(&exists, slNameFreeList);

return(EXIT_SUCCESS);
}
