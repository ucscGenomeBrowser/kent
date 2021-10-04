/* annoFormatVep -- write functional predictions in the same format as Ensembl's
 * Variant Effect Predictor to fileName, interpreting input rows according to config.
 * See http://uswest.ensembl.org/info/docs/variation/vep/vep_formats.html */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoFormatVep.h"
#include "annoGratorGpVar.h"
#include "annoGratorQuery.h"
#include "annoStreamDbPslPlus.h"
#include "asParse.h"
#include "bigGenePred.h"
#include "dystring.h"
#include "genePred.h"
#include "gpFx.h"
#include "hdb.h"
#include "hgHgvs.h"
#include "htmshell.h"
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
    boolean isRegulatory;			// TRUE if overlap implies regulatory_region
    boolean isBoolean;				// TRUE if we don't need to print out the value,
						// only whether it is found
    };

struct annoFormatVep;

struct annoFormatVepExtraSource
    // A streamer or grator that supplies at least one value for Extras output column.
    {
    struct annoFormatVepExtraSource *next;
    struct annoStreamer *source;		// streamer or grator: same pointers as below
    struct annoFormatVepExtraItem *items;	// one or more columns of source and their tags

    void (*printExtra)(struct annoFormatVepExtraSource *self,
                       struct annoFormatVep *afVep, struct annoFormatVepExtraItem *extraItem,
		       struct annoRow *extraRows, struct annoRow *gpvRow, boolean *pGotExtra);
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

enum afVariantDataType
    // What type of primary data (variants) are we integrating with?
    {
    afvdtInvalid,                       // uninitialized/unknown
    afvdtVcf,                           // primary data source is VCF
    afvdtPgSnpTable,                    // primary data source is pgSnp table with bin in .as
    afvdtPgSnpFile                      // primary data source is pgSnp file, no bin
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
    struct annoAssembly *assembly;	// Reference assembly (for sequence)
    int lmRowCount;			// counter for periodic localmem cleanup
    int varNameIx;			// Index of name column from variant source, or -1 if N/A
    int varAllelesIx;			// Index of alleles column from variant source, or -1
    int txNameIx;                       // Index of transcript name from (big)genePred/PSL
    int geneNameIx;			// Index of gene name from (big)genePred/PSL+ if included
    int hgvsGIx;			// Index of HGVS g. column from annoGratorGpVar
    int hgvsCNIx;			// Index of HGVS c./n. column from annoGratorGpVar
    int hgvsPIx;			// Index of HGVS p. column from annoGratorGpVar
    int snpNameIx;			// Index of name column from dbSNP source, or -1
    boolean needHeader;			// TRUE if we should print out the header
    enum afVariantDataType variantType; // Are variants VCF or a flavor of pgSnp?
    boolean doHtml;			// TRUE if we should include html tags & make a <table>.
    struct seqWindow *gSeqWin;          // genomic sequence fetcher for HGVS term generation
    boolean hgvsMakeG;                  // Generate genomic (g.) HGVS terms only if this is set
    boolean hgvsBreakDelIns;            // Include deleted sequence (not only ins) e.g. delGGinsAT
    };


INLINE void afVepLineBreak(FILE *f, boolean doHtml)
/* Depending on whether we're printing HTML, print either a newline or <BR>. */
{
if (doHtml)
    fputs("<BR>", f);
fputc('\n', f);
}

INLINE void afVepStartRow(FILE *f, boolean doHtml)
/* If we're printing HTML, print a <TR><TD>. */
{
if (doHtml)
    fputs("<TR><TD>", f);
}

INLINE void afVepNextColumn(FILE *f, boolean doHtml)
/* Depending on whether we're printing HTML, print either a tab or </TD><TD>. */
{
if (doHtml)
    fputs("</TD><TD>", f);
else
    fputc('\t', f);
}

INLINE void afVepEndRow(FILE *f, boolean doHtml)
/* If we're printing HTML, print a </TD></TR>, otherwise, just a newline. */
{
if (doHtml)
    fputs("</TD></TR>\n", f);
else
    fputc('\n', f);
}

INLINE void afVepPuts(char *text, struct annoFormatVep *self)
/* Write text to self->f.  If we're printing HTML, write encoded text, otherwise just write text. */
{
if (self->doHtml)
    htmTextOut(self->f, text);
else
    fputs(text, self->f);
}

static void afVepPrintf(struct annoFormatVep *self, char *format, ...)
/*  Printf to self->f, but if self->doHtml then encode before writing out. */
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
    ;

static void afVepPrintf(struct annoFormatVep *self, char *format, ...)
/*  Printf to self->f, but if self->doHtml then encode before writing out. */
{
va_list args;
va_start(args, format);
if (self->doHtml)
    vaHtmlFprintf(self->f, format, args);
else
    vfprintf(self->f, format, args);
va_end(args);
}

static void afVepPrintHeaderExtraTags(struct annoFormatVep *self, char *bStart, char *bEnd)
/* For each extra column described in config, write out its tag and a brief description.
 * bStart and bEnd are for bold output in HTML, or if not HTML, starting the line with "## ". */
{
struct annoFormatVepExtraSource *extras = self->config->extraSources, *extraSrc;
if (extras == NULL)
    return;
fprintf(self->f, "%sKeys for Extra column items:%s", bStart, bEnd);
afVepLineBreak(self->f, self->doHtml);
for (extraSrc = extras;  extraSrc != NULL;  extraSrc = extraSrc->next)
    {
    struct annoFormatVepExtraItem *extraItem;
    for (extraItem = extraSrc->items;  extraItem != NULL;  extraItem = extraItem->next)
	if (isNotEmpty(extraItem->tag))
	    {
	    fprintf(self->f, "%s%s%s: %s", bStart, extraItem->tag, bEnd, extraItem->description);
	    afVepLineBreak(self->f, self->doHtml);
	    }
    }
}

static void afVepPrintHeaderDate(FILE *f, boolean doHtml)
/* VEP header includes a date formatted like "2012-06-16 16:09:38" */
{
long now = clock1();
struct tm *tm = localtime(&now);
fprintf(f, "## Output produced at %d-%02d-%02d %02d:%02d:%02d",
	1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
afVepLineBreak(f, doHtml);
}

static char *columnLabels[] = { "Uploaded Variation",
				"Location",
				"Allele",
				"Gene",
				"Feature",
				"Feature type",
				"Consequence",
				"Position in cDNA",
				"Position in CDS",
				"Position in protein",
				"Amino acid change",
				"Codon change",
				"Co-located Variation",
				"Extra",
				NULL };

static void afVepPrintColumnLabels(struct annoFormatVep *self)
/* If we're printing HTML, begin a table and use TH for labels; otherwise just
 * print out a tab-sep text line with labels.  VEP doesn't begin this line with a #! */
{
char **pLabel = columnLabels;
if (self->doHtml)
    {
    fputs("<BR><TABLE class='stdTbl'>\n<TR><TH>", self->f);
    fputs(*pLabel++, self->f);
    while (*pLabel != NULL)
	{
	fputs("</TH><TH>", self->f);
	fputs(*pLabel++, self->f);
	}
    fputs("</TH></TR>\n", self->f);
    }
else
    {
    fputs(*pLabel++, self->f);
    while (*pLabel != NULL)
	{
	fputc('\t', self->f);
	fputs(*pLabel++, self->f);
	}
    fputc('\n', self->f);
    }
}

static void afVepPrintHeader(struct annoFormatVep *self, char *db)
/* Print a header that looks almost like a VEP header. */
{
FILE *f = self->f;
boolean doHtml = self->doHtml;
char *bStart = doHtml ? "<B>" : "## ";
char *bEnd = doHtml ? "</B>" : "";
if (!doHtml)
    {
    // Suppress these lines from HTML output -- IMO they're better suited for a file header:
    fputs("## ENSEMBL VARIANT EFFECT PREDICTOR format (UCSC Variant Annotation Integrator)", f);
    afVepLineBreak(f, doHtml);
    afVepPrintHeaderDate(f, doHtml);
    fprintf(f, "## Connected to UCSC database %s", db);
    afVepLineBreak(f, doHtml);
    }
struct annoFormatVepConfig *config = self->config;
fprintf(f, "%sVariants:%s ", bStart, bEnd);
afVepPrintf(self, "%s", config->variantDescription);
if (! strstr(config->variantSource->name, "/trash/"))
    fprintf(f, " (%s)", config->variantSource->name);
afVepLineBreak(f, doHtml);
fprintf(f, "%sTranscripts:%s %s (%s)", bStart, bEnd,
	config->gpVarDescription, config->gpVarSource->name);
afVepLineBreak(f, doHtml);
if (config->snpSource != NULL)
    {
    fprintf(f, "%sdbSNP:%s %s (%s)", bStart, bEnd,
	    config->snpDescription, config->snpSource->name);
    afVepLineBreak(f, doHtml);
    }
afVepPrintHeaderExtraTags(self, bStart, bEnd);
afVepPrintColumnLabels(self);
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

static void afVepComment(struct annoFormatter *fSelf, char *content)
/* Print out a comment, either starting with "# " or as a warnBox depending on doHtml. */
{
if (strchr(content, '\n'))
    errAbort("afVepComment: no multi-line input");
struct annoFormatVep *self = (struct annoFormatVep *)fSelf;
if (self->doHtml)
    warn("%s", content);
else
    fprintf(self->f, "# %s\n", content);
}

static void compressDashes(char *string)
/* If string has a run of '-' characters, turn it into single '-'. */
{
char *p = string;
while ((p = strchr(p, '-')) != NULL)
    {
    char *end = p+1;
    while (*end == '-')
	end++;
    if (end > p+1)
	memmove(p+1, end, strlen(end)+1);
    p++;
    }
}

static char *afVepGetAlleles(struct annoFormatVep *self, struct annoRow *varRow)
/* Return a string with slash-separated alleles.  Result is alloc'd by self->lm. */
{
char **varWords = (char **)(varRow->data);
char *alleles = NULL;
if (self->variantType == afvdtVcf)
    alleles = lmCloneString(self->lm, vcfGetSlashSepAllelesFromWords(varWords, self->dyScratch));
else if (self->varAllelesIx >= 0)
    alleles = lmCloneString(self->lm, varWords[self->varAllelesIx]);
else
    errAbort("annoFormatVep: afVepSetConfig didn't specify how to get alleles");
compressDashes(alleles);
return alleles;
}

static void afVepPrintNameAndLoc(struct annoFormatVep *self, struct annoRow *varRow)
/* Print variant name and position in genome. */
{
char **varWords = (char **)(varRow->data);
uint start1Based = varRow->start + 1;
// Use variant name if available, otherwise construct an identifier:
if (self->varNameIx >= 0 && !sameString(varWords[self->varNameIx], "."))
    afVepPuts(varWords[self->varNameIx], self);
else
    {
    char *alleles = afVepGetAlleles(self, varRow);
    afVepPrintf(self, "%s_%u_%s", varRow->chrom, start1Based, alleles);
    }
afVepNextColumn(self->f, self->doHtml);
// Location is chr:start for single-base, chr:start-end for indels:
if (varRow->end == start1Based)
    afVepPrintf(self, "%s:%u", varRow->chrom, start1Based);
else if (start1Based > varRow->end)
    afVepPrintf(self, "%s:%u-%u", varRow->chrom, varRow->end, start1Based);
else
    afVepPrintf(self, "%s:%u-%u", varRow->chrom, start1Based, varRow->end);
afVepNextColumn(self->f, self->doHtml);
}

INLINE void afVepPrintPlaceholders(FILE *f, int count, boolean doHtml)
/* VEP uses "-" for N/A.  Sometimes there are several consecutive N/A columns.
 * Count = 0 means print "-" with no tab. Count > 0 means print that many "-" columns. */
{
if (count == 0)
    fputc('-', f);
else
    {
    int i;
    for (i = 0;  i < count;  i++)
	{
	fputc('-', f);
	afVepNextColumn(f, doHtml);
	}
    }
}

#define afVepPrintPlaceholder(f, doHtml) afVepPrintPlaceholders(f, 1, doHtml)

INLINE char *placeholderForEmpty(char *val)
/* Return VEP placeholder "-" if val is empty, otherwise return val. */
{
return (isEmpty(val) ? "-" : val);
}


static void afVepPrintGene(struct annoFormatVep *self, struct annoRow *gpvRow)
/* If the genePred portion of gpvRow contains a gene name (in addition to transcript name),
 * print it out; otherwise print a placeholder. */
{
if (self->geneNameIx >= 0)
    {
    char **words = (char **)(gpvRow->data);
    afVepPuts(placeholderForEmpty(words[self->geneNameIx]), self);
    afVepNextColumn(self->f, self->doHtml);
    }
else
    afVepPrintPlaceholder(self->f, self->doHtml);
}

static void limitLength(char *seq, int baseCount, char *unit)
/* If seq is longer than an abbreviated version of itself, change it to the abbreviated version. */
{
if (isEmpty(seq))
    return;
int len = strlen(seq);
const int elipsLen = 3;
char lengthNote[512];
safef(lengthNote, sizeof(lengthNote), "(%d %s)", len, unit);
if (len > 2*baseCount + elipsLen + strlen(lengthNote))
    {
    // First baseCount bases, then "...":
    int offset = baseCount;
    safecpy(seq+offset, len+1-offset, "...");
    offset += elipsLen;
    // then last baseCount bases:
    safecpy(seq+offset, len+1-offset, seq+len-baseCount);
    offset += baseCount;
    // then lengthNote:
    safecpy(seq+offset, len+1-offset, lengthNote);
    }
}

static void tweakStopCodonAndLimitLength(char *aaSeq, char *codonSeq)
/* If aa from gpFx has a stop 'Z', replace it with "*".
 * If the strings are very long, truncate with a note about how long they are. */
{
char *earlyStop = strchr(aaSeq, 'Z');
if (earlyStop)
    {
    earlyStop[0] = '*';
    earlyStop[1] = '\0';
    }
limitLength(aaSeq, 5, "aa");
limitLength(codonSeq, 12, "nt");
}

INLINE char *dashForEmpty(char *s)
/* Represent empty alleles in insertions/deletions by the customary "-". */
{
if (isEmpty(s))
    return "-";
else
    return s;
}

static void afVepPrintPredictions(struct annoFormatVep *self, struct annoRow *varRow,
				  struct annoRow *gpvRow, struct gpFx *gpFx)
/* Print VEP columns computed by annoGratorGpVar (or placeholders) */
{
// variant allele used to calculate the consequence (or first alternate allele)
char *abbrevAllele = cloneString(gpFx->gAllele);
limitLength(abbrevAllele, 24, "nt");
afVepPuts(placeholderForEmpty(abbrevAllele), self);
afVepNextColumn(self->f, self->doHtml);
// ID of affected gene
afVepPrintGene(self, gpvRow);
// ID of feature
afVepPrintf(self, "%s", placeholderForEmpty(gpFx->transcript));
afVepNextColumn(self->f, self->doHtml);
// type of feature {Transcript, RegulatoryFeature, MotifFeature}
if (gpFx->soNumber == intergenic_variant)
    afVepPrintPlaceholder(self->f, self->doHtml);
else
    {
    fputs("Transcript", self->f);
    afVepNextColumn(self->f, self->doHtml);
    }
// consequence: SO term e.g. splice_region_variant
afVepPuts(soTermToString(gpFx->soNumber), self);
afVepNextColumn(self->f, self->doHtml);
if (gpFx->detailType == codingChange)
    {
    struct codingChange *change = &(gpFx->details.codingChange);
    uint refLen = strlen(change->txRef), altLen = strlen(change->txAlt);
    boolean isInsertion = (refLen == 0);
    boolean isDeletion = altLen < refLen;
    if (isInsertion)
	{
	fprintf(self->f, "%u-%u", change->cDnaPosition, change->cDnaPosition+1);
	afVepNextColumn(self->f, self->doHtml);
	fprintf(self->f, "%u-%u", change->cdsPosition, change->cdsPosition+1);
	}
    else if (isDeletion && refLen > 1)
	{
	fprintf(self->f, "%u-%u", change->cDnaPosition+1, change->cDnaPosition+refLen);
	afVepNextColumn(self->f, self->doHtml);
	fprintf(self->f, "%u-%u", change->cdsPosition+1, change->cdsPosition+refLen);
	}
    else
	{
	fprintf(self->f, "%u", change->cDnaPosition+1);
	afVepNextColumn(self->f, self->doHtml);
	fprintf(self->f, "%u", change->cdsPosition+1);
	}
    afVepNextColumn(self->f, self->doHtml);
    fprintf(self->f, "%u", change->pepPosition+1);
    afVepNextColumn(self->f, self->doHtml);
    int variantFrame = change->cdsPosition % 3;
    strLower(change->codonOld);
    int upLen = min(strlen(change->codonOld+variantFrame), refLen);
    toUpperN(change->codonOld+variantFrame, upLen);
    strLower(change->codonNew);
    int alleleLength = altLen;
    // watch out for symbolic alleles [actually now that we're using txAlt we shouldn't see these]:
    if (sameString(change->txAlt, "<X>") || sameString(change->txAlt, "<*>"))
        alleleLength = upLen;
    else if (startsWith("<", change->txAlt))
        alleleLength = 0;
    toUpperN(change->codonNew+variantFrame, alleleLength);
    tweakStopCodonAndLimitLength(change->aaOld, change->codonOld);
    tweakStopCodonAndLimitLength(change->aaNew, change->codonNew);
    fprintf(self->f, "%s/%s", dashForEmpty(change->aaOld), dashForEmpty(change->aaNew));
    afVepNextColumn(self->f, self->doHtml);
    fprintf(self->f, "%s/%s", dashForEmpty(change->codonOld), dashForEmpty(change->codonNew));
    afVepNextColumn(self->f, self->doHtml);
    }
else if (gpFx->detailType == nonCodingExon)
    {
    struct nonCodingExon *change = &(gpFx->details.nonCodingExon);
    uint refLen = strlen(change->txRef), altLen = strlen(change->txAlt);
    boolean isInsertion = (refLen == 0);
    boolean isDeletion = altLen < refLen;
    int cDnaPosition = change->cDnaPosition;
    if (isInsertion)
	fprintf(self->f, "%u-%u", cDnaPosition, cDnaPosition+1);
    else if (isDeletion)
	fprintf(self->f, "%u-%u", cDnaPosition+1, cDnaPosition+refLen);
    else
	fprintf(self->f, "%u", cDnaPosition+1);
    afVepNextColumn(self->f, self->doHtml);
    // Coding effect columns (except for cDnaPosition) are N/A:
    afVepPrintPlaceholders(self->f, 4, self->doHtml);
    }
else
    // Coding effect columns are N/A:
    afVepPrintPlaceholders(self->f, 5, self->doHtml);
freez(&abbrevAllele);
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
	int count = 0;
	for (row = snpRows;  row != NULL;  row = row->next)
	    {
	    char **snpWords = (char **)(row->data);
	    if (row->start == varRow->start && row->end == varRow->end)
		{
		if (count > 0)
		    fputc(',', self->f);
		afVepPrintf(self, "%s", snpWords[self->snpNameIx]);
		count++;
		}
	    }
	if (count == 0)
	    afVepPrintPlaceholder(self->f, self->doHtml);
	else
	    afVepNextColumn(self->f, self->doHtml);
	}
    else
	afVepPrintPlaceholder(self->f, self->doHtml);
    }
else
    afVepPrintPlaceholder(self->f, self->doHtml);
}

static boolean isCodingSnv(struct annoRow *primaryRow, struct gpFx *gpFx)
/* Return TRUE if this is a single-nucleotide non-synonymous change. */
{
if (gpFx == NULL ||
    gpFx->detailType != codingChange ||
    gpFx->soNumber == synonymous_variant)
    return FALSE;
char *txRef = gpFx->details.codingChange.txRef;
char *txAlt = gpFx->details.codingChange.txAlt;
if (txRef == NULL || txAlt == NULL ||
    strlen(txRef) != 1 || sameString(txRef, "-") ||
    strlen(txAlt) != 1 || sameString(txAlt, "-"))
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

static char *commaSepWordFromIxOrWhole(int ix, char *s, struct lm *lm)
/* Treating comma-separated, non-NULL s as an array of words,
 * return the word at ix in the array -- but if s has no commas,
 * just return (lmCloned) s.  errAborts if s is comma-sep but ix is out of bounds. */
{
int i = 0;
char *p = strchr(s, ',');
if (p == NULL)
    return lmCloneString(lm, s);
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
errAbort("commaSepWordFromIxOrWhole: Bad index %d for string '%s'", ix, s);
return NULL;
}

INLINE void afVepNewExtra(struct annoFormatVep *self, boolean *pGotExtra)
/* If we already printed an extra item, print the extra column's separator; set pGotExtra. */
{
assert(pGotExtra);
if (*pGotExtra)
    {
    if (self->doHtml)
	fputs("; ", self->f);
    else
	fputc(';', self->f);
    }
*pGotExtra = TRUE;
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
    ensTxIdIx = asColumnMustFindIx(columns, "ensTxId");
    altAl1Ix = asColumnMustFindIx(columns, "altAl1");
    score1Ix = asColumnMustFindIx(columns, "score1");
    altAl2Ix = asColumnMustFindIx(columns, "altAl2");
    score2Ix = asColumnMustFindIx(columns, "score2");
    altAl3Ix = asColumnMustFindIx(columns, "altAl3");
    score3Ix = asColumnMustFindIx(columns, "score3");
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
    if (sameString(gpFx->gAllele, words[altAl1Ix]))
	scoreStr = words[score1Ix];
    else if (sameString(gpFx->gAllele, words[altAl2Ix]))
	scoreStr = words[score2Ix];
    else if (sameString(gpFx->gAllele, words[altAl3Ix]))
	scoreStr = words[score3Ix];
    double score = atof(scoreStr);
    char prediction = '?';
    if (score < 0.05)
	prediction = 'D';
    else
	prediction = 'T';
    if (isNotEmpty(scoreStr) && differentString(scoreStr, "."))
	{
	afVepNewExtra(self, pGotExtra);
	fprintf(self->f, "%s=%c(%s)", extraItem->tag, prediction, scoreStr);
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
    aaPosIx = asColumnMustFindIx(columns, "uniProtAaPos");
    altAl1Ix = asColumnMustFindIx(columns, "altAl1");
    hDivScore1Ix = asColumnMustFindIx(columns, "hDivScore1");
    hDivPred1Ix = asColumnMustFindIx(columns, "hDivPred1");
    hVarScore1Ix = asColumnMustFindIx(columns, "hVarScore1");
    hVarPred1Ix = asColumnMustFindIx(columns, "hVarPred1");
    altAl2Ix = asColumnMustFindIx(columns, "altAl2");
    hDivScore2Ix = asColumnMustFindIx(columns, "hDivScore2");
    hDivPred2Ix = asColumnMustFindIx(columns, "hDivPred2");
    hVarScore2Ix = asColumnMustFindIx(columns, "hVarScore2");
    hVarPred2Ix = asColumnMustFindIx(columns, "hVarPred2");
    altAl3Ix = asColumnMustFindIx(columns, "altAl3");
    hDivScore3Ix = asColumnMustFindIx(columns, "hDivScore3");
    hDivPred3Ix = asColumnMustFindIx(columns, "hDivPred3");
    hVarScore3Ix = asColumnMustFindIx(columns, "hVarScore3");
    hVarPred3Ix = asColumnMustFindIx(columns, "hVarPred3");
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
	if (sameString(gpFx->gAllele, words[altAl1Ix]))
	    {
	    predIx = isHdiv ? hDivPred1Ix : hVarPred1Ix;
	    scoreIx = isHdiv ? hDivScore1Ix : hVarScore1Ix;
	    }
	else if (sameString(gpFx->gAllele, words[altAl2Ix]))
	    {
	    predIx = isHdiv ? hDivPred2Ix : hVarPred2Ix;
	    scoreIx = isHdiv ? hDivScore2Ix : hVarScore2Ix;
	    }
	else if (sameString(gpFx->gAllele, words[altAl3Ix]))
	    {
	    predIx = isHdiv ? hDivPred3Ix : hVarPred3Ix;
	    scoreIx = isHdiv ? hDivScore3Ix : hVarScore3Ix;
	    }
	char *pred = (predIx < 0) ? NULL : words[predIx];
	if (pred == NULL || sameString(pred, "."))
	    continue;
	pred = commaSepWordFromIxOrWhole(txIx, pred, self->lm);
	if (isNotEmpty(pred))
	    {
	    if (count == 0)
		{
		afVepNewExtra(self, pGotExtra);
		fprintf(self->f, "%s=", extraItem->tag);
		}
	    else
		fputc(',', self->f);
	    char *score = (scoreIx < 0) ? "?" : commaSepWordFromIxOrWhole(txIx, words[scoreIx], self->lm);
	    fprintf(self->f, "%s(%s)", pred, score);
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
    ensTxIdIx = asColumnMustFindIx(columns, "ensTxId");
    altAl1Ix = asColumnMustFindIx(columns, "altAl1");
    score1Ix = asColumnMustFindIx(columns, "score1");
    pred1Ix = asColumnMustFindIx(columns, "pred1");
    altAl2Ix = asColumnMustFindIx(columns, "altAl2");
    score2Ix = asColumnMustFindIx(columns, "score2");
    pred2Ix = asColumnMustFindIx(columns, "pred2");
    altAl3Ix = asColumnMustFindIx(columns, "altAl3");
    score3Ix = asColumnMustFindIx(columns, "score3");
    pred3Ix = asColumnMustFindIx(columns, "pred3");
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
    if (sameString(gpFx->gAllele, words[altAl1Ix]))
	{
	score = words[score1Ix];
	pred = words[pred1Ix];
	}
    else if (sameString(gpFx->gAllele, words[altAl2Ix]))
	{
	score = words[score2Ix];
	pred = words[pred2Ix];
	}
    else if (sameString(gpFx->gAllele, words[altAl3Ix]))
	{
	score = words[score3Ix];
	pred = words[pred3Ix];
	}
    if (isNotEmpty(score) && differentString(score, "."))
	{
	afVepNewExtra(self, pGotExtra);
	if (isEmpty(pred))
	    pred = "?";
	fprintf(self->f, "%s=%s(%s)", extraItem->tag, pred, score);
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
    ensTxIdIx = asColumnMustFindIx(columns, "ensTxId");
    altAl1Ix = asColumnMustFindIx(columns, "altAl1");
    score1Ix = asColumnMustFindIx(columns, "score1");
    pred1Ix = asColumnMustFindIx(columns, "pred1");
    altAl2Ix = asColumnMustFindIx(columns, "altAl2");
    score2Ix = asColumnMustFindIx(columns, "score2");
    pred2Ix = asColumnMustFindIx(columns, "pred2");
    altAl3Ix = asColumnMustFindIx(columns, "altAl3");
    score3Ix = asColumnMustFindIx(columns, "score3");
    pred3Ix = asColumnMustFindIx(columns, "pred3");
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
    if (sameString(gpFx->gAllele, words[altAl1Ix]))
	{
	score = words[score1Ix];
	pred = words[pred1Ix];
	}
    else if (sameString(gpFx->gAllele, words[altAl2Ix]))
	{
	score = words[score2Ix];
	pred = words[pred2Ix];
	}
    else if (sameString(gpFx->gAllele, words[altAl3Ix]))
	{
	score = words[score3Ix];
	pred = words[pred3Ix];
	}
    if (isNotEmpty(score) && differentString(score, "."))
	{
	afVepNewExtra(self, pGotExtra);
	if (isEmpty(pred))
	    pred = "?";
	fprintf(self->f, "%s=%s(%s)", extraItem->tag, pred, score);
	}
    }
}

static void afVepPrintDbNsfpVest(struct annoFormatVep *self,
                                 struct annoFormatVepExtraSource *extraSrc,
                                 struct annoRow *extraRows, struct gpFx *gpFx, char *ensTxId,
                                 boolean *pGotExtra)
/* Match the allele from gpFx to the per-allele scores in row from dbNsfpVest */
{
// TODO: also compare var[123] protein change columns to gpFx to make sure we have the right one
// Look up column indices only once:
static int ensTxIdIx=-1, altAl1Ix, score1Ix, altAl2Ix, score2Ix, altAl3Ix, score3Ix;
if (ensTxIdIx == -1)
    {
    struct asColumn *columns = extraSrc->source->asObj->columnList;
    ensTxIdIx = asColumnMustFindIx(columns, "ensTxId");
    altAl1Ix = asColumnMustFindIx(columns, "altAl1");
    score1Ix = asColumnMustFindIx(columns, "score1");
    altAl2Ix = asColumnMustFindIx(columns, "altAl2");
    score2Ix = asColumnMustFindIx(columns, "score2");
    altAl3Ix = asColumnMustFindIx(columns, "altAl3");
    score3Ix = asColumnMustFindIx(columns, "score3");
    }

struct annoRow *row;
for (row = extraRows;  row != NULL;  row = row->next)
    {
    // Skip this row unless it contains the ensTxId found by getDbNsfpEnsTx
    char **words = row->data;
    if (differentString(ensTxId, ".") && differentString(words[ensTxIdIx], ".") &&
	commaSepFindIx(ensTxId, words[ensTxIdIx]) < 0)
	continue;
    struct annoFormatVepExtraItem *extraItem = extraSrc->items;
    char *score = NULL;
    if (sameString(gpFx->gAllele, words[altAl1Ix]))
	score = words[score1Ix];
    else if (sameString(gpFx->gAllele, words[altAl2Ix]))
	score = words[score2Ix];
    else if (sameString(gpFx->gAllele, words[altAl3Ix]))
	score = words[score3Ix];
    if (isNotEmpty(score) && differentString(score, "."))
	{
	afVepNewExtra(self, pGotExtra);
	fprintf(self->f, "%s=%s", extraItem->tag, score);
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
    domainsIx = asColumnMustFindIx(columns, "domains");
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
	afVepNewExtra(self, pGotExtra);
	fprintf(self->f, "%s=%s", extraItem->tag, domains);
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
	if (strcasecmp(cc->codonOld, words[7]) != 0)
	    continue;
	if (!allelesAgree(gpFx->gAllele[0], cc->aaNew[0], words))
	    continue;
	int txIx = commaSepFindIntIx(cc->pepPosition+1, words[10]);
	if (txIx >= 0)
	    {
	    if (sameString(words[4], "."))
		return ".";
	    char *ensTxId = commaSepWordFromIxOrWhole(txIx, words[4], self->lm);
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
        else if (sameString(asObjName, "dbNsfpVest"))
	    afVepPrintDbNsfpVest(self, extraSrc, extraRows, gpFx, ensTxId, pGotExtra);
	else if (sameString(asObjName, "dbNsfpInterPro"))
	    afVepPrintDbNsfpInterPro(self, extraSrc, extraRows, gpFx, ensTxId, pGotExtra);
	else if (!sameString(asObjName, "dbNsfpSeqChange"))
	    errAbort("Unrecognized asObj->name '%s' from dbNSFP source '%s'",
		     asObjName, extraSrc->source->name);
	}
    }
}

static void afVepPrintExtrasHgvs(struct annoFormatVep *self, struct annoRow *gpvRow,
                                 boolean *pGotExtra)
/* If not empty, print HGVS terms (last column of gpvRow). */
{
char **row = gpvRow->data;
if (isNotEmpty(row[self->hgvsGIx]))
    {
    afVepNewExtra(self, pGotExtra);
    fprintf(self->f, "HGVSG=%s", row[self->hgvsGIx]);
    }
if (isNotEmpty(row[self->hgvsCNIx]))
    {
    afVepNewExtra(self, pGotExtra);
    fprintf(self->f, "HGVSCN=%s", row[self->hgvsCNIx]);
    }
if (isNotEmpty(row[self->hgvsPIx]))
    {
    afVepNewExtra(self, pGotExtra);
    fprintf(self->f, "HGVSP=%s", row[self->hgvsPIx]);
    }
}

static void afVepPrintExtraWigVec(struct annoFormatVepExtraSource *extraSrc,
                                  struct annoFormatVep *self,
                                  struct annoFormatVepExtraItem *extraItem,
                                  struct annoRow *extraRows, struct annoRow *gpvRow,
                                  boolean *pGotExtra)
/* Print values from a wig source with type arWigVec (per-base floats) */
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
	if (i+j == 0)
	    {
	    afVepNewExtra(self, pGotExtra);
	    fprintf(self->f, "%s=", extraItem->tag);
	    }
	else
	    fputc(',', self->f);
	fprintf(self->f, "%g", vector[j]);
	}
    }
}

static void afVepPrintExtraWigSingle(struct annoFormatVepExtraSource *extraSrc,
                                     struct annoFormatVep *self,
                                     struct annoFormatVepExtraItem *extraItem,
                                     struct annoRow *extraRows, struct annoRow *gpvRow,
                                     boolean *pGotExtra)
/* Print values from a wig source with type arWigSingle (one double per row, e.g. an average) */
{
struct annoRow *row;
for (row = extraRows;  row != NULL;  row = row->next)
    {
    double *pValue = row->data;
    if (row == extraRows)
        {
        afVepNewExtra(self, pGotExtra);
        fprintf(self->f, "%s=", extraItem->tag);
        }
    else
        fputc(',', self->f);
    fprintf(self->f, "%g", *pValue);
    }
}

static void afVepPrintExtraWords(struct annoFormatVepExtraSource *extraSrc,
                                 struct annoFormatVep *self,
				 struct annoFormatVepExtraItem *extraItem,
				 struct annoRow *extraRows, struct annoRow *gpvRow,
                                 boolean *pGotExtra)
/* Print comma-separated values in the specified column from the usual array-of-words. */
{
if (extraItem->rowIx < 0)
    errAbort("annoFormatVep: invalid rowIx for tag %s", extraItem->tag);
char **gpvWords = gpvRow ? gpvRow->data : NULL;
char *gpvTranscript = gpvWords ? gpvWords[self->txNameIx] : NULL;
boolean gotGpvTx = FALSE;
int i;
struct annoRow *row;
for (i = 0, row = extraRows;  row != NULL;  row = row->next)
    {
    char **words = row->data;
    char *extraTranscript = words[0];
    // If extraSrc happens to be gpVarSource, use only the row that matches the current
    // transcript, and use it only once.
    if (extraSrc->source == self->config->gpVarSource)
        {
        extraTranscript = words[self->txNameIx];
        if (gpvTranscript == NULL || differentString(extraTranscript, gpvTranscript) || gotGpvTx)
            continue;
        gotGpvTx = TRUE;
        }
    char *val = words[extraItem->rowIx];
    if (isEmpty(val))
        continue;
    // Strip trailing comma if found
    int valLen = strlen(val);
    if (val[valLen-1] == ',')
        val[valLen-1] = '\0';
    if (i == 0)
	{
	afVepNewExtra(self, pGotExtra);
	fprintf(self->f, "%s=", extraItem->tag);
        if (extraItem->isBoolean)
            fputs("YES", self->f);
	}
    else if (! extraItem->isBoolean)
	fputc(',', self->f);
    if (! extraItem->isBoolean)
        {
        // Watch out for characters that will mess up parsing of EXTRAS column:
        subChar(val, '=', '_');
        subChar(val, ';', '_');
        afVepPuts(val, self);
        }
    i++;
    }
}

static void afVepPrintExtrasOther(struct annoFormatVep *self, struct annoRow *varRow,
				  struct annoRow *gpvRow, struct gpFx *gpFx,
				  struct annoStreamRows gratorData[], int gratorCount,
				  boolean includeRegulatory, boolean *pGotExtra)
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
	    if (includeRegulatory || ! extraItem->isRegulatory)
		extraSrc->printExtra(extraSrc, self, extraItem, extraRows, gpvRow, pGotExtra);
	}
    }
