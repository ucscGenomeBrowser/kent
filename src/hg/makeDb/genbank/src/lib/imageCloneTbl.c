/* Code to create and access the imageClone table */
#include "imageCloneTbl.h"
#include "sqlUpdater.h"
#include "jksql.h"
#include "gbDefs.h"


/* name of the table */
char *IMAGE_CLONE_TBL = "imageClone";

/* SQL to create the table */
static char *createSql =
"NOSQLINJ create table imageClone ("
"    imageId int not null,"                         /* imageId */
"    acc char(12) not null,"                        /* GenBank accession */
"    type enum('EST','mRNA') not null,"             /* EST or mRNA? */
"    direction enum('5','3','0') not null,"         /* Read direction. */
"    index(imageId),"
"    unique(acc),"
"    index(direction),"
"    index(type))";

unsigned imageCloneGBParse(char *cloneSpec)
/* Parse an image clone id from the string take from the genbank
 * /clone field.  There are many weird case, some are handled and some are
 * infrequent enough to skip.  Several of these weird case to seem to
 * have current IMAGE clones anyway.
 * Returns 0 if not a parsable IMAGE clone spec.
 *
 * Various patterns handled:
 *   IMAGE:993981
 *   IMAGE: 993981
 *   IMAGE Consortium clone 569778
 *   HCG IV.9; IMAGE Consortium ID 362673; ze53b05
 *   IMAGE clone 23979
 *   IMAGE Consortium 23778
 *   IMAGE Consortium ID 685674
 *   IMAGE ID 511239
 *   IMAGE consortium ID# 219691
 *   IMAGE 51092 (bp 2178-4099)
 *   IMAGE consortium clone #40054
 *   IMAGE Consortium CloneID 682637
 *   IMAGE IF 612462
 *   based on IMAGE clones 39892 and 29760
 *   EST Image consortium clone ID 382073
 *   IMAGE cDNA clone 179130
 *   IMAGE-46434 (0zc01)
 *   IMAGE cDNA clone 261436, 3' end
 *   
 * For ones with two clones ids, only the first is used, as the few case seen
 * don't seem to have current IMAGE id:
 *   IMAGE 649838 and 664517
 *   IMAGE Consortium 24760 and 24814
 *   IMAGE Consortium ID 159073 and 512824
 *
 * Also punt on :
 *   KH251; cloneID 145052 from IMAGE Consortium
 *   IMAGE cDNA clone DKFZp434P1912, 3' end
 *   333357; IMAGE Consortium cDNA, GenBank Accession
 *   247801 (IMAGE)
 * 
 * Must avoid calling this an image id:
 *   EUROIMAGE 901558
 *
 * We look for the string IMAGE that is not preceeded or followed by a
 * alphanumeric character and then use the first number after that.
 */
{
char *imageStr, *next;
char c;

if (cloneSpec == NULL)
    return 0;  /* no id */

/* search for string image, ignoring case. */
imageStr = strstrNoCase(cloneSpec, "image");
if (imageStr == NULL)
    return 0;

/* make sure it's not embedded in another string. */
if ((imageStr > cloneSpec) && isalnum(imageStr[-1]))
    return 0;
next = imageStr+5;
if (isalnum(*next))
    return 0;

/* Find the next number, stoping at non-whitespace, non-alpha other than
 * :-# */
c = *next;
while (!isdigit(c))
    {
    if ((c == '\0')
        || !(isalpha(c) || isspace(c)
             || (c == ':') || (c == '-') || (c == '#')))
        return 0;  /* can't parse an id */
    c = *(++next);
    }

/* have at least one number, parse starting here till not a number */
return strtoul(next, NULL, 10);
}

struct imageCloneTbl *imageCloneTblNew(struct sqlConnection *conn,
                                       char *tmpDir, boolean verbEnabled)
/* Create an object for loading the clone table */
{
struct imageCloneTbl *ict;
AllocVar(ict);
if (!sqlTableExists(conn, IMAGE_CLONE_TBL))
    sqlRemakeTable(conn, IMAGE_CLONE_TBL, createSql);
ict->updater = sqlUpdaterNew(IMAGE_CLONE_TBL, tmpDir, verbEnabled, NULL);
return ict;
}

void imageCloneTblFree(struct imageCloneTbl **ictPtr)
/* Free object */
{
struct imageCloneTbl *ict = *ictPtr;
if (ict != NULL)
    {
    sqlUpdaterFree(&ict->updater);
    free(ict);
    *ictPtr = NULL;
    }
}

unsigned imageCloneTblGetId(struct sqlConnection *conn, char *acc)
/* get id for an acc, or 0 if none */
{
char query[256];
sqlSafef(query, sizeof(query), "SELECT imageId from %s WHERE acc = '%s'",
      IMAGE_CLONE_TBL, acc);
return sqlQuickNum(conn, query);
}

void imageCloneTblAdd(struct imageCloneTbl *ict, unsigned imageId,
                      char *acc, unsigned type, char direction)
/* Add an entry to the table */
{
if (!((direction == '5') || (direction == '3') || (direction == '0')))
    errAbort("invalid direction value for imageCLone table: %d", direction);

sqlUpdaterAddRow(ict->updater, "%u\t%s\t%s\t%c", imageId, acc,
                 gbTypeName(type), direction);
}

void imageCloneTblMod(struct imageCloneTbl *ict, unsigned imageId,
                      char *acc, char direction)
/* update an entry in the table */
{
char query[512];

if (!((direction == '5') || (direction == '3') || (direction == '0')))
    errAbort("invalid direction value for imageCLone table: %d", direction);

/* if type changes, entry is already deleted */
sqlSafefFrag(query, sizeof(query), "imageId = %u, direction = '%c' WHERE acc = '%s'",
      imageId, direction, acc);

sqlUpdaterModRow(ict->updater, 1, "%s", query);
}

void imageCloneTblCommit(struct imageCloneTbl *ict,
                         struct sqlConnection *conn)
/* Commit pending changes */
{
sqlUpdaterCommit(ict->updater, conn);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

