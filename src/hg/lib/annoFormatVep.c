/* annoFormatVep -- write functional predictions in the same format as Ensembl's
 * Variant Effect Predictor to fileName, interpreting input rows according to config.
 * See http://uswest.ensembl.org/info/docs/variation/vep/vep_formats.html */

#include "annoFormatVep.h"
#include "annoGratorGpVar.h"
#include "annoGratorQuery.h"
#include "dystring.h"
#include "gpFx.h"
#include "portable.h"
#include <time.h>

struct annoFormatVep
// Subclass of annoFormatter that writes VEP-equivalent output to a file.
    {
    struct annoFormatter formatter;	// superclass / external interface
    struct annoFormatVepConfig *config;	// Description of input sources and values for Extras col
    char *fileName;			// Output filename
    FILE *f;				// Output file handle
    boolean needHeader;			// TRUE if we should print out the header
    int varNameIx;			// Index of name column from variant source, or -1 if N/A
    int varAllelesIx;			// Index of alleles column from variant source, or -1
    int geneNameIx;			// Index of gene name (not transcript name) from genePred
    int snpNameIx;			// Index of name column from dbSNP source, or -1

    char *(*getSlashSepAlleles)(char **words);
    // If the variant source doesn't have a single column with slash-separated alleles
    // (varAllelesIx is -1), provide a function that translates into slash-sep.
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

static void afVepPrintHeader(struct annoFormatVep *self)
/* Print a header that looks almost like a VEP header. */
{
FILE *f = self->f;
fprintf(f, "## ENSEMBL VARIANT EFFECT PREDICTOR format (UCSC Variant Annotation Integrator)\n");
afVepPrintHeaderDate(f);
char *db = self->formatter.query->assemblyName;
fprintf(f, "## Connected to UCSC database %s\n", db);

//#*** IMPLEMENT ME
char *primaryName = "getNameFromPrimarySource";

fprintf(f, "## Variants: %s\n", primaryName);
afVepPrintHeaderExtraTags(self);
fputs("Uploaded Variation\tLocation\tAllele\tGene\tFeature\tFeature type\tConsequence\t"
      "Position in cDNA\tPosition in CDS\tPosition in protein\tAmino acid change\t"
      "Codon change\tCo-located Variation\tExtra\n", f);
self->needHeader = FALSE;
}

static void afVepInitialize(struct annoFormatter *fSelf, struct annoGratorQuery *query)
/* Print header, regardless of whether we get any data after this. */
{
fSelf->query = query;
struct annoFormatVep *self = (struct annoFormatVep *)fSelf;
if (self->needHeader)
    afVepPrintHeader(self);
}

static char *getSlashSepAllelesFromVcf(char **words)
/* Construct a /-separated allele string from VCF. Do not free result. */
//#*** is this duplicating anything in pgSnpFromVcf?
{
static struct dyString *dy = NULL;
if (dy == NULL)
    dy = dyStringNew(0);
else
    dyStringClear(dy);
// VCF reference allele gets its own column:
dyStringAppend(dy, words[3]);
// VCF alternate alleles are comma-separated, make them /-separated:
if (isNotEmpty(words[4]))
    {
    char *altAlleles = words[4], *p;
    while ((p = strchr(altAlleles, ',')) != NULL)
	{
	dyStringAppendC(dy, '/');
	dyStringAppendN(dy, altAlleles, p-altAlleles);
	altAlleles = p+1;
	}
    dyStringAppendC(dy, '/');
    dyStringAppend(dy, altAlleles);
    }
return dy->string;
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
    char *alleles;
    if (self->varAllelesIx >= 0)
	alleles = varWords[self->varAllelesIx];
    else
	alleles = self->getSlashSepAlleles(varWords);
    fprintf(self->f, "%s_%u_%s\t", varRow->chrom, start1Based, alleles);
    }
// Location is chr:start for single-base, chr:start-end for indels:
if (varRow->end == start1Based)
    fprintf(self->f, "%s:%u\t", varRow->chrom, start1Based);
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

static void afVepPrintGene(struct annoFormatVep *self, struct annoRow *gpvRow)
/* If the genePred portion of gpvRow contains a gene name (in addition to transcript name),
 * print it out; otherwise print a placeholder. */
{
if (self->geneNameIx >= 0)
    {
    char **words = (char **)(gpvRow->data);
    fprintf(self->f, "%s\t", words[self->geneNameIx]);
    }
else
    afVepPrintPlaceholders(self->f, 1);
}

static void afVepPrintPredictions(struct annoFormatVep *self, struct annoRow *gpvRow,
				  struct gpFx *gpFx)
/* Print VEP columns computed by annoGratorGpVar (or placeholders) */
{
// variant allele used to calculate the consequence -- need to add to gpFx I guess
fprintf(self->f, "%s\t", gpFx->allele);
// ID of affected gene
afVepPrintGene(self, gpvRow);
// ID of feature
fprintf(self->f, "%s\t", gpFx->so.transcript);
// type of feature {Transcript, RegulatoryFeature, MotifFeature}
fputs("Transcript\t", self->f);
// consequence: SO term e.g. splice_region_variant
fprintf(self->f, "%s\t", soTermToString(gpFx->so.soNumber));
if (gpFxIsCodingChange(gpFx))
    {
    struct codingChange *change = &(gpFx->so.sub.codingChange);
    fprintf(self->f, "%u\t", change->cDnaPosition+1);
    fprintf(self->f, "%u\t", change->cdsPosition+1);
    fprintf(self->f, "%u\t", change->pepPosition+1);
    char *earlyStop = strchr(change->aaNew, 'Z');
    if (earlyStop)
	{
	earlyStop[0] = '*';
	earlyStop[1] = '\0';
	int earlyStopIx = (earlyStop - change->aaNew + 1) * 3;
	change->codonNew[earlyStopIx] = '\0';
	}
    fprintf(self->f, "%s/%s\t", change->aaOld, change->aaNew);
    fprintf(self->f, "%s/%s\t", change->codonOld, change->codonNew);
    }
//#*** need a case for non-coding transcript changes -- put in cDnaPosition + 4 placeholders
else
    // Coding effect columns are N/A:
    afVepPrintPlaceholders(self->f, 5);
}

static void afVepPrintExistingVar(struct annoFormatVep *self, struct slRef *gratorRowList)
/* Print existing variant ID (or placeholder) */
{
if (self->snpNameIx >= 0)
    {
    struct annoRow *snpRows = gratorRowList->val, *row;
    if (snpRows != NULL)
	{
	int i;
	for (i = 0, row = snpRows;  row != NULL;  i++, row = row->next)
	    {
	    if (i > 0)
		fputc(',', self->f);
	    char **snpWords = (char **)(row->data);
	    fprintf(self->f, "%s", snpWords[self->snpNameIx]);
	    }
	fputc('\t', self->f);
	}
    else
	afVepPrintPlaceholders(self->f, 1);
    }
else
    afVepPrintPlaceholders(self->f, 1);
}

static void afVepPrintExtraWig(struct annoFormatVep *self,
			struct annoFormatVepExtraItem *extraItem, struct annoRow *extraRows)
/* Print values from a wig source */
//#*** Probably what we really want here is the average..... ??
//#*** just listing them doesn't show where gaps are. overlap is possible too.
//#*** Look into what VEP does for numerics.
{
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
    }
}

