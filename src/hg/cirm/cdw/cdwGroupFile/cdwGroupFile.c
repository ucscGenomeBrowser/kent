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
  "   cdwGroupFile group 'boolean expression like after a SQL where'\n"
  "options:\n"
  "   -dry - if set just print what we _would_ add group to.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"dry", OPTION_BOOLEAN},
   {NULL, 0},
};

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

void cdwGroupFile(char *groupName, char *where)
/* cdwGroupFile - Associate a file with a group.. */
{
/* Get group from database, error out if no good */
struct sqlConnection *conn = cdwConnectReadWrite();
struct cdwGroup *group = cdwNeedGroupFromName(conn, groupName);

/* Get list of all stanzas matching query */
struct tagStorm *tags = cdwTagStorm(conn);
struct dyString *rqlQuery = dyStringNew(0);
dyStringPrintf(rqlQuery, "select accession from cdwFileTags where accession");
if (where != NULL)
    dyStringPrintf(rqlQuery, " and %s", where);
struct slRef *ref, *matchRefList = tagStanzasMatchingQuery(tags, rqlQuery->string);

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

/* Second pass through matching list we call routine that actually adds
 * the group/file relationship. */
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
