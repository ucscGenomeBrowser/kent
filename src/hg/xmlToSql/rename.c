/* Rename - Rename things to avoid symbol conflicts with C and
 * SQL. */
/* This file is copyright 2005 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "hash.h"
#include "dtdParse.h"
#include "rename.h"

void renameAddSqlWords(struct hash *hash)
/* Add sql reserved words to hash */
{
hashAdd(hash, "create", NULL);
hashAdd(hash, "update", NULL);
hashAdd(hash, "database", NULL);
hashAdd(hash, "select", NULL);
hashAdd(hash, "from", NULL);
hashAdd(hash, "desc", NULL);
hashAdd(hash, "where", NULL);
hashAdd(hash, "table", NULL);
hashAdd(hash, "by", NULL);
hashAdd(hash, "order", NULL);
hashAdd(hash, "key", NULL);
hashAdd(hash, "case", NULL);
}

void renameAddCWords(struct hash *hash)
/* Add C reserved words to hash */
{
hashAdd(hash, "struct", NULL);
hashAdd(hash, "char", NULL);
hashAdd(hash, "short", NULL);
hashAdd(hash, "int", NULL);
hashAdd(hash, "long", NULL);
hashAdd(hash, "float", NULL);
hashAdd(hash, "double", NULL);
hashAdd(hash, "while", NULL);
hashAdd(hash, "for", NULL);
hashAdd(hash, "create", NULL);
hashAdd(hash, "if", NULL);
hashAdd(hash, "do", NULL);
hashAdd(hash, "switch", NULL);
hashAdd(hash, "break", NULL);
hashAdd(hash, "continue", NULL);
hashAdd(hash, "register", NULL);
hashAdd(hash, "static", NULL);
hashAdd(hash, "auto", NULL);
hashAdd(hash, "typedef", NULL);
hashAdd(hash, "enum", NULL);
hashAdd(hash, "case", NULL);
}

char *renameUnique(struct hash *hash, char *name)
/* Return a copy of name.  If name is already in
 * hash try abbreviating it to three letters or
 * adding some number to make it unique.  Note this
 * does not add name to table - it figures you may
 * want to associate some value with name in hash. */
{
char *newName = NULL;
if (!hashLookup(hash, name))
    newName = cloneString(name);

/* Try to abbreviate by taking first 3 letters. */
if (newName == NULL && strlen(name) > 3)
    {
    newName = cloneStringZ(name, 3);
    if (hashLookup(hash, newName))
        freez(&newName);
    }

/* Add numerical suffixes until hit paydirt. */
if (newName == NULL)
    {
    char buf[256];
    int i;
    for (i=2; ; ++i)
        {
	safef(buf, sizeof(buf), "%s%d", name, i);
	if (!hashLookup(hash, buf))
	    {
	    newName = cloneString(buf);
	    break;
	    }
	}
    }
if (!sameString(newName, name)) verbose(3, "renamed %s to %s\n", name, newName);
return newName;
}


