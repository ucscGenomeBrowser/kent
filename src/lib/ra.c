/* Stuff to parse .ra files. Ra files are simple text databases.
 * The database is broken into records by blank lines.
 * Each field takes a line.  The name of the field is the first
 * word in the line.  The value of the field is the rest of the line.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "dystring.h"
#include "ra.h"

static char const rcsid[] = "$Id: ra.c,v 1.17 2009/12/08 20:42:50 kent Exp $";

boolean raSkipLeadingEmptyLines(struct lineFile *lf, struct dyString *dy)
/* Skip leading empty lines and comments.  Returns FALSE at end of file.
 * Together with raNextTagVal you can construct your own raNextRecord....
 * If dy parameter is non-null, then the text parsed gets placed into dy. */
{
char *line;
/* Skip leading empty lines and comments. */
if (dy)
    dyStringClear(dy);
for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
       return FALSE;
    char *tag = skipLeadingSpaces(line);
    if (tag[0] == 0 || tag[0] == '#')
       {
       if (dy)
	   {
	   dyStringAppend(dy, line);
	   dyStringAppendC(dy, '\n');
	   }
	}
    else
       break;
    }
lineFileReuse(lf);
return TRUE;
}

boolean raNextTagVal(struct lineFile *lf, char **retTag, char **retVal, struct dyString *dy)
/* Read next line.  Return FALSE at end of file or blank line.  Otherwise
 * fill in *retTag and *retVal and return TRUE.
 * If dy parameter is non-null, then the text parsed gets appended to dy. */
{
char *line;
for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
       return FALSE;
    char *tag = skipLeadingSpaces(line);
    if (tag[0] == 0)
       {
       if (dy)
	   lineFileReuse(lf);	/* Just so don't lose leading space in dy. */
       return FALSE;
       }
    if (dy)
       {
       dyStringAppend(dy, line);
       dyStringAppendC(dy, '\n');
       }
    if (tag[0] == '#')
       {
       if (startsWith("#EOF", tag))
	   return FALSE;
       else
	   {
	   continue;
	   }
       }
    break;
    }
*retTag = nextWord(&line);
*retVal = trimSpaces(line);
return TRUE;
}

struct hash *raNextRecord(struct lineFile *lf)
/* Return a hash containing next record.
 * Returns NULL at end of file.  freeHash this
 * when done.  Note this will free the hash
 * keys and values as well, so you'll have to
 * cloneMem them if you want them for later. */
{
struct hash *hash = NULL;
char *key, *val;

if (!raSkipLeadingEmptyLines(lf, NULL))
    return NULL;
while (raNextTagVal(lf, &key, &val, NULL))
    {
    if (hash == NULL)
	hash = newHash(7);
    val = lmCloneString(hash->lm, val);
    hashAdd(hash, key, val);
    }
return hash;
}

struct slPair *raNextRecordAsSlPairList(struct lineFile *lf)
/* Return ra record as a slPair list instead of a hash.  Handy if you want to preserve the order.
 * Do a slPairFreeValsAndList on result when done. */
{
struct slPair *list = NULL;
char *key, *val;
if (!raSkipLeadingEmptyLines(lf, NULL))
    return NULL;
while (raNextTagVal(lf, &key, &val, NULL))
    slPairAdd(&list, key, cloneString(val));
slReverse(&list);
return list;
}

struct hash *raFromString(char *string)
/* Return hash of key/value pairs from string.
 * As above freeHash this when done. */
{
char *dupe = cloneString(string);
char *s = dupe, *lineEnd;
struct hash *hash = newHash(7);
char *key, *val;

for (;;)
    {
    s = skipLeadingSpaces(s);
    if (s == NULL || s[0] == 0)
        break;
    lineEnd = strchr(s, '\n');
    if (lineEnd != NULL)
        *lineEnd++ = 0;
    key = nextWord(&s);
    val = skipLeadingSpaces(s);
    s = lineEnd;
    val = lmCloneString(hash->lm, val);
    hashAdd(hash, key, val);
    }
freeMem(dupe);
return hash;
}

