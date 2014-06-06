/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "base64.h"

void binary(unsigned int b, int width, int group) {
int g=1;
b <<= (32-width);
while (width--) 
    {
    printf("%d", b & 0x80000000 ? 1 : 0);
    b <<= 1;
    if (g++ >= group)
	{
	printf(" ");
	g = 1;
	}
    }
}

int main(int argc, char *argv[])
{
char *encoded = NULL;
char *decoded = NULL;
char *input = cloneString(argv[1]);
boolean validB64 = TRUE;
if (argc != 2)
errAbort("%s: Specify a string to encode/decode on commandline using quotes.\n"
       , argv[0]);
	    
encoded = base64Encode(argv[1], strlen(argv[1]));

validB64 = base64Validate(input);  /* removes whitespace */

if (validB64)
    decoded = base64Decode(input, NULL);

printf("original input: [%s]\nbase-64 encoding: [%s]\nbase-64 decoding: [%s]\n", argv[1], encoded, decoded);

freez(&encoded);
freez(&decoded);

return 0;

}

