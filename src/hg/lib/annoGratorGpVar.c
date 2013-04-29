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
    boolean cdsOnly;		// if TRUE, restrict output to CDS effects
    struct lm *lm;		// localmem scratch storage
    struct dyString *dyScratch;	// dyString for local temporary use

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
// if cdsOnly and gpFx is not in CDS, return NULL;
{
if (self->cdsOnly &&
    ! (effect->detailType == codingChange && effect->soNumber != synonymous_variant))
    return NULL;
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

struct dnaSeq *genePredToGenomicSequence(struct genePred *pred, struct twoBitFile *tbf,
					 struct lm *lm)
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
    struct dnaSeq *block = twoBitReadSeqFragLower(tbf, pred->chrom, pred->exonStarts[i],
						  pred->exonEnds[i]);
    memcpy(seq+offset, block->dna, sizeof(*seq) * block->size);
    offset += (pred->exonEnds[i] - pred->exonStarts[i]);
    dnaSeqFree(&block);
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

static struct annoRow *aggvGenRows( struct annoGratorGpVar *self, struct variant *variant,
				    struct genePred *pred, struct annoRow *inRow,
				    struct lm *callerLm)
// put out annoRows for all the gpFx that arise from variant and pred
{
struct annoGrator *gSelf = &(self->grator);
struct annoStreamer *sSelf = &(gSelf->streamer);
struct dnaSeq *transcriptSequence = genePredToGenomicSequence(pred, sSelf->assembly->tbf,
							      self->lm);
struct gpFx *effects = gpFxPredEffect(variant, pred, transcriptSequence, self->lm);
struct annoRow *rows = NULL;

for(; effects; effects = effects->next)
    {
    struct annoRow *row = aggvEffectToRow(self, effects, inRow, callerLm);
    if (row != NULL)
	slAddHead(&rows, row);
    }
slReverse(&rows);

return rows;
}

static struct variant *variantFromPgSnpRow(struct annoGratorGpVar *self, struct annoRow *row)
/* Translate pgSnp array of words into variant. */
{
struct pgSnp pgSnp;
pgSnpStaticLoad(row->data, &pgSnp);
return variantFromPgSnp(&pgSnp, self->lm);
}

static struct variant *variantFromVcfRow(struct annoGratorGpVar *self, struct annoRow *row)
/* Translate pgSnp array of words into variant. */
{
char *alStr = vcfGetSlashSepAllelesFromWords(row->data, self->dyScratch);
unsigned alCount = chopByChar(alStr, '/', NULL, 0);
return variantNew(row->chrom, row->start, row->end, alCount, alStr, self->lm);
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
    if (self->cdsOnly && retRJFilterFailed)
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
if (self->cdsOnly && outRows == NULL && retRJFilterFailed != NULL)
    *retRJFilterFailed = TRUE;
return outRows;
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
sSelf->setAutoSqlObject(sSelf, annoGpVarAsObj(mySource->asObj));
sSelf->close = aggvClose;
// integrate by adding gpFx fields
gSelf->integrate = annoGratorGpVarIntegrate;
self->dyScratch = dyStringNew(0);
self->cdsOnly = cdsOnly;

return gSelf;
}
