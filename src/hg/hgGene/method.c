/* method - UCSC Known Genes Method. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "spDb.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: method.c,v 1.2 2005/02/25 17:13:01 fanhsu Exp $";

static void methodPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out link to UCSC KG method and credits details. */
{
hPrintf("Click ");
hPrintf("<A HREF=\"../cgi-bin/hgGene?hgg_do_kgMethod=1&hgg_type=%s\" target=_blank>", curGeneType);
hPrintf("here</A>\n");
hPrintf(" for details.");
}

struct section *methodSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create UCSC KG Method section. */
{
struct section *section = sectionNew(sectionRa, "method");
section->print = methodPrint;
return section;
}