// VEP automatically adds DISTANCE for upstream/downstream variants
if (gpFx && (gpFx->soNumber == upstream_gene_variant || gpFx->soNumber == downstream_gene_variant))
    {
    afVepNewExtra(self, pGotExtra);
    // In order to really understand the boundary conditions of Ensembl's DISTANCE function,
    // I'll have to read the code someday.  Upstream deletions on + strand still not handled
    // quite right.
    int ensemblFudge = 0;
    int distance = gpvRow->start - varRow->start + ensemblFudge;
    if (distance < 0)
	{
	ensemblFudge = 1;
	distance = varRow->start - gpvRow->end + ensemblFudge;
	}
    fprintf(self->f, "DISTANCE=%d", distance);
    }
boolean includeExonNumber = TRUE;  //#*** optional in VEP
if (includeExonNumber && gpFx)
    {
    // Add Exon or intron number if applicable
    enum detailType deType = gpFx->detailType;
    int num = -1, count = 0;
    if (deType == codingChange)
        {
	num = gpFx->details.codingChange.exonNumber;
	count = gpFx->details.codingChange.exonCount;
        }
    else if (deType == nonCodingExon)
        {
	num = gpFx->details.nonCodingExon.exonNumber;
	count = gpFx->details.nonCodingExon.exonCount;
        }
    else if (deType == intron)
        {
	num = gpFx->details.intron.intronNumber;
        count = gpFx->details.intron.intronCount;
        }
    if (num >= 0)
	{
	afVepNewExtra(self, pGotExtra);
        fprintf(self->f, "%s=%d/%d", (deType == intron) ? "INTRON" : "EXON", num+1, count);
        }
    }
