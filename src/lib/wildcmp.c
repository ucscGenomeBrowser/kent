/* Wildcard matching. */

#include "common.h"

static int subMatch(char *str, char *wild)
/* Returns number of characters that match between str and wild up
 * to the next wildcard in wild (or up to end of string.). */
{
int len = 0;

for(;;)
    {
    if(toupper(*str++) != toupper(*wild++) )
        return(0);
    ++len;
    switch(*wild)
        {
        case 0:
        case '?':
        case '*':
            return(len);
        }
    }
}

boolean wildMatch(char *wildCard, char *string)
/* does a case sensitive wild card match with a string.
 * * matches any string or no character.
 * ? matches any single character.
 * anything else etc must match the character exactly. */
{
boolean matchStar = 0;
int starMatchSize;

for(;;)
    {
NEXT_WILD:
    switch(*wildCard)
	{
	case 0: /* end of wildcard */
	    {
	    if(matchStar)
		{
		while(*string++)
                    ;
		return TRUE;
                }
            else if(*string)
		return FALSE;
            else
                return TRUE;
	    }
	case '*':
	    matchStar = TRUE;
	    break;
	case '?': /* anything will do */
	    {
	    if(*string == 0)
	        return FALSE; /* out of string, no match for ? */
	    ++string;
	    break;
	    }
	default:
	    {
	    if(matchStar)
    	        {
		for(;;)
		    {
		    if(*string == 0) /* if out of string no match */
		        return FALSE;

		    /* note matchStar is re-used here for substring
		     * after star match length */
		    if((starMatchSize = subMatch(string,wildCard)) != 0)
		        {
			string += starMatchSize;
			wildCard += starMatchSize;
			matchStar = FALSE;
			goto NEXT_WILD;
		        }
		    ++string;
		    }
	        }

	    /* default: they must be equal or no match */
	    if(toupper(*string) != toupper(*wildCard))
		return FALSE;
	    ++string;
	    break;
	    }
	}
    ++wildCard;
    }
}
