/* cdwChangeAccess - Change access to files.. */
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
boolean clId = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwChangeAccess - Change access to files.\n"
  "usage:\n"
  "   cdwChangeAccess code ' boolean query in rql'\n"
  "where code is three letters encode who, direction, and access in the same fashion\n"
  "more modern versions of the unix chmod function does.  The who can be either\n"
  "   a - for all\n"
  "   g - for group\n"
  "   u - for user\n"
  "and direction is either + to add a permission or - to take it away,  and access is either\n"
  "   r - for read\n"
  "   w - for write\n"
  "for instance a+r gives everyone read access\n"
  "\n"
  "Unfortunately the query may include an = in which case it will fight with the option parsing\n"
  "The way around this is to include spaces on either side of the equal\n"
  "options:\n"
  "   -dry - if set just print what we _would_ change access to.\n"
  "   -id - if set then instead of a boolean query, just use a file ID\n"
  "example:\n"
  "    cdwChangeAccess a+r ' data_set_id=\"quakeBrainGeo\"'\n"
  "This would make all files associated with the data set quakeBrainGeo readable for all,\n"
  "something you might do when a data set is published.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"dry", OPTION_BOOLEAN},
   {"id", OPTION_BOOLEAN},
   {NULL, 0},
};

void parseChmodString(char *s, char *retWhere, char *retDir, char *retAccess)
/* Make sure s is in "a+w" or "g-r" sort of format and parse out the three
 * letters. */
{
if (strlen(s) != 3)
    errAbort("Expecting 3 chars in parseChmodString, got %d", (int)strlen(s));
char where = tolower(s[0]), dir = s[1], access = tolower(s[2]);
if (where != 'a' && where != 'g' && where != 'u')
    errAbort("Unrecognized first letter %c in %s", where, s);
if (dir != '+' && dir != '-')
    errAbort("Unrecognized second character %c in %s", dir, s);
if (access != 'r' && access != 'w')
    errAbort("Unrecognized third character %c in %s", access, s);
*retWhere = where;
*retDir = dir;
*retAccess = access;
}

void changeAccess(struct sqlConnection *conn, long long fileId, 
    char cWhere, char cDir, char cAccess)
/* Change access attribute of given file according to code in cWhere/cDir/cAccess */
{
int access = 0;
if (cDir == '+')
    {
    if (cAccess == 'w')
        access = cdwAccessWrite;
    else
        access = cdwAccessRead;
    }
else
    {
    if (cAccess == 'w')
        access = cdwAccessRead;
    }
char *field = NULL;
if (cWhere == 'a')
    field = "allAccess";
else if (cWhere == 'g')
    field = "groupAccess";
else if (cWhere == 'u')
    field = "userAccess";
else
    errAbort("Unknown cWhere '%c' in changeAccess", cWhere);

char query[256];
sqlSafef(query, sizeof(query), "update cdwFile set %s=%d where id=%lld", field, access, fileId);
if (clDry)
    {
    int uninformativePrefixLen = 9;
    verbose(1, "%s\n", query+uninformativePrefixLen);
    }
else
    sqlUpdate(conn, query);
}

void cdwChangeAccess(char *chmodString, char *rqlWhere)
/* cdwChangeAccess - Change access to files.. */
{
char cWhere, cDir, cAccess;
parseChmodString(chmodString, &cWhere, &cDir, &cAccess);
struct sqlConnection *conn = cdwConnectReadWrite();

/* Get list of all stanzas matching query */
if (clId)
    {
    long long id = sqlLongLong(rqlWhere);
    changeAccess(conn, id, cWhere, cDir, cAccess);
    }
else
    {
    struct tagStorm *tags = cdwTagStorm(conn);
    struct dyString *rqlQuery = dyStringNew(0);
    dyStringPrintf(rqlQuery, "select accession from cdwFileTags where accession and %s", rqlWhere);
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
		changeAccess(conn, vf->fileId, cWhere, cDir, cAccess);
		}
	    }
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clDry = optionExists("dry");
clId = optionExists("id");
cdwChangeAccess(argv[1], argv[2]);
return 0;
}