static void afVepPrintExtraWords(struct annoFormatVep *self,
			struct annoFormatVepExtraItem *extraItem, struct annoRow *extraRows)
/* Print comma-separated values in the specified column from the usual array-of-words. */
{
int i;
struct annoRow *row;
for (i = 0, row = extraRows;  row != NULL;  i++, row = row->next)
    {
    if (i > 0)
	fputc(',', self->f);
    char **words = row->data;
    fprintf(self->f, words[extraItem->rowIx]);
    }
}

static void afVepPrintExtras(struct annoFormatVep *self, struct annoRow *varRow,
			     struct annoRow *gpvRow, struct gpFx *gpFx,
			     struct slRef *gratorRowList)
/* Print the Extra column's tag=value; components if we have any. */
{
boolean gotExtra = FALSE;
struct annoRow *snpRows = NULL;
if (self->snpNameIx >= 0)
    {
    snpRows = gratorRowList->val;
    gratorRowList = gratorRowList->next;
    }
struct annoFormatVepExtraSource *extras = self->config->extraSources, *extraSrc;
for (extraSrc = extras;  extraSrc != NULL;  extraSrc = extraSrc->next)
    {
    if (gratorRowList == NULL)
	errAbort("afVepPrintExtra: more extras specified than gratorRowLists delivered.");
    if (extraSrc != extras)
	fputc(';', self->f);
    struct annoRow *extraRows = gratorRowList->val;
    if (extraSrc->source == self->config->snpSource)
	extraRows = snpRows;
    else
	gratorRowList = gratorRowList->next;
    struct annoFormatVepExtraItem *extraItem;
    for (extraItem = extraSrc->items;  extraItem != NULL;  extraItem = extraItem->next)
	{
	fprintf(self->f, "%s=", extraItem->tag);
	if (extraSrc->source->rowType == arWig)
	    afVepPrintExtraWig(self, extraItem, extraRows);
	else
	    afVepPrintExtraWords(self, extraItem, extraRows);
	}
    gotExtra = TRUE;
    }
if (gratorRowList != NULL)
    errAbort("afVepPrintExtra: more gratorRowLists delivered than extras specified.");
// VEP automatically adds DISTANCE for upstream/downstream variants
if (gpFx->so.soNumber == upstream_gene_variant || gpFx->so.soNumber == downstream_gene_variant)
    {
    if (gotExtra)
	fputc(';', self->f);
    // Using varRow->start for both up & down -- just seems more natural,
    // and also it's possible for the variant to overlap txStart or txEnd
    int distance = gpvRow->start - varRow->start;
    if (distance < 0)
	distance = varRow->start - gpvRow->end;
    fprintf(self->f, "DISTANCE=%d", distance);
    gotExtra = TRUE;
    }
if (!gotExtra)
    afVepPrintPlaceholders(self->f, 0);
}

