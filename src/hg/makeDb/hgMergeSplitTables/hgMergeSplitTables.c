/* hgMergeSplitTables - Merge old-stype split tables into single tables in atomic manner suitable for a running browser. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "dystring.h"
#include "verbose.h"

/* Some types of tables have SQL buried in the load programs and don't have
 * good autosql support.  There also differences between the on-disk format
 * and table format, so they are not straight loads.  Since this program is
 * not intended to have a long life, it was decided to copy the create scheme
 * here rather than do a major refactoring of the chain table code. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgMergeSplitTables - Merge old-stype split tables into single tables in atomic manner suitable for a running browser\n"
  "usage:\n"
  "   hgMergeSplitTables db action\n"
  "\n"
  "Possible actions:\n"
  "   list - list split table in db, but don't merge\n"
  "   merge - merge all split tables in DB, but leave under temporary name\n"
  "       ending with _mst_new_tmp\n"
  "   install - replace split tables with single table, leaving old table with\n"
  "        suffix _mst_old_tmp\n"
  "   cleanup - remove tables with  _mst_old_tmp suffix\n"
  "\n"
  "options:\n"
  "  -table=root - only do this table (root name, less chrom)\n"
  "  -verbose=n - verbose tracing\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    {"table", OPTION_STRING},
    {NULL, 0},
};
static char* onlyThisTable = NULL;


/* suffixes for table names before moving into place or dropping.
* `mst' == merge split tables. */
static char *NEW_TABLE_EXT = "_mst_new_tmp";
static char *OLD_TABLE_EXT = "_mst_old_tmp";

struct splitTableInfo
/* information about a single split table.  Since this is a short-term
 * program, never bother to free these. */
{
    struct splitTableInfo *next;
    char *rootName;
    char *trackDbType;
    struct slName *tables;
};

static char *getNewTableTmpName(char *table)
/* get temporary table name for a new table. WARNING: static return. */
{
static char tmpname[PATH_LEN];
safef(tmpname, sizeof(tmpname), "%s%s", table, NEW_TABLE_EXT);
return tmpname;
}

static char *getOldTableTmpName(char *table)
/* get temporary table name for an old table. WARNING: static return. */
{
static char tmpname[PATH_LEN];
safef(tmpname, sizeof(tmpname), "%s%s", table, OLD_TABLE_EXT);
return tmpname;
}

static boolean hasField(struct sqlConnection *conn, char *table, char *field)
/* does a table have a field? */
{
struct slName *fields = sqlListFields(conn, table);
boolean fieldExists = slNameInListUseCase(fields, field);
slFreeList(&fields);
return fieldExists;
}

static void verifyHasBin(struct sqlConnection *conn, struct splitTableInfo* sti)
/* for conversions that assume there is a bin column, check that a table does
 * indeed have bin */
{
char* table = sti->tables->name;
if (!hasField(conn, table, "bin"))
    errAbort("table %s.%s does not have required bin column", sqlGetDatabase(conn), table);
}

static int countExistingTables(struct sqlConnection *conn, struct slName *tables)
/* count number of split tables that exist */
{
int cnt = 0;
struct slName *table;
for (table = tables; table != NULL; table = table->next)
    {
    if (sqlTableExists(conn, table->name))
        cnt++;
    }
return cnt;
}

static struct splitTableInfo* getSplitTable(struct sqlConnection *conn, struct trackDb *trackDb)
/* get table info for split table, or NULL if does not exist or is not split */
{
// n.b. tried hFindTableInfo, but it was very slow on whole trackDb.

if ((onlyThisTable != NULL) && (!sameString(trackDb->table, onlyThisTable)))
    return NULL;
struct slName *tables = hSplitTableNames(sqlGetDatabase(conn), trackDb->table);
if (slCount(tables) <= 1)
    {
    slFreeList(&tables);
    return NULL;
    }
struct splitTableInfo *sti;
AllocVar(sti);
sti->rootName = cloneString(trackDb->table);
sti->trackDbType = cloneString(trackDb->type);
sti->tables = tables;
return sti;
}

