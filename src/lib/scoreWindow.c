/* scoreWindow - find window with most matches to a given char */
#include "common.h"


int scoreWindow(char c, char *s, int size, int *score, int *start, int *end, int match, int misMatch)
/* simple program to find max scoring window representing string of char c in a string s of size size */
/* index of max score is returned , match and misMatch are the scores to assign, suggested defaults are match=1 and misMatch=1*/
/* when used for scoring polyA tails, set c='A' for positive strand  or c='T' for neg strand */
/* start, end are returned pointing to the start and end of the highest scoring window in s */
{
int i=0, j=0, max=0, count = 0; 

*end = 0;

for (i=0 ; i<size ; i++)
    {
    int prevScore = (i > 0) ? score[i-1] : 0;

    if (toupper(s[i]) == toupper(c) )
        score[i] = prevScore+match;
    else
        score[i] = prevScore-misMatch;
    if (score[i] >= max)
        {
        max = score[i];
        *end = i;
        /* traceback to find start */
        for (j=i ; j>=0 ; j--)
            if (score[j] == 0)
                {
                *start = j+1;
                break;
                }
        }
    if (score[i] < 0) 
        score[i] = 0;
    }
assert (*end < size);

for (i=*start ; i<=*end ; i++)
    {
    assert (i < size);
    if (toupper(s[i]) == toupper(c) )
        count++;
    }
return count;
}
