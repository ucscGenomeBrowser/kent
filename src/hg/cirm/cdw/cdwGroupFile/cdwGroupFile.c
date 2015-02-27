/* cdwGroupFile - Associate a file with a group.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"
#include "tagStorm.h"
#include "rql.h"

boolean dry = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwGroupFile - Associate a file with a group.\n"
  "usage:\n"
  "   cdwGroupFile group 'rqlQuery'\n"
  "options:\n"
  "   -dry - if set just print what we _would_ add group to.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"dry", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean cdwFileInGroup(struct sqlConnection *conn, unsigned int fileId, unsigned int groupId)
/* Return TRUE if user is in group */
{
char query[256];
sqlSafef(query, sizeof(query), "select count(*) from cdwGroupFile where fileId=%u and groupId=%u",
    fileId, groupId);
return sqlQuickNum(conn, query) > 0;
}

void addGroupToValidFile(struct sqlConnection *conn, 
    struct cdwValidFile *vf, struct cdwGroup *group)
/* If we don't already have file/group association, make it */
{
if (cdwFileInGroup(conn, vf->fileId, group->id))
    return;	// All done by someone else
char query[256];
sqlSafef(query, sizeof(query), "insert cdwGroupFile (fileId,groupId) values (%u,%u)",
    vf->fileId, group->id);
if (!dry)
    sqlUpdate(conn, query);
}

void cdwGroupFile(char *groupName, char *rqlQuery)
/* cdwGroupFile - Associate a file with a group.. */
{
/* Get group from database, error out if no good */
struct sqlConnection *conn = cdwConnectReadWrite();
struct cdwGroup *group = cdwNeedGroupFromName(conn, groupName);

/* Get list of all stanzas matching query */
struct tagStorm *tags = cdwTagStorm(conn);
struct slRef *ref, *matchRefList = tagStanzasMatchingQuery(tags, rqlQuery);

/* Make one pass through mostly for early error reporting and building up 
 * hash of cdwValidFiles keyed by accession */
struct hash *validHash = hashNew(0);
for (ref = matchRefList; ref != NULL; ref = ref->next)
    {
    struct tagStanza *stanza = ref->val;
    char *acc = tagFindVal(stanza, "accession");
    if (acc != NULL)
        {
	struct cdwValidFile *vf = cdwValidFileFromLicensePlate(conn, acc);
	if (vf == NULL)
	    errAbort("%s not found in cdwValidFile", acc);
	hashAdd(validHash, acc, vf);
	}
    }

for (ref = matchRefList; ref != NULL; ref = ref->next)
    {
    struct tagStanza *stanza = ref->val;
    char *acc = tagFindVal(stanza, "accession");
    if (acc != NULL)
        {
	struct cdwValidFile *vf = hashFindVal(validHash, acc);
	if (vf != NULL)
	    {
	    addGroupToValidFile(conn, vf, group);
	    }
	}
    }
if (dry)
    verbose(1, "Would have added");
else
    verbose(1, "Added");
verbose(1, " group %s to %d files\n", group->name, validHash->elCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
dry = optionExists("dry");
cdwGroupFile(argv[1], argv[2]);
return 0;
}