static struct splitTableInfo* getSplitTablesRecusive(struct sqlConnection *conn, struct trackDb *trackDbs)
/* get splitTableInfo for a list of trackDb and subtracks */
{
struct splitTableInfo* splitTableInfos = NULL;
struct trackDb *trackDb;
for (trackDb = trackDbs; trackDb != NULL; trackDb = trackDb->next)
    {
    struct splitTableInfo* sti = getSplitTable(conn, trackDb);
    if (sti != NULL)
        slAddHead(&splitTableInfos, sti);
    if (trackDb->subtracks != NULL)
        splitTableInfos = slCat(splitTableInfos, getSplitTablesRecusive(conn, trackDb->subtracks));
    }
return splitTableInfos;
}    

static struct splitTableInfo* getSplitTables(struct sqlConnection *conn)
/* get table info for all split tables */
{
struct trackDb *trackDbs = hTrackDb(sqlGetDatabase(conn));
struct splitTableInfo* splitTableInfos = getSplitTablesRecusive(conn, trackDbs);
trackDbFreeList(&trackDbs);
return splitTableInfos;
}

static void listSplitTable(struct sqlConnection *conn, struct splitTableInfo* sti, FILE* fh)
/* print info about a split table */
{
fprintf(fh, "%s\t%s\t%s\t", sqlGetDatabase(conn), sti->rootName, sti->trackDbType);
struct slName* table;
for (table = sti->tables; table != NULL; table = table->next)
    {
    fputs(table->name, fh);
    if (table->next != NULL)
        fputc(' ', fh);
    }
fputc('\n', fh);
}

static void listSplitTables(struct sqlConnection *conn, FILE* fh)
/* print information about split tables. */
{
struct splitTableInfo* splitTableInfos = getSplitTables(conn);
struct splitTableInfo* sti;
fprintf(fh, "db\troot\ttrackType\ttables\n");
for (sti = splitTableInfos; sti != NULL; sti = sti->next)
    listSplitTable(conn, sti, fh);
}

static boolean shouldSkipWithMessage(struct splitTableInfo* sti)
/* should this table be skipped */
{
if (sameString(sti->rootName, "wabaCbr"))
    {
    verbose(1, "NOTE: skipping table \"%s\" type \"%s\"\n", sti->rootName, sti->trackDbType);
    return TRUE;
    }
else
    return FALSE;
}

static char* getTmpDump(char *table)
/* get temporary dump file */
{
char *tmpDir = getenv("TMPDIR");
if (tmpDir == NULL)
    tmpDir = "/scratch/tmp";
if (!fileExists(tmpDir))
    tmpDir = "/tmp";
return cloneString(rTempName(tmpDir, table, ".tab"));
}

static struct sqlResult *selectAllRows(struct sqlConnection *conn, char *table)
/* do select to get all rows in table */
{
char query[512];
safef(query, sizeof(query), "select * from %s", table);
return sqlGetResult(conn, query);
}

/* function type that copies a row use autoSql for extra paranoia */
typedef void (*rowDumpFunction)(char **row, int numColumns, char *chrom, int offset, FILE* dumpFh);

static void dumpTable(struct sqlConnection *conn, char *table, FILE* dumpFh,
                      rowDumpFunction rowDumpFn)
/* write a table row to a tab separate file, less bin */
{
// pull chrom name out of table name
char chrom[256];
safecpy(chrom, sizeof(chrom), table);
char *bar = strrchr(chrom, '_');
if (bar == NULL)
    errAbort("can't find chrom in table name \"%s\"", table);
*bar = '\0';

int offset = hOffsetPastBin(sqlGetDatabase(conn), NULL, table);
struct sqlResult *sr = selectAllRows(conn, table);
int numColumns = sqlCountColumns(sr);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    (*rowDumpFn)(row, numColumns, chrom, offset, dumpFh);
sqlFreeResult(&sr);
}

static void dumpRowAsIs(char **row, int numColumns, char *chrom, int offset, FILE* dumpFh)
/* dump a row as-is, keeping bin columns. */
{
int i;
for (i = 0; i < numColumns; i++)
    {
    if (i != 0)
        fputc('\t', dumpFh);
    fputs(row[i], dumpFh);
    }
fputc('\n',dumpFh);
}

static void dumpTables(struct sqlConnection *conn, struct splitTableInfo* sti, char *dumpFile,
                       rowDumpFunction rowDumpFn)
/* dump all split tables into the file for loading, using the specified row
 * load and dump function */
{
FILE *dumpFh = mustOpen(dumpFile, "w");
struct slName *table;
for (table = sti->tables; table != NULL; table = table->next)
    dumpTable(conn, table->name, dumpFh, rowDumpFn);
carefulClose(&dumpFh);
}

