/* annoGratorGpVar -- integrate pgSNP or VCF with gene pred and make gpFx predictions */

#include "annoGratorGpVar.h"
#include "genePred.h"
#include "pgSnp.h"
#include "vcf.h"
#include "variant.h"
#include "gpFx.h"
#include "twoBit.h"
#include "annoGratorQuery.h"

struct annoGratorGpVar
    {
    struct annoGrator grator;	// external annoGrator/annoStreamer interface
    struct lm *lm;		// localmem scratch storage
    struct dyString *dyScratch;	// dyString for local temporary use
    struct dnaSeq *curChromSeq;	// sequence cache, to avoid repeated calls to twoBitReadSeqFrag
    struct annoGratorGpVarFuncFilter *funcFilter; // Which categories of effect should we output?
    enum annoGratorOverlap gpVarOverlapRule;	  // Should we set RJFail if no overlap?

    struct variant *(*variantFromRow)(struct annoGratorGpVar *self, struct annoRow *row);
    // Translate row from whatever format it is (pgSnp or VCF) into generic variant.
    };


static char *aggvAutoSqlStringStart =
"table genePredWithSO"
"\"genePred with Sequence Ontology annotation\""
"(";

static char *aggvAutoSqlStringEnd =
"string  allele;             \"Allele used to predict functional effect\""
"string  transcript;         \"ID of affected transcript\""
"uint    soNumber;           \"Sequence Ontology Number \""
"uint    detailType;         \"gpFx detail type (1=codingChange, 2=nonCodingExon, 3=intron)\""
"string  detail0;            \"detail column (per detailType) 0\""
"string  detail1;            \"detail column (per detailType) 1\""
"string  detail2;            \"detail column (per detailType) 2\""
"string  detail3;            \"detail column (per detailType) 3\""
"string  detail4;            \"detail column (per detailType) 4\""
"string  detail5;            \"detail column (per detailType) 5\""
"string  detail6;            \"detail column (per detailType) 6\""
"string  detail7;            \"detail column (per detailType) 7\""
")";

struct asObject *annoGpVarAsObj(struct asObject *sourceAsObj)
// Return asObject describing fields of internal source plus the fields we add here.
{
struct dyString *gpPlusGpFx = dyStringCreate(aggvAutoSqlStringStart);
// Translate each column back into autoSql text for combining with new output fields:
struct asColumn *col;
for (col = sourceAsObj->columnList;  col != NULL;  col = col->next)
    {
    if (col->fixedSize)
	dyStringPrintf(gpPlusGpFx, "%s[%d]\t%s;\t\"%s\"",
		       col->lowType->name, col->fixedSize, col->name, col->comment);
    else if (col->isArray || col->isList)
	{
	if (col->linkedSizeName)
	    dyStringPrintf(gpPlusGpFx, "%s[%s]\t%s;\t\"%s\"",
			   col->lowType->name, col->linkedSizeName, col->name, col->comment);
	else
	    errAbort("Neither col->fixedSize nor col->linkedSizeName given for "
		     "asObj %s column '%s'",
		     sourceAsObj->name, col->name);
	}
    else
	dyStringPrintf(gpPlusGpFx, "%s\t%s;\t\"%s\"", col->lowType->name, col->name, col->comment);
    }
dyStringAppend(gpPlusGpFx, aggvAutoSqlStringEnd);
struct asObject *asObj = asParseText(gpPlusGpFx->string);
dyStringFree(&gpPlusGpFx);
return asObj;
}

static boolean passesFilter(struct annoGratorGpVar *self, struct gpFx *gpFx)
/* Based on type of effect, should we print this one? */
{
struct annoGratorGpVarFuncFilter *filt = self->funcFilter;
enum soTerm term = gpFx->soNumber;
if (filt->intron && (term == intron_variant || term == complex_transcript_variant))
    return TRUE;
if (filt->upDownstream && (term == upstream_gene_variant || term == downstream_gene_variant))
    return TRUE;
if (filt->utr && (term == _5_prime_UTR_variant || term == _3_prime_UTR_variant))
    return TRUE;
if (filt->cdsSyn && term == synonymous_variant)
    return TRUE;
if (filt->cdsNonSyn && term != synonymous_variant
    && (gpFx->detailType == codingChange || term == complex_transcript_variant))
    return TRUE;
if (filt->nonCodingExon && term == non_coding_exon_variant)
    return TRUE;
if (filt->splice && (term == splice_donor_variant || term == splice_acceptor_variant ||
		     term == splice_region_variant))
    return TRUE;
return FALSE;
}

