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
    struct lm *lm;		// localmem scratch storage
};


static char *aggvAutoSqlStringStart =
"table genePredWithSO"
"\"genePred with Sequence Ontology annotation\""
"(";

static char *aggvAutoSqlStringEnd =
"string  allele;             \"Allele used to predict functional effect\""
"string  transcript;         \"ID of affected transcript\""
"uint    soNumber;           \"Sequence Ontology Number \" "
"string  soOther0;           \"Ancillary detail 0\""
"string  soOther1;           \"Ancillary detail 1\""
"string  soOther2;           \"Ancillary detail 2\""
"string  soOther3;           \"Ancillary detail 3\""
"string  soOther4;           \"Ancillary detail 4\""
"string  soOther5;           \"Ancillary detail 5\""
"string  soOther6;           \"Ancillary detail 6\""
"string  soOther7;           \"Ancillary detail 7\""
")";

struct asObject *annoGpVarAsObj(struct asObject *sourceAsObj)
// Return asObject describing fields of internal source plus the fields we add here.
{
struct dyString *gpPlusGpFx = dyStringCreate(aggvAutoSqlStringStart);
// Translate each column back into autoSql text for combining with new output fields:
struct asColumn *col;
for (col = sourceAsObj->columnList;  col != NULL;  col = col->next)
    {
    if (col->isArray || col->isList)
	{
	if (col->fixedSize)
	    dyStringPrintf(gpPlusGpFx, "%s[%d]\t%s;\t\"%s\"",
			   col->lowType->name, col->fixedSize, col->name, col->comment);
	else if (col->linkedSizeName)
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
// turn gpFx structure into a list of words
{
int count = 0;

words[count++] = lmCloneString(lm, effect->allele);
words[count++] = lmCloneString(lm, blankIfNull(effect->so.transcript));
words[count++] = uintToString(lm, effect->so.soNumber);

struct codingChange *cc = NULL;
switch(effect->so.soNumber)
    {
    case intron_variant:
	words[count++] = uintToString(lm, effect->so.sub.intron.intronNumber);
	break;

    case inframe_deletion:
    case inframe_insertion:
    case frameshift_variant:
    case synonymous_variant:
    case missense_variant:
    case stop_gained:
	cc = &(effect->so.sub.codingChange);
	words[count++] = uintToString(lm, cc->exonNumber);
	words[count++] = uintToString(lm, cc->cDnaPosition);
	words[count++] = uintToString(lm, cc->cdsPosition);
	words[count++] = uintToString(lm, cc->pepPosition);
	words[count++] = strUpper(lmCloneString(lm, blankIfNull(cc->aaOld)));
	words[count++] = strUpper(lmCloneString(lm, blankIfNull(cc->aaNew)));
	words[count++] = strUpper(lmCloneString(lm, blankIfNull(cc->codonOld)));
	words[count++] = strUpper(lmCloneString(lm, blankIfNull(cc->codonNew)));
	break;

    default:
	// write out ancillary information
	words[count++] = lmCloneString(lm, blankIfNull(effect->so.sub.generic.soOther0));
	words[count++] = lmCloneString(lm, blankIfNull(effect->so.sub.generic.soOther1));
	words[count++] = lmCloneString(lm, blankIfNull(effect->so.sub.generic.soOther2));
	words[count++] = lmCloneString(lm, blankIfNull(effect->so.sub.generic.soOther3));
	words[count++] = lmCloneString(lm, blankIfNull(effect->so.sub.generic.soOther4));
	words[count++] = lmCloneString(lm, blankIfNull(effect->so.sub.generic.soOther5));
	words[count++] = lmCloneString(lm, blankIfNull(effect->so.sub.generic.soOther6));
	words[count++] = lmCloneString(lm, blankIfNull(effect->so.sub.generic.soOther7));
	break;
    };

int needWords = sizeof(effect->so.sub.generic) / sizeof(char *) + 1;
while (count < needWords)
    words[count++] = lmCloneString(lm, "");
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
effect->so.transcript = lmCloneString(lm, words[count++]);
effect->so.soNumber = atol(words[count++]);
switch(effect->so.soNumber)
    {
    case intron_variant:
	effect->so.sub.intron.intronNumber = atol(words[count++]);
	break;

    case inframe_deletion:
    case inframe_insertion:
    case frameshift_variant:
    case synonymous_variant:
    case missense_variant:
    case stop_gained:
	effect->so.sub.codingChange.exonNumber = atol(words[count++]);
	effect->so.sub.codingChange.cDnaPosition = atol(words[count++]);
	effect->so.sub.codingChange.cdsPosition = atol(words[count++]);
	effect->so.sub.codingChange.pepPosition = atol(words[count++]);
	effect->so.sub.codingChange.aaOld = lmCloneString(lm, words[count++]);
	effect->so.sub.codingChange.aaNew = lmCloneString(lm, words[count++]);
	effect->so.sub.codingChange.codonOld = lmCloneString(lm, words[count++]);
	effect->so.sub.codingChange.codonNew = lmCloneString(lm, words[count++]);
	break;

    default:
	effect->so.sub.generic.soOther0 = lmCloneString(lm, words[count++]);
	effect->so.sub.generic.soOther1 = lmCloneString(lm, words[count++]);
	effect->so.sub.generic.soOther2 = lmCloneString(lm, words[count++]);
	effect->so.sub.generic.soOther3 = lmCloneString(lm, words[count++]);
	effect->so.sub.generic.soOther4 = lmCloneString(lm, words[count++]);
	effect->so.sub.generic.soOther5 = lmCloneString(lm, words[count++]);
	effect->so.sub.generic.soOther6 = lmCloneString(lm, words[count++]);
	effect->so.sub.generic.soOther7 = lmCloneString(lm, words[count++]);
	break;
    }
return effect;
}

static struct annoRow *aggvEffectToRow(struct annoGratorGpVar *self, struct gpFx *effect,
				       struct annoRow *rowIn, struct lm *callerLm)
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
lmAllocArray(self->lm, wordsOut, sSelf->numCols);

// copy the genePred fields over
memcpy(wordsOut, wordsIn, sizeof(char *) * gSelf->mySource->numCols);

// stringify the gpFx structure 
int count = gSelf->mySource->numCols;
aggvStringifyGpFx(&wordsOut[count], effect, callerLm);

return annoRowFromStringArray(rowIn->chrom, rowIn->start, rowIn->end, rowIn->rightJoinFail,
			      wordsOut, sSelf->numCols, callerLm);
}

/* Get the sequence associated with a particular bed concatenated together. */
struct dnaSeq *twoBitSeqFromBed(struct twoBitFile *tbf, struct bed *bed, struct lm *lm)
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
    bedSeq->name = lmCloneString(lm, bed->name);
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
    bedSeq->name = lmCloneString(lm, bed->name);
    bedSeq->dna = lmCloneString(lm, currentSeq->string);
    bedSeq->size = strlen(bedSeq->dna);
    dyStringFree(&currentSeq);
    }
if(bed->strand[0] == '-')
    reverseComplement(bedSeq->dna, bedSeq->size);
return bedSeq;
}

