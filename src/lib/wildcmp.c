/* Wildcard matching. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "hash.h"


static int subMatch(const char *str, const char *wild, char single, char multi)
/* Returns number of characters that match between str and wild up
 * to the next wildcard in wild (or up to end of string.). */
{
int len = 0;

for(;;)
    {
    if(toupper(*str++) != toupper(*wild++) )
        return(0);
    ++len;
    char c = *wild;
    if (c == 0 || c == single || c == multi)
       return len;
    }
}

boolean anyWild(const char *string)
/* Return TRUE if any wild card characters in string. */
{
char c;
while ((c = *string++) != 0)
    {
    if (c == '?' || c == '*')
        return TRUE;
    }
return FALSE;
}

static boolean globMatch(const char *wildCard, const char *string, char single, char multi)
/* does a case sensitive wild card match with a string.
 * * matches any string or no character.
 * ? matches any single character.
 * anything else etc must match the character exactly. */
{
boolean matchStar = 0;
int starMatchSize;
char c;

for(;;)
    {
NEXT_WILD:
    c = *wildCard;
    if (c == 0)
	{
	if(matchStar)
	    {
	    while(*string++)
		;
	    return TRUE;
	    }
	else if(*string)
	    return FALSE;
	else
	    return TRUE;
	}
    else if (c == multi)
	{
	matchStar = TRUE;
	}
    else if (c == single)
	{
	if(*string == 0)
	    return FALSE; /* out of string, no match for ? */
	++string;
	}
    else
	{
	if(matchStar)
	    {
	    for(;;)
		{
		if(*string == 0) /* if out of string no match */
		    return FALSE;

		/* note matchStar is re-used here for substring
		 * after star match length */
		if((starMatchSize = subMatch(string,wildCard,single,multi)) != 0)
		    {
		    string += starMatchSize;
		    wildCard += starMatchSize;
		    matchStar = FALSE;
		    goto NEXT_WILD;
		    }
		++string;
		}
	    }

	/* default: they must be equal or no match */
	if(toupper(*string) != toupper(*wildCard))
	    return FALSE;
	++string;
	}
    ++wildCard;
    }
}

boolean wildMatch(const char *wildCard, const char *string)
/* Match using * and ? wildcards. */
{
return globMatch(wildCard, string, '?', '*');
}

boolean sqlMatchLike(char *wildCard, char *string)
/* Match using % and _ wildcards. */
{
return globMatch(wildCard, string, '_', '%');
}

static int addMatching(char *pattern, struct slName *itemList, struct slName **retMatchList)
/* Add all items that match pattern to retMatchList */
{
int count = 0;
struct slName *item;
for (item = itemList; item != NULL; item = item->next)
    {
    char *name = item->name;
    if (wildMatch(pattern, name))
         {
	 slNameAddHead(retMatchList, name);
	 ++count;
	 }
    }
return count;
}

struct slName *wildExpandList(struct slName *allList, struct slName *wildList, boolean abortMissing)
/* Wild list is a list of names, possibly including * and ? wildcard characters.  This 
 * function returns names taken from allList that match patterns in wildList.  Works much
 * like wildcard expansion over a file system but expands over allList instead. */
{
/* Build hash of allList */
struct hash *allHash = hashNew(0);
struct slName *name;
for (name = allList; name != NULL; name = name->next)
    hashAdd(allHash, name->name, NULL);

/* Expand any field names with wildcards. */
struct slName *expandedList = NULL;
struct slName *field;
for (field = wildList; field != NULL; field = field->next)
    {
    char *name = field->name;
    if (anyWild(name))
        {
	int addCount = addMatching(name, allList, &expandedList);
	if (addCount == 0 && abortMissing)
	    errAbort("No match for %s", name);
	}
    else
        {
	if (abortMissing && !hashLookup(allHash, name))
	    errAbort("No match for %s", name);
	slNameAddHead(&expandedList, name);
	}
    }
hashFree(&allHash);
slReverse(&expandedList);
return expandedList;
}