static char *blankIfNull(char *input)
{
if (input == NULL)
    return "";

return input;
}

static char *uintToString(struct lm *lm, uint num)
{
char buffer[10];

safef(buffer,sizeof buffer, "%d", num);
return lmCloneString(lm, buffer);
}

static void aggvStringifyGpFx(char **words, struct gpFx *effect, struct lm *lm)
// turn gpFx structure into array of words
{
int count = 0;

words[count++] = lmCloneString(lm, effect->allele);
words[count++] = lmCloneString(lm, blankIfNull(effect->transcript));
words[count++] = uintToString(lm, effect->soNumber);
words[count++] = uintToString(lm, effect->detailType);
int gpFxNumCols = 4;

if (effect->detailType == intron)
    words[count++] = uintToString(lm, effect->details.intron.intronNumber);
else if (effect->detailType == nonCodingExon)
    {
    words[count++] = uintToString(lm, effect->details.nonCodingExon.exonNumber);
    words[count++] = uintToString(lm, effect->details.nonCodingExon.cDnaPosition);
    }
else if (effect->detailType == codingChange)
    {
    struct codingChange *cc = &(effect->details.codingChange);
    words[count++] = uintToString(lm, cc->exonNumber);
    words[count++] = uintToString(lm, cc->cDnaPosition);
    words[count++] = uintToString(lm, cc->cdsPosition);
    words[count++] = uintToString(lm, cc->pepPosition);
    words[count++] = strUpper(lmCloneString(lm, blankIfNull(cc->aaOld)));
    words[count++] = strUpper(lmCloneString(lm, blankIfNull(cc->aaNew)));
    words[count++] = strUpper(lmCloneString(lm, blankIfNull(cc->codonOld)));
    words[count++] = strUpper(lmCloneString(lm, blankIfNull(cc->codonNew)));
    }
else if (effect->detailType != none)
    errAbort("annoGratorGpVar: unknown effect type %d", effect->detailType);

// Add max number of columns added in any if clause above
gpFxNumCols += 8;

while (count < gpFxNumCols)
    words[count++] = "";
}

struct gpFx *annoGratorGpVarGpFxFromRow(struct annoStreamer *sSelf, struct annoRow *row,
					struct lm *lm)
// Turn the string array representation back into a real gpFx.
// I know this is inefficient and am thinking about a better way.
{
if (row == NULL)
    return NULL;
struct gpFx *effect;
lmAllocVar(lm, effect);
struct annoGrator *gSelf = (struct annoGrator *)sSelf;
// get gpFx words which follow internal source's words:
char **words = (char **)(row->data);
int count = gSelf->mySource->numCols;

effect->allele = lmCloneString(lm, words[count++]);
effect->transcript = lmCloneString(lm, words[count++]);
effect->soNumber = atol(words[count++]);
effect->detailType = atol(words[count++]);

if (effect->detailType == intron)
    effect->details.intron.intronNumber = atol(words[count++]);
else if (effect->detailType == nonCodingExon)
    {
    effect->details.nonCodingExon.exonNumber = atol(words[count++]);
    effect->details.nonCodingExon.cDnaPosition = atol(words[count++]);
    }
else if (effect->detailType == codingChange)
    {
    effect->details.codingChange.exonNumber = atol(words[count++]);
    effect->details.codingChange.cDnaPosition = atol(words[count++]);
    effect->details.codingChange.cdsPosition = atol(words[count++]);
    effect->details.codingChange.pepPosition = atol(words[count++]);
    effect->details.codingChange.aaOld = lmCloneString(lm, words[count++]);
    effect->details.codingChange.aaNew = lmCloneString(lm, words[count++]);
    effect->details.codingChange.codonOld = lmCloneString(lm, words[count++]);
    effect->details.codingChange.codonNew = lmCloneString(lm, words[count++]);
    }
else if (effect->detailType != none)
    errAbort("annoGratorGpVar: unknown effect type %d", effect->detailType);
return effect;
}

