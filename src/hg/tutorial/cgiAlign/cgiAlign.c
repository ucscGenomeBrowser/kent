/* cgiAlign - Align two sequences using protein scoring.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "dnaseq.h"
#include "fa.h"
#include "axt.h"


static char const rcsid[] = "$Id: cgiAlign.c,v 1.1.44.1 2008/08/01 06:10:56 markd Exp $";

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;

#define varPrefix "cgiAlign."
#define p1Var varPrefix "p1"
#define p2Var varPrefix "p2"


void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
cart = theCart;
cartWebStart(cart, database, "Align two sequences using protein scoring.");
printf("<FORM ACTION=\"../cgi-bin/cgiAlign\">");
cartSaveSession(cart);
cgiMakeSubmitButton();
printf("<BR>");
printf("First protein sequence in plain text or fasta format<BR>");
char *p1Text = cartUsualString(cart, p1Var, "");
cgiMakeTextArea(p1Var, p1Text, 8, 80);
printf("<BR>");
printf("Second protein sequence in plain text or fasta format<BR>");
char *p2Text = cartUsualString(cart, p2Var, "");
cgiMakeTextArea(p2Var, p2Text, 8, 80);
printf("<BR>");
cgiMakeSubmitButton();
printf("<BR>");

bioSeq *seq1 = faSeqFromMemText(p1Text, FALSE);
bioSeq *seq2 = faSeqFromMemText(p2Text, FALSE);
struct axt *axt = axtAffine(seq1, seq2, axtScoreSchemeProteinDefault());
printf("<TT><PRE>");
axtOutPretty(axt, 80, stdout);
printf("</PRE></TT>");

printf("</FORM>");
cartWebEnd();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
