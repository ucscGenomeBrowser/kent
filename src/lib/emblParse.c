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

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "emblParse.h"


boolean emblLineGroup(struct lineFile *lf, char type[16], struct dyString *val)
/* Read next line of embl file.  Read line after that too if it
 * starts with the same type field. Return FALSE at EOF. */
{
char *line, *word;
int typeLen = 0;

dyStringClear(val);
while (lineFileNext(lf, &line, NULL))
    {
    line = skipLeadingSpaces(line);

    /* Parse out first word into type. */
    if (isspace(line[0]))
        errAbort("embl line that doesn't start with type line %d of %s", 
		lf->lineIx, lf->fileName);
    if (typeLen == 0)
        {
	word = nextWord(&line);
	typeLen = strlen(word);
	if (typeLen >= 16)
	    errAbort("Type word at start of line too long for embl file line %d of %s",
	    	lf->lineIx, lf->fileName);
	strcpy(type, word);
	}
    else if (!startsWith(type, line) || !isspace(line[typeLen]))
        {
	lineFileReuse(lf);
	break;
	}
    else
        {
	dyStringAppendC(val, '\n');
	word = nextWord(&line);
	}

    if (line != NULL)
	{
	/* Usually have two spaces after type. */
	if (isspace(line[0]))
	   ++line;
	if (isspace(line[0]))
	   ++line;

	/* Append what's rest of line to return value. */
	dyStringAppend(val, line);
	}
    }
return typeLen > 0;
}

struct hash *emblRecord(struct lineFile *lf)
/* Read next record and return it in hash.   (Free this
 * hash with freeHashAndVals.)   Hash is keyed by type
 * and has string values. */
{
struct hash *hash = NULL;
char type[16];
struct dyString *val = newDyString(256);
boolean gotEnd = FALSE;

while (emblLineGroup(lf, type, val))
    {
    if (hash == NULL)
        hash = newHash(7);
    if (sameString(type, "//"))
        {
	gotEnd = TRUE;
	break;
	}
    hashAdd(hash, type, cloneString(val->string));
    }
if (hash != NULL && !gotEnd)
    warn("Incomplete last record of embl file %s\n", lf->fileName);
return hash;
}

static void notEmbl(char *fileName)
/* Complain it's not really an EMBL file. */
{
errAbort("%s is not an emblFile", fileName);
}

struct lineFile *emblOpen(char *fileName, char type[256])
/* Open up embl file, verify format and optionally  return 
 * type (VV line).  Close this with lineFileClose(). */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = emblRecord(lf);
char *vv;

if (hash == NULL)
    notEmbl(fileName);
if ((vv = hashFindVal(hash, "VV")) == NULL)
    notEmbl(fileName);
if (type != NULL)
    {
    if (strlen(vv) >= 256)
	notEmbl(fileName);
    strcpy(type, vv);
    }
freeHashAndVals(&hash);
return lf;
}
