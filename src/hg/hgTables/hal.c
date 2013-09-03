#include "common.h"
#include "cart.h"
#include "jksql.h"
#include "grp.h"
#include "hubConnect.h"
#include "hdb.h"
#include "hgTables.h"

extern struct trackDb *curTrack;	/* Currently selected track. */
extern char *database;
extern struct hash *fullTableToTdbHash;        /* All tracks and subtracks keyed by tdb->table field. */

boolean isHalTable(char *table)
/* Return TRUE if table corresponds to a HAL file. */
{
if (isHubTrack(table))
    {
    struct trackDb *tdb = hashFindVal(fullTableToTdbHash, table);
    printf("HERE tdb type is %s\n", tdb->type);
    return startsWithWord("halSnake", tdb->type);
    }
else
    return trackIsType(database, table, curTrack, "halSnake", ctLookupName);
}

static char *samAlignmentAutoSqlString =
"table samAlignment\n"
"\"The fields of a SAM short read alignment, the text version of BAM.\"\n"
    "(\n"
    "string qName;	\"Query template name - name of a read\"\n"
    "ushort flag;	\"Flags.  0x10 set for reverse complement.  See SAM docs for others.\"\n"
    "string rName;	\"Reference sequence name (often a chromosome)\"\n"
    "uint pos;		\"1 based position\"\n"
    "ubyte mapQ;		\"Mapping quality 0-255, 255 is best\"\n"
    "string cigar;	\"CIGAR encoded alignment string.\"\n"
    "string rNext;	\"Ref sequence for next (mate) read. '=' if same as rName, '*' if no mate\"\n"
    "int pNext;		\"Position (1-based) of next (mate) sequence. May be -1 or 0 if no mate\"\n"
    "int tLen;	        \"Size of DNA template for mated pairs.  -size for one of mate pairs\"\n"
    "string seq;		\"Query template sequence\"\n"
    "string qual;	\"ASCII of Phred-scaled base QUALity+33.  Just '*' if no quality scores\"\n"
    "string tagTypeVals; \"Tab-delimited list of tag:type:value optional extra fields\"\n"
    ")\n";

struct asObject *halAsObj()
// Return asObject describing fields of SAM/BAM
{
return asParseText(samAlignmentAutoSqlString);
}


struct slName *halGetFields(char *table)
/* Get fields of bam as simple name list. */
{
struct asObject *as = halAsObj();
return asColNames(as);
}

void halTabOut(char *db, char *table, struct sqlConnection *conn, char *fields, FILE *f)
/* Print out selected fields from HAL.  If fields is NULL, then print out all fields. */
{
}
