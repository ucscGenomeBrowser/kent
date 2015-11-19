/* annoStreamDbFactorSource -- factorSource track w/related tables */

#include "annoStreamDbFactorSource.h"
#include "annoStreamDb.h"
#include "factorSource.h"
#include "hdb.h"
#include "sqlNum.h"

static char *asfsAutoSqlString =
"table factorSourcePlus"
"\"factorSourcePlus: Peaks clustered by factor w/normalized scores plus cell type and treatment\""
"   ("
"   string  chrom;          \"Chromosome\""
"   uint    chromStart;     \"Start position in chrom\""
"   uint    chromEnd;       \"End position in chrom\""
"   string  name;           \"factor that has a peak here\""
"   uint   score;           \"Score from 0-1000\""
"   uint expCount;          \"Number of experiment values\""
"   uint[expCount] expNums; \"Comma separated list of experiment numbers\""
"   float[expCount] expScores; \"Comma separated list of experiment scores\""
"   string[expCount] cellType; \"Comma separated list of experiment cell types\""
"   string[expCount] treatment; \"Comma separated list of experiment treatments\""
"   )";

#define FACTORSOURCEPLUS_NUM_COLS 10

struct annoStreamDbFactorSource
{
    struct annoStreamer streamer;	// Parent class members & methods (external interface)
    // Private members
    struct annoStreamer *mySource;	// Internal source of track table rows
    // Data from related tables
    int expCount;			// Number of experiments whose results were clustered
    char **expCellType;			// Array[expCount] of cellType used in each experiment
    char **expTreatment;		// Array[expCount] of treatment used in each experiment
};

struct asObject *annoStreamDbFactorSourceAsObj()
/* Return an autoSql object that describs fields of a joining query on a factorSource table
 * and its inputs. */
{
return asParseText(asfsAutoSqlString);
}

static void asdfsSetRegion(struct annoStreamer *sSelf, char *chrom, uint rStart, uint rEnd)
/* Pass setRegion down to internal source. */
{
annoStreamerSetRegion(sSelf, chrom, rStart, rEnd);
struct annoStreamDbFactorSource *self = (struct annoStreamDbFactorSource *)sSelf;
self->mySource->setRegion(self->mySource, chrom, rStart, rEnd);
}

static char *commaSepFromExpData(char **expAttrs, int *expNums, uint expCount, struct lm *lm)
/* Look up experiment attribute strings by experiment numbers; return a comma-separated string
 * of experiment attributes, allocated using lm. */
{
int i;
int len = 0, offset = 0;
for (i = 0;  i < expCount;  i++)
    len += (strlen(expAttrs[expNums[i]]) + 1);
char *str = lmAlloc(lm, len + 1);
for (i = 0;  i < expCount;  i++)
    {
    char *attr = expAttrs[expNums[i]];
    safef(str + offset, len + 1 - offset, "%s,", attr);
    offset += strlen(attr) + 1;
    }
return str;
}

INLINE void getCommaSepInts(char *commaSep, int *values, int expectedCount)
/* Parse comma-separated ints into values[]. This is like sqlSignedStaticArray,
 * but we give it an expected count and it's thread-safe because it doesn't use
 * static variables. */
{
char *s = commaSep, *e = NULL;
int count;
for (count = 0;  isNotEmpty(s);  count++, s = e)
    {
    e = strchr(s, ',');
    if (e != NULL)
	*e++ = 0;
    if (count < expectedCount)
	values[count] = sqlSigned(s);
    }
if (count != expectedCount)
    errAbort("getCommaSepInts: expected %d values but found %d", expectedCount, count);
}

static void factorSourceToFactorSourcePlus(struct annoStreamDbFactorSource *self,
					   char **fsWords, char **fspWords, struct lm *lm)
/* Copy fsWords into fspWords and add columns for cellTypes and treatments corresponding to
 * expNums. */
{
// Parse out experiment IDs from expNums column
uint expCount = sqlUnsigned(fsWords[5]);
int expNums[expCount];
getCommaSepInts(fsWords[6], expNums, expCount);
// Copy factorSource columns, then add experiment attribute columns.
int i;
for (i = 0;  i < FACTORSOURCE_NUM_COLS;  i++)
    fspWords[i] = fsWords[i];
fspWords[i++] = commaSepFromExpData(self->expCellType, expNums, expCount, lm);
fspWords[i++] = commaSepFromExpData(self->expTreatment, expNums, expCount, lm);
if (i != FACTORSOURCEPLUS_NUM_COLS)
    errAbort("annoStreamDbFactorSource %s: expected to make %d columns but made %d",
	     self->streamer.name, FACTORSOURCEPLUS_NUM_COLS, i);
}

