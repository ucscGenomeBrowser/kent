/* Stuff to parse .ra files. Ra files are simple text databases.
 * The database is broken into records by blank lines.
 * Each field takes a line.  The name of the field is the first
 * word in the line.  The value of the field is the rest of the line.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef RA_H

struct hash *raNextStanza(struct lineFile *lf);
// Return a hash containing next record.
// Will ignore '#' comments and joins continued lines (ending in '\').
// Returns NULL at end of file.  freeHash this when done.
// Note this will free the hash keys and values as well,
// so you'll have to cloneMem them if you want them for later.
#define raNextRecord(lf)  raNextStanza(lf)

struct slPair *raNextStanzAsPairs(struct lineFile *lf);
// Return ra stanza as an slPair list instead of a hash.  Handy to preserve the
// order.  Will ignore '#' comments and joins continued lines (ending in '\').
#define raNextRecordAsSlPairList(lf)  raNextStanzAsPairs(lf)

struct slPair *raNextStanzaLinesAndUntouched(struct lineFile *lf);
// Return list of lines starting from current position, up through last line of next stanza.
// May return a few blank/comment lines at end with no real stanza.
// Will join continuation lines, allocating memory as needed.
// returns pairs with name=joined line and if joined,
// val will contain raw lines '\'s and linefeeds, else val will be NULL.

boolean raSkipLeadingEmptyLines(struct lineFile *lf, struct dyString *dy);
/* Skip leading empty lines and comments.  Returns FALSE at end of file.
 * Together with raNextTagVal you can construct your own raNextRecord....
 * If dy parameter is non-null, then the text parsed gets placed into dy. */

boolean raNextTagVal(struct lineFile *lf, char **retTag, char **retVal, struct dyString  *dy);
// Read next line.  Return FALSE at end of file or blank line.  Otherwise fill in
// *retTag and *retVal and return TRUE.  If dy parameter is non-null, then the text parsed
// gets appended to dy. Continuation lines in RA file will be joined to produce tag and val,
// but dy will be filled with the unedited multiple lines containing the continuation chars.

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

struct hash *raReadWithFilter(char *fileName, char *keyField,char *filterKey,char *filterValue);
/* Return hash that contains all filtered ra records in file keyed by given field, which must exist.
 * The values of the hash are themselves hashes.  The filter is a key/value pair that must exist.
 * Example raReadWithFilter(file,"term","type","antibody"): returns hash of hashes of every term with type=antibody */

struct hash *raTagVals(char *fileName, char *tag);
/* Return a hash of all values of given tag seen in any stanza of ra file. */

#endif /* RA_H */

