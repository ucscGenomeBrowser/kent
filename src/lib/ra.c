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
#include "ra.h"

static char const rcsid[] = "$Id: ra.c,v 1.10 2006/06/28 20:25:55 kent Exp $";

struct hash *raNextRecord(struct lineFile *lf)
/* Return a hash containing next record.   
 * Returns NULL at end of file.  freeHash this
 * when done.  Note this will free the hash
 * keys and values as well, so you'll have to
 * cloneMem them if you want them for later. */
{
struct hash *hash = NULL;
char *line, *key, *val;

/* Skip leading empty lines. */
for (;;)
   {
   if (!lineFileNext(lf, &line, NULL))
       return NULL;
   line = skipLeadingSpaces(line);
   if (line[0] != 0)
       break;
   }
lineFileReuse(lf);
for (;;)
   {
   if (!lineFileNext(lf, &line, NULL))
       break;
   line = skipLeadingSpaces(line);
   if (line[0] == 0)
       break;
   if (line[0] == '#')
       {
       if (startsWith("#EOF", line))
           return NULL;
       else
	   continue;
       }
   if (hash == NULL)
       hash = newHash(7);
   key = nextWord(&line);
   val = skipLeadingSpaces(line);
   if (line == NULL)
       line = "";
   val = lmCloneString(hash->lm, val);
   hashAdd(hash, key, val);
   }
return hash;
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

boolean raFoldInOne(struct lineFile *lf, struct hash *hashOfHash)
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
    return FALSE;
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
return TRUE;
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
    while (raFoldInOne(lf, hashOfHash))
        ;
    lineFileClose(&lf);
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
