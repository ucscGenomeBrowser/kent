#include "common.h"
#include "linefile.h"
#include "blackList.h"

struct blackListRange *blackListParse(char *blackList)
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
    char *number1 = skipToNumeric(prefix1);
    int begin = atoi(number1); 
    *number1 = 0;   // null so now prefix1 points to only the prefix

    char *prefix2 = cloneString(words[1]);
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

boolean blackListFail(char *accession, struct blackListRange *ranges)
/* check to see if accession is black listed */
{
char prefix[20];  // way too big, prefixes are only 2 chars now.
int count = 0;
char *ptr = accession;

while(isalpha(*ptr))
    {
    prefix[count++] = *ptr++;
    if (count > sizeof(prefix) - 1)
        errAbort("overflowed prefix buffer. Accession prefix > %lu chars\n",
            sizeof(prefix) - 1);
    }
prefix[count] = 0;

unsigned number = atoi(ptr);	// ignores version number if present

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
