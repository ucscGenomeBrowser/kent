/* microarray - Gene Ontology annotations. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "spDb.h"
#include "hgGene.h"

static boolean microarrayExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if microarray tables exist and have something
 * on this one. */
{
}

static void microarrayPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out microarray annotations. */
{
}

struct section *microarraySection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create microarray section. */
{
struct section *section = sectionNew(sectionRa, "microarray");
// section->exists = microarrayExists;
// section->print = microarrayPrint;
return section;
}

