/* Things to do with ENCODE 3 validation. */

#ifndef ENCODE3VALID_H
#define ENCODE3VALID_H

char *encode3CalcValidationKey(char *md5Hex, long long fileSize);
/* calculate validation key to discourage faking of validation.  Do freeMem on 
 *result when done. */

#endif /* ENCODE3VALID_H */
