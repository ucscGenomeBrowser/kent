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

struct hash *raNextRecord(struct lineFile *lf)
/* Return a hash containing next record.   
 * Returns NULL at end of file.  freeHash this
 * when done.  Note this will free the hash
 * keys and values as well, so you'll have to
 * cloneMem them if you want them for later. */
{
struct hash *hash = NULL;
char *line, *key, *val;

for (;;)
   {
   if (!lineFileNext(lf, &line, NULL))
       break;
   line = skipLeadingSpaces(line);
   if (line[0] == 0)
       break;
   if (hash == NULL)
       hash = newHash(7);
   key = nextWord(&line);
   val = skipLeadingSpaces(line);
   if (val == NULL)
       errAbort("Expecting line/value pair line %d of %s", lf->lineIx, lf->fileName);
   val = lmCloneString(hash->lm, val);
   hashAdd(hash, key, val);
   }
return hash;
}
