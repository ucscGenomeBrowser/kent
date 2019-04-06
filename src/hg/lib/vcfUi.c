/* vcfUi - Variant Call Format user interface controls that are shared
 * between more than one CGI. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "cheapcgi.h"
#include "errCatch.h"
#include "hCommon.h"
#include "hui.h"
#include "jsHelper.h"
#include "vcf.h"
#include "vcfUi.h"
#include "knetUdc.h"
#include "udc.h"

INLINE char *nameOrDefault(char *thisName, char *defaultVal)
/* If thisName is not a placeholder value, return it; otherwise return default. */
{
if (isNotEmpty(thisName) && !sameString(thisName, "."))
    return thisName;
return defaultVal;
}

#define VCF_HAPLOSORT_DEFAULT_DESC "middle variant in viewing window"

static void vcfCfgHaplotypeCenterHiddens(char *track, char *ctrName, char *ctrChrom, int ctrPos)
/* Make hidden form inputs and button for setting the center variant for haplotype
 * clustering/sorting in hgTracks. */
{
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s.centerVariantChrom", track);
cgiMakeHiddenVar(cartVar, ctrChrom);
safef(cartVar, sizeof(cartVar), "%s.centerVariantPos", track);
char ctrPosStr[16];
safef(ctrPosStr, sizeof(ctrPosStr), "%d", ctrPos);
cgiMakeHiddenVar(cartVar, ctrPosStr);
safef(cartVar, sizeof(cartVar), "%s.centerVariantName", track);
cgiMakeHiddenVar(cartVar, ctrName);
}

void vcfCfgHaplotypeCenter(struct cart *cart, struct trackDb *tdb, char *track,
			   boolean parentLevel, struct vcfFile *vcff,
			   char *thisName, char *thisChrom, int thisPos, char *formName)
/* If vcff has genotype data, show status and controls for choosing the center variant
 * for haplotype clustering/sorting in hgTracks. */
{
if (vcff != NULL && vcff->genotypeCount > 1)
    {
    printf("<TABLE cellpadding=0><TR><TD>"
	   "<B>Haplotype sorting order:</B> using ");
    char *centerChrom = cartOptionalStringClosestToHome(cart, tdb, parentLevel,
							"centerVariantChrom");
    if (isEmpty(centerChrom))
	{
	// Unspecified in cart -- describe the default action
	printf(VCF_HAPLOSORT_DEFAULT_DESC " as anchor.</TD></TR>\n");
	if (isNotEmpty(thisChrom))
	    {
	    // but we do have a candidate, so offer to make it the center:
	    puts("<TR><TD>");
	    vcfCfgHaplotypeCenterHiddens(track, thisName, thisChrom, thisPos);
	    char label[256];
	    safef(label, sizeof(label), "Use %s", nameOrDefault(thisName, "this variant"));
	    cgiMakeButton("setCenterSubmit", label);
	    printf(" as anchor</TD></TR>\n");
	    }
	else
	    printf("<TR><TD>To anchor the sorting to a particular variant, "
		   "click on the variant in the genome browser, "
		   "and then click on the 'Use this variant' button on the next page."
		   "</TD></TR>\n");
	}
    else
	{
	// Describe the one specified in cart.
	int centerPos = cartUsualIntClosestToHome(cart, tdb, parentLevel, "centerVariantPos",
						  -1);
	char *centerName = cartStringClosestToHome(cart, tdb, parentLevel, "centerVariantName");
	if (isNotEmpty(thisChrom))
	    {
	    // These form inputs are for either "use me" or clear:
	    vcfCfgHaplotypeCenterHiddens(track, thisName, thisChrom, thisPos);
	    // Is this variant the same as the center variant specified in cart?
	    if (sameString(thisChrom, centerChrom) && sameString(thisName, centerName) &&
		thisPos == centerPos)
		printf("this variant as anchor.</TD></TR>\n");
	    else
		{
		// make a "use me" button
		printf("%s at %s:%d as anchor.</TD></TR>\n<TR><TD>\n",
		       nameOrDefault(centerName, "variant"), centerChrom, centerPos+1);
		char label[256];
		safef(label, sizeof(label), "Use %s", nameOrDefault(thisName, "this variant"));
		cgiMakeButton("replaceCenterSubmit", label);
		printf(" as anchor</TD></TR>\n");
		}
	    }
	else
	    {
	    // Form inputs (in case the clear button is clicked)
	    vcfCfgHaplotypeCenterHiddens(track, centerName, centerChrom, centerPos);
	    printf("%s at %s:%d as anchor.</TD></TR>\n",
		   nameOrDefault(centerName, "variant"), centerChrom, centerPos+1);
	    }
	// Make a clear button that modifies the hiddens using onClick
	puts("<TR><TD>");
	struct dyString *onClick = dyStringNew(0);
	dyStringPrintf(onClick, "updateOrMakeNamedVariable(%s, '%s.centerVariantChrom', ''); ",
		       formName, track);
	dyStringPrintf(onClick, "updateOrMakeNamedVariable(%s, '%s.centerVariantName', ''); ",
		       formName, track);
	dyStringPrintf(onClick, "updateOrMakeNamedVariable(%s, '%s.centerVariantPos', 0);",
		       formName, track);
	dyStringPrintf(onClick, "document.%s.submit(); return false;", formName);
	cgiMakeButtonWithOnClick("clearCenterSubmit", "Clear selection", NULL, onClick->string);
	printf(" (use " VCF_HAPLOSORT_DEFAULT_DESC ")</TD></TR>\n");
	}
    puts("</TABLE>");
    }
}