static struct annoRow *asdfsNextRow(struct annoStreamer *sSelf, char *minChrom, uint minEnd,
				    struct lm *lm)
/* Join experiment data with expNums from track table and apply filters. */
{
struct annoStreamDbFactorSource *self = (struct annoStreamDbFactorSource *)sSelf;
char **fspWords;
lmAllocArray(lm, fspWords, FACTORSOURCEPLUS_NUM_COLS);
struct annoRow *fsRow;
boolean rightJoinFail = FALSE;
while ((fsRow = self->mySource->nextRow(self->mySource, minChrom, minEnd, lm)) != NULL)
    {
    char **fsWords = fsRow->data;
    factorSourceToFactorSourcePlus(self, fsWords, fspWords, lm);
    // If there are filters on experiment attributes, apply them, otherwise just return aRow.
    if (sSelf->filters)
	{
	boolean fails = annoFilterRowFails(sSelf->filters, fspWords, FACTORSOURCEPLUS_NUM_COLS,
					   &rightJoinFail);
	// If this row passes the filter, or fails but is rightJoin, then we're done looking.
	if (!fails || rightJoinFail)
	    break;
	}
    else
	// no filtering to do, just use this row
	break;
    }
if (fsRow != NULL)
    return annoRowFromStringArray(fsRow->chrom, fsRow->start, fsRow->end, rightJoinFail,
				  fspWords, FACTORSOURCEPLUS_NUM_COLS, lm);
else
    return NULL;
}

static void getExperimentData(struct annoStreamDbFactorSource *self, char *db,
			      char *sourceTable, char *inputsTable)
/* Join two small tables to relate experiment IDs from the track tables expNums column
 * to experiment attributes cellType and treatment. */
{
struct sqlConnection *conn = hAllocConn(db);
self->expCount = sqlRowCount(conn, sourceTable);
AllocArray(self->expCellType, self->expCount);
AllocArray(self->expTreatment, self->expCount);
struct dyString *query = sqlDyStringCreate("select id, cellType, treatment "
					   "from %s, %s where %s.description = %s.source",
					   sourceTable, inputsTable, sourceTable, inputsTable);
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    int id = sqlSigned(row[0]);
    if (id < 0 || id >= self->expCount)
	errAbort("annoStreamDbFactorSource %s: found out-of-range id %d in %s (expected [0-%d])",
		 ((struct annoStreamer *)self)->name, id, sourceTable, self->expCount - 1);
    self->expCellType[id] = cloneString(row[1]);
    self->expTreatment[id] = cloneString(row[2]);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

static void asdfsClose(struct annoStreamer **pSSelf)
/* Free up state. */
{
if (pSSelf == NULL)
    return;
struct annoStreamDbFactorSource *self = *(struct annoStreamDbFactorSource **)pSSelf;
self->mySource->close(&(self->mySource));
int i;
for (i = 0;  i < self->expCount;  i++)
    {
    freeMem(self->expCellType[i]);
    freeMem(self->expTreatment[i]);
    }
freez(&self->expCellType);
freez(&self->expTreatment);
annoStreamerFree(pSSelf);
}

struct annoStreamer *annoStreamDbFactorSourceNew(char *db, char *trackTable, char *sourceTable,
						 char *inputsTable, struct annoAssembly *aa,
						 int maxOutRows)
/* Create an annoStreamer (subclass) object using three database tables:
 * trackTable: the table for a track with type factorSource (bed5 + exp{Count,Nums,Scores})
 * sourceTable: trackTable's tdb setting sourceTable; expNums -> source name "cellType+lab+antibody"
 * inputsTable: trackTable's tdb setting inputTrackTable; source name -> cellType, treatment, etc.
 */
{
struct annoStreamDbFactorSource *self;
AllocVar(self);
struct annoStreamer *streamer = &(self->streamer);
// Set up external streamer interface
annoStreamerInit(streamer, aa, annoStreamDbFactorSourceAsObj(), trackTable);
streamer->rowType = arWords;
// Get internal streamer for trackTable
self->mySource = annoStreamDbNew(db, trackTable, aa, maxOutRows, NULL);
// Slurp in data from small related tables
getExperimentData(self, db, sourceTable, inputsTable);
// Override methods that need to pass through to internal source:
streamer->setRegion = asdfsSetRegion;
streamer->nextRow = asdfsNextRow;
streamer->close = asdfsClose;

return (struct annoStreamer *)self;
}
