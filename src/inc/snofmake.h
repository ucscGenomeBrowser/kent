/* snofmake.h - Sorted Name Offset File creation functions. 
 *
 * This file is copyright 2000 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

boolean snofMakeIndex(FILE *inFile, char *outName, 
    boolean (*nextRecord)(FILE *inFile, void *data, char **rName, int *rNameLen), 
    void *data);
/* Make an index file - name/offset pairs that are sorted by name.
 * Inputs:
 *     inFile - open file that you're indexing with header read and verified.
 *     outName - name of index file to create
 *     nextRecord - function that reads next record in file you're indexing
 *                  and returns the name of that record.  Returns FALSE at
 *                  end of file.  Can set *rNameLen to zero you want indexer
 *                  to ignore the record. 
 *     data - void pointer passed through to nextRecord.
 *
 * Writes diagnostic output to stderr and returns FALSE if there's a problem.
 */

boolean snofDupeOkIndex(FILE *inFile, char *outName, 
    boolean (*nextRecord)(FILE *inFile, void *data, char **rName, int *rNameLen), 
    void *data, boolean dupeOk);
/* Make an index file, as in snofMakeIndex, but optionally allow duplicates
 * without complaining. */

void snofSignature(char **rSig, int *rSigSize);
/* The get signature that should be at start of a snof indexed 
 * file. */

boolean isSnofSig(void *sig);
/* Return true if sig is right. */

