/* annoFormatVep -- write functional predictions in the same format as Ensembl's
 * Variant Effect Predictor to fileName, interpreting input rows according to config.
 * See http://uswest.ensembl.org/info/docs/variation/vep/vep_formats.html */

#include "annoFormatVep.h"
#include "annoGratorGpVar.h"
#include "annoGratorQuery.h"
#include "asParse.h"
#include "dystring.h"
#include "genePred.h"
#include "gpFx.h"
#include "pgSnp.h"
#include "portable.h"
#include "vcf.h"
#include <time.h>

struct annoFormatVepExtraItem
    // A single input column whose value should be placed in the Extras output column,
    // identified by tag.
    {
    struct annoFormatVepExtraItem *next;
    char *tag;					// Keyword to use in extras column (tag=value;)
    char *description;				// Text description for output header
    int rowIx;					// Offset of column in row from data source
						// (N/A for wig sources)
    };

struct annoFormatVep;

struct annoFormatVepExtraSource
    // A streamer or grator that supplies at least one value for Extras output column.
    {
    struct annoFormatVepExtraSource *next;
    struct annoStreamer *source;		// streamer or grator: same pointers as below
    struct annoFormatVepExtraItem *items;	// one or more columns of source and their tags

    void (*printExtra)(struct annoFormatVep *afVep, struct annoFormatVepExtraItem *extraItem,
		       struct annoRow *extraRows, boolean *pGotExtra);
    /* Method for printing items from this source; pGotExtra is in/out to keep track of
     * whether any call has actually printed something yet. */
    };

struct annoFormatVepConfig
    // Describe the primary source and grators (whose rows must be delivered in this order)
    // that provide data for VEP output columns.
    {
    struct annoStreamer *variantSource;		// Primary source: variants
    char *variantDescription;			// Description to appear in output header
    struct annoStreamer *gpVarSource;		// annoGratorGpVar makes the core predictions
    char *gpVarDescription;			// Description to appear in output header
    struct annoStreamer *snpSource;		// Latest dbSNP provides IDs of known variants
    char *snpDescription;			// Description to appear in output header
    struct annoFormatVepExtraSource *extraSources;	// Everything else that may be tacked on
    };

struct annoFormatVep
// Subclass of annoFormatter that writes VEP-equivalent output to a file.
    {
    struct annoFormatter formatter;	// superclass / external interface
    struct annoFormatVepConfig *config;	// Description of input sources and values for Extras col
    char *fileName;			// Output filename
    FILE *f;				// Output file handle
    struct lm *lm;			// localmem for scratch storage
    struct dyString *dyScratch;		// dyString for local temporary use
    int lmRowCount;			// counter for periodic localmem cleanup
    int varNameIx;			// Index of name column from variant source, or -1 if N/A
    int varAllelesIx;			// Index of alleles column from variant source, or -1
    int geneNameIx;			// Index of gene name (not transcript name) from genePred
    int snpNameIx;			// Index of name column from dbSNP source, or -1
    boolean needHeader;			// TRUE if we should print out the header
    boolean primaryIsVcf;		// TRUE if primary rows are VCF
    boolean skippedFirstBase;		// 1 if VCF included 1 extra initial identical base (indel)
    };


static void afVepPrintHeaderExtraTags(struct annoFormatVep *self)
/* For each extra column described in config, write out its tag and a brief description. */
{
struct annoFormatVepExtraSource *extras = self->config->extraSources, *extraSrc;
if (extras == NULL)
    return;
fprintf(self->f, "## Extra column keys:\n");
for (extraSrc = extras;  extraSrc != NULL;  extraSrc = extraSrc->next)
    {
    struct annoFormatVepExtraItem *extraItem;
    for (extraItem = extraSrc->items;  extraItem != NULL;  extraItem = extraItem->next)
	if (isNotEmpty(extraItem->tag))
	    fprintf(self->f, "## %s: %s\n", extraItem->tag, extraItem->description);
    }
}