if (!*pGotExtra)
    afVepPrintPlaceholders(self->f, 0, self->doHtml);
}

static void afVepLmCleanup(struct annoFormatVep *self)
/* Clean out localmem every 1k rows. */
{
self->lmRowCount++;
if (self->lmRowCount > 1024)
    {
    lmCleanup(&(self->lm));
    self->lm = lmInit(0);
    self->lmRowCount = 0;
    }
}

static void afVepPrintOneLineGpVar(struct annoFormatVep *self, struct annoStreamRows *varData,
				   struct annoRow *gpvRow,
				   struct annoStreamRows gratorData[], int gratorCount)
/* Print one line of VEP with a coding gene consequence: a variant, an allele,
 * functional consequences of that allele, and whatever else is included in the config. */
{
afVepLmCleanup(self);
struct annoRow *varRow = varData->rowList;
struct gpFx *gpFx = annoGratorGpVarGpFxFromRow(self->config->gpVarSource, gpvRow, self->lm);
afVepStartRow(self->f, self->doHtml);
afVepPrintNameAndLoc(self, varRow);
afVepPrintPredictions(self, varRow, gpvRow, gpFx);
afVepPrintExistingVar(self, varRow, gratorData, gratorCount);
boolean gotExtra = FALSE;
afVepPrintExtrasDbNsfp(self, varRow, gpvRow, gpFx, gratorData, gratorCount, &gotExtra);
afVepPrintExtrasHgvs(self, gpvRow, &gotExtra);
afVepPrintExtrasOther(self, varRow, gpvRow, gpFx, gratorData, gratorCount, FALSE, &gotExtra);
afVepEndRow(self->f, self->doHtml);
}