static struct annoRow *aggvEffectToRow(struct annoGratorGpVar *self, struct gpFx *effect,
				       struct annoRow *rowIn, struct lm *callerLm)
// convert a single genePred annoRow and gpFx record to an augmented genePred annoRow;
{
struct annoGrator *gSelf = &(self->grator);
struct annoStreamer *sSelf = &(gSelf->streamer);
assert(sSelf->numCols > gSelf->mySource->numCols);

char **wordsOut;
lmAllocArray(self->lm, wordsOut, sSelf->numCols);

// copy the genePred fields over
int gpColCount = gSelf->mySource->numCols;
char **wordsIn = (char **)rowIn->data;
memcpy(wordsOut, wordsIn, sizeof(char *) * gpColCount);

// stringify the gpFx structure 
aggvStringifyGpFx(&wordsOut[gpColCount], effect, callerLm);

return annoRowFromStringArray(rowIn->chrom, rowIn->start, rowIn->end, rowIn->rightJoinFail,
			      wordsOut, sSelf->numCols, callerLm);
}

struct dnaSeq *genePredToGenomicSequence(struct genePred *pred, char *chromSeq, struct lm *lm)
/* Return concatenated genomic sequence of exons of pred. */
{
int txLen = 0;
int i;
for (i=0; i < pred->exonCount; i++)
    txLen += (pred->exonEnds[i] - pred->exonStarts[i]);
char *seq = lmAlloc(lm, txLen + 1);
int offset = 0;
for (i=0; i < pred->exonCount; i++)
    {
    int blockStart = pred->exonStarts[i];
    int blockSize = pred->exonEnds[i] - blockStart;
    memcpy(seq+offset, chromSeq+blockStart, blockSize*sizeof(*seq));
    offset += blockSize;
    }
if(pred->strand[0] == '-')
    reverseComplement(seq, txLen);
struct dnaSeq *txSeq = NULL;
lmAllocVar(lm, txSeq);
txSeq->name = lmCloneString(lm, pred->name);
txSeq->dna = seq;
txSeq->size = txLen;
return txSeq;
}

char *variantToGenomicSequence(struct variant *variant, char *chromSeq, struct lm *lm)
/* Return variant's reference allele. */
{
return lmCloneStringZ(lm, chromSeq+variant->chromStart, (variant->chromEnd - variant->chromStart));
}

static struct annoRow *aggvGenRows( struct annoGratorGpVar *self, struct variant *variant,
				    struct genePred *pred, struct annoRow *inRow,
				    struct lm *callerLm)
// put out annoRows for all the gpFx that arise from variant and pred
{
if (self->curChromSeq == NULL || differentString(self->curChromSeq->name, pred->chrom))
    {
    dnaSeqFree(&self->curChromSeq);
    struct twoBitFile *tbf = self->grator.streamer.assembly->tbf;
    self->curChromSeq = twoBitReadSeqFragLower(tbf, pred->chrom, 0, 0);
    }
// TODO Performance improvement: instead of creating the transcript sequence for each
// variant that intersects the transcript, cache transcript sequence; possibly
// an slPair with a concatenation of {chrom, txStart, txEnd, cdsStart, cdsEnd,
// exonStarts, exonEnds} as the name, and sequence as the val.  When something in
// the list is no longer in the list of rows from the internal annoGratorIntegrate call,
// drop it.
struct dnaSeq *transcriptSequence = genePredToGenomicSequence(pred, self->curChromSeq->dna,
							      self->lm);
char *refAllele = variantToGenomicSequence(variant, self->curChromSeq->dna, self->lm);
struct gpFx *effects = gpFxPredEffect(variant, pred, refAllele, transcriptSequence, self->lm);
struct annoRow *rows = NULL;

for(; effects; effects = effects->next)
    {
    // Intergenic variants never pass through here so we don't have to filter them out
    // here if self->funcFilter is null.
    if (self->funcFilter == NULL || passesFilter(self, effects))
	{
	struct annoRow *row = aggvEffectToRow(self, effects, inRow, callerLm);
	slAddHead(&rows, row);
	}
    }
slReverse(&rows);

return rows;
}

