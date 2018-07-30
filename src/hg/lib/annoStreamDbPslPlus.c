/* annoStreamDbPslPlus -- subclass of annoStreamer for joining PSL+CDS+seq database tables */

/* Copyright (C) 2017 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoStreamDbPslPlus.h"
#include "annoStreamDb.h"
#include "hdb.h"

static char *pslPlusAutoSqlString =
"table pslPlus"
"\"transcript PSL, CDS and seq info\""
"   ("
"    uint    match;      \"Number of bases that match that aren't repeats\""
"    uint    misMatch;   \"Number of bases that don't match\""
"    uint    repMatch;   \"Number of bases that match but are part of repeats\""
"    uint    nCount;       \"Number of 'N' bases\""
"    uint    qNumInsert;   \"Number of inserts in query (transcript)\""
"    int     qBaseInsert;  \"Number of bases inserted in query (transcript)\""
"    uint    tNumInsert;   \"Number of inserts in target (chromosome/scaffold)\""
"    int     tBaseInsert;  \"Number of bases inserted in target (chromosome/scaffold)\""
"    char[2] strand;       \"+ or - for query strand (transcript to genome orientation)\""
"    string  qName;        \"Transcript accession\""
"    uint    qSize;        \"Transcript sequence size\""
"    uint    qStart;       \"Alignment start position in query (transcript)\""
"    uint    qEnd;         \"Alignment end position in query (transcript)\""
"    string  tName;        \"Target (chromosome/scaffold) name\""
"    uint    tSize;        \"Target (chromosome/scaffold) size\""
"    uint    tStart;       \"Alignment start position in target\""
"    uint    tEnd;         \"Alignment end position in target\""
"    uint    blockCount;   \"Number of blocks in alignment\""
"    uint[blockCount] blockSizes;  \"Size of each block\""
"    uint[blockCount] qStarts;     \"Start of each block in query.\""
"    uint[blockCount] tStarts;     \"Start of each block in target.\""
"    string  cds;          \"CDS start and end in transcript (if applicable)\""
"    string  protAcc;      \"Protein accession (if applicable)\""
"    string  name2;        \"Gene symbolic name\""
"    string  path;         \"Path to FASTA file containing transcript sequence\""
"    uint    fileOffset;   \"Offset of transcript record in FASTA file\""
"    uint    fileSize;     \"Number of bytes of transcript record in FASTA file\""
"   )";

struct annoStreamDbPslPlus
    {
    struct annoStreamer streamer;	// Parent class members & methods (external interface)
    // Private members
    char *gpTable;                      // Associated genePred (refGene, ncbiRefSeqCurated etc)
    struct annoStreamer *mySource;	// Internal source of PSL+CDS+seq info
    struct hash *idHash;		// Used to restrict PSL query result to curated/predicted
    };

// select p.*, c.cds, l.protAcc, l.name, e.path, s.file_offset, s.file_size 
//   from (((ncbiRefSeqPsl p // NOT ANYMORE (#21770): join ncbiRefSeqCurated n on p.qName = n.name)
//          left join ncbiRefSeqCds c on p.qName = c.id)
//         join ncbiRefSeqLink l on p.qName = l.mrnaAcc)
//        left join (seqNcbiRefSeq s left join extNcbiRefSeq e on s.extFile = e.id) on p.qName = s.acc
//   where p.tName = "chr1"
//   order by p.tName, p.tStart
//  limit 5;

static char *ncbiRefSeqConfigJsonFormat =
    "{ \"naForMissing\": false,"
    "  \"relatedTables\": [ { \"table\": \"ncbiRefSeqCds\","
    "                         \"fields\": [\"cds\"] },"
    "                       { \"table\": \"ncbiRefSeqLink\","
    "                         \"fields\": [\"protAcc\", \"name\"] },"
    "                       { \"table\": \"extNcbiRefSeq\","
    "                         \"fields\": [\"path\"] },"
    "                       { \"table\": \"seqNcbiRefSeq\","
    "                         \"fields\": [\"file_offset\", \"file_size\"] } ] }";

//select p.*,c.name,l.protAcc,l.name,e.path,s.file_offset,s.file_size, i.version
//from refSeqAli p
//  join (hgFixed.gbCdnaInfo i
//        left join hgFixed.cds c on i.cds = c.id) on i.acc = p.qName
//       left join (hgFixed.gbSeq s
//                  join hgFixed.gbExtFile e on e.id = s.gbExtFile) on s.acc = p.qName
//       join hgFixed.refLink l on p.qName = l.mrnaAcc
//  where p.tName = "chr1"
//   order by p.tName, p.tStart
//  limit 5;

static char *refSeqAliConfigJson =
    "{ \"naForMissing\": false,"
    "  \"relatedTables\": [ { \"table\": \"hgFixed.cds\","
    "                         \"fields\": [\"name\"] },"
    "                       { \"table\": \"hgFixed.refLink\","
    "                         \"fields\": [\"protAcc\", \"name\"] },"
    "                       { \"table\": \"hgFixed.gbExtFile\","
    "                         \"fields\": [\"path\"] },"
    "                       { \"table\": \"hgFixed.gbSeq\","
    "                         \"fields\": [\"file_offset\", \"file_size\"] } ] }";

struct asObject *annoStreamDbPslPlusAsObj()
/* Return an autoSql object with PSL, gene name, protein acc, CDS and sequence file info fields.
 * An annoStreamDbPslPlus instance may return additional additional columns if configured, but
 * these columns will always be present. */
{
return asParseText(pslPlusAutoSqlString);
}

