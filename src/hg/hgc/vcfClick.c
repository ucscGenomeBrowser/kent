/* vcfTrack -- handlers for Variant Call Format data. */

#ifdef USE_TABIX

#include "common.h"
#include "dystring.h"
#include "errCatch.h"
#include "hCommon.h"
#include "hdb.h"
#include "hgc.h"
#include "htmshell.h"
#include "jsHelper.h"
#if (defined USE_TABIX && defined KNETFILE_HOOKS)
#include "knetUdc.h"
#include "udc.h"
#endif//def USE_TABIX && KNETFILE_HOOKS
#include "pgSnp.h"
#include "trashDir.h"
#include "vcf.h"
#include "vcfUi.h"

#define NA "<em>n/a</em>"

static void printKeysWithDescriptions(struct vcfFile *vcff, int wordCount, char **words,
				      struct vcfInfoDef *infoDefs)
/* Given an array of keys, print out a list of values with
 * descriptions if descriptions are available. */
{
int i;
for (i = 0;  i < wordCount; i++)
    {
    if (i > 0)
	printf(", ");
    char *key = words[i];
    const struct vcfInfoDef *def = vcfInfoDefForKey(vcff, key);
    if (def != NULL)
	printf("%s (%s)", htmlEncode(key), def->description);
    else
	printf("%s", htmlEncode(key));
    }
printf("<BR>\n");
}

static void printListWithDescriptions(struct vcfFile *vcff, char *str, char *sep, struct vcfInfoDef *infoDefs)
/* Given a VCF field, its separator char and a list of vcfInfoDefs, print out a list
 * of values with descriptions if descriptions are available. */
{
char *copy = cloneString(str);
char *words[256];
int wordCount = chopString(copy, sep, words, ArraySize(words));
printKeysWithDescriptions(vcff, wordCount, words, infoDefs);
}

static void vcfAltAlleleDetails(struct vcfRecord *rec)
/* If VCF header specifies any symbolic alternate alleles, pull in descriptions. */
{
printf("<B>Alternate allele(s):</B> ");
if (sameString(rec->alt, "."))
    {
    printf(NA"<BR>\n");
    return;
    }
struct vcfFile *vcff = rec->file;
printListWithDescriptions(vcff, rec->alt, ",", vcff->altDefs);
}

static void vcfQualDetails(struct vcfRecord *rec)
/* If VCF header specifies a quality/confidence score (not "."), print it out. */
{
printf("<B>Quality/confidence score:</B> %s<BR>\n", sameString(rec->qual, ".") ? NA : rec->qual);
}

static void vcfFilterDetails(struct vcfRecord *rec)
/* If VCF header specifies any filters, pull in descriptions. */
{
if (rec->filterCount == 0 || sameString(rec->filters[0], "."))
    printf("<B>Filter:</B> "NA"<BR>\n");
else if (rec->filterCount == 1 && sameString(rec->filters[0], "PASS"))
    printf("<B>Filter:</B> PASS<BR>\n");
else
    {
    printf("<B>Filter failures:</B> ");
    printf("<font style='font-weight: bold; color: #FF0000;'>\n");
    struct vcfFile *vcff = rec->file;
    printKeysWithDescriptions(vcff, rec->filterCount, rec->filters, vcff->filterDefs);
    printf("</font>\n");
    }
}

static void vcfInfoDetails(struct vcfRecord *rec)
/* Expand info keys to descriptions, then print out keys and values. */
{
if (rec->infoCount == 0)
    return;
struct vcfFile *vcff = rec->file;
puts("<B>INFO column annotations:</B><BR>");
puts("<TABLE border=0 cellspacing=0 cellpadding=0>");
int i;
for (i = 0;  i < rec->infoCount;  i++)
    {
    struct vcfInfoElement *el = &(rec->infoElements[i]);
    const struct vcfInfoDef *def = vcfInfoDefForKey(vcff, el->key);
    if (def == NULL)
	continue;
    printf("<TR><TD align=\"right\"><B>%s:</B></TD><TD>&nbsp;", el->key);
    int j;
    enum vcfInfoType type = def->type;
    if (type == vcfInfoFlag && el->count == 0)
	printf("Yes"); // no values, so we can't call vcfPrintDatum...
    // However, if this is older VCF, type vcfInfoFlag might have a value.
    for (j = 0;  j < el->count;  j++)
	{
	if (j > 0)
	    printf(", ");
	vcfPrintDatum(stdout, el->values[j], type);
	}
    if (def != NULL)
	printf("</TD><TD>&nbsp;%s", def->description);
    else
	printf("</TD><TD>");
    printf("</TD></TR>\n");
    }
puts("</TABLE>");
}

static char *hapFromIx(char *ref, char *altAlleles[], unsigned char altAlCount, unsigned char hapIx)
/* Look up the allele specified by hapIx: 0 = ref, 1 & up = offset index into altAlleles */
{
if (hapIx == 0)
    return ref;
else if (hapIx-1 < altAlCount)
    return altAlleles[hapIx-1];
else
    errAbort("hapFromIx: index %d is out of range (%d alleles specified)", hapIx, altAlCount+1);
return NULL;
}

