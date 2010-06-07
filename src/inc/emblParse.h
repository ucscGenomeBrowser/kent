/* Parse EMBL formatted files. EMBL files are basically line
 * oriented.  Each line begins with a short (usually two letter)
 * type word.  Adjacent lines with the same type are generally
 * considered logical extensions of each other.  In many cases
 * lines can be considered fields in an EMBL database.  Records
 * are separated by lines starting with '//'  Generally lines
 * starting with XX are empty and used to make the records more
 * human readable.   Here is an example record:
 
 C  M00001
 XX
 ID  V$MYOD_01
 XX
 NA  MyoD
 XX
 DT  EWI (created); 19.10.92.
 DT  ewi (updated); 22.06.95.
 XX
 PO     A     C     G     T
 01     0     0     0     0
 02     0     0     0     0
 03     1     2     2     0
 04     2     1     2     0
 05     3     0     1     1
 06     0     5     0     0
 07     5     0     0     0
 08     0     0     4     1
 09     0     1     4     0
 10     0     0     0     5
 11     0     0     5     0
 12     0     1     2     2
 13     0     2     0     3
 14     1     0     3     1
 15     0     0     0     0
 16     0     0     0     0
 17     0     0     0     0
 XX
 BF  T00526; MyoD                         ; mouse
 XX
 BA  5 functional elements in 3 genes
 XX
 XX
 //
 */

#ifndef EMBLPARSE_H
#define EMBLPARSE_H

#ifndef HASH_H
#include "hash.h"
#endif

#ifndef DYSTRING_H
#include "dystring.h"
#endif

#ifndef LINEFILE_H
#include "linefile.h"
#endif

boolean emblLineGroup(struct lineFile *lf, char type[16], struct dyString *val);
/* Read next line of embl file.  Read line after that too if it
 * starts with the same type field. Return FALSE at EOF. */

struct hash *emblRecord(struct lineFile *lf);
/* Read next record and return it in hash.   (Free this
 * hash with freeHashAndVals.)   Hash is keyed by type
 * and has string values. */

struct lineFile *emblOpen(char *fileName, char type[256]);
/* Open up embl file, verify format and optionally  return 
 * type (VV line).  Close this with lineFileClose(). */

#endif /* EMBLPARSE_H */

