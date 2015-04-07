/* annoStreamDbKnownGene -- knownGene with kgXref.geneSymbol added as an extra field */

#include "annoStreamDbKnownGene.h"
#include "annoStreamDb.h"
#include "hdb.h"
#include "sqlNum.h"

static char *askgAutoSqlString =
"table knownGenePlusSymbol\n"
"\"Fields of the knownGene table plus symbolic gene name from kgXref.geneSymbol\"\n"
"   ("
"    string  name;               \"Name of gene\"\n"
"    string  chrom;              \"Reference sequence chromosome or scaffold\"\n"
"    char[1] strand;             \"+ or - for strand\"\n"
"    uint    txStart;            \"Transcription start position\"\n"
"    uint    txEnd;              \"Transcription end position\"\n"
"    uint    cdsStart;           \"Coding region start\"\n"
"    uint    cdsEnd;             \"Coding region end\"\n"
"    uint    exonCount;          \"Number of exons\"\n"
"    uint[exonCount] exonStarts; \"Exon start positions\"\n"
"    uint[exonCount] exonEnds;   \"Exon end positions\"\n"
"    string  proteinID;          \"UniProt display ID for Known Genes,  UniProt accession or RefSeq protein ID for UCSC Genes\" \n"
"    string  alignID;            \"Unique identifier for each (known gene, alignment position) pair\"\n"
"    string geneSymbol;          \"HGNC gene symbol\"\n"
"   )\n";

#define KNOWNGENEPLUS_NUM_COLS 13

struct annoStreamDbKnownGene
{
    struct annoStreamer streamer;	// Parent class members & methods (external interface)
    // Private members
    struct annoStreamer *mySource;	// Internal source of knownGene rows
    // Data from related table kgXref
    struct hash *geneSymbols;
};

struct asObject *annoStreamDbKnownGeneAsObj()
/* Return an autoSql object that describs fields of a joining query on knownGene and
 * kgXref.geneSymbol. */
{
return asParseText(askgAutoSqlString);
}

// It would be nice for this to go in a knownGene.[ch], but to avoid having to add
// two new files, just add what we need here:
static char *kgAutoSqlString =
"table knownGene\n"
"\"Genes based on RefSeq, GenBank, and UniProt.\"\n"
"(\n"
"    string  name;               \"Name of gene\"\n"
"    string  chrom;              \"Reference sequence chromosome or scaffold\"\n"
"    char[1] strand;             \"+ or - for strand\"\n"
"    uint    txStart;            \"Transcription start position\"\n"
"    uint    txEnd;              \"Transcription end position\"\n"
"    uint    cdsStart;           \"Coding region start\"\n"
"    uint    cdsEnd;             \"Coding region end\"\n"
"    uint    exonCount;          \"Number of exons\"\n"
"    uint[exonCount] exonStarts; \"Exon start positions\"\n"
"    uint[exonCount] exonEnds;   \"Exon end positions\"\n"
"    string  proteinID;          \"UniProt display ID for Known Genes,  UniProt accession or RefSeq protein ID for UCSC Genes\" \n"
"    string  alignID;            \"Unique identifier for each (known gene, alignment position) pair\"\n"
")\n";

struct asObject *knownGeneAsObj()
/* Return an autoSql object for knownGene. */
{
return asParseText(kgAutoSqlString);
}

#define KNOWNGENE_NUM_COLS 12

static void askgSetAutoSqlObject(struct annoStreamer *self, struct asObject *asObj)
/* Abort if something external tries to change the autoSql object. */
{
errAbort("annoStreamDbKnownGene %s: can't change autoSqlObject.",
	 ((struct annoStreamer *)self)->name);
}

static void askgSetRegion(struct annoStreamer *sSelf, char *chrom, uint rStart, uint rEnd)
/* Pass setRegion down to internal source. */
{
annoStreamerSetRegion(sSelf, chrom, rStart, rEnd);
struct annoStreamDbKnownGene *self = (struct annoStreamDbKnownGene *)sSelf;
self->mySource->setRegion(self->mySource, chrom, rStart, rEnd);
}

