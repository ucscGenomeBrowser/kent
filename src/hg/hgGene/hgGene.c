/* hgGene - A CGI script to display the gene details page.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "cart.h"
#include "dbDb.h"
#include "hdb.h"
#include "hui.h"
#include "web.h"
#include "ra.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: hgGene.c,v 1.1 2003/10/10 21:59:59 kent Exp $";

/* ---- Global variables. ---- */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
struct hash *oldCart;	/* Old cart hash. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
char *genome;		/* Name of genome - mouse, human, etc. */
char *geneId;		/* Name of gene. */
struct hash *genomeSettings;  /* Genome-specific settings from settings.ra. */


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGene - A CGI script to display the gene details page.\n"
  "usage:\n"
  "   hgGene cgi-vars in var=val format\n"
  "options:\n"
  "   -hgsid=XXX Session ID to grab vars from session database\n"
  "   -db=XXX  Genome database associated with gene\n"
  "   -org=XXX  Organism associated with gene\n"
  "   -hgg_gene=XXX ID of gene\n"
  );
}


void cartMain(struct cart *theCart)
/* We got the persistent/CGI variable cart.  Now
 * set up the globals and make a web page. */
{
cart = theCart;
getDbAndGenome(cart, &database, &genome);
geneId = cartString(cart, hggGene);
cartWebStart(cart, "%s Gene %s Details", genome, geneId);
cartWebEnd();
}

char *excludeVars[] = {"Submit", "submit", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 1)
    usage();
cartEmptyShell(cartMain, hUserCookie(), excludeVars, oldCart);
return 0;
}
