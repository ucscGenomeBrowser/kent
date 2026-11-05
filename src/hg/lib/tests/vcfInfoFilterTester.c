/* vcfInfoFilterTester - check the trackDb filters and colors on VCF INFO fields.
 *
 * A VCF track can filter its items on INFO fields with the same trackDb settings a bigBed
 * uses on its columns (filter.*, filterText.*, filterValues.*), and can color its items by
 * the value of one INFO field (colorByInfo).  Both also reach a named sub-field of a
 * pipe-separated annotation such as vep.Consequence, whose names come from the "Format:"
 * clause of the ##INFO description.
 *
 * On a page, a filter that drops the wrong items or a color taken from the wrong annotation
 * looks like data.  This test reads a five-line VCF, builds each filter and color map from a
 * hand-made trackDb and cart, and prints what happens to every record.
 *
 * refs #37617, #37618 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "errAbort.h"
#include "cart.h"
#include "hui.h"
#include "trackDb.h"
#include "vcf.h"
#include "vcfUi.h"

#define TRACK "vcfTest"

static char *vcfFileName = "input/vcfInfoFilter/test.vcf";
static struct vcfFile *vcff = NULL;

static void warnToStdout(char *format, va_list args)
/* Print a warning in line with the rest of the output, so the expected file shows which
 * case it came from. */
{
printf("    warning: ");
vprintf(format, args);
printf("\n");
}

static struct trackDb *makeTdb(char *settings)
/* Make a trackDb for a vcfTabix track from newline-separated settings. */
{
struct trackDb *tdb;
AllocVar(tdb);
tdb->track = cloneString(TRACK);
tdb->table = cloneString(TRACK);
tdb->type = cloneString("vcfTabix");
tdb->settings = cloneString(settings);
tdb->settingsHash = trackDbSettingsFromString(tdb, tdb->settings);
return tdb;
}

static struct cart *makeCart()
/* A cart with nothing in it.  Only its hash is read by the filter code. */
{
struct cart *cart;
AllocVar(cart);
cart->hash = hashNew(0);
return cart;
}

static void showFilters(char *what, char *settings, struct cart *cart)
/* Build the filters for one trackDb and print which records pass them. */
{
printf("%s\n", what);
struct trackDb *tdb = makeTdb(settings);
if (cart == NULL)
    cart = makeCart();
struct vcfInfoFilter *filters = buildVcfInfoFilters(vcff, cart, tdb);
printf("    %d filter%s\n", slCount(filters), slCount(filters) == 1 ? "" : "s");
printf("    pass:");
struct vcfRecord *rec;
for (rec = vcff->records; rec != NULL; rec = rec->next)
    if (filters == NULL || vcfInfoFilterOneRecord(rec, filters))
        printf(" %s", rec->name);
printf("\n");
}

static void filterTests()
/* Each filter type on a top-level INFO field, a sub-field, and the cart overriding trackDb. */
{
printf("FILTERS\n");
showFilters("no filter settings", "shortLabel test", NULL);
showFilters("filter.AF 0.01 (a minimum; a missing AF fails)", "filter.AF 0.01", NULL);
showFilters("filter.AF 0:0.3 (a range)", "filter.AF 0:0.3", NULL);
showFilters("filter.AC 5 (an Integer field)", "filter.AC 5", NULL);

struct cart *cart = makeCart();
cartSetString(cart, TRACK ".filterMin.AF", "0.3");
showFilters("filter.AF 0.01 with the cart minimum at 0.3", "filter.AF 0.01", cart);

showFilters("filterText.GENE BRCA* (wildcard)", "filterText.GENE BRCA*", NULL);
showFilters("filterText.GENE ^TP (regular expression)",
            "filterText.GENE ^TP\nfilterType.GENE regexp", NULL);

cart = makeCart();
cartSetString(cart, TRACK ".filterBy.GENE", "TP53");
showFilters("filterValues.GENE with TP53 chosen in the cart",
            "filterValues.GENE BRCA1,BRCA2,TP53", cart);

cart = makeCart();
cartSetString(cart, TRACK ".filterBy.vep.Consequence", "missense_variant");
showFilters("filterValues.vep.Consequence, missense_variant chosen",
            "filterValues.vep.Consequence missense_variant,intron_variant,stop_gained\n"
            "filterType.vep.Consequence multipleListOr", cart);

cart = makeCart();
cartSetString(cart, TRACK ".filterBy.vep.Consequence", "intron_variant");
showFilters("filterValues.vep.Consequence, intron_variant chosen (a second annotation, "
            "and one term of an &-joined pair)",
            "filterValues.vep.Consequence missense_variant,intron_variant,stop_gained\n"
            "filterType.vep.Consequence multipleListOr", cart);

cart = makeCart();
cartAddString(cart, TRACK ".filterBy.vep.Consequence", "stop_gained");
cartAddString(cart, TRACK ".filterBy.vep.Consequence", "synonymous_variant");
showFilters("filterValues.vep.Consequence, two values chosen",
            "filterValues.vep.Consequence missense_variant,stop_gained,synonymous_variant\n"
            "filterType.vep.Consequence multipleListOr", cart);

showFilters("two filters together: filter.AF 0.01 and filterText.GENE BRCA*",
            "filter.AF 0.01\nfilterText.GENE BRCA*", NULL);
}