static void asdppSetRegion(struct annoStreamer *sSelf, char *chrom, uint rStart, uint rEnd)
/* Pass setRegion down to internal source. */
{
annoStreamerSetRegion(sSelf, chrom, rStart, rEnd);
struct annoStreamDbPslPlus *self = (struct annoStreamDbPslPlus *)sSelf;
self->mySource->setRegion(self->mySource, chrom, rStart, rEnd);
}

static struct annoRow *asdppNextRow(struct annoStreamer *sSelf, char *minChrom, uint minEnd,
				    struct lm *lm)
/* Return next psl+ row. */
{
struct annoStreamDbPslPlus *self = (struct annoStreamDbPslPlus *)sSelf;
char **ppWords;
lmAllocArray(lm, ppWords, sSelf->numCols);
struct annoRow *ppRow;
boolean rightJoinFail = FALSE;
while ((ppRow = self->mySource->nextRow(self->mySource, minChrom, minEnd, lm)) != NULL)
    {
    ppWords = ppRow->data;
    // If self->idHash is non-NULL, check PSL qName; skip this row if qName not found.
    char *qName = ppWords[9];
    if (self->idHash && ! hashLookup(self->idHash, qName))
        continue;
    // If there are filters, apply them, otherwise just return aRow.
    if (sSelf->filters)
	{
	boolean fails = annoFilterRowFails(sSelf->filters, ppWords, sSelf->numCols,
					   &rightJoinFail);
	// If this row passes the filter, or fails but is rightJoin, then we're done looking.
	if (!fails || rightJoinFail)
	    break;
	}
    else
	// no filtering to do, just use this row
	break;
    }
if (ppRow != NULL)
    return annoRowFromStringArray(ppRow->chrom, ppRow->start, ppRow->end, rightJoinFail,
				  ppWords, sSelf->numCols, lm);
else
    return NULL;
}

static void asdppClose(struct annoStreamer **pSSelf)
/* Free up state. */
{
if (pSSelf == NULL)
    return;
struct annoStreamDbPslPlus *self = *(struct annoStreamDbPslPlus **)pSSelf;
freez(&self->gpTable);
self->mySource->close(&(self->mySource));
annoStreamerFree(pSSelf);
}