char *raFoldInOneRetName(struct lineFile *lf, struct hash *hashOfHash)
/* Fold in one record from ra file into hashOfHash.
 * This will add ra's and ra fields to whatever already
 * exists in the hashOfHash,  overriding fields of the
 * same name if they exist already. */
{
char *word, *line, *name;
struct hash *ra;
struct hashEl *hel;

/* Get first nonempty non-comment line and make sure
 * it contains name. */
if (!lineFileNextReal(lf, &line))
    return NULL;
word = nextWord(&line);
if (!sameString(word, "name"))
    errAbort("Expecting 'name' line %d of %s, got %s",
    	lf->lineIx, lf->fileName, word);
name = nextWord(&line);
if (name == NULL)
    errAbort("Short name field line %d of %s", lf->lineIx, lf->fileName);

/* Find ra hash associated with name, making up a new
 * one if need be. */
if ((ra = hashFindVal(hashOfHash, name)) == NULL)
    {
    ra = newHash(7);
    hashAdd(hashOfHash, name, ra);
    hashAdd(ra, "name", lmCloneString(ra->lm, name));
    }

/* Fill in fields of ra hash with data up to next
 * blank line or end of file. */
for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
        break;
    line = skipLeadingSpaces(line);
    if (line[0] == 0)
        break;
    if (line[0] == '#')
        continue;
    word = nextWord(&line);
    line = skipLeadingSpaces(line);
    if (line == NULL)
        line = "";
    hel = hashLookup(ra, word);
    if (hel == NULL)
        hel = hashAdd(ra, word, lmCloneString(ra->lm, line));
    else
        hel->val = lmCloneString(ra->lm, line);
    }
return hashFindVal(ra, "name");
}

boolean raFoldInOne(struct lineFile *lf, struct hash *hashOfHash)
{
return raFoldInOneRetName(lf, hashOfHash) != NULL;
}

void raFoldIn(char *fileName, struct hash *hashOfHash)
/* Read ra's in file name and fold them into hashOfHash.
 * This will add ra's and ra fields to whatever already
 * exists in the hashOfHash,  overriding fields of the
 * same name if they exist already. */
{
struct lineFile *lf = lineFileMayOpen(fileName, TRUE);
if (lf != NULL)
    {
    struct hash *uniqHash = hashNew(0);
    char *name;
    while ((name = raFoldInOneRetName(lf, hashOfHash)) != NULL)
	{
	if (hashLookup(uniqHash, name))
	    errAbort("%s duplicated in record ending line %d of %s", name,
	    	lf->lineIx, lf->fileName);
	hashAdd(uniqHash, name, NULL);
	}
    lineFileClose(&lf);
    hashFree(&uniqHash);
    }
}

struct hash *raReadSingle(char *fileName)
/* Read in first ra record in file and return as hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = raNextRecord(lf);
lineFileClose(&lf);
return hash;
}

struct hash *raReadAll(char *fileName, char *keyField)
/* Return hash that contains all ra records in file keyed
 * by given field, which must exist.  The values of the
 * hash are themselves hashes. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *bigHash = hashNew(0);
struct hash *hash;
while ((hash = raNextRecord(lf)) != NULL)
    {
    char *key = hashFindVal(hash, keyField);
    if (key == NULL)
        errAbort("Couldn't find key field %s line %d of %s",
		keyField, lf->lineIx, lf->fileName);
    hashAdd(bigHash, key, hash);
    }
lineFileClose(&lf);
return bigHash;
}

struct hash *raReadWithFilter(char *fileName, char *keyField,char *filterKey,char *filterValue)
/* Return hash that contains all filtered ra records in file keyed by given field, which must exist.
 * The values of the hash are themselves hashes.  The filter is a key/value pair that must exist.
 * Example raReadWithFilter(file,"term","type","antibody"): returns hash of hashes of every term with type=antibody */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *bigHash = hashNew(0);
struct hash *hash;
while ((hash = raNextRecord(lf)) != NULL)
    {
    char *key = hashFindVal(hash, keyField);
    if (key == NULL)
        errAbort("Couldn't find key field %s line %d of %s",
                keyField, lf->lineIx, lf->fileName);
    if (filterKey != NULL)
        {
        char *filter = hashFindVal(hash, filterKey);
        if (filter == NULL)
            {
            hashFree(&hash);
            continue;
            }
        if (filterValue != NULL && differentString(filterValue,filter))
            {
            hashFree(&hash);
            continue;
            }
        }
        hashAdd(bigHash, key, hash);
    }
lineFileClose(&lf);
if (hashNumEntries(bigHash) == 0)
    hashFree(&bigHash);
return bigHash;
}