struct annoRow *aggvIntergenicRow(struct annoGratorGpVar *self, struct annoStreamRows *primaryData,
				  boolean *retRJFilterFailed, struct lm *callerLm)
/* If intergenic variants (no overlapping or nearby genes) are to be included in output,
 * make an output row with empty genePred and a gpFx that is empty except for soNumber. */
{
struct annoGrator *gSelf = &(self->grator);
struct annoStreamer *sSelf = &(gSelf->streamer);
char **wordsOut;
lmAllocArray(self->lm, wordsOut, sSelf->numCols);
// Add empty strings for genePred string columns:
int gpColCount = gSelf->mySource->numCols;
int i;
for (i = 0;  i < gpColCount;  i++)
    wordsOut[i] = "";
struct gpFx *intergenicGpFx;
lmAllocVar(self->lm, intergenicGpFx);
intergenicGpFx->allele = "";
intergenicGpFx->soNumber = intergenic_variant;
intergenicGpFx->detailType = none;
aggvStringifyGpFx(&wordsOut[gpColCount], intergenicGpFx, self->lm);
struct annoRow *varRow = primaryData->rowList;
boolean rjFail = (retRJFilterFailed && *retRJFilterFailed);
return annoRowFromStringArray(varRow->chrom, varRow->start, varRow->end, rjFail,
			      wordsOut, sSelf->numCols, callerLm);
}

static struct variant *variantFromPgSnpRow(struct annoGratorGpVar *self, struct annoRow *row)
/* Translate pgSnp array of words into variant. */
{
struct pgSnp pgSnp;
pgSnpStaticLoad(row->data, &pgSnp);
return variantFromPgSnp(&pgSnp, self->lm);
}

static struct variant *variantFromVcfRow(struct annoGratorGpVar *self, struct annoRow *row)
/* Translate vcf array of words into variant. */
{
boolean skippedFirstBase = FALSE;
char *alStr = vcfGetSlashSepAllelesFromWords(row->data, self->dyScratch, &skippedFirstBase);
unsigned alCount = chopByChar(alStr, '/', NULL, 0);
return variantNew(row->chrom, row->start+skippedFirstBase, row->end, alCount, alStr, self->lm);
}

static void setVariantFromRow(struct annoGratorGpVar *self, struct annoStreamRows *primaryData)
/* Depending on autoSql definition of primary source, choose a function to translate
 * incoming rows into generic variants. */
{
if (asObjectsMatch(primaryData->streamer->asObj, pgSnpAsObj()))
    self->variantFromRow = variantFromPgSnpRow;
else if (asObjectsMatch(primaryData->streamer->asObj, vcfAsObj()))
    self->variantFromRow = variantFromVcfRow;
}

struct annoRow *annoGratorGpVarIntegrate(struct annoGrator *gSelf,
					 struct annoStreamRows *primaryData,
					 boolean *retRJFilterFailed, struct lm *callerLm)