//TODO: share this code w/hgTracks, hgc in hg/lib/vcfFile.c
static struct vcfFile *vcfHopefullyOpenHeader(struct cart *cart, struct trackDb *tdb)
/* Defend against network errors and return the vcfFile object with header data, or NULL. */
{
knetUdcInstall();
if (udcCacheTimeout() < 300)
    udcSetCacheTimeout(300);
char *fileOrUrl = trackDbSetting(tdb, "bigDataUrl");
if (isEmpty(fileOrUrl))
    {
    char *db = cartString(cart, "db");
    char *table = tdb->table;
    char *dbTableName = trackDbSetting(tdb, "dbTableName");
    struct sqlConnection *conn;
    if (isCustomTrack(tdb->track) && isNotEmpty(dbTableName))
        {
        conn =  hAllocConn(CUSTOM_TRASH);
        table = dbTableName;
        }
    else
        conn = hAllocConnTrack(db, tdb);
    char *chrom = cartOptionalString(cart, "c");
    if (chrom != NULL)
        fileOrUrl = bbiNameFromSettingOrTableChrom(tdb, conn, table, chrom);
    if (fileOrUrl == NULL)
        fileOrUrl = bbiNameFromSettingOrTableChrom(tdb, conn, table, hDefaultChrom(db));
    hFreeConn(&conn);
    }
if (fileOrUrl == NULL)
    return NULL;
int vcfMaxErr = 100;
struct vcfFile *vcff = NULL;
/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    if (startsWithWord("vcfTabix", tdb->type))
	vcff = vcfTabixFileMayOpen(fileOrUrl, NULL, 0, 0, vcfMaxErr, -1);
    else
	vcff = vcfFileMayOpen(fileOrUrl, NULL, 0, 0, vcfMaxErr, -1, FALSE);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    if (isNotEmpty(errCatch->message->string))
	warn("unable to open %s: %s", fileOrUrl, errCatch->message->string);
    }
errCatchFree(&errCatch);
return vcff;
}

static void vcfCfgHapClusterEnable(struct cart *cart, struct trackDb *tdb, char *name,
				   boolean parentLevel)
/* Let the user enable/disable haplotype sorting display. */
{
boolean hapClustEnabled = cartOrTdbBoolean(cart, tdb, VCF_HAP_ENABLED_VAR, TRUE);
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s." VCF_HAP_ENABLED_VAR, name);
cgiMakeCheckBox(cartVar, hapClustEnabled);
printf("<B>Enable Haplotype sorting display</B><BR>\n");
}

static void vcfCfgHapClusterColor(struct cart *cart, struct trackDb *tdb, char *name,
				   boolean parentLevel)