static char *getGeneSymbol(struct annoStreamDbKnownGene *self, char *kgID, struct lm *lm)
/* Look up kgID in our geneSymbols hash from kgXref. */
{
char *symbol = hashFindVal(self->geneSymbols, kgID);
if (symbol == NULL)
    symbol = "";
return lmCloneString(lm, symbol);
}

static void knownGeneToKnownGenePlus(struct annoStreamDbKnownGene *self,
                                     char **kgWords, char **kgpWords, struct lm *lm)
/* Copy kgWords into kgpWords and add column geneSymbol. */
{
CopyArray(kgWords, kgpWords, KNOWNGENE_NUM_COLS);
char *kgID = kgWords[0];
kgpWords[KNOWNGENE_NUM_COLS] = getGeneSymbol(self, kgID, lm);
}

static struct annoRow *askgNextRow(struct annoStreamer *sSelf, char *minChrom, uint minEnd,
				    struct lm *lm)
/* Join kgXref.geneSymbol with row from knownGene track table. */
{
struct annoStreamDbKnownGene *self = (struct annoStreamDbKnownGene *)sSelf;
char **kgpWords;
lmAllocArray(lm, kgpWords, KNOWNGENEPLUS_NUM_COLS);
struct annoRow *kgRow;
boolean rightJoinFail = FALSE;
kgRow = self->mySource->nextRow(self->mySource, minChrom, minEnd, lm);
if (kgRow != NULL)
    {
    char **kgWords = kgRow->data;
    knownGeneToKnownGenePlus(self, kgWords, kgpWords, lm);
    return annoRowFromStringArray(kgRow->chrom, kgRow->start, kgRow->end, rightJoinFail,
				  kgpWords, KNOWNGENEPLUS_NUM_COLS, lm);
    }
else
    return NULL;
}

static void getGeneSymbols(struct annoStreamDbKnownGene *self, char *db)
/* Read in kgXref's columns kgID and geneSymbol; hash ids to symbols for joining later. */
{
struct sqlConnection *conn = hAllocConn(db);
struct dyString *query = sqlDyStringCreate("select kgID, geneSymbol from kgXref");
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(self->geneSymbols, row[0], cloneString(row[1]));
sqlFreeResult(&sr);
hFreeConn(&conn);
}

static void askgClose(struct annoStreamer **pSSelf)
/* Close internal annoStreamer for knownGene, free geneSymbols hash and close self. */
{
if (pSSelf == NULL)
    return;
struct annoStreamDbKnownGene *self = *(struct annoStreamDbKnownGene **)pSSelf;
self->mySource->close(&(self->mySource));
freeHashAndVals(&self->geneSymbols);
annoStreamerFree(pSSelf);
}

struct annoStreamer *annoStreamDbKnownGeneNew(char *db, struct annoAssembly *aa, int maxOutRows)
/* Create an annoStreamer (subclass) object using two database tables:
 * knownGene: the UCSC Genes main track table
 * kgXref: the related table that contains the HGNC gene symbol that everyone wants to see
 * This streamer's rows are just like a plain annoStreamDb on knownGene, but with an
 * extra column at the end, 'geneSymbol', which is recognized as a gene symbol column due to
 * its use in refGene.
 */
{
struct annoStreamDbKnownGene *self;
AllocVar(self);
struct annoStreamer *streamer = &(self->streamer);
// Set up external streamer interface
annoStreamerInit(streamer, aa, annoStreamDbKnownGeneAsObj(), "knownGene");
streamer->rowType = arWords;
// Get internal streamer for knownGene
self->mySource = annoStreamDbNew(db, "knownGene", aa, knownGeneAsObj(), maxOutRows);
// Slurp in data from kgXref
self->geneSymbols = hashNew(7);
getGeneSymbols(self, db);
// Override methods that need to pass through to internal source:
streamer->setAutoSqlObject = askgSetAutoSqlObject;
streamer->setRegion = askgSetRegion;
streamer->nextRow = askgNextRow;
streamer->close = askgClose;

return (struct annoStreamer *)self;
}