static boolean afVepIsRegulatory(struct annoFormatVep *self,
				 struct annoStreamRows gratorData[], int gratorCount)
/* Return TRUE if one of the extraSource items is a regulatory region. */
{
struct annoFormatVepExtraSource *extras = self->config->extraSources, *extraSrc;
for (extraSrc = extras;  extraSrc != NULL;  extraSrc = extraSrc->next)
    {
    struct annoRow *extraRows = getRowsFromSource(extraSrc->source, gratorData, gratorCount);
    if (extraRows != NULL)
	{
	struct annoFormatVepExtraItem *extraItem;
	for (extraItem = extraSrc->items;  extraItem != NULL;  extraItem = extraItem->next)
	    {
	    if (extraItem->isRegulatory)
		return TRUE;
	    }
	}
    }
return FALSE;
}

static char *afVepGetFirstAltAllele(struct annoFormatVep *self, struct annoRow *varRow)
/* For VEP we must print out some alternate allele, whether the alternate allele is
 * related to any other info (like coding changes) or not.  Pick the first non-reference
 * allele. */
{
int refAlBufSize = varRow->end - varRow->start + 1;
char refAllele[refAlBufSize];
annoAssemblyGetSeq(self->assembly, varRow->chrom, varRow->start, varRow->end,
		   refAllele, sizeof(refAllele));
struct variant *variant = NULL;
if (self->variantType == afvdtVcf)
    variant = variantFromVcfAnnoRow(varRow, refAllele, self->lm, self->dyScratch);
else if (self->variantType == afvdtPgSnpTable)
    variant = variantFromPgSnpAnnoRow(varRow, refAllele, TRUE, self->lm);
else if (self->variantType == afvdtPgSnpFile)
    variant = variantFromPgSnpAnnoRow(varRow, refAllele, FALSE, self->lm);
return firstAltAllele(variant->alleles);
}