static void pslDumpRow(char **row, int numColumns, char *chrom, int offset, FILE* dumpFh)
/* dump a PSL row */
{
struct psl *psl = pslLoad(row + offset);
pslTabOut(psl, dumpFh);
pslFree(&psl);
}

static void pslMergeTables(struct sqlConnection *conn, struct splitTableInfo* sti)
/* merge psl tables */
{
char *dumpFile = getTmpDump(sti->rootName);
dumpTables(conn, sti, dumpFile, pslDumpRow);
char cmd[1024];
safef(cmd, sizeof(cmd), "hgLoadPsl -clientLoad -table=%s %s %s",
      getNewTableTmpName(sti->rootName), sqlGetDatabase(conn), dumpFile);
mustSystem(cmd);
unlink(dumpFile);
freeMem(dumpFile);
}

static void chainCreateTable(struct sqlConnection *conn, char* table,
                             boolean inclNormScore)
/* Create a chain table.  This code is copied from hgLoadChain.  Chains have
 * different file and table formats and the table format and the table format
 * doesn't have good autoSql support.
 */
{
static char *createTemplate = 
    "CREATE TABLE %s (\n"
    "  bin smallint unsigned not null,\n"
    "  score double not null,\n"
    "  tName varchar(255) not null,\n"
    "  tSize int unsigned not null, \n"
    "  tStart int unsigned not null,\n"
    "  tEnd int unsigned not null,\n"
    "  qName varchar(255) not null,\n"
    "  qSize int unsigned not null,\n"
    "  qStrand char(1) not null,\n"
    "  qStart int unsigned not null,\n"
    "  qEnd int unsigned not null,\n"
    "  id int unsigned not null,\n"
    "  %s"
    "  INDEX(tName(%d),bin),\n"
    "  INDEX(id)\n"
    ")\n";
char createSql[1024];
int minIndexLen = hGetMinIndexLength(sqlGetDatabase(conn));
safef(createSql, sizeof(createSql), createTemplate, table,
      (inclNormScore ? "normScore double not null,\n" : ""),
      minIndexLen);
sqlRemakeTable(conn, table, createSql);
}
    
static void chainMergeTables(struct sqlConnection *conn, struct splitTableInfo* sti)
/* merge chain tables */
{
verifyHasBin(conn, sti);
boolean inclNormScore = hasField(conn, sti->tables->name, "normScore");
char *dumpFile = getTmpDump(sti->rootName);
dumpTables(conn, sti, dumpFile, dumpRowAsIs);
chainCreateTable(conn, getNewTableTmpName(sti->rootName), inclNormScore);
sqlLoadTabFile(conn, dumpFile, getNewTableTmpName(sti->rootName), SQL_TAB_FILE_CONCURRENT);
unlink(dumpFile);
freeMem(dumpFile);
}

static void chainLinkCreateTable(struct sqlConnection *conn, char* table)
/* Create a chain link table.  This code is copied from hgLoadChain.
 */
{
static char *createTemplate = 
    "CREATE TABLE %s (\n"
    "  bin smallint unsigned not null,\n"
    "  tName varchar(255) not null,\n"
    "  tStart int unsigned not null,\n"
    "  tEnd int unsigned not null,\n"
    "  qStart int unsigned not null,\n"
    "  chainId int unsigned not null,\n"
    "  INDEX(tName(%d),bin),\n"
    "  INDEX(chainId)\n"
    ")\n";
char createSql[1024];
int minIndexLen = hGetMinIndexLength(sqlGetDatabase(conn));
safef(createSql, sizeof(createSql), createTemplate, table, minIndexLen);
sqlRemakeTable(conn, table, createSql);
}
    
static struct splitTableInfo* createChainLinkSplitTableInfo(struct splitTableInfo *chainSti)
/* get a splitTableInfo object for a chainLink table that parallels a chain table */
{
struct splitTableInfo *sti;
AllocVar(sti);
char linkTable[256];
safef(linkTable, sizeof(linkTable), "%sLink", chainSti->rootName);
sti->rootName = cloneString(linkTable);
struct slName *table;
for (table = chainSti->tables; table != NULL; table = table->next)
    {
    safef(linkTable, sizeof(linkTable), "%sLink", table->name);
    slAddHead(&sti->tables, slNameNew(linkTable));
    }
return sti;
}

