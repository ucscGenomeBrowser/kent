/* hgFreen - Test out some CGI thing.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "web.h"
#include "cart.h"


static void choiceMenu()
/* Put up choice menu. */
{
static char *choices[] = {"one", "two", "three"};
char *var = "num";
char *val = cgiUsualString(var, "two");
char *script = "onchange=\""
    " document.mainForm.submit();\"";
cgiMakeDropListFull(var, choices, choices, ArraySize(choices), val, script);
}

void hgFreen()
/* hgFreen - Test out some CGI thing.. */
{
long uses = incCounterFile("../trash/hgFreen.count");
printf("Hello world #%ld\n", uses);
printf("<FORM ACTION=\"../cgi-bin/hgFreen\" METHOD=\"GET\" NAME=\"mainForm\">\n");
choiceMenu();
printf(" ");
cgiMakeButton("Submit", "submit");
printf("\n</FORM>");
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
htmShell("hgFreen testing 123", hgFreen, NULL);
return 0;
}
