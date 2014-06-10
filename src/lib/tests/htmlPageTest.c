/* htmlPageTest - test some stuff in htmlPage module. */

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "htmlPage.h"
#include "obscure.h"


void usage()
/* Explain usage and exit */
{
errAbort(
"htmlPageTest - print some diagnostic info on page\n"
"usage:\n"
"   htmlPageTest file.html\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void htmlPageTest(char *fileName)
/* Do some checks on file and print. */
{
char *html;
struct htmlPage *page;
struct htmlForm *form;
readInGulp(fileName, &html, NULL);
page = htmlPageParseNoHead(fileName, html);
htmlPageValidateOrAbort(page);
for (form = page->forms; form != NULL; form = form->next)
    htmlFormPrint(form, stdout);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
htmlPageTest(argv[1]);
return 0;
}