static void afVepPrintPredictionsReg(struct annoFormatVep *self, char *alt)
/* Print VEP allele, placeholder gene and item names, 'RegulatoryFeature',
 * 'regulatory_region_variant', and placeholder coding effect columns. */
{
afVepPuts(alt, self);
afVepNextColumn(self->f, self->doHtml);
afVepPrintPlaceholders(self->f, 2, self->doHtml);
fputs("RegulatoryFeature", self->f);
afVepNextColumn(self->f, self->doHtml);
fputs(soTermToString(regulatory_region_variant), self->f);
afVepNextColumn(self->f, self->doHtml);
// Coding effect columns are N/A:
afVepPrintPlaceholders(self->f, 5, self->doHtml);
}

static void afVepPrintExtrasHgvsGOnly(struct annoFormatVep *self, struct annoRow *varRow, char *alt,
                                      boolean *pGotExtra)
/* Make our own HGVS g. term (when we aren't taking HGVS terms from annoGratorGpVar). */
{
if (self->hgvsMakeG)
    {
    struct bed3 *variantBed = (struct bed3 *)varRow;
    char *chromAcc = hRefSeqAccForChrom(self->assembly->name, varRow->chrom);
    char *hgvsG = hgvsGFromVariant(self->gSeqWin, variantBed, alt, chromAcc, self->hgvsBreakDelIns);
    afVepNewExtra(self, pGotExtra);
    fprintf(self->f, "HGVSG=%s", hgvsG);
    freeMem(hgvsG);
    }
}

