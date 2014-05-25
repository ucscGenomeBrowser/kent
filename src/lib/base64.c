/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "base64.h"


char *base64Encode(char *input, size_t inplen)
/* Use base64 to encode a string.  Returns one long encoded
 * string which need to be freeMem'd. Note: big-endian algorithm.
 * For some applications you may need to break the base64 output
 * of this function into lines no longer than 76 chars.
 */
{
char b64[] = B64CHARS;
int words = (inplen+2)/3;
int remains = inplen % 3;
char *result = (char *)needMem(4*words+1);
size_t i=0, j=0;
int word = 0;
unsigned char *p = (unsigned char*) input;  
/* p must be unsigned char*,  because without "unsigned",
sign extend messes up last group outputted
when the value of the chars following last in input
happens to be char 0x80 or higher */
for(i=1; i<=words; i++)
    {
    word = 0;
    word |= *p++;
    word <<= 8;
    word |= *p++;
    word <<= 8;
    word |= *p++;
    if (i==words && remains>0)
	{
	word &= 0x00FFFF00;
    	if (remains==1)
    	    word &= 0x00FF0000;
	}
    result[j++]=b64[word >> 18 & 0x3F];
    result[j++]=b64[word >> 12 & 0x3F];
    result[j++]=b64[word >> 6 & 0x3F];
    result[j++]=b64[word & 0x3F];
    }
result[j] = 0;
if (remains >0) result[j-1] = '=';    
if (remains==1) result[j-2] = '=';    
return result;
}


boolean base64Validate(char *input)
/* Return true if input is valid base64.
 * Note that the input string is changed by 
 * eraseWhiteSpace(). */
{
size_t i = 0, l = 0;
char *p = input;
boolean validB64 = TRUE;

/* remove whitespace which is unnecessary and  */
eraseWhiteSpace(input);  

l = strlen(p);
for(i=0;i<l;i++)
    {
    char c = ' ';
    if (!strchr(B64CHARS,c=*p++))
        {
        if (c != '=')
            {
            validB64 = FALSE;
            break;
            }
        }
    }
if (l%4)
    validB64 = FALSE;
return validB64;
}

char *base64Decode(char *input, size_t *returnSize)
/* Use base64 to decode a string.  Return decoded
 * string which will be freeMem'd. Note: big-endian algorithm.
 * Call eraseWhiteSpace() and check for invalid input 
 * before passing in input if needed.  
 * Optionally set return size for use with binary data.
 */
{
static int *map=NULL;
char b64[] = B64CHARS;
size_t inplen = strlen(input);
int words = (inplen+3)/4;
char *result = (char *)needMem(3*words+1);
size_t i=0, j=0;
int word = 0;
char *p = input;

if (!map)
    {
    int i = 0;
    map = needMem(256*sizeof(int));
    for (i = 0; i < 256; ++i)
	{
	map[i]=0;
	}
    for (i = 0; i < 64; ++i)
	{
	map[(int)b64[i]]=i;
	}
    }
for(i=0; i<words; i++)
    {
    word = 0;
    word |= map[(int)*p++];
    word <<= 6;
    word |= map[(int)*p++];
    word <<= 6;
    word |= map[(int)*p++];
    word <<= 6;
    word |= map[(int)*p++];
    result[j++]=word >> 16 & 0xFF;
    result[j++]=word >> 8 & 0xFF;
    result[j++]=word & 0xFF;
    }
result[j] = 0;
if (returnSize)
    *returnSize = j;
     
return result;
}

