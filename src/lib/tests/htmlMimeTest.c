/* htmlMimeTest - test mime-encoded posts using htmlPage module. */

/* Copyright (C) 2005 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "verbose.h"
#include "htmlPage.h"
#include "obscure.h"
#include "qa.h"


void usage()
/* Explain usage and exit */
{
errAbort(
"htmlMimeTest - post mime-encoded page with series on input sizes\n"
"usage:\n"
"   htmlMimeTest url-to-hgBlat dnaText startSize endSize\n"
"e.g. htmlMimeTest https://hgwdev-${user}.gi.ucsc.edu/cgi-bin/hgBlat input/htmlMime.txt 100 200\n"
"options:\n"
"   -asFile - send data as type FILE upload (seqFile) instead of TEXTAREA (userSeq) \n"
);
}

static struct optionSpec options[] = {
   {"asFile", OPTION_BOOLEAN},
   {NULL, 0},
};

void htmlMimeTest(char *url, char *dnaFileName, int size)
/* Submit a sequence to hgBlat which will exercise mime parsing code 
 * because the form uses ENCTYPE="multipart/form-data" */
{
struct htmlPage *rootPage, *page;
struct htmlForm *form, *mainForm;
struct qaStatus *qs;
char *dna, *dnaSeg;
qs = qaPageGet(url, &rootPage);
if (qs->errMessage)
    errAbort("%s",qs->errMessage);
if (qs->hardError)
    errAbort("hard error");
htmlPageValidateOrAbort(rootPage);

if (verboseLevel() == 2)
    {
    for (form = rootPage->forms; form != NULL; form = form->next)
	htmlFormPrint(form, stderr);
    }
    
if ((mainForm = htmlFormGet(rootPage, "mainForm")) == NULL)
    errAbort("Couldn't get main form");

readInGulp(dnaFileName, &dna, NULL);
dnaSeg = cloneStringZ(dna,size);
verbose(2,"dna=[%s]\n",dnaSeg);

if (optionExists("asFile"))
    htmlPageSetVar(rootPage, mainForm, "seqFile", dnaFileName);
else    
    htmlPageSetVar(rootPage, mainForm, "userSeq", dnaSeg);

qs = qaPageFromForm(rootPage, mainForm,
                "Submit", "submit", &page);

if (qs->errMessage)
    errAbort("%s",qs->errMessage);
if (qs->hardError)
    errAbort("hard error");
htmlPageValidateOrAbort(page);

verbose(2,"full html response = [%s]\n",page->fullText);

fprintf(stdout,"%d ok\n",size);

freez(&dnaSeg);
freez(&dna);
htmlPageFree(&page);
htmlPageFree(&rootPage);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int start, end, i;
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
start = atoi(argv[3]);    
end   = atoi(argv[4]);
for (i=start; i<=end; ++i)
    htmlMimeTest(argv[1],argv[2],i);
return 0;
}

