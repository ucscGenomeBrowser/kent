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
    };

// select p.*, c.cds, l.protAcc, l.name, e.path, s.file_offset, s.file_size 
//   from (((ncbiRefSeqPsl p join ncbiRefSeqCurated n on p.qName = n.name)
//          left join ncbiRefSeqCds c on p.qName = c.id)
//         join ncbiRefSeqLink l on p.qName = l.mrnaAcc)
//        left join (seqNcbiRefSeq s left join extNcbiRefSeq e on s.extFile = e.id) on p.qName = s.acc
//   where p.tName = "chr1"
//   order by p.tName, p.tStart
//  limit 5;

static char *ncbiRefSeqConfigJsonFormat =
    "{ \"naForMissing\": false,"
    "  \"rightJoinTable\": \"%s\","
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
/* Return an autoSql object with PSL, gene name, protein acc, CDS and sequence file info fields. */
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
lmAllocArray(lm, ppWords, PSLPLUS_NUM_COLS);
struct annoRow *ppRow;
boolean rightJoinFail = FALSE;
while ((ppRow = self->mySource->nextRow(self->mySource, minChrom, minEnd, lm)) != NULL)
    {
    ppWords = ppRow->data;
    // If there are filters on experiment attributes, apply them, otherwise just return aRow.
    if (sSelf->filters)
	{
	boolean fails = annoFilterRowFails(sSelf->filters, ppWords, PSLPLUS_NUM_COLS,
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
				  ppWords, PSLPLUS_NUM_COLS, lm);
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

struct annoStreamer *annoStreamDbPslPlusNew(struct annoAssembly *aa, char *gpTable, int maxOutRows)
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
struct annoStreamer *streamer = &(self->streamer);
// Set up external streamer interface
annoStreamerInit(streamer, aa, annoStreamDbPslPlusAsObj(), pslTable);
streamer->rowType = arWords;
self->gpTable = cloneString(gpTable);
// Get internal streamer for joining PSL with other tables.
struct jsonElement *configEl = jsonParse(configJson);
self->mySource = annoStreamDbNew(aa->name, pslTable, aa, maxOutRows, configEl);
// Override methods that need to pass through to internal source:
streamer->setRegion = asdppSetRegion;
streamer->nextRow = asdppNextRow;
streamer->close = asdppClose;
return (struct annoStreamer *)self;
}