/* Let the user choose how to color the sorted haplotypes. */
{
printf("<B>Haplotype coloring scheme:</B><BR>\n");
char *colorBy = cartOrTdbString(cart, tdb, VCF_HAP_COLORBY_VAR, VCF_DEFAULT_HAP_COLORBY);
char varName[1024];
safef(varName, sizeof(varName), "%s." VCF_HAP_COLORBY_VAR, name);
cgiMakeRadioButton(varName, VCF_HAP_COLORBY_ALTONLY, sameString(colorBy, VCF_HAP_COLORBY_ALTONLY));
printf("reference alleles invisible, alternate alleles in black<BR>\n");
cgiMakeRadioButton(varName, VCF_HAP_COLORBY_REFALT, sameString(colorBy, VCF_HAP_COLORBY_REFALT));
printf("reference alleles in blue, alternate alleles in red<BR>\n");
cgiMakeRadioButton(varName, VCF_HAP_COLORBY_BASE, sameString(colorBy, VCF_HAP_COLORBY_BASE));
printf("first base of allele (A = red, C = blue, G = green, T = magenta)<BR>\n");
}

static void vcfCfgHapClusterTreeAngle(struct cart *cart, struct trackDb *tdb, char *name,
				   boolean parentLevel)
/* Let the user choose branch shape. */
{
printf("<B>Haplotype clustering tree leaf shape:</B><BR>\n");
char *treeAngle = cartOrTdbString(cart, tdb, VCF_HAP_TREEANGLE_VAR, VCF_DEFAULT_HAP_TREEANGLE);
char varName[1024];
safef(varName, sizeof(varName), "%s." VCF_HAP_TREEANGLE_VAR, name);
cgiMakeRadioButton(varName, VCF_HAP_TREEANGLE_TRIANGLE,
		   sameString(treeAngle, VCF_HAP_TREEANGLE_TRIANGLE));
printf("draw leaf clusters as &lt;<BR>\n");
cgiMakeRadioButton(varName, VCF_HAP_TREEANGLE_RECTANGLE,
		   sameString(treeAngle, VCF_HAP_TREEANGLE_RECTANGLE));
printf("draw leaf clusters as [<BR>\n");
}

static void vcfCfgHapClusterHeight(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff,
				   char *name, boolean parentLevel)
/* Let the user specify a height for the track. */
{
if (vcff != NULL && vcff->genotypeCount > 1)
    {
    printf("<B>Haplotype sorting display height:</B> \n");
    int cartHeight = cartOrTdbInt(cart, tdb, VCF_HAP_HEIGHT_VAR, VCF_DEFAULT_HAP_HEIGHT);
    char varName[1024];
    safef(varName, sizeof(varName), "%s." VCF_HAP_HEIGHT_VAR, name);
    cgiMakeIntVarInRange(varName, cartHeight, "Height (in pixels) of track", 5, "4", "2500");
    puts("<BR>");
    }
}

static void vcfCfgHapCluster(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff,
			     char *name, boolean parentLevel)
/* Show controls for haplotype-sorting display, which only makes sense to do when
 * the VCF file describes multiple genotypes. */
{
vcfCfgHapClusterEnable(cart, tdb, name, parentLevel);
vcfCfgHaplotypeCenter(cart, tdb, name, parentLevel, vcff, NULL, NULL, 0, "mainForm");
vcfCfgHapClusterColor(cart, tdb, name, parentLevel);
vcfCfgHapClusterTreeAngle(cart, tdb, name, parentLevel);
vcfCfgHapClusterHeight(cart, tdb, vcff, name, parentLevel);
}

static void vcfCfgMinQual(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff,
			  char *name, boolean parentLevel)
/* If checkbox is checked, apply minimum value filter to QUAL column. */
{
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s." VCF_APPLY_MIN_QUAL_VAR, name);
boolean applyFilter = cartOrTdbBoolean(cart, tdb, VCF_APPLY_MIN_QUAL_VAR,
				       VCF_DEFAULT_APPLY_MIN_QUAL);
cgiMakeCheckBox(cartVar, applyFilter);
printf("<B>Exclude variants with Quality/confidence score (QUAL) score less than</B>\n");
double minQual = cartOrTdbDouble(cart, tdb, VCF_MIN_QUAL_VAR, VCF_DEFAULT_MIN_QUAL);
safef(cartVar, sizeof(cartVar), "%s." VCF_MIN_QUAL_VAR, name);
cgiMakeDoubleVar(cartVar, minQual, 10);
printf("<BR>\n");
}

static void vcfCfgFilterColumn(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff,
			       char *name, boolean parentLevel)
