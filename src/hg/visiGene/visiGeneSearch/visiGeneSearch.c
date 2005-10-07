/* visiGeneSearch - Test out search routines for VisiGene. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"
#include "jksql.h"
#include "visiGene.h"

static char const rcsid[] = "$Id: visiGeneSearch.c,v 1.1 2005/10/03 20:13:02 kent Exp $";

char *database = "visiGene";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "visiGeneSearch - Test out search routines for VisiGene\n"
  "usage:\n"
  "   visiGeneSearch searchTerm\n"
  "options:\n"
  "   -db=XXX set another database (default visiGene)\n"
  );
}

static struct optionSpec options[] = {
   {"db", OPTION_STRING},
   {NULL, 0},
};

char *stripSpacesEtc(char *s)
/* Return a copy of s with spaces, periods, and dashes removed */
{
char *d = cloneString(s);
stripChar(d, ' ');
stripChar(d, '.');
stripChar(d, '-');
touppers(d);
return d;
}

int countWordsUsedInName(char *name, struct slName *wordList)
/* The name here is last name followed by initials with a 
 * period after each initial and no space between initials.
 * The last name may contain multiple words.  Some complex
 * examples are "van den Eijnden-van Raaij A.J." and
 * "Van De Water T."   A typical example "Kwong W.H."
 * The word list is just space separated words.  We'd
 * Want the last example to be matched by "Kwong" "W.H."
 * as well as "Kwong WH" as well as "Kwong W H". */
{
int count = 0;
struct slName *word;
char *stripped = stripSpacesEtc(name);
char *pt = stripped;

for (word = wordList; word != NULL; word = word->next)
    {
    char *w = stripSpacesEtc(word->name);
    if (startsWith(w, pt))
        {
	count += 1;
	pt += strlen(word->name);
	freeMem(w);
	}
    else
        {
	freeMem(w);
	break;
	}
    }
freeMem(stripped);
return count;
}

struct slInt *visiGeneSelectContributor(struct sqlConnection *conn, 
        char *contributors)
/* Return ids of images that were contributed by all people in list. 
 * We want the behavior to be such that given a list of last names,
 * say "Smith Mahoney" it will return only those that match both 
 * names.  We also want it so that if you include the initials
 * after the last name either with or without periods that will
 * just return those matching the last name and initials.  For
 * instance "Smith JJ" or "Smith J.J." or "Smith J. J." all would
 * match a particular John Jacob Smith, but not Francis K. Smith.
 * Making this a little more interesting is a case like
 * "smith li" which could either be two last names, or a last
 * name followed by initials.  We would want to match both
 * cases.  Finally, making it even more interesting, is the
 * case where the last name is compound, like "Van Koppen" or
 * "de la Cruz" and the like.  Also don't forget the apostrophe
 * containing names like O'Shea. */
{
struct slName *wordList = stringToSlNames(contributors);
struct slName *word;
struct slInt *idList = NULL;
struct hash *hash = hashNew(0);
struct dyString *query = dyStringNew(0);
struct sqlResult *sr;
char **row;

for (word = wordList; word != NULL;  )
    {
    struct slName *lastNameList, *lastName;
    struct slName *matchList = NULL, *match;
    int maxWordsUsed = 0;
    dyStringClear(query);
    dyStringAppend(query, "select name from contributor where name like \"");
    dyStringAppend(query, word->name);
    dyStringAppend(query, " %\"");
    uglyf("Searching starting at %s\n", word->name);
    lastNameList = sqlQuickList(conn, query->string);
    if (lastNameList == NULL)
        {
	slFreeList(&matchList);
	break;
	}
    for (lastName = lastNameList; lastName != NULL; lastName = lastName->next)
        {
	int wordsUsed = countWordsUsedInName(lastName->name, word);
	if (wordsUsed > maxWordsUsed)
	    maxWordsUsed = wordsUsed;
	}
    for (lastName = lastNameList; lastName != NULL; lastName = lastName->next)
        {
	if (countWordsUsedInName(lastName->name, word) == maxWordsUsed)
	    {
	    match = slNameNew(lastName->name);
	    slAddHead(&matchList, match);
	    uglyf("%d %s\n", maxWordsUsed, lastName->name);
	    }
	}
    while (--maxWordsUsed >= 0)
        word = word->next;
    slFreeList(&lastNameList);
    }
   
dyStringFree(&query);
hashFree(&hash);
slFreeList(&wordList);
return idList;
}

void visiGeneSearch(char *searchString)
/* visiGeneSearch - Test out search routines for VisiGene. */
{
struct sqlConnection *conn = sqlConnect(database);
struct slInt *matchIdList;
printf("Searching %s for \"%s\"\n", database, searchString);
matchIdList = visiGeneSelectContributor(conn, searchString);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
database = optionVal("db", database);
if (argc != 2)
    usage();
visiGeneSearch(argv[1]);
return 0;
}
