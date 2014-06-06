/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "genbank.h"
#include "genbankBlackList.h"

struct blackListRange *genbankBlackListParse(char *blackList)
/* parse a black list file into blackListRange data structure */
{
struct lineFile *lf = lineFileMayOpen(blackList, TRUE);
if (lf == NULL)
    errAbort("Could not open black list file %s. ", blackList);

struct blackListRange *ranges = NULL;
char *words[2];
                                  
while(lineFileRow(lf, words))
    {
    char *prefix1 = cloneString(words[0]);
    genbankDropVer(prefix1, prefix1);
    char *number1 = skipToNumeric(prefix1);
    int begin = atoi(number1); 
    *number1 = 0;   // null so now prefix1 points to only the prefix

    char *prefix2 = cloneString(words[1]);
    genbankDropVer(prefix2, prefix2);
    char *number2 = skipToNumeric(prefix2);
    int end = atoi(number2);
    *number2 = 0;   // null so now prefix2 points to only the prefix

    if (differentString(prefix1, prefix2))
        errAbort("blackList file %s has accesions with different prefixes on line %d\n",
            lf->fileName, lf->lineIx);
    if (begin > end)
        errAbort("blackList file %s has end before begin on line %d\n",
            lf->fileName, lf->lineIx);

    struct blackListRange *range;
    AllocVar(range);
    range->prefix = prefix1;
    range->begin = begin;
    range->end = end;
    slAddHead(&ranges, range);
    }
return ranges;
}

boolean genbankBlackListFail(char *accession, struct blackListRange *ranges)
/* check to see if accession is black listed */
{
char prefix[20];  // way too big, prefixes are only 2 chars now.
int count = 0;
char *ptr = accession;

while((*ptr) && !isdigit(*ptr))
    {
    prefix[count++] = *ptr++;
    if (count > sizeof(prefix) - 1)
        errAbort("overflowed prefix buffer. Accession prefix > %lu chars\n",
            (unsigned long)sizeof(prefix) - 1);
    }
prefix[count] = 0;

if (*ptr == 0)
    errAbort("accession not in proper format (%s)", accession);

// get number of accession up till optional version number
unsigned number = atoi(ptr);	// returns number up to '.' or null

struct blackListRange *range = ranges;
for(; range ; range = range->next)
    {
    if (sameString(prefix, range->prefix) &&
        (number >= range->begin) &&
        (number <= range->end))
        {
            verbose(2, "blacklisted %s\n", accession);
            return TRUE;
        }
    }

return FALSE;
}