static void chainLinkMergeTables(struct sqlConnection *conn, struct splitTableInfo* chainSti)
/* merge chainLink tables */
{
struct splitTableInfo* sti = createChainLinkSplitTableInfo(chainSti);
verifyHasBin(conn, sti);
char *dumpFile = getTmpDump(sti->rootName);
dumpTables(conn, sti, dumpFile, dumpRowAsIs);
chainLinkCreateTable(conn, getNewTableTmpName(sti->rootName));
sqlLoadTabFile(conn, dumpFile, getNewTableTmpName(sti->rootName), SQL_TAB_FILE_CONCURRENT);
unlink(dumpFile);
freeMem(dumpFile);
// just leak sti
}

static void goldCreateTable(struct sqlConnection *conn, char* table)
/* Create a gold table.  This code is copied from hgGoldGapGl.  Gold tables
 * are load from AGP files.
 */
{
static char *createTemplate = 
    "CREATE TABLE %s (\n"
    "   bin smallint not null,"
    "   chrom varchar(255) not null,        # which chromosome\n"
    "   chromStart int unsigned not null,   # start position in chromosome\n"
    "   chromEnd int unsigned not null,     # end position in chromosome\n"
    "   ix int not null,    # ix of this fragment (useless)\n"
    "   type char(1) not null,      # (P)redraft, (D)raft, (F)inished or (O)ther\n"
    "   frag varchar(255) not null, # which fragment\n"
    "   fragStart int unsigned not null,    # start position in frag\n"
    "   fragEnd int unsigned not null,      # end position in frag\n"
    "   strand char(1) not null,    # + or - (orientation of fragment)\n"
    "   INDEX(chrom(%d),bin),\n"
    "   UNIQUE(chrom(%d),chromStart),\n"
    "   INDEX(frag(%d)));\n";
char createSql[1024];
int minIndexLen = hGetMinIndexLength(sqlGetDatabase(conn));
safef(createSql, sizeof(createSql), createTemplate, table,
      minIndexLen, minIndexLen, minIndexLen);
sqlRemakeTable(conn, table, createSql);
}
    
static void goldMergeTables(struct sqlConnection *conn, struct splitTableInfo* sti)
/* merge gold tables */
{
verifyHasBin(conn, sti);
char *dumpFile = getTmpDump(sti->rootName);
dumpTables(conn, sti, dumpFile, dumpRowAsIs);
goldCreateTable(conn, getNewTableTmpName(sti->rootName));
sqlLoadTabFile(conn, dumpFile, getNewTableTmpName(sti->rootName), SQL_TAB_FILE_CONCURRENT);
unlink(dumpFile);
freeMem(dumpFile);
}

static void glCreateTable(struct sqlConnection *conn, char* table)
/* Create a gl table.  This code is copied from hgGoldGapGl.  Gl tables
 * are load from AGP files.
 */
{
/* Native table assume being split and has no chrom.  Add chrom at the
 * end so autoSql continues to work. */
static char *createTemplate = 
    "CREATE TABLE %s (\n"
    "    bin smallint not null,\n"
    "    frag varchar(255) not null,	# Fragment name\n"
    "    start int unsigned not null,	# Start position in golden path\n"
    "    end int unsigned not null,	# End position in golden path\n"
    "    strand char(1) not null,	# + or - for strand\n"
    "    chrom varchar(255) not null,\n"
    "   INDEX(chrom(%d),bin),\n"
    "   INDEX(frag));\n";
char createSql[1024];
int minIndexLen = hGetMinIndexLength(sqlGetDatabase(conn));
safef(createSql, sizeof(createSql), createTemplate, table,
      minIndexLen);
sqlRemakeTable(conn, table, createSql);
}
    
static struct splitTableInfo* createGlSplitTableInfo(struct splitTableInfo *goldSti)
/* get a splitTableInfo object for a gl table that parallels a gold table */
{
struct splitTableInfo *sti;
AllocVar(sti);
char glTable[256];
sti->rootName = cloneString("gl");
struct slName *table;
for (table = goldSti->tables; table != NULL; table = table->next)
    {
    // copy all but _gold
    safef(glTable, sizeof(glTable), "%.*s_gl", (int)strlen(table->name)-5, table->name);
    slAddHead(&sti->tables, slNameNew(glTable));
    }
return sti;
}

static void dumpGlRow(char **row, int numColumns, char *chrom, int offset, FILE* dumpFh)
/* dump a gl row, adding chrom. */
{
int i;
for (i = 0; i < numColumns; i++)
    {
    if (i != 0)
        fputc('\t', dumpFh);
    fputs(row[i], dumpFh);
    }
fputc('\t', dumpFh);
fputs(chrom, dumpFh);
fputc('\n',dumpFh);
}