static struct asColumn *asColumnClone(struct asColumn *colIn)
/* Return a full clone of colIn, or NULL if colIn is NULL. */
{
if (colIn == NULL)
    return NULL;
if (colIn->obType != NULL || colIn->index != NULL)
    errAbort("asColumnClone: support for obType and index not implemented");
struct asColumn *colOut;
AllocVar(colOut);
colOut->name = cloneString(colIn->name);
colOut->comment = cloneString(colIn->comment);
colOut->lowType = colIn->lowType; // static struct in asParse.c
colOut->obName = cloneString(colIn->obName);
colOut->fixedSize = colIn->fixedSize;
colOut->linkedSizeName = cloneString(colIn->linkedSizeName);
colOut->linkedSize = asColumnClone(colIn->linkedSize);
colOut->isSizeLink = colIn->isSizeLink;
colOut->isList = colIn->isList;
colOut->isArray = colIn->isArray;
colOut->autoIncrement = colIn->autoIncrement;
colOut->values = slNameCloneList(colIn->values);
return colOut;
}

static void asObjAppendExtraColumns(struct asObject *asObjTarget, struct asObject *asObjSource)
/* If asObjSource has more columns than asObjTarget then clone and append those additional columns
 * to asObjTarget. */
{
int tColCount = slCount(asObjTarget->columnList);
int sColCount = slCount(asObjSource->columnList);
if (tColCount < 1)
    errAbort("asObjAppendExtraColumns: support for empty target columnList not implemented");
if (sColCount > tColCount)
    {
    struct asColumn *tCol = asObjTarget->columnList, *sCol = asObjSource->columnList;
    int i;
    for (i = 0;  i < tColCount-1;  i++)
        {
        tCol = tCol->next;
        sCol = sCol->next;
        }
    while (sCol->next != NULL)
        {
        tCol->next = asColumnClone(sCol->next);
        tCol = tCol->next;
        sCol = sCol->next;
        }
    }
}

struct annoStreamer *annoStreamDbPslPlusNew(struct annoAssembly *aa, char *gpTable, int maxOutRows,
                                            struct jsonElement *extraConfig)
/* Create an annoStreamer (subclass) object that streams PSL, CDS and seqFile info.
 * gpTable is a genePred table that has associated PSL, CDS and sequence info
 * (i.e. refGene, ncbiRefSeq, ncbiRefSeqCurated or ncbiRefSeqPredicted). */
{
char *pslTable = NULL, *configJson = NULL;
if (sameString("refGene", gpTable))
    {
    pslTable = "refSeqAli";
    configJson = refSeqAliConfigJson;
    }
else if (startsWith("ncbiRefSeq", gpTable))
    {
    pslTable = "ncbiRefSeqPsl";
    struct dyString *dy = dyStringCreate(ncbiRefSeqConfigJsonFormat, gpTable);
    configJson = dyStringCannibalize(&dy);
    }
else
    errAbort("annoStreamDbPslPlusNew: unrecognized table \"%s\"", gpTable);
struct annoStreamDbPslPlus *self;
AllocVar(self);
// Get internal streamer for joining PSL with other tables.
struct jsonElement *config = jsonParse(configJson);
if (extraConfig)
    jsonObjectMerge(config, extraConfig);
self->mySource = annoStreamDbNew(aa->name, pslTable, aa, maxOutRows, config);
struct asObject *asObj = annoStreamDbPslPlusAsObj();
if (extraConfig)
    asObjAppendExtraColumns(asObj, self->mySource->asObj);
if (startsWith("ncbiRefSeq", gpTable) && differentString("ncbiRefSeq", gpTable))
    {
    // Load up an ID hash to restrict PSL query results to the subset in gpTable:
    struct sqlConnection *conn = hAllocConn(aa->name);
    char query[1024];
    sqlSafef(query, sizeof(query), "select name, 1 from %s", gpTable);
    self->idHash = sqlQuickHash(conn, query);
    hFreeConn(&conn);
    }
// Set up external streamer interface
struct annoStreamer *streamer = &(self->streamer);
annoStreamerInit(streamer, aa, asObj, pslTable);
streamer->rowType = arWords;
self->gpTable = cloneString(gpTable);
// Override methods that need to pass through to internal source:
streamer->setRegion = asdppSetRegion;
streamer->nextRow = asdppNextRow;
streamer->close = asdppClose;
return (struct annoStreamer *)self;
}