struct dnaSeq *genePredToGenomicSequence(struct genePred *pred, struct twoBitFile *tbf,
					 struct lm *lm)
{
struct bed *bed = bedFromGenePred(pred);
struct dnaSeq *dnaSeq = twoBitSeqFromBed(tbf, bed, lm);

return dnaSeq;
}


static struct annoRow *aggvGenRows( struct annoGratorGpVar *self, struct variant *variant,
				    struct genePred *pred, struct annoRow *inRow,
				    struct lm *callerLm)
// put out annoRows for all the gpFx that arise from variant and pred
{
struct annoGrator *gSelf = &(self->grator);
struct annoStreamer *sSelf = &(gSelf->streamer);
// FIXME:  accessing query's tbf is probably bad
struct dnaSeq *transcriptSequence = genePredToGenomicSequence(pred, sSelf->assembly->tbf,
							      self->lm);
struct gpFx *effects = gpFxPredEffect(variant, pred, transcriptSequence);
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

struct annoRow *annoGratorGpVarIntegrate(struct annoGrator *gSelf, struct annoRow *primaryRow,
					 boolean *retRJFilterFailed, struct lm *callerLm)
// integrate a pgSnp and a genePred, generate as many rows as
// needed to capture all the changes
{
struct annoGratorGpVar *self = (struct annoGratorGpVar *)gSelf;
lmCleanup(&(self->lm));
self->lm = lmInit(0);
// Temporarily tweak primaryRow's start and end to find upstream/downstream overlap:
int pStart = primaryRow->start, pEnd = primaryRow->end;
primaryRow->start -= GPRANGE;
if (primaryRow->start < 0)
    primaryRow->start = 0;
primaryRow->end += GPRANGE;
struct annoRow *rows = annoGratorIntegrate(gSelf, primaryRow, retRJFilterFailed, self->lm);
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

char **primaryWords = primaryRow->data;
struct pgSnp *pgSnp = pgSnpLoad(primaryWords);
struct variant *variant = variantFromPgSnp(pgSnp);
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
self->cdsOnly = cdsOnly;

return gSelf;
}