static void glMergeTables(struct sqlConnection *conn, struct splitTableInfo* goldSti)
/* merge gl tables */
{
// not all assembles with gold have gl tables
struct splitTableInfo* sti = createGlSplitTableInfo(goldSti);
if (countExistingTables(conn, sti->tables))
    {
    verifyHasBin(conn, sti);
    char *dumpFile = getTmpDump(sti->rootName);
    dumpTables(conn, sti, dumpFile, dumpGlRow);
    glCreateTable(conn, getNewTableTmpName(sti->rootName));
    sqlLoadTabFile(conn, dumpFile, getNewTableTmpName(sti->rootName), SQL_TAB_FILE_CONCURRENT);
    unlink(dumpFile);
    freeMem(dumpFile);
    }
}

static void gapCreateTable(struct sqlConnection *conn, char* table)
/* Create a gap table.  This code is copied from hgGoldGapGl.  Gap tables
 * are load from AGP files.
 */
{
static char *createTemplate = 
    "CREATE TABLE %s (\n"
    "   bin smallint not null,"
    "   chrom varchar(255) not null,	# which chromosome\n"
    "   chromStart int unsigned not null,	# start position in chromosome\n"
    "   chromEnd int unsigned not null,	# end position in chromosome\n"
    "   ix int not null,	# ix of this fragment (useless)\n"
    "   n char(1) not null,	# always 'N'\n"
    "   size int unsigned not null,	# size of gap\n"
    "   type varchar(255) not null,	# contig, clone, fragment, etc.\n"
    "   bridge varchar(255) not null,	# yes, no, mrna, bacEndPair, etc.\n"
    "             #Indices\n"
    "   INDEX(chrom(%d),bin),\n"
    "   UNIQUE(chrom(%d),chromStart)\n"
    ")\n";

char createSql[1024];
int minIndexLen = hGetMinIndexLength(sqlGetDatabase(conn));
safef(createSql, sizeof(createSql), createTemplate, table, minIndexLen, minIndexLen);
sqlRemakeTable(conn, table, createSql);
}
    
static void gapMergeTables(struct sqlConnection *conn, struct splitTableInfo* sti)
/* merge gap tables */
{
verifyHasBin(conn, sti);
char *dumpFile = getTmpDump(sti->rootName);
dumpTables(conn, sti, dumpFile, dumpRowAsIs);
gapCreateTable(conn, getNewTableTmpName(sti->rootName));
sqlLoadTabFile(conn, dumpFile, getNewTableTmpName(sti->rootName), SQL_TAB_FILE_CONCURRENT);
unlink(dumpFile);
freeMem(dumpFile);
}

static void rmskCreateTable(struct sqlConnection *conn, char* table)
/* Create a rmsk table.  This code is copied from hgLoadOut.  Rmsk tables
 * are load from repeat master out files.
 */
{
static char *createTemplate = 
    "CREATE TABLE %s (\n"
    "   bin smallint unsigned not null,     # bin index field for range queries\n"
    "   swScore int unsigned not null,	# Smith Waterman alignment score\n"
    "   milliDiv int unsigned not null,	# Base mismatches in parts per thousand\n"
    "   milliDel int unsigned not null,	# Bases deleted in parts per thousand\n"
    "   milliIns int unsigned not null,	# Bases inserted in parts per thousand\n"
    "   genoName varchar(255) not null,	# Genomic sequence name\n"
    "   genoStart int unsigned not null,	# Start in genomic sequence\n"
    "   genoEnd int unsigned not null,	# End in genomic sequence\n"
    "   genoLeft int not null,		# -#bases after match in genomic sequence\n"
    "   strand char(1) not null,		# Relative orientation + or -\n"
    "   repName varchar(255) not null,	# Name of repeat\n"
    "   repClass varchar(255) not null,	# Class of repeat\n"
    "   repFamily varchar(255) not null,	# Family of repeat\n"
    "   repStart int not null,		# Start (if strand is +) or -#bases after match (if strand is -) in repeat sequence\n"
    "   repEnd int not null,		# End in repeat sequence\n"
    "   repLeft int not null,		# -#bases after match (if strand is +) or start (if strand is -) in repeat sequence\n"
    "   id char(1) not null,		# First digit of id field in RepeatMasker .out file.  Best ignored.\n"
    "             #Indices\n"
    "INDEX(genoName(%d),bin))\n";
char createSql[4096];
int minIndexLen = hGetMinIndexLength(sqlGetDatabase(conn));
safef(createSql, sizeof(createSql), createTemplate, table, minIndexLen);
sqlRemakeTable(conn, table, createSql);
}
    
