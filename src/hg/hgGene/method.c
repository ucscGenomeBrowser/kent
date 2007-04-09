/* method - UCSC Known Genes Method. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "spDb.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: method.c,v 1.4 2007/04/09 02:10:58 kent Exp $";

static void methodPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out link to UCSC KG method and credits details. */
{
hPrintf("Click ");
hPrintf("<A HREF=\"../cgi-bin/hgGene?hgg_do_kgMethod=1&db=%s\" target=_blank>", database);
hPrintf("here</A>\n");
hPrintf(" for details on how this gene model was made and data restrictions if any.");
}

struct section *methodSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create UCSC KG Method section. */
{
struct section *section = sectionNew(sectionRa, "method");
section->print = methodPrint;
return section;
}

