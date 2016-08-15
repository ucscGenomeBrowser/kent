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

boolean clDry = FALSE;
boolean clRemove = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwGroupFile - Associate a file with a group.\n"
  "usage:\n"
  "   cdwGroupFile group ' boolean expression like after a SQL where'\n"
  "options:\n"
  "   -remove - instead of adding this group permission, subtract it\n"
  "   -dry - if set just print what we _would_ add group to.\n"
  "example:\n"
  "    cdwGroupFile fan_lab ' lab=\"fan\"'\n"
  "This would add all files with the lab tag fan to the fan_lab group.  Note the\n"
  "initial space before lab is required due to quirks in C command line option parser.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"dry", OPTION_BOOLEAN},
   {"remove", OPTION_BOOLEAN},
   {NULL, 0},
};

void addGroupToValidFile(struct sqlConnection *conn, 
    struct cdwValidFile *vf, struct cdwGroup *group)
/* If we don't already have file/group association, make it */
{
/* Check curent status in database, and if state matches what we
 * wantreturn early. */
boolean inGroup = cdwFileInGroup(conn, vf->fileId, group->id);
if (inGroup && !clRemove)
    return;
if (!inGroup && clRemove)
    return;

char query[256];
if (clRemove)
    sqlSafef(query, sizeof(query), "delete from cdwGroupFile where fileId=%u and groupId=%u",
	vf->fileId, group->id);
else
    sqlSafef(query, sizeof(query), "insert cdwGroupFile (fileId,groupId) values (%u,%u)",
	vf->fileId, group->id);
if (clDry)
    printf("%s\n", query);
else
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
if (clDry)
    verbose(1, "Would have %s", (clRemove ? "removed" : "added"));
else
    verbose(1, "%s", (clRemove ? "Removed" : "Added"));
verbose(1, " group %s to %d files\n", group->name, validHash->elCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clDry = optionExists("dry");
clRemove = optionExists("remove");
cdwGroupFile(argv[1], argv[2]);
return 0;
}