static void rmskMergeTables(struct sqlConnection *conn, struct splitTableInfo* sti)
/* merge rmsk tables */
{
verifyHasBin(conn, sti);
char *dumpFile = getTmpDump(sti->rootName);
dumpTables(conn, sti, dumpFile, dumpRowAsIs);
rmskCreateTable(conn, getNewTableTmpName(sti->rootName));
sqlLoadTabFile(conn, dumpFile, getNewTableTmpName(sti->rootName), SQL_TAB_FILE_CONCURRENT);
unlink(dumpFile);
freeMem(dumpFile);
}

static void sampleCreateTable(struct sqlConnection *conn, char* splitTable, char* table)
/* Create a sample table.  Oddly, the only split sample table hg15.hg15Mm3L
 * has the correct indexes (see hgLoadSample.c) for a single table, so
 * we use the same schema.
 */
{
char createSql[4096];
safef(createSql, sizeof(createSql), "create table %s like %s", table, splitTable);
sqlRemakeTable(conn, table, createSql);
}
    
static void sampleMergeTables(struct sqlConnection *conn, struct splitTableInfo* sti)
/* merge sample tables */
{
verifyHasBin(conn, sti);
char *dumpFile = getTmpDump(sti->rootName);
dumpTables(conn, sti, dumpFile, dumpRowAsIs);
sampleCreateTable(conn, sti->tables->name, getNewTableTmpName(sti->rootName));
sqlLoadTabFile(conn, dumpFile, getNewTableTmpName(sti->rootName), SQL_TAB_FILE_CONCURRENT);
unlink(dumpFile);
freeMem(dumpFile);
}
static void mergeSplitTable(struct sqlConnection *conn, struct splitTableInfo* sti)
/* merger on split table */
{
verbose(1, "merge table \"%s\" type \"%s\"\n", sti->rootName, sti->trackDbType);
if (startsWithWord("psl", sti->trackDbType))
    pslMergeTables(conn, sti);
else if (startsWithWord("chain", sti->trackDbType))
    {
    chainMergeTables(conn, sti);
    chainLinkMergeTables(conn, sti);
    }
else if (sameString("gold", sti->rootName))
    {
    goldMergeTables(conn, sti);
    glMergeTables(conn, sti);
    }
else if (sameString("gap", sti->rootName))
    gapMergeTables(conn, sti);
else if (startsWithWord("rmsk", sti->trackDbType))
    rmskMergeTables(conn, sti);
else if (startsWithWord("sample", sti->trackDbType))
    sampleMergeTables(conn, sti);
else
    errAbort("don't know how to merger table \"%s\" type \"%s\"", sti->rootName, sti->trackDbType);
}

static void mergeSplitTables(struct sqlConnection *conn)
/* merge all split tables. */
{
struct splitTableInfo* splitTableInfos = getSplitTables(conn);
struct splitTableInfo* sti;
for (sti = splitTableInfos; sti != NULL; sti = sti->next)
    if (!shouldSkipWithMessage(sti))
        mergeSplitTable(conn, sti);
}


static struct dyString *createTableRename(void)
/* create SQL rename table command */
{
struct dyString *sql = dyStringNew(512);
dyStringAppend(sql, "RENAME TABLE ");
return sql;
}

static void addOldTableRename(struct dyString *sql, struct slName *tables,
                              boolean isFirst)
/* add renames of old tables to SQL rename table command */
{
struct slName *table;
for (table = tables; table != NULL; table = table->next)
    {
    if (!isFirst)
        dyStringAppend(sql, ", ");
    dyStringPrintf(sql, " %s to %s", table->name, getOldTableTmpName(table->name));
    isFirst = FALSE;
    }
}

static void addNewTableRename(struct dyString *sql, char *table)
/* add renames of new table to SQL rename table command */
{
dyStringPrintf(sql, ", %s to %s", getNewTableTmpName(table), table);
}

