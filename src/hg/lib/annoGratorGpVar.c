/* annoGratorGpVar -- integrate pgSNP with gene pred and make gpFx predictions */

#include "annoGratorGpVar.h"
#include "genePred.h"
#include "pgSnp.h"
#include "gpFx.h"

static char *annoGpVarDataLineAutoSqlString =
"table knownGeneGpVar"
"\"Genes based on RefSeq, GenBank, and UniProt.\""
"("
"string  name;               \"Name of gene\""
"string  chrom;              \"Reference sequence chromosome or scaffold\""
"char[1] strand;             \"+ or - for strand\""
"uint    txStart;            \"Transcription start position\""
"uint    txEnd;              \"Transcription end position\""
"uint    cdsStart;           \"Coding region start\""
"uint    cdsEnd;             \"Coding region end\""
"uint    exonCount;          \"Number of exons\""
"uint[exonCount] exonStarts; \"Exon start positions\""
"uint[exonCount] exonEnds;   \"Exon end positions\""
"string  proteinID;          \"UniProt display ID for Known Genes,  UniProt accession or RefSeq protein ID for UCSC Genes\" "
"string  alignID;            \"Unique identifier for each (known gene, alignment position) pair\""
"uint    gpFxType;           \"Effect type (see annoGratorGpVar.h)\""
"uint    gpFxNumber;         \"exon or intron number\""
"uint    gpFxTransOffset;    \"offset in transcript\""
"string  gpFxBaseChange;     \"base change in transcript\""
"uint    gpFxCodonChange;    \"codon triplet change in transcript\""
"uint    gpFxProteinOffset;  \"offset in protein\""
"uint    gpFxProteinChange;  \"peptide change in protein\""
")";

struct asObject *annoGpVarAsObj()
// Return asObject describing fields of genePred (at the moment, knownGene)
{
return asParseText(annoGpVarDataLineAutoSqlString);
}

static void aggvStringifyGpFx(char **words, struct gpFx *effect)
// turn gpFx structure into a list of words
{
int count = 0;
char buffer[10];

// gpFxType
safef(buffer,sizeof buffer, "%d", effect->gpFxType);
words[count++] = cloneString(buffer);

// gpFxNumber
safef(buffer,sizeof buffer, "%d", effect->gpFxNumber);
words[count++] = cloneString(buffer);

// gpFxTransOffset
safef(buffer,sizeof buffer, "%d", effect->gpFxTransOffset);
words[count++] = cloneString(buffer);

// gpFxBaseChange
if ( effect->gpFxBaseChange == NULL)
    words[count++] = "";
else
    words[count++] = effect->gpFxBaseChange;     

// gpFxCodonChange
safef(buffer,sizeof buffer, "%d", effect->gpFxCodonChange);
words[count++] = cloneString(buffer);

//  gpFxProteinOffset
safef(buffer,sizeof buffer, "%d", effect->gpFxProteinOffset);
words[count++] = cloneString(buffer);

//  gpFxProteinChange
safef(buffer,sizeof buffer, "%d", effect->gpFxProteinChange);
words[count++] = cloneString(buffer);
}

static struct annoRow *aggvEffectToRow( struct annoGrator *self,
    struct gpFx *effect, struct annoRow *rowIn)
// covert a single gpFx record to an augmented genePred annoRow
{
char **wordsOut;
char **wordsIn = (char **)rowIn->data;

assert(self->streamer.numCols > self->mySource->numCols);
AllocArray(wordsOut, self->streamer.numCols);

// copy the genePred fields over
memcpy(wordsOut, wordsIn, sizeof(char *) * self->mySource->numCols);

// stringify the gpFx structure 
int count = self->mySource->numCols;
aggvStringifyGpFx(&wordsOut[count], effect);

return annoRowFromStringArray(rowIn->chrom, rowIn->start, rowIn->end, 
    rowIn->rightJoinFail, wordsOut, self->streamer.numCols);
}

static struct annoRow *aggvGenRows( struct annoGrator *self,
    struct pgSnp *pgSnp, struct genePred *pred, struct annoRow *inRow)
// put out annoRows for all the gpFx that arise from pgSnp and pred
{
struct gpFx *effects = gpFxPredEffect(pgSnp, pred);
struct annoRow *rows = NULL;

for(; effects; effects = effects->next)
    {
    struct annoRow *row = aggvEffectToRow(self, effects, inRow);
    slAddHead(&rows, row);
    }
slReverse(&rows);

return rows;
}

struct annoRow *annoGratorGpVarIntegrate(struct annoGrator *self, 
	struct annoRow *primaryRow, boolean *retRJFilterFailed)
// integrate a pgSnp and a genePred, generate as many rows as
// needed to capture all the changes
{
struct annoRow *rows = annoGratorIntegrate(self, primaryRow, retRJFilterFailed);

if ((rows == NULL) || (retRJFilterFailed && *retRJFilterFailed))
    return NULL;

char **primaryWords = primaryRow->data;
struct pgSnp *pgSnp = pgSnpLoad(primaryWords);
struct annoRow *outRows = NULL;

for(; rows; rows = rows->next)
    {
    char **inWords = rows->data;
    struct genePred *gp = genePredLoad(inWords);
    struct annoRow *outRow = aggvGenRows(self, pgSnp, gp, rows);
    slAddHead(&outRows, outRow);
    }

return outRows;
}

/* Given a single row from the primary source, get all overlapping rows from internal
 * source, and produce joined output rows.  If retRJFilterFailed is non-NULL and any
 * overlapping row has a rightJoin filter failure (see annoFilter.h),
 * set retRJFilterFailed and stop. */

struct annoGrator *annoGratorGpVarNew(struct annoStreamer *mySource)
// make a subclass of annoGrator that knows about genePreds and pgSnp
{
struct annoGrator *self = annoGratorNew(mySource);

// integrate by adding gpFx fields
self->integrate = annoGratorGpVarIntegrate;

// TODO: overriding these should be a single function call
self->streamer.asObj =  annoGpVarAsObj();
self->streamer.numCols = slCount(self->streamer.asObj->columnList);
self->streamer.columns = annoColumnsFromAsObject(self->streamer.asObj);

return self;
}