// integrate a variant and a genePred, generate as many rows as
// needed to capture all the changes
{
struct annoGratorGpVar *self = (struct annoGratorGpVar *)gSelf;
lmCleanup(&(self->lm));
self->lm = lmInit(0);
// Temporarily tweak primaryRow's start and end to find upstream/downstream overlap:
struct annoRow *primaryRow = primaryData->rowList;
int pStart = primaryRow->start, pEnd = primaryRow->end;
primaryRow->start -= GPRANGE;
if (primaryRow->start < 0)
    primaryRow->start = 0;
primaryRow->end += GPRANGE;
struct annoRow *rows = annoGratorIntegrate(gSelf, primaryData, retRJFilterFailed, self->lm);
primaryRow->start = pStart;
primaryRow->end = pEnd;

if (rows == NULL)
    {
    // No genePreds means that the primary variant is intergenic.  By default we don't
    // include those, but if funcFilter->intergenic has been set then we do.
    if (self->funcFilter != NULL && self->funcFilter->intergenic)
	return aggvIntergenicRow(self, primaryData, retRJFilterFailed, callerLm);
    else if (retRJFilterFailed && self->gpVarOverlapRule == agoMustOverlap)
	*retRJFilterFailed = TRUE;
    return NULL;
    }
if (retRJFilterFailed && *retRJFilterFailed)
    return NULL;

if (self->variantFromRow == NULL)
    setVariantFromRow(self, primaryData);
struct variant *variant = self->variantFromRow(self, primaryRow);
struct annoRow *outRows = NULL;

for(; rows; rows = rows->next)
    {
    char **inWords = rows->data;

    // work around genePredLoad's trashing its input
    char *saveExonStarts = lmCloneString(self->lm, inWords[8]);
    char *saveExonEnds = lmCloneString(self->lm, inWords[9]);
    struct genePred *gp = genePredLoad(inWords);
    inWords[8] = saveExonStarts;
    inWords[9] = saveExonEnds;

    struct annoRow *outRow = aggvGenRows(self, variant, gp, rows, callerLm);
    if (outRow != NULL)
	{
	slReverse(&outRow);
	outRows = slCat(outRow, outRows);
	}
    genePredFree(&gp);
    }
slReverse(&outRows);
// If all rows failed the filter, and we must overlap, set *retRJFilterFailed.
if (outRows == NULL && retRJFilterFailed && self->gpVarOverlapRule == agoMustOverlap)
    *retRJFilterFailed = TRUE;
return outRows;
}

static void aggvSetOverlapRule(struct annoGrator *gSelf, enum annoGratorOverlap rule)
/* We need an overlap rule that is independent of the genePred integration because
 * if we're including intergenic variants, then we return a row even though no genePred
 * rows overlap. */
{
struct annoGratorGpVar *self = (struct annoGratorGpVar *)gSelf;
self->gpVarOverlapRule = rule;
// For mustOverlap, if we're including intergenic variants in output, don't let simple
// integration set retRjFilterFailed:
if (rule == agoMustOverlap && self->funcFilter != NULL && self->funcFilter->intergenic)
    gSelf->overlapRule = agoNoConstraint;
else
    gSelf->overlapRule = rule;
}

void aggvClose(struct annoStreamer **pSSelf)
/* Close out localmem and all the usual annoGrator stuff. */
{
if (*pSSelf != NULL)
    {
    struct annoGratorGpVar *self = (struct annoGratorGpVar *)(*pSSelf);
    lmCleanup(&(self->lm));
    dyStringFree(&(self->dyScratch));
    annoGratorClose(pSSelf);
    }
}

struct annoGrator *annoGratorGpVarNew(struct annoStreamer *mySource)
/* Make a subclass of annoGrator that combines genePreds from mySource with
 * pgSnp rows from primary source to predict functional effects of variants
 * on genes.
 * mySource becomes property of the new annoGrator (don't close it, close annoGrator). */
{
struct annoGratorGpVar *self;
AllocVar(self);
struct annoGrator *gSelf = &(self->grator);
annoGratorInit(gSelf, mySource);
struct annoStreamer *sSelf = &(gSelf->streamer);
// We add columns beyond what comes from mySource, so update our public-facing asObject:
sSelf->setAutoSqlObject(sSelf, annoGpVarAsObj(mySource->asObj));
gSelf->setOverlapRule = aggvSetOverlapRule;
sSelf->close = aggvClose;
// integrate by adding gpFx fields
gSelf->integrate = annoGratorGpVarIntegrate;
self->dyScratch = dyStringNew(0);
return gSelf;
}

void annoGratorGpVarSetFuncFilter(struct annoGrator *gSelf,
				  struct annoGratorGpVarFuncFilter *funcFilter)
/* If funcFilter is non-NULL, it specifies which functional categories
 * to include in output; if NULL, by default intergenic variants are excluded and
 * all other categories are included.
 * NOTE: After calling this, call gpVar->setOverlapRule() because implementation
 * of that depends on filter settings.  */
{
struct annoGratorGpVar *self = (struct annoGratorGpVar *)gSelf;
freez(&self->funcFilter);
if (funcFilter != NULL)
    self->funcFilter = CloneVar(funcFilter);
// Since our overlapRule behavior depends on filter settings, reevaluate:
aggvSetOverlapRule(gSelf, self->gpVarOverlapRule);
}

