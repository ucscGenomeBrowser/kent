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

static char const rcsid[] = "$Id: ra.c,v 1.7 2003/09/21 15:33:13 kent Exp $";

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

