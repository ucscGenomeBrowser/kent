/* annoGratorGpVar -- integrate pgSNP with gene pred and make gpFx predictions */

#include "annoGratorGpVar.h"
#include "genePred.h"
#include "pgSnp.h"
#include "variant.h"
#include "gpFx.h"
#include "twoBit.h"
#include "annoGratorQuery.h"

struct annoGratorGpVar
{
    struct annoGrator grator;	// external annoGrator/annoStreamer interface
    boolean cdsOnly;		// if TRUE, restrict output to CDS effects
};


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

static struct annoRow *aggvEffectToRow( struct annoGratorGpVar *self,
    struct gpFx *effect, struct annoRow *rowIn)
// convert a single gpFx record to an augmented genePred annoRow;
// if cdsOnly and gpFx is not in CDS, return NULL;
{
if (self->cdsOnly && !gpFxIsCodingChange(effect))
    return NULL;
char **wordsOut;
char **wordsIn = (char **)rowIn->data;
struct annoGrator *gSelf = &(self->grator);
struct annoStreamer *sSelf = &(gSelf->streamer);

assert(sSelf->numCols > gSelf->mySource->numCols);
AllocArray(wordsOut, sSelf->numCols);

// copy the genePred fields over
memcpy(wordsOut, wordsIn, sizeof(char *) * gSelf->mySource->numCols);

// stringify the gpFx structure 
int count = gSelf->mySource->numCols;
aggvStringifyGpFx(&wordsOut[count], effect);

return annoRowFromStringArray(rowIn->chrom, rowIn->start, rowIn->end, 
    rowIn->rightJoinFail, wordsOut, sSelf->numCols);
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


static struct annoRow *aggvGenRows( struct annoGratorGpVar *self,
    struct variant *variant, struct genePred *pred, struct annoRow *inRow)
// put out annoRows for all the gpFx that arise from variant and pred
{
struct annoGrator *gSelf = &(self->grator);
struct annoStreamer *sSelf = &(gSelf->streamer);
// FIXME:  accessing query's tbf is probably bad
struct dnaSeq *transcriptSequence = genePredToGenomicSequence(pred, sSelf->query->tbf);
struct gpFx *effects = gpFxPredEffect(variant, pred, transcriptSequence);
struct annoRow *rows = NULL;

for(; effects; effects = effects->next)
    {
    struct annoRow *row = aggvEffectToRow(self, effects, inRow);
    if (row != NULL)
	slAddHead(&rows, row);
    }
slReverse(&rows);

return rows;
}

struct annoRow *annoGratorGpVarIntegrate(struct annoGrator *gSelf,
	struct annoRow *primaryRow, boolean *retRJFilterFailed)
// integrate a pgSnp and a genePred, generate as many rows as
// needed to capture all the changes
{
struct annoGratorGpVar *self = (struct annoGratorGpVar *)gSelf;
struct annoRow *rows = annoGratorIntegrate(gSelf, primaryRow, retRJFilterFailed);

if (rows == NULL)
    {
    if (self->cdsOnly && retRJFilterFailed)
	*retRJFilterFailed = TRUE;
    return NULL;
    }
if (retRJFilterFailed && *retRJFilterFailed)
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
    if (outRow != NULL)
	{
	slReverse(&outRow);
	outRows = slCat(outRow, outRows);
	}
    }
slReverse(&outRows);
if (self->cdsOnly && outRows == NULL && retRJFilterFailed != NULL)
    *retRJFilterFailed = TRUE;
return outRows;
}


struct annoGrator *annoGratorGpVarNew(struct annoStreamer *mySource, boolean cdsOnly)
/* Make a subclass of annoGrator that combines genePreds from mySource with
 * pgSnp rows from primary source to predict functional effects of variants
 * on genes.  If cdsOnly is true, return only rows with effects on coding seq.
 * mySource becomes property of the new annoGrator. */
{
struct annoGratorGpVar *self;
AllocVar(self);
struct annoGrator *gSelf = &(self->grator);
annoGratorInit(gSelf, mySource);
struct annoStreamer *sSelf = &(gSelf->streamer);
// We add columns beyond what comes from mySource, so update our public-facing asObject:
sSelf->setAutoSqlObject(sSelf, annoGpVarAsObj());

// integrate by adding gpFx fields
gSelf->integrate = annoGratorGpVarIntegrate;
self->cdsOnly = cdsOnly;

return gSelf;
}
