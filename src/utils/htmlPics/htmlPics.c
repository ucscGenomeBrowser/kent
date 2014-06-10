/* Create a html file from a list of pictures in the command line. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "htmshell.h"


void usage()
/* Print usage instructions and bail. */
{
errAbort("htmlPics - create an html file from a list of pictures\n"
         "usage\n"
	 "    htmlPics picFile(s)\n"
	 "The html will be printed to standard out.");
}

int main(int argc, char *argv[])
{
int i;
char *picName;

if (argc < 2)
    usage();
htmStart(stdout, "Some Pics");
for (i=1; i<argc; ++i)
    {
    picName = argv[i];
    printf("<IMG SRC=\"%s\">\n", picName);
    }
htmEnd(stdout);
return 0;
}