static void afVepPrintRegulatory(struct annoFormatVep *self, struct annoStreamRows *varData,
				 struct annoStreamRows gratorData[], int gratorCount)
/* If there are EXTRAS column sources that imply regulatory_region_variant, print out
 * lines with that consequence (no gpFx/dbNSFP). */
{
if (afVepIsRegulatory(self, gratorData, gratorCount))
    {
    afVepLmCleanup(self);
    struct annoRow *varRow = varData->rowList;
    afVepStartRow(self->f, self->doHtml);
    afVepPrintNameAndLoc(self, varRow);
    char *alt = afVepGetFirstAltAllele(self, varRow);
    afVepPrintPredictionsReg(self, alt);
    afVepPrintExistingVar(self, varRow, gratorData, gratorCount);
    boolean gotExtra = FALSE;
    afVepPrintExtrasHgvsGOnly(self, varRow, alt, &gotExtra);
    afVepPrintExtrasOther(self, varRow, NULL, NULL, gratorData, gratorCount, TRUE, &gotExtra);
    afVepEndRow(self->f, self->doHtml);
    }
}

static void afVepFormatOne(struct annoFormatter *fSelf, struct annoStreamRows *primaryData,
			   struct annoStreamRows gratorData[], int gratorCount)
/* Print one variant's VEP (possibly multiple lines) using collected rows. */
{
struct annoFormatVep *self = (struct annoFormatVep *)fSelf;
struct annoRow *gpVarRows = gratorData[0].rowList, *gpvRow;
for (gpvRow = gpVarRows;  gpvRow != NULL;  gpvRow = gpvRow->next)
    afVepPrintOneLineGpVar(self, primaryData, gpvRow, gratorData, gratorCount);
afVepPrintRegulatory(self, primaryData, gratorData, gratorCount);
}