static void badFilterTests()
/* A filter the VCF cannot support is dropped with a warning, and the others still apply. */
{
printf("\nFILTERS THE VCF CANNOT SUPPORT\n");
showFilters("filter.NOPE 1 (not in the header)", "filter.NOPE 1", NULL);
showFilters("filter.GENE 1 (a number filter on a String field)", "filter.GENE 1", NULL);
showFilters("filterText.AF 0.5 (a text filter on a Float field)", "filterText.AF 0.5", NULL);
showFilters("filterText.PAIR a (a Number=2 field)", "filterText.PAIR a", NULL);

struct cart *cart = makeCart();
cartSetString(cart, TRACK ".filterBy.vep.Nope", "x");
showFilters("filterValues.vep.Nope (not in the Format clause)", "filterValues.vep.Nope x", cart);

cart = makeCart();
cartSetString(cart, TRACK ".filterBy.AF.x", "x");
showFilters("filterValues.AF.x (a sub-field of a Float field)", "filterValues.AF.x x", cart);

showFilters("filter.NOPE 1 beside filter.AF 0.01", "filter.NOPE 1\nfilter.AF 0.01", NULL);
}

static void subFieldIndexTests()
/* The sub-field names come from the Format clause of the INFO description. */
{
printf("\nSUB-FIELD INDEX\n");
struct vcfInfoDef *vep = vcfInfoDefForKey(vcff, "vep");
struct vcfInfoDef *gene = vcfInfoDefForKey(vcff, "GENE");
printf("    vep Allele      %d\n", vcfInfoDefSubFieldIndex(vep, "Allele"));
printf("    vep Consequence %d\n", vcfInfoDefSubFieldIndex(vep, "Consequence"));
printf("    vep SYMBOL      %d\n", vcfInfoDefSubFieldIndex(vep, "SYMBOL"));
printf("    vep Nope        %d\n", vcfInfoDefSubFieldIndex(vep, "Nope"));
printf("    vep empty name  %d\n", vcfInfoDefSubFieldIndex(vep, ""));
printf("    GENE (no Format clause) %d\n", vcfInfoDefSubFieldIndex(gene, "SYMBOL"));
printf("    no INFO def     %d\n", vcfInfoDefSubFieldIndex(NULL, "SYMBOL"));
}

static void showColors(char *what, char *settings)
/* Build the color map for one trackDb and print the color each record gets. */
{
printf("%s\n", what);
struct trackDb *tdb = makeTdb(settings);
struct vcfColorByInfo *cbi = vcfColorByInfoFromTdb(tdb, vcff);
if (cbi == NULL)
    {
    printf("    no color map\n");
    return;
    }
struct vcfRecord *rec;
for (rec = vcff->records; rec != NULL; rec = rec->next)
    {
    struct rgbColor color = {0, 0, 0, 0};
    if (vcfColorByInfoLookup(cbi, rec, &color))
        printf("    %s %d,%d,%d\n", rec->name, color.r, color.g, color.b);
    else
        printf("    %s default\n", rec->name);
    }
}

static void colorTests()
/* colorByInfo on a top-level field and on a sub-field, where one record can carry several
 * annotations and the value declared first in the setting wins. */
{
printf("\nCOLORS\n");
showColors("colorByInfo CLASS, a hex code and a color name",
           "colorByInfo CLASS\ncolorByInfo.CLASS pathogenic=#FF0000,benign=green");
showColors("colorByInfo vep.Consequence, stop_gained declared before synonymous_variant",
           "colorByInfo vep.Consequence\n"
           "colorByInfo.vep.Consequence stop_gained=#D55E00,splice_donor_variant=#D55E00,"
           "missense_variant=#E69F00,synonymous_variant=#009E73");
showColors("colorByInfo vep.Consequence, synonymous_variant declared first",
           "colorByInfo vep.Consequence\n"
           "colorByInfo.vep.Consequence synonymous_variant=#009E73,stop_gained=#D55E00");
showColors("an entry that is not a color is skipped, the rest still apply",
           "colorByInfo CLASS\ncolorByInfo.CLASS pathogenic=notAColor,benign=#0000FF");
showColors("no colorByInfo setting", "shortLabel test");
showColors("colorByInfo with no colorByInfo.<FIELD> map", "colorByInfo CLASS");
showColors("colorByInfo AF (a Float field)", "colorByInfo AF\ncolorByInfo.AF 0.5=#FF0000");
showColors("colorByInfo NOPE (not in the header)", "colorByInfo NOPE\ncolorByInfo.NOPE x=#FF0000");
showColors("colorByInfo vep.Nope (not in the Format clause)",
           "colorByInfo vep.Nope\ncolorByInfo.vep.Nope x=#FF0000");
}

int main(int argc, char *argv[])
{
pushWarnHandler(warnToStdout);
vcff = vcfFileMayOpen(vcfFileName, NULL, 0, 0, 0, -1, TRUE);
if (vcff == NULL)
    errAbort("could not open %s", vcfFileName);
printf("%d records in %s\n\n", slCount(vcff->records), vcfFileName);
filterTests();
badFilterTests();
subFieldIndexTests();
colorTests();
return 0;
}
