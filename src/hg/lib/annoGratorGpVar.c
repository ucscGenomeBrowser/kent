/* annoGratorGpVar -- integrate pgSNP with gene pred and make gpFx predictions */

#include "annoGratorGpVar.h"
#include "genePred.h"
#include "pgSnp.h"
#include "variant.h"
#include "gpFx.h"
#include "twoBit.h"
#include "annoGratorQuery.h"

static char *annoGpVarDataLineAutoSqlString =
"table genePredWithSO"
"\"genePred with Sequence Ontology annotation\""
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
"uint    soNumber;           \"Sequence Ontology Number \" "
"uint    soOther0;           \"Ancillary detail 0\""
"uint    soOther1;           \"Ancillary detail 1\""
"uint    soOther2;           \"Ancillary detail 2\""
"uint    soOther3;           \"Ancillary detail 3\""
"uint    soOther4;           \"Ancillary detail 4\""
"uint    soOther5;           \"Ancillary detail 5\""
"uint    soOther6;           \"Ancillary detail 6\""
")";

struct asObject *annoGpVarAsObj()
// Return asObject describing fields of genePred (at the moment, knownGene)
{
return asParseText(annoGpVarDataLineAutoSqlString);
}

static char *blankIfNull(char *input)
{
if (input == NULL)
    return "";

return input;
}

static char *uintToString(uint num)
{
char buffer[10];

safef(buffer,sizeof buffer, "%d", num);
return cloneString(buffer);
}

static void aggvStringifyGpFx(char **words, struct gpFx *effect)
// turn gpFx structure into a list of words
{
int count = 0;

words[count++] = uintToString(effect->so.soNumber);

switch(effect->so.soNumber)
    {
    case intron_variant:
	words[count++] = cloneString(effect->so.sub.intron.transcript);
	words[count++] = uintToString(effect->so.sub.intron.intronNumber);
	break;

    case inframe_deletion:
    case frameshift_variant:
    case synonymous_variant:
    case non_synonymous_variant:
	words[count++] = cloneString(effect->so.sub.codingChange.transcript);
	words[count++] = uintToString(effect->so.sub.codingChange.exonNumber);
	words[count++] = uintToString(effect->so.sub.codingChange.cDnaPosition);
	words[count++] = uintToString(effect->so.sub.codingChange.cdsPosition);
	words[count++] = uintToString(effect->so.sub.codingChange.pepPosition);
	words[count++] = cloneString(effect->so.sub.codingChange.aaChanges);
	words[count++] = cloneString(effect->so.sub.codingChange.codonChanges);
	break;

    default:
	// write out ancillary information
	words[count++] = blankIfNull(effect->so.sub.generic.soOther0);
	words[count++] = blankIfNull(effect->so.sub.generic.soOther1);
	words[count++] = blankIfNull(effect->so.sub.generic.soOther2);
	words[count++] = blankIfNull(effect->so.sub.generic.soOther3);
	words[count++] = blankIfNull(effect->so.sub.generic.soOther4);
	words[count++] = blankIfNull(effect->so.sub.generic.soOther5);
	words[count++] = blankIfNull(effect->so.sub.generic.soOther6);
	break;
    };

int needWords = sizeof(effect->so.sub.generic) / sizeof(char *) + 1;
while (count < needWords)
	words[count++] = "";
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

/* Get the sequence associated with a particular bed concatenated together. */
struct dnaSeq *twoBitSeqFromBed(struct twoBitFile *tbf, struct bed *bed)
{
struct dnaSeq *block = NULL;
struct dnaSeq *bedSeq = NULL;
int i = 0 ;
int size;
assert(bed);
/* Handle very simple beds and beds with blocks. */
if(bed->blockCount == 0)
    {
    bedSeq = twoBitReadSeqFragExt(tbf, bed->chrom, bed->chromStart, bed->chromEnd, FALSE, &size);
    freez(&bedSeq->name);
    bedSeq->name = cloneString(bed->name);
    }
else
    {
    int offSet = bed->chromStart;
    struct dyString *currentSeq = newDyString(2048);
    //hNibForChrom(db, bed->chrom, fileName);
    for(i=0; i<bed->blockCount; i++)
	{
	block = twoBitReadSeqFragExt(tbf, bed->chrom, 
	      offSet+bed->chromStarts[i], offSet+bed->chromStarts[i]+bed->blockSizes[i], FALSE, &size);
	dyStringAppendN(currentSeq, block->dna, block->size);
	dnaSeqFree(&block);
	}
    AllocVar(bedSeq);
    bedSeq->name = cloneString(bed->name);
    bedSeq->dna = cloneString(currentSeq->string);
    bedSeq->size = strlen(bedSeq->dna);
    dyStringFree(&currentSeq);
    }
if(bed->strand[0] == '-')
    reverseComplement(bedSeq->dna, bedSeq->size);
return bedSeq;
}

struct dnaSeq *genePredToGenomicSequence(struct genePred *pred, struct twoBitFile *tbf)
{
struct bed *bed = bedFromGenePred(pred);
struct dnaSeq *dnaSeq = twoBitSeqFromBed(tbf, bed);

return dnaSeq;
}


static struct annoRow *aggvGenRows( struct annoGrator *self,
    struct variant *variant, struct genePred *pred, struct annoRow *inRow)
// put out annoRows for all the gpFx that arise from variant and pred
{
// FIXME:  accessing query's tbf is probably bad
struct dnaSeq *transcriptSequence = genePredToGenomicSequence(pred, 
    self->streamer.query->tbf);
struct gpFx *effects = gpFxPredEffect(variant, pred, transcriptSequence);
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
struct variant *variant = variantFromPgSnp(pgSnp);
struct annoRow *outRows = NULL;

for(; rows; rows = rows->next)
    {
    char **inWords = rows->data;

    // work around genePredLoad's trashing its input
    char *saveExonStarts = cloneString(inWords[8]);
    char *saveExonEnds = cloneString(inWords[9]);
    struct genePred *gp = genePredLoad(inWords);
    inWords[8] = saveExonStarts;
    inWords[9] = saveExonEnds;

    struct annoRow *outRow = aggvGenRows(self, variant, gp, rows);
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
