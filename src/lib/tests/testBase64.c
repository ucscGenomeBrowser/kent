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
int i = 0, l = 0;
char *input = cloneString(argv[1]);
char *p = input;
boolean validB64 = TRUE;
if (argc != 2)
errAbort("Specify a string to encode/decode on commandline using quotes.\n"
       , argv[0]);
	    
encoded = base64Encode(argv[1], strlen(argv[1]));

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
	
if (validB64)
    decoded = base64Decode(input, NULL);

printf("original input: [%s]\nbase-64 encoding: [%s]\nbase-64 decoding: [%s]\n", argv[1], encoded, decoded);

freez(&encoded);
freez(&decoded);

return 0;

}

