/* otherOrgs.c - Handle other organisms section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "hgGene.h"


struct section *otherOrgsSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create links section. */
{
struct section *section = sectionNew(sectionRa, "otherOrgs");
return section;
}