/* Show controls for filtering by value of VCF's FILTER column, which uses values defined
 * in the header. */
{
int filterCount = slCount(vcff->filterDefs);
if (filterCount < 1)
    return;
printf("<B>Exclude variants with these FILTER values:</B><BR>\n");
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s."VCF_EXCLUDE_FILTER_VAR, name);
jsMakeCheckboxGroupSetClearButton(cartVar, TRUE);
puts("&nbsp;");
jsMakeCheckboxGroupSetClearButton(cartVar, FALSE);
char *values[filterCount];
char *labels[filterCount];
int i;
struct vcfInfoDef *filt;
for (i=0, filt=vcff->filterDefs;  filt != NULL;  i++, filt = filt->next)
    {
    values[i] = filt->key;
    struct dyString *dy = dyStringNew(0);
    dyStringAppend(dy, filt->key);
    if (isNotEmpty(filt->description))
	dyStringPrintf(dy, " (%s)", filt->description);
    labels[i] = dyStringCannibalize(&dy);
    }
struct slName *selectedValues = NULL;
if (cartListVarExistsAnyLevel(cart, tdb, FALSE, VCF_EXCLUDE_FILTER_VAR))
    selectedValues = cartOptionalSlNameListClosestToHome(cart, tdb, FALSE, VCF_EXCLUDE_FILTER_VAR);
cgiMakeCheckboxGroupWithVals(cartVar, labels, values, filterCount, selectedValues, 1);
}

static void vcfCfgMinAlleleFreq(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff,
				char *name, boolean parentLevel)
/* Show input for minimum allele frequency, if we can extract it from the VCF INFO column. */
{
printf("<B>Minimum minor allele frequency (if INFO column includes AF or AC+AN):</B>\n");
double cartMinFreq = cartOrTdbDouble(cart, tdb, VCF_MIN_ALLELE_FREQ_VAR,
				     VCF_DEFAULT_MIN_ALLELE_FREQ);
char varName[1024];
safef(varName, sizeof(varName), "%s." VCF_MIN_ALLELE_FREQ_VAR, name);
cgiMakeDoubleVarInRange(varName, cartMinFreq, "minor allele frequency between 0.0 and 0.5", 5,
			"0.0", "0.5");
puts("<BR>");
}

void vcfCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed)
/* VCF: Variant Call Format.  redmine #3710 */
{
boxed = cfgBeginBoxAndTitle(tdb, boxed, title);
printf("<TABLE%s><TR><TD>", boxed ? " width='100%'" : "");
struct vcfFile *vcff = vcfHopefullyOpenHeader(cart, tdb);
if (vcff != NULL)
    {
    boolean parentLevel = isNameAtParentLevel(tdb, name);
    if (vcff->genotypeCount > 1)
	{
	puts("<H3>Haplotype sorting display</H3>");
	puts("<P>When this display mode is enabled and genotypes are phased or homozygous, "
	     "each genotype is split into two independent haplotypes. "
	     "These local haplotypes are clustered by similarity around a central variant. "
	     "Haplotypes are reordered for display using the clustering tree, which is "
	     "drawn in the left label area. "
	     "Local haplotype blocks can often be identified using this display.</P>");
	vcfCfgHapCluster(cart, tdb, vcff, name, parentLevel);
	}
    if (differentString(tdb->track,"evsEsp6500"))
        {
        puts("<H3>Filters</H3>");
        vcfCfgMinQual(cart, tdb, vcff, name, parentLevel);
        vcfCfgFilterColumn(cart, tdb, vcff, name, parentLevel);
        }
    vcfCfgMinAlleleFreq(cart, tdb, vcff, name, parentLevel);
    }
else
    {
    printf("Sorry, couldn't access VCF file.<BR>\n");
    }

puts("</TD>");
if (boxed && fileExists(hHelpFile("hgVcfTrackHelp")))
    printf("<TD style='text-align:right'><A HREF=\"../goldenPath/help/hgVcfTrackHelp.html\" "
           "TARGET=_BLANK>VCF configuration help</A></TD>");

printf("</TR></TABLE>");

if (!boxed && fileExists(hHelpFile("hgVcfTrackHelp")))
    printf("<P><A HREF=\"../goldenPath/help/hgVcfTrackHelp.html\" TARGET=_BLANK>VCF "
	   "configuration help</A></P>");
cfgEndBox(boxed);
}