static void afVepPrintOneLine(struct annoFormatVep *self, struct annoRow *varRow,
			      struct annoRow *gpvRow, struct slRef *gratorRowList)
/* Print one line of VEP: a variant, an allele, functional consequences of that allele,
 * and whatever else is included in the config. */
{
struct gpFx *gpFx = annoGratorGpVarGpFxFromRow(self->config->gpVarSource, gpvRow);
afVepPrintNameAndLoc(self, varRow);
afVepPrintPredictions(self, gpvRow, gpFx);
afVepPrintExistingVar(self, gratorRowList);
afVepPrintExtras(self, varRow, gpvRow, gpFx, gratorRowList);
fputc('\n', self->f);
//#*** mem leak gpFx...
}

static void afVepFormatOne(struct annoFormatter *fSelf, struct annoRow *primaryRow,
			   struct slRef *gratorRowList)
/* Print one variant's VEP (possibly multiple lines) using collected rows. */
{
struct annoFormatVep *self = (struct annoFormatVep *)fSelf;
struct annoRow *gpVarRows = gratorRowList->val, *gpvRow;
for (gpvRow = gpVarRows;  gpvRow != NULL;  gpvRow = gpvRow->next)
    afVepPrintOneLine(self, primaryRow, gpvRow, gratorRowList->next);
}

static void afVepClose(struct annoFormatter **pFSelf)
/* Close file handle, free self. */
{
if (pFSelf == NULL)
    return;
struct annoFormatVep *self = *(struct annoFormatVep **)pFSelf;
freeMem(self->fileName);
carefulClose(&(self->f));
annoFormatterFree(pFSelf);
}

static void afVepSetConfig(struct annoFormatVep *self, struct annoFormatVepConfig *config)
/* Check config and figure out where various output columns will come from. */
{
self->config = config;
struct asColumn *varAsColumns = config->variantSource->asObj->columnList;
self->varNameIx = -1;
self->varAllelesIx = -1;
//#*** get real autoSql col checking:
if (asColumnFindIx(varAsColumns, "alleleCount"))
    {
    // don't use pgSnp's "name" column as name, it's just alleles
    self->varAllelesIx = asColumnFindIx(varAsColumns, "name");
    }
else
    {
    boolean looksLikeVcf = TRUE;
    self->varNameIx = asColumnFindIx(varAsColumns, "name");
    if (looksLikeVcf)
	self->getSlashSepAlleles = getSlashSepAllelesFromVcf;
    else
	errAbort("afVepSetConfig: can't figure out how to extract variant alleles");
    }
if (config->gpVarSource == NULL)
    errAbort("afVepSetConfig: config must have a gpVarSource");
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

struct annoFormatter *annoFormatVepNew(char *fileName, struct annoFormatVepConfig *config)
/* Return a formatter that will write functional predictions in the same format as Ensembl's
 * Variant Effect Predictor to fileName, interpreting input rows according to config. */
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
self->needHeader = TRUE;
afVepSetConfig(self, config);
return (struct annoFormatter *)self;
}
