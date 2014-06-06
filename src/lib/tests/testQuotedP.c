/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "quotedP.h"

int main(int argc, char *argv[])
{
char *encoded = NULL;
char *decoded = NULL;
if (argc != 2)
errAbort("%s: Specify a string to encode/decode on commandline using quotes.\n"
       , argv[0]);
	    
encoded = quotedPrintableEncode(argv[1]);
decoded = quotedPrintableDecode(argv[1]);

printf("original input: [%s]\nquoted-printable encoding: [%s]\nquoted-p decoding: [%s]\n", argv[1], encoded, decoded);

freez(&encoded);
freez(&decoded);

return 0;
}

