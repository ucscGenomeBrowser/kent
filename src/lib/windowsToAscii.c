/* Copyright (C) 2017 The Regents of the University of California 
 *  * See README in this or parent directory for licensing information. */

#include "iconv.h"
#include "common.h"

char *windowsToAscii(char *input)
/* Convert windows-1250 to ascii. */
{
if (input == NULL)
    return NULL;

iconv_t convType = iconv_open("ASCII//TRANSLIT", "WINDOWS-1250");  
size_t inSize = strlen(input);
size_t outSize = 1024;  
char buffer[outSize];  
char *outString = buffer;  
char *inPtr = input;  
iconv(convType, &inPtr, &inSize, &outString, &outSize);  
*outString = 0;  

return cloneString(buffer);
}
