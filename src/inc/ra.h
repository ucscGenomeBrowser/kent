/* Stuff to parse .ra files. Ra files are simple text databases.
 * The database is broken into records by blank lines. 
 * Each field takes a line.  The name of the field is the first
 * word in the line.  The value of the field is the rest of the line. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef RA_H

struct hash *raNextRecord(struct lineFile *lf);
/* Return a hash containing next record.   
 * Returns NULL at end of file.  freeHash this
 * when done.  Note this will free the hash
 * keys and values as well, so you'll have to
 * cloneMem them if you want them for later. */

struct hash *raFromString(char *string);
/* Return hash of key/value pairs from string.
 * As above freeHash this when done. */

boolean raFoldInOne(struct lineFile *lf, struct hash *hashOfHash);
/* Fold in one record from ra file into hashOfHash. 
 * This will add ra's and ra fields to whatever already
 * exists in the hashOfHash,  overriding fields of the
 * same name if they exist already. */

void raFoldIn(char *fileName, struct hash *hashOfHash);
/* Read ra's in file name and fold them into hashOfHash. 
 * This will add ra's and ra fields to whatever already
 * exists in the hashOfHash,  overriding fields of the
 * same name if they exist already. */

struct hash *raReadSingle(char *fileName);
/* Read in first ra record in file and return as hash. */

struct hash *raReadAll(char *fileName, char *keyField);
/* Return hash that contains all ra records in file keyed
 * by given field, which must exist.  The values of the
 * hash are themselves hashes. */

#endif /* RA_H */