static void afVepPrintHeaderDate(FILE *f)
/* VEP header includes a date formatted like "2012-06-16 16:09:38" */
{
long now = clock1();
struct tm *tm = localtime(&now);
fprintf(f, "## Output produced at %d-%02d-%02d %02d:%02d:%02d\n",
	1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

static void afVepPrintHeader(struct annoFormatVep *self, char *db)
/* Print a header that looks almost like a VEP header. */
{
FILE *f = self->f;
fprintf(f, "## ENSEMBL VARIANT EFFECT PREDICTOR format (UCSC Variant Annotation Integrator)\n");
afVepPrintHeaderDate(f);
fprintf(f, "## Connected to UCSC database %s\n", db);
struct annoFormatVepConfig *config = self->config;
fprintf(f, "## Variants: %s (%s)\n", config->variantDescription, config->variantSource->name);
fprintf(f, "## Transcripts: %s (%s)\n", config->gpVarDescription, config->gpVarSource->name);
if (config->snpSource != NULL)
    fprintf(f, "## dbSNP: %s (%s)\n", config->snpDescription, config->snpSource->name);
afVepPrintHeaderExtraTags(self);
fputs("Uploaded Variation\tLocation\tAllele\tGene\tFeature\tFeature type\tConsequence\t"
      "Position in cDNA\tPosition in CDS\tPosition in protein\tAmino acid change\t"
      "Codon change\tCo-located Variation\tExtra\n", f);
self->needHeader = FALSE;
}

static void afVepInitialize(struct annoFormatter *fSelf, struct annoStreamer *primarySource,
			    struct annoStreamer *integrators)
/* Print header, regardless of whether we get any data after this. */
{
struct annoFormatVep *self = (struct annoFormatVep *)fSelf;
if (self->needHeader)
    afVepPrintHeader(self, primarySource->assembly->name);
}

static void afVepPrintNameAndLoc(struct annoFormatVep *self, struct annoRow *varRow)
/* Print variant name and position in genome. */
{
char **varWords = (char **)(varRow->data);
uint start1Based = varRow->start + 1;
// Use variant name if available, otherwise construct an identifier:
if (self->varNameIx >= 0)
    fprintf(self->f, "%s\t", varWords[self->varNameIx]);
else
    {
    char *alleles = NULL;
    if (self->primaryIsVcf)
	alleles = vcfGetSlashSepAllelesFromWords(varWords, self->dyScratch,
						 &(self->skippedFirstBase));
    else if (self->varAllelesIx >= 0)
	alleles = varWords[self->varAllelesIx];
    else
	errAbort("annoFormatVep: afVepSetConfig didn't specify how to get alleles");
    if (self->skippedFirstBase)
	start1Based++;
    fprintf(self->f, "%s_%u_%s\t", varRow->chrom, start1Based, alleles);
    }
// Location is chr:start for single-base, chr:start-end for indels:
if (varRow->end == start1Based)
    fprintf(self->f, "%s:%u\t", varRow->chrom, start1Based);
else if (start1Based > varRow->end)
    fprintf(self->f, "%s:%u-%u\t", varRow->chrom, varRow->end, start1Based);
else
    fprintf(self->f, "%s:%u-%u\t", varRow->chrom, start1Based, varRow->end);
}

INLINE void afVepPrintPlaceholders(FILE *f, int count)
/* VEP uses "-" for N/A.  Sometimes there are several consecutive N/A columns.
 * Count = 0 means print "-" with no tab. Count > 0 means print that many "-\t"s. */
{
if (count == 0)
    fputc('-', f);
else
    {
    int i;
    for (i = 0;  i < count;  i++)
	fputs("-\t", f);
    }
}

#define afVepPrintPlaceholder(f) afVepPrintPlaceholders(f, 1)

#define placeholderForEmpty(val) (isEmpty(val) ? "-" : val)


static void afVepPrintGene(struct annoFormatVep *self, struct annoRow *gpvRow)
/* If the genePred portion of gpvRow contains a gene name (in addition to transcript name),
 * print it out; otherwise print a placeholder. */
{
if (self->geneNameIx >= 0)
    {
    char **words = (char **)(gpvRow->data);
    fprintf(self->f, "%s\t", placeholderForEmpty(words[self->geneNameIx]));
    }
else
    afVepPrintPlaceholder(self->f);
}

static void tweakStopCodonAndLimitLength(char *aaSeq, char *codonSeq, boolean isFrameshift)
/* If aa from gpFx has a stop 'Z', replace it with '*' and truncate 
 * codons following the stop if necessary just in case they run on.
 * Then if isFrameshift and the strings are very long, truncate with
 * a note about how long they are. */
{
char *earlyStop = strchr(aaSeq, 'Z');
if (earlyStop)
    {
    earlyStop[0] = '*';
    earlyStop[1] = '\0';
    int earlyStopIx = (earlyStop - aaSeq + 1) * 3;
    codonSeq[earlyStopIx] = '\0';
    }
if (isFrameshift)
    {
    int len = strlen(aaSeq);
    char lengthNote[512];
    safef(lengthNote, sizeof(lengthNote), "...(%d bases)", len);
    if (len > 5 + strlen(lengthNote))
	safecpy(aaSeq+5, len+1-5, lengthNote);
    len = strlen(codonSeq);
    safef(lengthNote, sizeof(lengthNote), "...(%d bases)", len);
    if (len > 12 + strlen(lengthNote))
	safecpy(codonSeq+12, len+1-12, lengthNote);
    }
}

static void afVepPrintPredictions(struct annoFormatVep *self, struct annoRow *gpvRow,
				  struct gpFx *gpFx, boolean isInsertion)
/* Print VEP columns computed by annoGratorGpVar (or placeholders) */
{
// variant allele used to calculate the consequence
// For upstream/downstream variants, gpFx leaves allele empty which I think is appropriate,
// but VEP uses non-reference allele... #*** can we determine that here?
fprintf(self->f, "%s\t", placeholderForEmpty(gpFx->allele));
// ID of affected gene
afVepPrintGene(self, gpvRow);
// ID of feature
fprintf(self->f, "%s\t", placeholderForEmpty(gpFx->transcript));
// type of feature {Transcript, RegulatoryFeature, MotifFeature}
if (gpFx->soNumber == intergenic_variant)
    afVepPrintPlaceholder(self->f);
else
    fputs("Transcript\t", self->f);
// consequence: SO term e.g. splice_region_variant
fprintf(self->f, "%s\t", soTermToString(gpFx->soNumber));
if (gpFx->detailType == codingChange)
    {
    struct codingChange *change = &(gpFx->details.codingChange);
    if (isInsertion)
	{
	fprintf(self->f, "%u-%u\t", change->cDnaPosition, change->cDnaPosition+1);
	fprintf(self->f, "%u-%u\t", change->cdsPosition, change->cdsPosition+1);
	}
    else
	{
	fprintf(self->f, "%u\t", change->cDnaPosition+1);
	fprintf(self->f, "%u\t", change->cdsPosition+1);
	}
    fprintf(self->f, "%u\t", change->pepPosition+1);
    boolean isFrameshift = (gpFx->soNumber == frameshift_variant);
    tweakStopCodonAndLimitLength(change->aaOld, change->codonOld, isFrameshift);
    tweakStopCodonAndLimitLength(change->aaNew, change->codonNew, isFrameshift);
    fprintf(self->f, "%s/%s\t", change->aaOld, change->aaNew);
    fprintf(self->f, "%s/%s\t", change->codonOld, change->codonNew);
    }
else if (gpFx->detailType == nonCodingExon)
    {
    int cDnaPosition = gpFx->details.nonCodingExon.cDnaPosition;
    if (isInsertion)
	fprintf(self->f, "%u-%u\t", cDnaPosition, cDnaPosition+1);
    else
	fprintf(self->f, "%u\t", cDnaPosition+1);
    // Coding effect columns (except for cDnaPosition) are N/A:
    afVepPrintPlaceholders(self->f, 4);
    }
else
    // Coding effect columns are N/A:
    afVepPrintPlaceholders(self->f, 5);
}

static void afVepPrintExistingVar(struct annoFormatVep *self, struct annoRow *varRow,
				  struct annoStreamRows gratorData[], int gratorCount)
/* Print existing variant ID (or placeholder) */
{
if (self->snpNameIx >= 0)
    {
    if (gratorCount < 2 || gratorData[1].streamer != self->config->snpSource)
	errAbort("annoFormatVep: config error, snpSource is not where expected");
    struct annoRow *snpRows = gratorData[1].rowList, *row;
    if (snpRows != NULL)
	{
	int varStart = varRow->start + self->skippedFirstBase;
	int count = 0;
	for (row = snpRows;  row != NULL;  row = row->next)
	    {
	    char **snpWords = (char **)(row->data);
	    if (row->start == varStart && row->end == varRow->end)
		{
		if (count > 0)
		    fputc(',', self->f);
		fprintf(self->f, "%s", snpWords[self->snpNameIx]);
		count++;
		}
	    }
	if (count == 0)
	    afVepPrintPlaceholder(self->f);
	fputc('\t', self->f);
	}
    else
	afVepPrintPlaceholder(self->f);
    }
else
    afVepPrintPlaceholder(self->f);
}

static boolean isCodingSnv(struct annoRow *primaryRow, struct gpFx *gpFx)
/* Return TRUE if this is a single-nucleotide non-synonymous change. */
{
if (primaryRow->end != primaryRow->start + 1)
    return FALSE;
if (gpFx == NULL || gpFx->allele == NULL ||
    strlen(gpFx->allele) != 1 || sameString(gpFx->allele, "-") ||
    gpFx->detailType != codingChange ||
    gpFx->soNumber == synonymous_variant)
    return FALSE;
return TRUE;
}

static int commaSepFindIx(char *item, char *s)
/* Treating comma-separated, non-NULL s as an array of words,
 * return the index of item in the array, or -1 if item is not in array. */
{
int itemLen = strlen(item);
int ix = 0;
char *p = strchr(s, ',');
while (p != NULL)
    {
    int elLen = (p - s);
    if (elLen == itemLen && strncmp(s, item, itemLen) == 0)
	return ix;
    s = p+1;
    p = strchr(s, ',');
    ix++;
    }
if (strlen(s) == itemLen && strncmp(s, item, itemLen) == 0)
    return ix;
return -1;
}

static int commaSepFindIntIx(int item, char *s)
/* Treating comma-separated, non-NULL s as an array of words that encode integers,
 * return the index of item in the array, or -1 if item is not in array. */
{
char itemString[64];
safef(itemString, sizeof(itemString), "%d", item);
return commaSepFindIx(itemString, s);
}

static char *commaSepWordFromIx(int ix, char *s, struct lm *lm)
/* Treating comma-separated, non-NULL s as an array of words,
 * return the word at ix in the array. This errAborts if ix is not valid. */
{
int i = 0;
char *p = strchr(s, ',');
while (p != NULL)
    {
    if (i == ix)
	return lmCloneStringZ(lm, s, p-s);
    s = p+1;
    p = strchr(s, ',');
    i++;
    }
if (i == ix)
    return lmCloneString(lm, s);
errAbort("commaSepWordFromIx: Bad index %d for string '%s'", ix, s);
return NULL;
}

static void afVepPrintDbNsfpSift(struct annoFormatVep *self,
				 struct annoFormatVepExtraSource *extraSrc,
				 struct annoRow *extraRows, struct gpFx *gpFx, char *ensTxId,
				 boolean *pGotExtra)
/* Match the allele from gpFx to the per-allele scores in row from dbNsfpSift. */
{
// Look up column indices only once:
static int ensTxIdIx=-1, altAl1Ix, score1Ix, altAl2Ix, score2Ix, altAl3Ix, score3Ix;
if (ensTxIdIx == -1)
    {
    struct asColumn *columns = extraSrc->source->asObj->columnList;
    ensTxIdIx = asColumnFindIx(columns, "ensTxId");
    altAl1Ix = asColumnFindIx(columns, "altAl1");
    score1Ix = asColumnFindIx(columns, "score1");
    altAl2Ix = asColumnFindIx(columns, "altAl2");
    score2Ix = asColumnFindIx(columns, "score2");
    altAl3Ix = asColumnFindIx(columns, "altAl3");
    score3Ix = asColumnFindIx(columns, "score3");
    }

struct annoRow *row;
for (row = extraRows;  row != NULL;  row = row->next)
    {
    // Skip this row unless it contains the ensTxId found by getDbNsfpEnsTx
    // (but handle rare cases where dbNsfpSeqChange has "." for ensTxId, lame)
    char **words = row->data;
    if (differentString(ensTxId, ".") && differentString(words[ensTxIdIx], ".") &&
	commaSepFindIx(ensTxId, words[ensTxIdIx]) < 0)
	continue;
    struct annoFormatVepExtraItem *extraItem = extraSrc->items;
    char *scoreStr = NULL;
    if (sameString(gpFx->allele, words[altAl1Ix]))
	scoreStr = words[score1Ix];
    else if (sameString(gpFx->allele, words[altAl2Ix]))
	scoreStr = words[score2Ix];
    else if (sameString(gpFx->allele, words[altAl3Ix]))
	scoreStr = words[score3Ix];
    double score = atof(scoreStr);
    char prediction = '?';
    if (score < 0.05)
	prediction = 'D';
    else
	prediction = 'T';
    if (isNotEmpty(scoreStr) && differentString(scoreStr, "."))
	{
	if (*pGotExtra)
	    fputc(';', self->f);
	fprintf(self->f, "%s=%c(%s)", extraItem->tag, prediction, scoreStr);
	*pGotExtra = TRUE;
	}
    }
}

static void afVepPrintDbNsfpPolyPhen2(struct annoFormatVep *self,
				      struct annoFormatVepExtraSource *extraSrc,
				      struct annoRow *extraRows, struct gpFx *gpFx,
				      boolean *pGotExtra)
/* Match the allele from gpFx to the per-allele scores in each row from dbNsfpPolyPhen2. */
{
// Look up column indices only once:
static int aaPosIx=-1,
    altAl1Ix, hDivScore1Ix, hDivPred1Ix, hVarScore1Ix, hVarPred1Ix,
    altAl2Ix, hDivScore2Ix, hDivPred2Ix, hVarScore2Ix, hVarPred2Ix,
    altAl3Ix, hDivScore3Ix, hDivPred3Ix, hVarScore3Ix, hVarPred3Ix;
if (aaPosIx == -1)
    {
    struct asColumn *columns = extraSrc->source->asObj->columnList;
    aaPosIx = asColumnFindIx(columns, "uniProtAaPos");
    altAl1Ix = asColumnFindIx(columns, "altAl1");
    hDivScore1Ix = asColumnFindIx(columns, "hDivScore1");
    hDivPred1Ix = asColumnFindIx(columns, "hDivPred1");
    hVarScore1Ix = asColumnFindIx(columns, "hVarScore1");
    hVarPred1Ix = asColumnFindIx(columns, "hVarPred1");
    altAl2Ix = asColumnFindIx(columns, "altAl2");
    hDivScore2Ix = asColumnFindIx(columns, "hDivScore2");
    hDivPred2Ix = asColumnFindIx(columns, "hDivPred2");
    hVarScore2Ix = asColumnFindIx(columns, "hVarScore2");
    hVarPred2Ix = asColumnFindIx(columns, "hVarPred2");
    altAl3Ix = asColumnFindIx(columns, "altAl3");
    hDivScore3Ix = asColumnFindIx(columns, "hDivScore3");
    hDivPred3Ix = asColumnFindIx(columns, "hDivPred3");
    hVarScore3Ix = asColumnFindIx(columns, "hVarScore3");
    hVarPred3Ix = asColumnFindIx(columns, "hVarPred3");
    }

struct codingChange *cc = &(gpFx->details.codingChange);
int count = 0;
struct annoRow *row;
for (row = extraRows;  row != NULL;  row = row->next)
    {
    char **words = row->data;
//#*** polyphen2 can actually have multiple scores/preds for the same pepPosition...
//#*** so what we really should do is loop on comma-sep words[aaPosIx] and print scores/preds
//#*** whenever pepPosition matches.
    int txIx = commaSepFindIntIx(cc->pepPosition+1, words[aaPosIx]);
    if (txIx < 0)
	continue;
    struct annoFormatVepExtraItem *extraItem = extraSrc->items;
    for (;  extraItem != NULL;  extraItem = extraItem->next)
	{
	boolean isHdiv = (stringIn("HDIV", extraItem->tag) != NULL);
	int predIx = -1, scoreIx = -1;
	if (sameString(gpFx->allele, words[altAl1Ix]))
	    {
	    predIx = isHdiv ? hDivPred1Ix : hVarPred1Ix;
	    scoreIx = isHdiv ? hDivScore1Ix : hVarScore1Ix;
	    }
	else if (sameString(gpFx->allele, words[altAl2Ix]))
	    {
	    predIx = isHdiv ? hDivPred2Ix : hVarPred2Ix;
	    scoreIx = isHdiv ? hDivScore2Ix : hVarScore2Ix;
	    }
	else if (sameString(gpFx->allele, words[altAl3Ix]))
	    {
	    predIx = isHdiv ? hDivPred3Ix : hVarPred3Ix;
	    scoreIx = isHdiv ? hDivScore3Ix : hVarScore3Ix;
	    }
	char *pred = (predIx < 0) ? NULL : words[predIx];
	if (pred == NULL || sameString(pred, "."))
	    continue;
	pred = commaSepWordFromIx(txIx, pred, self->lm);
	if (isNotEmpty(pred))
	    {
	    if (count == 0)
		{
		if (*pGotExtra)
		    fputc(';', self->f);
		fprintf(self->f, "%s=", extraItem->tag);
		}
	    else
		fputc(',', self->f);
	    char *score = (scoreIx < 0) ? "?" : commaSepWordFromIx(txIx, words[scoreIx], self->lm);
	    fprintf(self->f, "%s(%s)", pred, score);
	    *pGotExtra = TRUE;
	    count++;
	    }
	}
    }
}

static void afVepPrintDbNsfpMutationTA(struct annoFormatVep *self,
				       struct annoFormatVepExtraSource *extraSrc,
				       struct annoRow *extraRows, struct gpFx *gpFx, char *ensTxId,
				       boolean *pGotExtra)
/* Match the allele from gpFx to the per-allele scores in row from dbNsfpMutationTaster
 * or dbNsfpMutationAssessor -- they have identical column indices. */
{
// Look up column indices only once:
static int ensTxIdIx=-1, altAl1Ix, score1Ix, pred1Ix,
    altAl2Ix, score2Ix, pred2Ix, altAl3Ix, score3Ix, pred3Ix;
if (ensTxIdIx == -1)
    {
    struct asColumn *columns = extraSrc->source->asObj->columnList;
    ensTxIdIx = asColumnFindIx(columns, "ensTxId");
    altAl1Ix = asColumnFindIx(columns, "altAl1");
    score1Ix = asColumnFindIx(columns, "score1");
    pred1Ix = asColumnFindIx(columns, "pred1");
    altAl2Ix = asColumnFindIx(columns, "altAl2");
    score2Ix = asColumnFindIx(columns, "score2");
    pred2Ix = asColumnFindIx(columns, "pred2");
    altAl3Ix = asColumnFindIx(columns, "altAl3");
    score3Ix = asColumnFindIx(columns, "score3");
    pred3Ix = asColumnFindIx(columns, "pred3");
    }

struct annoRow *row;
for (row = extraRows;  row != NULL;  row = row->next)
    {
    // Skip this row unless it contains the ensTxId found by getDbNsfpEnsTx
    // (but handle rare cases where dbNsfpSeqChange has "." for ensTxId, lame)
    char **words = row->data;
    if (differentString(ensTxId, ".") && differentString(words[ensTxIdIx], ".") &&
	commaSepFindIx(ensTxId, words[ensTxIdIx]) < 0)
	continue;
    struct annoFormatVepExtraItem *extraItem = extraSrc->items;
    char *score = NULL, *pred = NULL;
    if (sameString(gpFx->allele, words[altAl1Ix]))
	{
	score = words[score1Ix];
	pred = words[pred1Ix];
	}
    else if (sameString(gpFx->allele, words[altAl2Ix]))
	{
	score = words[score2Ix];
	pred = words[pred2Ix];
	}
    else if (sameString(gpFx->allele, words[altAl3Ix]))
	{
	score = words[score3Ix];
	pred = words[pred3Ix];
	}
    if (isNotEmpty(score) && differentString(score, "."))
	{
	if (*pGotExtra)
	    fputc(';', self->f);
	if (isEmpty(pred))
	    pred = "?";
	fprintf(self->f, "%s=%s(%s)", extraItem->tag, pred, score);
	*pGotExtra = TRUE;
	}
    }
}

static void afVepPrintDbNsfpLrt(struct annoFormatVep *self,
				struct annoFormatVepExtraSource *extraSrc,
				struct annoRow *extraRows, struct gpFx *gpFx, char *ensTxId,
				boolean *pGotExtra)
/* Match the allele from gpFx to the per-allele scores in row from dbNsfpLrt --
 * it also has omega{1,2,3} columns, but I'm not sure what those mean so am leaving out for now. */
{
// Look up column indices only once:
static int ensTxIdIx=-1, altAl1Ix, score1Ix, pred1Ix,
    altAl2Ix, score2Ix, pred2Ix, altAl3Ix, score3Ix, pred3Ix;
if (ensTxIdIx == -1)
    {
    struct asColumn *columns = extraSrc->source->asObj->columnList;
    ensTxIdIx = asColumnFindIx(columns, "ensTxId");
    altAl1Ix = asColumnFindIx(columns, "altAl1");
    score1Ix = asColumnFindIx(columns, "score1");
    pred1Ix = asColumnFindIx(columns, "pred1");
    altAl2Ix = asColumnFindIx(columns, "altAl2");
    score2Ix = asColumnFindIx(columns, "score2");
    pred2Ix = asColumnFindIx(columns, "pred2");
    altAl3Ix = asColumnFindIx(columns, "altAl3");
    score3Ix = asColumnFindIx(columns, "score3");
    pred3Ix = asColumnFindIx(columns, "pred3");
    }

struct annoRow *row;
for (row = extraRows;  row != NULL;  row = row->next)
    {
    // Skip this row unless it contains the ensTxId found by getDbNsfpEnsTx
    // (but handle rare cases where dbNsfpSeqChange has "." for ensTxId, lame)
    char **words = row->data;
    if (differentString(ensTxId, ".") && differentString(words[ensTxIdIx], ".") &&
	commaSepFindIx(ensTxId, words[ensTxIdIx]) < 0)
	continue;
    struct annoFormatVepExtraItem *extraItem = extraSrc->items;
    char *score = NULL, *pred = NULL;
    if (sameString(gpFx->allele, words[altAl1Ix]))
	{
	score = words[score1Ix];
	pred = words[pred1Ix];
	}
    else if (sameString(gpFx->allele, words[altAl2Ix]))
	{
	score = words[score2Ix];
	pred = words[pred2Ix];
	}
    else if (sameString(gpFx->allele, words[altAl3Ix]))
	{
	score = words[score3Ix];
	pred = words[pred3Ix];
	}
    if (isNotEmpty(score) && differentString(score, "."))
	{
	if (*pGotExtra)
	    fputc(';', self->f);
	if (isEmpty(pred))
	    pred = "?";
	fprintf(self->f, "%s=%s(%s)", extraItem->tag, pred, score);
	*pGotExtra = TRUE;
	}
    }
}

static void afVepPrintDbNsfpInterPro(struct annoFormatVep *self,
				     struct annoFormatVepExtraSource *extraSrc,
				     struct annoRow *extraRows, struct gpFx *gpFx, char *ensTxId,
				     boolean *pGotExtra)
/* Print out any overlapping (comma-sep list, HTML-encoded ',' and '=') InterPro domains. */
{
// Look up column indices only once:
static int domainsIx = -1;
if (domainsIx == -1)
    {
    struct asColumn *columns = extraSrc->source->asObj->columnList;
    domainsIx = asColumnFindIx(columns, "domains");
    }

struct annoRow *row;
for (row = extraRows;  row != NULL;  row = row->next)
    {
    char **words = row->data;
    struct annoFormatVepExtraItem *extraItem = extraSrc->items;
    char *domains = words[domainsIx];
    int lastC = strlen(domains) - 1;
    if (domains[lastC] == ',')
	domains[lastC] = '\0';
    if (isNotEmpty(domains) && differentString(domains, "."))
	{
	if (*pGotExtra)
	    fputc(';', self->f);
	fprintf(self->f, "%s=%s", extraItem->tag, domains);
	*pGotExtra = TRUE;
	}
    }
}

static boolean allelesAgree(char altNt, char altAa, char **words)
/* Return TRUE if dbNsfpSeqChange words have altAa associated with altNt. */
{
//#*** TODO: handle stop codon representation
if ((altNt == words[11][0] && altAa == words[12][0]) ||
    (altNt == words[13][0] && altAa == words[14][0]) ||
    (altNt == words[15][0] && altAa == words[16][0]))
    return TRUE;
return FALSE;
}

static struct annoRow *getRowsFromSource(struct annoStreamer *src,
					 struct annoStreamRows gratorData[], int gratorCount)
/* Search gratorData for src, and return its rows when found. */
{
int i;
for (i = 0;  i < gratorCount;  i++)
    {
    if (gratorData[i].streamer == src)
	return gratorData[i].rowList;
    }
errAbort("annoFormatVep: Can't find source %s in gratorData", src->name);
return NULL;
}

static char *getDbNsfpEnsTx(struct annoFormatVep *self, struct gpFx *gpFx,
			    struct annoStreamRows *gratorData, int gratorCount)
/* Find the Ensembl transcript ID, if any, for which dbNsfp has results consistent
 * with gpFx. */
{
int i;
for (i = 0;  i < gratorCount;  i++)
    {
    struct annoStreamer *source = gratorData[i].streamer;
    if (!sameString(source->asObj->name, "dbNsfpSeqChange")) //#*** need metadata!
	continue;
    struct codingChange *cc = &(gpFx->details.codingChange);
    struct annoRow *extraRows = getRowsFromSource(source, gratorData, gratorCount);
    struct annoRow *row;
    for (row = extraRows;  row != NULL;  row = row->next)
	{
	char **words = row->data;
	if (!sameString(cc->codonOld, words[7]))
	    continue;
	if (!allelesAgree(gpFx->allele[0], cc->aaNew[0], words))
	    continue;
	int txIx = commaSepFindIntIx(cc->pepPosition+1, words[10]);
	if (txIx >= 0)
	    {
	    if (sameString(words[4], "."))
		return ".";
	    char *ensTxId = commaSepWordFromIx(txIx, words[4], self->lm);
	    return ensTxId;
	    }
	}
    break;
    }
return NULL;
}

static void afVepPrintExtrasDbNsfp(struct annoFormatVep *self, struct annoRow *varRow,
				   struct annoRow *gpvRow, struct gpFx *gpFx,
				   struct annoStreamRows gratorData[], int gratorCount,
				   boolean *pGotExtra)
/* Print the Extra column's tag=value; components from dbNSFP data, if we have any. */
{
// dbNSFP has data only for coding non-synonymous single-nucleotide changes:
if (!isCodingSnv(varRow, gpFx))
    return;
// Does dbNsfpSeqChange have a coding change consistent with gpFx?:
char *ensTxId = getDbNsfpEnsTx(self, gpFx, gratorData, gratorCount);
// Now cycle through selected dbNsfp* sources, printing out scores for ensTxId:
struct annoFormatVepExtraSource *extras = self->config->extraSources, *extraSrc;
for (extraSrc = extras;  extraSrc != NULL;  extraSrc = extraSrc->next)
    {
    char *asObjName = extraSrc->source->asObj->name;
    if (!startsWith("dbNsfp", asObjName))
	continue;
    struct annoRow *extraRows = getRowsFromSource(extraSrc->source, gratorData, gratorCount);
    if (sameString(asObjName, "dbNsfpPolyPhen2"))
	{
	// PolyPhen2 is based on UniProt proteins, not GENCODE/Ensembl transcripts,
	// so ensTxId doesn't apply.
	afVepPrintDbNsfpPolyPhen2(self, extraSrc, extraRows, gpFx, pGotExtra);
	if (ensTxId == NULL)
	    break; // all done now, no need to keep looking
	}
    else if (ensTxId != NULL)
	{
	if (sameString(asObjName, "dbNsfpSift"))
	    afVepPrintDbNsfpSift(self, extraSrc, extraRows, gpFx, ensTxId, pGotExtra);
	else if (sameString(asObjName, "dbNsfpMutationTaster") ||
		 sameString(asObjName, "dbNsfpMutationAssessor"))
	    afVepPrintDbNsfpMutationTA(self, extraSrc, extraRows, gpFx, ensTxId, pGotExtra);
	else if (sameString(asObjName, "dbNsfpLrt"))
	    afVepPrintDbNsfpLrt(self, extraSrc, extraRows, gpFx, ensTxId, pGotExtra);
	else if (sameString(asObjName, "dbNsfpInterPro"))
	    afVepPrintDbNsfpInterPro(self, extraSrc, extraRows, gpFx, ensTxId, pGotExtra);
	else if (!sameString(asObjName, "dbNsfpSeqChange"))
	    errAbort("Unrecognized asObj->name '%s' from dbNSFP source '%s'",
		     asObjName, extraSrc->source->name);
	}
    }
}

static void afVepPrintExtraWig(struct annoFormatVep *self,
			       struct annoFormatVepExtraItem *extraItem,
			       struct annoRow *extraRows, boolean *pGotExtra)
/* Print values from a wig source */
//#*** Probably what we really want here is the average..... ??
//#*** just listing them doesn't show where gaps are. overlap is possible too.
//#*** Look into what VEP does for numerics.
{
if (*pGotExtra)
    fputc(';', self->f);
fprintf(self->f, "%s=", extraItem->tag);
int i;
struct annoRow *row;
for (i = 0, row = extraRows;  row != NULL;  i++, row = row->next)
    {
    float *vector = row->data;
    int len = row->end - row->start;
    int j;
    for (j = 0;  j < len;  j++)
	{
	if (i+j > 0)
	    fputc(',', self->f);
	fprintf(self->f, "%g", vector[j]);
	}
    *pGotExtra = TRUE;
    }
}

static void afVepPrintExtraWords(struct annoFormatVep *self,
				 struct annoFormatVepExtraItem *extraItem,
				 struct annoRow *extraRows, boolean *pGotExtra)
/* Print comma-separated values in the specified column from the usual array-of-words. */
{
if (extraItem->rowIx < 0)
    errAbort("annoFormatVep: invalid rowIx for tag %s", extraItem->tag);
if (*pGotExtra)
    fputc(';', self->f);
fprintf(self->f, "%s=", extraItem->tag);
int i;
struct annoRow *row;
for (i = 0, row = extraRows;  row != NULL;  i++, row = row->next)
    {
    if (i > 0)
	fputc(',', self->f);
    char **words = row->data;
    char *val = words[extraItem->rowIx];
    // Watch out for characters that will mess up parsing of EXTRAS column:
    // #*** When producing HTML output, it would definitely be better to HTML-encode:
    subChar(val, '=', '_');
    subChar(val, ';', '_');
    fputs(val, self->f);
    *pGotExtra = TRUE;
    }
}

static void afVepPrintExtrasOther(struct annoFormatVep *self, struct annoRow *varRow,
				  struct annoRow *gpvRow, struct gpFx *gpFx,
				  struct annoStreamRows gratorData[], int gratorCount,
				  boolean *pGotExtra)
/* Print the Extra column's tag=value; components (other than dbNSFP) if we have any. */
{
struct annoFormatVepExtraSource *extras = self->config->extraSources, *extraSrc;
for (extraSrc = extras;  extraSrc != NULL;  extraSrc = extraSrc->next)
    {
    char *asObjName = extraSrc->source->asObj->name;
    if (startsWith("dbNsfp", asObjName))
	continue;
    struct annoRow *extraRows = getRowsFromSource(extraSrc->source, gratorData, gratorCount);
    if (extraRows != NULL)
	{
	struct annoFormatVepExtraItem *extraItem;
	for (extraItem = extraSrc->items;  extraItem != NULL;  extraItem = extraItem->next)
	    extraSrc->printExtra(self, extraItem, extraRows, pGotExtra);
	}
    }
// VEP automatically adds DISTANCE for upstream/downstream variants
if (gpFx->soNumber == upstream_gene_variant || gpFx->soNumber == downstream_gene_variant)
    {
    if (*pGotExtra)
	fputc(';', self->f);
    // Using varRow->start for both up & down -- just seems more natural,
    // and also it's possible for the variant to overlap txStart or txEnd
    // Note: I think it should be gpvRow->end - varRow->start and varRow->start - gpvRow->end
    // here, but this matches Ensembl output and gets the general idea across:
    int distance = gpvRow->start - varRow->start;
    if (distance < 0)
	distance = varRow->end - gpvRow->end;
    fprintf(self->f, "DISTANCE=%d", distance);
    *pGotExtra = TRUE;
    }
boolean includeExonNumber = TRUE;  //#*** optional in VEP
if (includeExonNumber)
    {
    // Add Exon or intron number if applicable
    enum detailType deType = gpFx->detailType;
    int exonNum = -1;
    if (deType == codingChange)
	exonNum = gpFx->details.codingChange.exonNumber;
    else if (deType == nonCodingExon)
	exonNum = gpFx->details.nonCodingExon.exonNumber;
    else if (deType == intron)
	exonNum = gpFx->details.intron.intronNumber;
    if (exonNum >= 0)
	{
	if (*pGotExtra)
	    fputc(';', self->f);
	char *exonCount = ((char **)(gpvRow->data))[7];
	fprintf(self->f, "%s=%d/%s", (deType == intron ? "INTRON" : "EXON"), exonNum+1, exonCount);
	*pGotExtra = TRUE;
	}
    }
if (!*pGotExtra)
    afVepPrintPlaceholders(self->f, 0);
}

static void afVepLmCleanup(struct annoFormatVep *self)
{
self->lmRowCount++;
if (self->lmRowCount > 1024)
    {
    lmCleanup(&(self->lm));
    self->lm = lmInit(0);
    self->lmRowCount = 0;
    }
}

static void afVepPrintOneLine(struct annoFormatVep *self, struct annoStreamRows *varData,
			      struct annoRow *gpvRow,
			      struct annoStreamRows gratorData[], int gratorCount)
/* Print one line of VEP: a variant, an allele, functional consequences of that allele,
 * and whatever else is included in the config. */
{
afVepLmCleanup(self);
struct annoRow *varRow = varData->rowList;
struct gpFx *gpFx = annoGratorGpVarGpFxFromRow(self->config->gpVarSource, gpvRow, self->lm);
afVepPrintNameAndLoc(self, varRow);
boolean isInsertion = (varRow->start == varRow->end);
afVepPrintPredictions(self, gpvRow, gpFx, isInsertion);
afVepPrintExistingVar(self, varRow, gratorData, gratorCount);
boolean gotExtra = FALSE;
afVepPrintExtrasDbNsfp(self, varRow, gpvRow, gpFx, gratorData, gratorCount, &gotExtra);
afVepPrintExtrasOther(self, varRow, gpvRow, gpFx, gratorData, gratorCount, &gotExtra);
fputc('\n', self->f);
}

static void afVepFormatOne(struct annoFormatter *fSelf, struct annoStreamRows *primaryData,
			   struct annoStreamRows gratorData[], int gratorCount)
/* Print one variant's VEP (possibly multiple lines) using collected rows. */
{
struct annoFormatVep *self = (struct annoFormatVep *)fSelf;
struct annoRow *gpVarRows = gratorData[0].rowList, *gpvRow;
for (gpvRow = gpVarRows;  gpvRow != NULL;  gpvRow = gpvRow->next)
    afVepPrintOneLine(self, primaryData, gpvRow, gratorData, gratorCount);
}

static void afVepClose(struct annoFormatter **pFSelf)
/* Close file handle, free self. */
{
if (pFSelf == NULL)
    return;
struct annoFormatVep *self = *(struct annoFormatVep **)pFSelf;
freeMem(self->fileName);
carefulClose(&(self->f));
lmCleanup(&(self->lm));
dyStringFree(&(self->dyScratch));
annoFormatterFree(pFSelf);
}

static void afVepSetConfig(struct annoFormatVep *self, struct annoFormatVepConfig *config)
/* Check config and figure out where various output columns will come from. */
{
self->config = config;
struct asColumn *varAsColumns = config->variantSource->asObj->columnList;
self->varNameIx = -1;
self->varAllelesIx = -1;
if (asObjectsMatch(config->variantSource->asObj, pgSnpAsObj()))
    {
    // pgSnp's "name" column actually contains slash-separated alleles
    self->varAllelesIx = asColumnFindIx(varAsColumns, "name");
    }
else if (asObjectsMatch(config->variantSource->asObj, vcfAsObj()))
    {
    self->primaryIsVcf = TRUE;
    self->varNameIx = asColumnFindIx(varAsColumns, "name");
    }
else
    errAbort("afVepSetConfig: variant source %s doesn't look like pgSnp or VCF",
	     config->variantSource->name);
if (config->gpVarSource == NULL)
    errAbort("afVepSetConfig: config must have a gpVarSource");
else if (! asObjectsMatchFirstN(config->gpVarSource->asObj, genePredAsObj(), 10))
    errAbort("afVepSetConfig: gpVarSource %s doesn't look like genePred",
	     config->gpVarSource->name);
struct asColumn *gpvAsColumns = config->gpVarSource->asObj->columnList;
self->geneNameIx = asColumnFindIx(gpvAsColumns, "proteinID");
if (self->geneNameIx < 0)
    self->geneNameIx = asColumnFindIx(gpvAsColumns, "name2");
if (config->snpSource != NULL)
    {
    struct asColumn *snpAsColumns = config->snpSource->asObj->columnList;
    self->snpNameIx = asColumnFindIx(snpAsColumns, "name");
    }
else
    self->snpNameIx = -1;
}

struct annoFormatVepConfig *annoFormatVepConfigNew(struct annoStreamer *variantSource,
						   char *variantDescription,
						   struct annoStreamer *gpVarSource,
						   char *gpVarDescription,
						   struct annoStreamer *snpSource,
						   char *snpDescription)
/* Return a basic configuration for VEP output.  variantSource and gpVarSource must be
 * provided; snpSource can be NULL. */
{
struct annoFormatVepConfig *config;
AllocVar(config);
config->variantSource = variantSource;
config->variantDescription = cloneString(variantDescription);
config->gpVarSource = gpVarSource;
config->gpVarDescription = cloneString(gpVarDescription);
config->snpSource = snpSource;
config->snpDescription = cloneString(snpDescription);
return config;
}

struct annoFormatter *annoFormatVepNew(char *fileName, struct annoStreamer *variantSource,
				       char *variantDescription,
				       struct annoStreamer *gpVarSource,
				       char *gpVarDescription,
				       struct annoStreamer *snpSource,
				       char *snpDescription)
/* Return a formatter that will write functional predictions in the same format as Ensembl's
 * Variant Effect Predictor to fileName (can be "stdout").
 * variantSource and gpVarSource must be provided; snpSource can be NULL. */
{
struct annoFormatVep *self;
AllocVar(self);
struct annoFormatter *fSelf = &(self->formatter);
fSelf->getOptions = annoFormatterGetOptions;
fSelf->setOptions = annoFormatterSetOptions;
fSelf->initialize = afVepInitialize;
fSelf->formatOne = afVepFormatOne;
fSelf->close = afVepClose;
self->fileName = cloneString(fileName);
self->f = mustOpen(fileName, "w");
self->lm = lmInit(0);
self->dyScratch = dyStringNew(0);
self->needHeader = TRUE;
struct annoFormatVepConfig *config = annoFormatVepConfigNew(variantSource, variantDescription,
							    gpVarSource, gpVarDescription,
							    snpSource, snpDescription);
afVepSetConfig(self, config);
return (struct annoFormatter *)self;
}

void annoFormatVepAddExtraItem(struct annoFormatter *fSelf, struct annoStreamer *extraSource,
			       char *tag, char *description, char *column)
/* Tell annoFormatVep that it should include the given column of extraSource
 * in the EXTRAS column with tag.  The VEP header will include tag's description.
 * For some special-cased sources e.g. dbNsfp files, column may be NULL/ignored. */
{
struct annoFormatVep *self = (struct annoFormatVep *)fSelf;
struct annoFormatVepExtraSource *src;
for (src = self->config->extraSources;  src != NULL;  src = src->next)
    if (src->source == extraSource)
	break;
if (src == NULL)
    {
    AllocVar(src);
    src->source = extraSource;
    if (extraSource->rowType == arWig)
	src->printExtra = afVepPrintExtraWig;
    else
	src->printExtra = afVepPrintExtraWords;
    slAddTail(&(self->config->extraSources), src);
    }
struct annoFormatVepExtraItem *item;
AllocVar(item);
item->tag = cloneString(tag);
item->description = cloneString(description);
item->rowIx = -1;
if (isNotEmpty(column))
    item->rowIx = asColumnFindIx(extraSource->asObj->columnList, column);
slAddTail(&(src->items), item);
}
