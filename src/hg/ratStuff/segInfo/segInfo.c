/* segInfo - Get info about a segment file. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "options.h"
#include "seg.h"

static char const rcsid[] = "$Id: segInfo.c,v 1.6 2008/03/11 17:03:49 rico Exp $";

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

static boolean listsEquivalent(struct slName *list1, struct slName *list2)
/* Check to see if two name lists re equivalent (same size and names). */
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
/* Check to see if the given list is already in lists. */
{
struct nameListList *nll;

for (nll = lists; nll != NULL; nll = nll->next)
	if (listsEquivalent(nll->nameList, list))
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
			if ((ref = segFirstCompSpecies(sb, '.')) == NULL)
				errAbort("ref is not set and the first segment block has no "
					"components.");

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

static int nameSubset(struct slName *a, struct slName *b)
/* Return 1 if the set of names in 'b' is a subset of the set of names in 'a'.
   Return 2 if the set of names in 'a' is a subset of the set of names in 'b'.
   Return 0 otherwise. */
{
int aSize = slCount(a), bSize = slCount(b);
struct slName *small = a, *big = b, *nl;

if (bSize < aSize)
	{
	small = b;
	big   = a;
	}

/* Every name in the small list MUST be in the big list. */
for (nl = small; nl != NULL; nl = nl->next)
	if (! slNameInList(big, nl->name))
		return(0);

if (big == a)
	return(1);
else
	return(2);
}

static void mergeLists(void)
/* Merge lists for each key which have a subset/superset relationship. */
{
struct slName *key, *temp;
struct hashEl *hel;
struct nameListList *data, *master, *prev, *curr;
int sub;

for (key = keyList; key != NULL; key = key->next)
	{
	if ((hel = hashLookup(exists, key->name)) == NULL)
		errAbort("Can't find %s in exists hash!", key->name);

	master = data = (struct nameListList *) hel->val;

	while (master->next != NULL)
		{
		prev = master;
		curr = master->next;

		while (curr != NULL)
			{
			/* Check for a subset relationship between master and curr. */
			if ((sub = nameSubset(master->nameList, curr->nameList)) != 0)
				{
				/* Swap nameLists so that curr contains the subset. */
				if (sub == 2)
					{
					temp = master->nameList;
					master->nameList = curr->nameList;
					curr->nameList = temp;
					}

				/* Delete curr (the subset). */
				prev->next = curr->next;
				slNameFreeList(&curr->nameList);
				freeMem(curr);

				/* Start over. */
				prev = master;
				curr = master->next;
				}
			else
				{
				prev = curr;
				curr = curr->next;
				}
			}

		if (master->next != NULL)
			master = master->next;
		}
	}
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

if (merge & !species)
	mergeLists();

outputData();

hashFreeWithVals(&exists, slNameFreeList);

return(EXIT_SUCCESS);
}