static void installMergedTable(struct sqlConnection *conn, struct splitTableInfo* sti)
/* replace a split with a merge table. */
{
verbose(1, "install table \"%s\" type \"%s\"\n", sti->rootName, sti->trackDbType);
struct dyString *sql = createTableRename();
addOldTableRename(sql, sti->tables, TRUE);
addNewTableRename(sql, sti->rootName);
if (startsWithWord("chain", sti->trackDbType))
    {
    struct splitTableInfo* chainLinkSti = createChainLinkSplitTableInfo(sti);
    addOldTableRename(sql, chainLinkSti->tables, FALSE);
    addNewTableRename(sql, chainLinkSti->rootName);
    }
else if (sameString("gold", sti->rootName))
    {
    // not all assembles with gold have gl tables
    struct splitTableInfo* glSti = createGlSplitTableInfo(sti);
    if (countExistingTables(conn, glSti->tables) > 0)
        {
        addOldTableRename(sql, glSti->tables, FALSE);
        addNewTableRename(sql, glSti->rootName);
        }
    }
sqlUpdate(conn, dyStringContents(sql));
dyStringFree(&sql);
}

static struct slName *getSplitGenbankTables(struct sqlConnection *conn,
                                            char *type)
/* get the list of split *_mrna or *_est tables */
{
struct slName *tables = NULL;
struct slName *chroms = hChromList(sqlGetDatabase(conn));
struct slName *chrom;

for (chrom = chroms; chrom != NULL; chrom = chrom->next)
    {
    char table[256];
    safef(table, sizeof(table), "%s_%s", chrom->name, type);
    if (sqlTableExists(conn, table))
        slAddHead(&tables, slNameNew(table));
    }
slFreeList(&chroms);
return tables;
}

static void renameSplitGenbankTables(struct sqlConnection *conn,
                                     char *type,
                                     struct slName *tables)
/* rename a list split genbank tables to add old extension */
{
verbose(1, "hide %d split GENBANK tables \"*_%s\" \n", slCount(tables), type);
struct dyString *sql = createTableRename();
addOldTableRename(sql, tables, TRUE);
sqlUpdate(conn, dyStringContents(sql));
dyStringFree(&sql);
}

static void hideSplitGenbankTables(struct sqlConnection *conn)
/* GENBANK has both split and unsplit versions on older assemblies.
 * They don't show up in the normal list of split tables. These
 * are renamed and then dropped.
 */
{
static char *types[] = {"mrna", "est", NULL};
int i;
for (i = 0; types[i] != NULL; i++)
    {
    struct slName *tables = getSplitGenbankTables(conn, types[i]);
    if (tables != NULL)
        {
        renameSplitGenbankTables(conn, types[i], tables);
        slFreeList(&tables);
        }
    }
}

static void installMergedTables(struct sqlConnection *conn)
/* replace split with merge tables. */
{
struct splitTableInfo* splitTableInfos = getSplitTables(conn);
struct splitTableInfo* sti;
for (sti = splitTableInfos; sti != NULL; sti = sti->next)
    {
    if (!shouldSkipWithMessage(sti))
        installMergedTable(conn, sti);
    }
hideSplitGenbankTables(conn);
}

static void cleanupSplitTables(struct sqlConnection *conn)
/* replace split with merge tables. */
{
char tableLike[512];
safef(tableLike, sizeof(tableLike), "%%\\%s", OLD_TABLE_EXT);
struct slName *oldSplitTables = sqlListTablesLike(conn, tableLike);
struct slName *oldSplitTable;
for (oldSplitTable = oldSplitTables; oldSplitTable != NULL; oldSplitTable = oldSplitTable->next)
    {
    verbose(1, "cleanup split table \"%s\" \n", oldSplitTable->name);
    sqlDropTable(conn, oldSplitTable->name);
    }
}

static void hgMergeSplitTables(char *db, char *action)
/* hgMergeSplitTables - Merge old-stype split tables into single tables in
 * atomic manner suitable for a running browser. */
{
struct sqlConnection *conn = sqlConnect(db);
if (sameString(action, "list"))
    listSplitTables(conn, stdout);
else if (sameString(action, "merge"))
    mergeSplitTables(conn);
else if (sameString(action, "install"))
    installMergedTables(conn);
else if (sameString(action, "cleanup"))
    cleanupSplitTables(conn);
else
    errAbort("invalid action: %s", action);
// exit() will clean up leaked memory
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
onlyThisTable = optionVal("table", NULL);
hgMergeSplitTables(argv[1], argv[2]);
return 0;
}