static void vcfGenotypesDetails(struct vcfRecord *rec, char *track)
/* Print genotypes in some kind of table... */
{
struct vcfFile *vcff = rec->file;
if (vcff->genotypeCount == 0)
    return;
static struct dyString *tmp1 = NULL, *tmp2 = NULL;
if (tmp1 == NULL)
    {
    tmp1 = dyStringNew(0);
    tmp2 = dyStringNew(0);
    }
jsBeginCollapsibleSection(cart, track, "genotypes", "Detailed genotypes", FALSE);
vcfParseGenotypes(rec);
dyStringClear(tmp1);
dyStringAppend(tmp1, rec->format);
enum vcfInfoType formatTypes[256];
char *formatKeys[256];
int formatCount = chopString(tmp1->string, ":", formatKeys, ArraySize(formatKeys));
puts("<B>Genotype info key:</B><BR>");
int i;
for (i = 0;  i < formatCount;  i++)
    {
    if (sameString(formatKeys[i], vcfGtGenotype))
	continue;
    const struct vcfInfoDef *def = vcfInfoDefForGtKey(vcff, formatKeys[i]);
    char *desc = def ? def->description : "<em>not described in VCF header</em>";
    printf("&nbsp;&nbsp;<B>%s:</B> %s<BR>\n", formatKeys[i], desc);
    formatTypes[i] = def->type;
    }
hTableStart();
puts("<TR><TH>Sample ID</TH><TH>Genotype</TH><TH>Phased?</TH>");
for (i = 0;  i < formatCount;  i++)
    {
    if (sameString(formatKeys[i], vcfGtGenotype))
	continue;
    printf("<TH>%s</TH>", formatKeys[i]);
    }
puts("</TR>\n");
dyStringClear(tmp2);
dyStringAppend(tmp2, rec->alt);
char *altAlleles[256];
unsigned char altCount = chopCommas(tmp2->string, altAlleles);
for (i = 0;  i < vcff->genotypeCount;  i++)
    {
    struct vcfGenotype *gt = &(rec->genotypes[i]);
    char *hapA = hapFromIx(rec->ref, altAlleles, altCount, gt->hapIxA);
    char *hapB = gt->isHaploid ? NA : hapFromIx(rec->ref, altAlleles, altCount, gt->hapIxB);
    char sep = gt->isPhased ? '|' : '/';
    char *phasing = gt->isHaploid ? NA : gt->isPhased ? "Y" : "n";
    printf("<TR><TD>%s</TD><TD>%s%c%s</TD><TD>%s</TD>", vcff->genotypeIds[i],
	   hapA, sep, hapB, phasing);
    int j;
    for (j = 0;  j < formatCount;  j++)
	{
	if (sameString(formatKeys[j], vcfGtGenotype))
	    continue;
	printf("<TD>");
	struct vcfInfoElement *el = &(gt->infoElements[j]);
	int k;
	for (k = 0;  k < el->count;  k++)
	    {
	    if (k > 0)
		printf(", ");
	    vcfPrintDatum(stdout, el->values[k], formatTypes[j]);
	    }
	printf("</TD>");
	}
    puts("</TR>");
    }
hTableEnd();
jsEndCollapsibleSection();
}

static void pgSnpCodingDetail(struct vcfRecord *rec)
/* Translate rec into pgSnp (with proper chrom name) and call Belinda's
 * coding effect predictor from pgSnp details. */
{
if (hTableExists(database, "knownGene"))
    {
    struct pgSnp *pgs = pgSnpFromVcfRecord(rec);
    if (!sameString(rec->chrom, seqName))
	// rec->chrom might be missing "chr" prefix:
	pgs->chrom = seqName;
    printSeqCodDisplay(database, pgs);
    }
}

static void vcfRecordDetails(struct trackDb *tdb, struct vcfRecord *rec)
/* Display the contents of a single line of VCF, assumed to be from seqName
 * (using seqName instead of rec->chrom because rec->chrom might lack "chr"). */
{
printf("<B>Name:</B> %s<BR>\n", rec->name);
printCustomUrl(tdb, rec->name, TRUE);
static char *formName = "vcfCfgHapCenter";
printf("<FORM NAME=\"%s\" ACTION=\"%s\">\n", formName, hgTracksName());
vcfCfgHaplotypeCenter(cart, tdb, rec->file, rec->name, seqName, rec->chromStart, formName);
printf("</FORM>\n");
printPosOnChrom(seqName, rec->chromStart, rec->chromEnd, NULL, FALSE, rec->name);
printf("<B>Reference allele:</B> %s<BR>\n", rec->ref);
vcfAltAlleleDetails(rec);
vcfQualDetails(rec);
vcfFilterDetails(rec);
vcfInfoDetails(rec);
pgSnpCodingDetail(rec);
// Wrapper table for collapsible section:
puts("<TABLE>");
vcfGenotypesDetails(rec, tdb->track);
puts("</TABLE>");
}

void doVcfTabixDetails(struct trackDb *tdb, char *item)
/* Show details of an alignment from a VCF file compressed and indexed by tabix. */
{
#if (defined USE_TABIX && defined KNETFILE_HOOKS)
knetUdcInstall();
if (udcCacheTimeout() < 300)
    udcSetCacheTimeout(300);
#endif//def USE_TABIX && KNETFILE_HOOKS
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct sqlConnection *conn = hAllocConnTrack(database, tdb);
// TODO: will need to handle per-chrom files like bam, maybe fold bamFileNameFromTable into this::
char *fileOrUrl = bbiNameFromSettingOrTable(tdb, conn, tdb->table);
hFreeConn(&conn);
int vcfMaxErr = 100;
struct vcfFile *vcff = NULL;
/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    vcff = vcfTabixFileMayOpen(fileOrUrl, seqName, start, end, vcfMaxErr);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    if (isNotEmpty(errCatch->message->string))
	warn("%s", errCatch->message->string);
    }
errCatchFree(&errCatch);
if (vcff != NULL)
    {
    struct vcfRecord *rec;
    for (rec = vcff->records;  rec != NULL;  rec = rec->next)
	if (rec->chromStart == start && rec->chromEnd == end) // in pgSnp mode, don't get name
	    vcfRecordDetails(tdb, rec);
    }
else
    printf("Sorry, unable to open %s<BR>\n", fileOrUrl);
}


#endif // no USE_TABIX
