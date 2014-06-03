/* rename - Rename things to avoid symbol conflicts. */

/* Copyright (C) 2005 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef RENAME_H
#define RENAME_H

void renameAddSqlWords(struct hash *hash);
/* Add sql reserved words to hash */

void renameAddCWords(struct hash *hash);
/* Add C reserved words to hash */

char *renameUnique(struct hash *hash, char *name);
/* Return a copy of name.  If name is already in
 * hash try abbreviating it to three letters or
 * adding some number to make it unique.  Note this
 * does not add name to table - it figures you may
 * want to associate some value with name in hash. */

#endif /* RENAME_H */

