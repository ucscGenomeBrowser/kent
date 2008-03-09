/* segInfo - Get info about a segment file. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "options.h"
#include "seg.h"

static char const rcsid[] = "$Id: segInfo.c,v 1.4 2008/03/09 01:13:09 rico Exp $";

static struct optionSpec options[] = {
	{"merge", OPTION_BOOLEAN},
	{"noChrom", OPTION_BOOLEAN},
	{"ref", OPTION_STRING},
	{"species", OPTION_BOOLEAN},
	{NULL, 0},
};

static boolean merge    = FALSE;
static boolean noChrom  = FALSE;
static char    *ref     = NULL;
static boolean species  = FALSE;

static struct hash   *exists  = NULL;
static struct slName *keyList = NULL;

struct nameListList {
/* A list of struct slName. */
	struct nameListList *next;
	struct slName *nameList;
};

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
  "   -merge    Merge species lists into largest superset.\n"
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

static boolean sameSpeciesInLists(struct slName *list1, struct slName *list2)
/* Check to see if two name lists have the same names. */
{
struct slName *nl;

if (slCount(list1) != slCount(list2))
	return(FALSE);

for (nl = list1; nl != NULL; nl = nl->next)
	if (! slNameInList(list2, nl->name))
		return(FALSE);

return(TRUE);
}

static boolean inLists(struct slName *list, struct nameListList *lists)
/* Check to see if the give list is already in lists. */
{
struct nameListList *nll;

for (nll = lists; nll != NULL; nll = nll->next)
	if (sameSpeciesInLists(nll->nameList, list))
		return(TRUE);

return(FALSE);
}

static void addData(struct segBlock *sb, struct segComp *refComp, char *key)
/* Keep a list of unique keys. For each key, maintain a lists of uniq
   species seen with that key. */
{
struct segComp *sc;
struct slName *blockList = NULL;
struct hashEl *hel;
struct nameListList *data, *new;
char *p;

/* Genrate a list of species in this block other than the reference species. */
for (sc = sb->components; sc != NULL; sc = sc->next)
	{
	if (sc == refComp)
		continue;

	if ((p = strchr(sc->src, '.')) != NULL)
		*p = '\0';

	slNameStore(&blockList, sc->src);

	if (p != NULL)
		*p = '.';
	}

/* Get the list of species lists associated with this key. */
if ((hel = hashLookup(exists, key)) == NULL)
	{
	hel = hashAdd(exists, key, (void *) NULL);
	slNameAddHead(&keyList, key);
	}
data = (struct nameListList *) hel->val;

/* Add this list of species if it's not aready there. */
if (inLists(blockList, data))
	slNameFreeList(&blockList);
else
	{
	AllocVar(new);
	new->nameList = blockList;
	slAddHead(&data, new);
	}

hel->val = (void *) data;
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
struct hashEl *hel;
struct nameListList *data, *nll;
struct slName *key, *slist;

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

		data = (struct nameListList *) hel->val;
		slReverse(&data);

		for (nll = data; nll != NULL; nll = nll->next)
			{
			slReverse(&nll->nameList);
			for (slist = nll->nameList; slist != NULL; slist = slist->next)
				{
				printf("%s", slist->name);
				if (slist->next != NULL)
					printf(".");
				}
			if (nll->next != NULL)
				printf(",");
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

merge   = optionExists("merge");
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