static void afVepEndHtml(struct annoFormatVep *self)
{
fputs("</TABLE><BR>\n", self->f);
}

static void afVepClose(struct annoFormatter **pFSelf)
/* Close file handle, free self. */
{
if (pFSelf == NULL)
    return;
struct annoFormatVep *self = *(struct annoFormatVep **)pFSelf;
if (self->doHtml)
    afVepEndHtml(self);
freeMem(self->fileName);
carefulClose(&(self->f));
lmCleanup(&(self->lm));
dyStringFree(&(self->dyScratch));
chromSeqWindowFree(&self->gSeqWin);
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
    self->variantType = afvdtPgSnpTable;
else if (asObjectsMatch(config->variantSource->asObj, pgSnpFileAsObj()))
    self->variantType = afvdtPgSnpFile;
else if (asObjectsMatch(config->variantSource->asObj, vcfAsObj()))
    {
    self->variantType = afvdtVcf;
    self->varNameIx = asColumnMustFindIx(varAsColumns, "id");
    }
else
    errAbort("afVepSetConfig: variant source %s doesn't look like pgSnp or VCF",
	     config->variantSource->name);
if (self->variantType == afvdtPgSnpTable || self->variantType == afvdtPgSnpFile)
    {
    // pgSnp's "name" column actually contains slash-separated alleles
    self->varAllelesIx = asColumnMustFindIx(varAsColumns, "name");
    }
if (config->gpVarSource == NULL)
    errAbort("afVepSetConfig: config must have a gpVarSource");
else if (! asColumnNamesMatchFirstN(config->gpVarSource->asObj, genePredAsObj(), 10) &&
         ! asColumnNamesMatchFirstN(config->gpVarSource->asObj, bigGenePredAsObj(), 16) &&
         ! asColumnNamesMatchFirstN(config->gpVarSource->asObj, annoStreamDbPslPlusAsObj(),
                                    PSLPLUS_NUM_COLS))
    errAbort("afVepSetConfig: gpVarSource %s doesn't look like genePred or pslPlus",
	     config->gpVarSource->name);
// refGene and augmented knownGene have extra name fields that have the HGNC gene symbol:
struct asColumn *gpvAsColumns = config->gpVarSource->asObj->columnList;
self->txNameIx = asColumnFindIx(gpvAsColumns, "name");
if (self->txNameIx < 0)
    self->txNameIx = asColumnFindIx(gpvAsColumns, "qName");
if (self->txNameIx < 0)
    errAbort("afVepSetConfig: can't find name or qName column in gpVarSource %s",
             config->gpVarSource->name);
self->geneNameIx = asColumnFindIx(gpvAsColumns, "geneSymbol");
if (self->geneNameIx < 0)
    self->geneNameIx = asColumnFindIx(gpvAsColumns, "kgXref_geneSymbol");
if (self->geneNameIx < 0)
    self->geneNameIx = asColumnFindIx(gpvAsColumns, "name2");
if (self->geneNameIx < 0)
    self->geneNameIx = asColumnFindIx(gpvAsColumns, "proteinID");
self->hgvsGIx = asColumnMustFindIx(gpvAsColumns, "hgvsG");
self->hgvsCNIx = asColumnMustFindIx(gpvAsColumns, "hgvsCN");
self->hgvsPIx = asColumnMustFindIx(gpvAsColumns, "hgvsP");
if (config->snpSource != NULL)
    {
    struct asColumn *snpAsColumns = config->snpSource->asObj->columnList;
    self->snpNameIx = asColumnMustFindIx(snpAsColumns, "name");
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

struct annoFormatter *annoFormatVepNew(char *fileName, boolean doHtml,
				       struct annoStreamer *variantSource,
				       char *variantDescription,
				       struct annoStreamer *gpVarSource,
				       char *gpVarDescription,
				       struct annoStreamer *snpSource,
				       char *snpDescription,
				       struct annoAssembly *assembly)
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
fSelf->comment = afVepComment;
fSelf->formatOne = afVepFormatOne;
fSelf->close = afVepClose;
self->fileName = cloneString(fileName);
self->f = mustOpen(fileName, "w");
self->lm = lmInit(0);
self->dyScratch = dyStringNew(0);
self->needHeader = TRUE;
self->doHtml = doHtml;
self->assembly = assembly;
struct annoFormatVepConfig *config = annoFormatVepConfigNew(variantSource, variantDescription,
							    gpVarSource, gpVarDescription,
							    snpSource, snpDescription);
afVepSetConfig(self, config);
return (struct annoFormatter *)self;
}

static void afvAddExtraItemMaybeReg(struct annoFormatter *fSelf, struct annoStreamer *extraSource,
				    char *tag, char *description, char *column, boolean isBoolean,
                                    boolean isReg)
/* Tell annoFormatVep that it should include the given column of extraSource
 * in the EXTRAS column with tag.  The VEP header will include tag's description.
 * If isReg, overlap implies that the variant has consequence regulatory_region_variant.
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
    if (extraSource->rowType == arWigVec)
	src->printExtra = afVepPrintExtraWigVec;
    else if (extraSource->rowType == arWigSingle)
	src->printExtra = afVepPrintExtraWigSingle;
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
    item->rowIx = asColumnMustFindIx(extraSource->asObj->columnList, column);
item->isRegulatory = isReg;
item->isBoolean = isBoolean;
slAddTail(&(src->items), item);
}

void annoFormatVepAddExtraItem(struct annoFormatter *fSelf, struct annoStreamer *extraSource,
			       char *tag, char *description, char *column, boolean isBoolean)
/* Tell annoFormatVep that it should include the given column of extraSource
 * in the EXTRAS column with tag.  The VEP header will include tag's description.
 * For some special-cased sources e.g. dbNsfp files, column may be NULL/ignored.
 * If isBoolean, the column's description won't be output, only whether a match was found. */
{
afvAddExtraItemMaybeReg(fSelf, extraSource, tag, description, column, isBoolean, FALSE);
}

void annoFormatVepAddRegulatory(struct annoFormatter *fSelf, struct annoStreamer *regSource,
				char *tag, char *description, char *column)
/* Tell annoFormatVep to use the regulatory_region_variant consequence if there is overlap
 * with regSource, and to include the given column of regSource in the EXTRAS column.
 * The VEP header will include tag's description. */
{
afvAddExtraItemMaybeReg(fSelf, regSource, tag, description, column, FALSE, TRUE);
struct annoFormatVep *self = (struct annoFormatVep *)fSelf;
if (self->gSeqWin == NULL)
    {
    char *db = regSource->assembly->name;
    self->gSeqWin = chromSeqWindowNew(db, NULL, 0, 0);
    }
}

void annoFormatVepSetHgvsOutOptions(struct annoFormatter *fSelf, uint hgvsOutOptions)
/* Import the HGVS output options described in hgHgvs.h */
{
struct annoFormatVep *self = (struct annoFormatVep *)fSelf;
// We only do g. terms, for regulatory region variants, so no need for transcript/protein opts.
if (hgvsOutOptions & HGVS_OUT_G)
    self->hgvsMakeG = TRUE;
if (hgvsOutOptions & HGVS_OUT_BREAK_DELINS)
    self->hgvsBreakDelIns = TRUE;
}
