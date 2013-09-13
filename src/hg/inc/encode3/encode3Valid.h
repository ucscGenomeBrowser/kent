/* Things to do with ENCODE 3 validation. */

#ifndef ENCODE3VALID_H
#define ENCODE3VALID_H

char *encode3CalcValidationKey(char *md5Hex, long long fileSize);
/* calculate validation key to discourage faking of validation.  Do freeMem on 
 *result when done. */

void encode3ValidateRcc(char *path);
/* Validate a nanostring rcc file. */

void encode3ValidateIdat(char *path);
/* Validate illumina idat file. */

boolean encode3IsGzipped(char *path);
/* Return TRUE if file at path starts with GZIP signature */

#endif /* ENCODE3VALID_H */
