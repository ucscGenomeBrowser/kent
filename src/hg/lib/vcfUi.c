/* vcfUi - Variant Call Format user interface controls that are shared
 * between more than one CGI. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

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

char *vcfHaplotypeOrSample(struct cart *cart)
/* Return "Sample" if the current organism is uniploid (like SARS-CoV-2), "Haplotype" otherwise. */
{
// We should make a better way of determining whether the organism is diploid,
// but for now this will prevent David from being bothered by diploid terminology
// when viewing SARS-CoV-2 variants:
return sameOk(cartOptionalString(cart, "db"), "wuhCor1") ? "Sample" : "Haplotype";
}

void vcfCfgHaplotypeCenter(struct cart *cart, struct trackDb *tdb, char *track,
			   boolean parentLevel, struct vcfFile *vcff,
			   char *thisName, char *thisChrom, int thisPos, char *formName)
/* If vcff has genotype data, show status and controls for choosing the center variant
 * for haplotype clustering/sorting in hgTracks. */
{
if (vcff != NULL && vcff->genotypeCount > 1)
    {
    printf("using ");
    char *centerChrom = cartOptionalStringClosestToHome(cart, tdb, parentLevel,
							"centerVariantChrom");
    if (isEmpty(centerChrom))
	{
	// Unspecified in cart -- describe the default action
	printf(VCF_HAPLOSORT_DEFAULT_DESC " as anchor.</TD></TR>\n");
	if (isNotEmpty(thisChrom))
	    {
	    // but we do have a candidate, so offer to make it the center:
	    puts("<TR><TD></TD><TD>");
	    vcfCfgHaplotypeCenterHiddens(track, thisName, thisChrom, thisPos);
	    char label[256];
	    safef(label, sizeof(label), "Use %s", nameOrDefault(thisName, "this variant"));
	    cgiMakeButton("setCenterSubmit", label);
	    printf(" as anchor</TD></TR>\n");
	    }
	else
            {
	    printf("<TR><TD></TD><TD>");
            char *hapOrSample = vcfHaplotypeOrSample(cart);
            if (sameString(hapOrSample, "Sample"))
                {
                puts("Samples are clustered by similarity around a central variant. "
                     "Samples are reordered for display using the clustering tree, which is "
                     "drawn in the left label area.");
                }
            else
                {
                puts("If this mode is selected and genotypes are phased or homozygous, "
                     "then each genotype is split into two independent haplotypes. "
                     "These local haplotypes are clustered by similarity around a central variant. "
                     "Haplotypes are reordered for display using the clustering tree, which is "
                     "drawn in the left label area. "
                     "Local haplotype blocks can often be identified using this display.");
                }
            printf("<br>To anchor the sorting to a particular variant, "
		   "click on the variant in the genome browser, "
		   "and then click on the 'Use this variant' button on the next page."
		   "</TD></TR>\n");
            }
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
		printf("%s at %s:%d as anchor.</TD></TR>\n<TR><TD></TD><TD>\n",
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
	puts("<TR><TD></TD><TD>");
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
    }
}

static void vcfCfgHaplotypeMethod(struct cart *cart, struct trackDb *tdb, char *track,
                                  boolean parentLevel, struct vcfFile *vcff)
/* If vcff has genotype data, offer the option of whether to cluster or just use the order
 * of genotypes in the VCF file.  For clustering, show status and controls for choosing the
 * center variant for haplotype clustering/sorting in hgTracks. */
{
if (vcff != NULL && vcff->genotypeCount > 1)
    {
    printf("<TABLE cellpadding=0><TR><TD colspan=2>"
	   "<B>%s sorting order:</B></TD></TR>\n", vcfHaplotypeOrSample(cart));
    // If trackDb specifies a treeFile, offer that as an option
    char *hapMethod = cartOrTdbString(cart, tdb, VCF_HAP_METHOD_VAR, VCF_DEFAULT_HAP_METHOD);
    char *hapMethodTdb = trackDbSetting(tdb, VCF_HAP_METHOD_VAR);
    char varName[1024];
    safef(varName, sizeof(varName), "%s." VCF_HAP_METHOD_VAR, track);
    if (hapMethodTdb && startsWithWord("treeFile", hapMethodTdb))
        {
        puts("<TR><TD>");
        cgiMakeRadioButton(varName, VCF_HAP_METHOD_TREE_FILE,
                           startsWithWord(VCF_HAP_METHOD_TREE_FILE, hapMethod));
        printf("</TD><TD>using the tree specified in file associated with track</TD></TR>");
        }
    printf("<TR><TD>");
    cgiMakeRadioButton(varName, VCF_HAP_METHOD_CENTER_WEIGHTED,
                       sameString(hapMethod, VCF_HAP_METHOD_CENTER_WEIGHTED));
    printf("</TD><TD>");
    vcfCfgHaplotypeCenter(cart, tdb, track, parentLevel, vcff, NULL, NULL, 0, "mainForm");
    puts("<TR><TD>");
    cgiMakeRadioButton(varName, VCF_HAP_METHOD_FILE_ORDER,
                       sameString(hapMethod, VCF_HAP_METHOD_FILE_ORDER));
    puts("</TD><TD>using the order in which samples appear in the underlying VCF file</TD></TR>");
    puts("</TABLE>");
    jsInlineF("$('input[type=radio][name=\"%s\"]').change(function() { "
              "if (this.value == '"VCF_HAP_METHOD_CENTER_WEIGHTED"') {"
              "  $('#leafShapeContainer').show();"
              "  $('#sampleColorContainer').hide();"
              "} else if (this.value == '"VCF_HAP_METHOD_TREE_FILE"') {"
              "  $('#sampleColorContainer').show();"
              "  $('#leafShapeContainer').hide();"
              "} else {"
              "  $('#sampleColorContainer').hide();"
              "  $('#leafShapeContainer').hide();"
              "}});\n",
              varName);
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
printf("<B>Enable %s sorting display</B><BR>\n", vcfHaplotypeOrSample(cart));
}

static void vcfCfgHapClusterColor(struct cart *cart, struct trackDb *tdb, char *name,
				   boolean parentLevel)
/* Let the user choose how to color the sorted haplotypes. */
{
printf("<B>Allele coloring scheme:</B><BR>\n");
char *colorBy = cartOrTdbString(cart, tdb, VCF_HAP_COLORBY_VAR, VCF_DEFAULT_HAP_COLORBY);
char varName[1024];
safef(varName, sizeof(varName), "%s." VCF_HAP_COLORBY_VAR, name);
cgiMakeRadioButton(varName, VCF_HAP_COLORBY_ALTONLY, sameString(colorBy, VCF_HAP_COLORBY_ALTONLY));
printf("reference alleles invisible, alternate alleles in black<BR>\n");
char *geneTrack = cartOrTdbString(cart, tdb, "geneTrack", NULL);
if (isNotEmpty(geneTrack))
    {
    cgiMakeRadioButton(varName, VCF_HAP_COLORBY_FUNCTION,
                       sameString(colorBy, VCF_HAP_COLORBY_FUNCTION));
    printf("reference alleles invisible, alternate alleles in "
           "<span style='color:red'>red</span> for non-synonymous, "
           "<span style='color:green'>green</span> for synonymous, "
           "<span style='color:blue'>blue</span> for UTR/noncoding, "
           "black otherwise<BR>\n");
    }
cgiMakeRadioButton(varName, VCF_HAP_COLORBY_REFALT, sameString(colorBy, VCF_HAP_COLORBY_REFALT));
printf("reference alleles in blue, alternate alleles in red<BR>\n");
cgiMakeRadioButton(varName, VCF_HAP_COLORBY_BASE, sameString(colorBy, VCF_HAP_COLORBY_BASE));
printf("first base of allele (A = red, C = blue, G = green, T = magenta)<BR>\n");
}

static void vcfCfgHapClusterSampleColor(struct cart *cart, struct trackDb *tdb, char *name,
                                        boolean parentLevel)
/* If sampleColorFile specifies multiple files, when hapClusterMethod treeFile is selected,
 * let the user choose sample-coloring scheme for the tree. */
{
char *tdbSetting = trackDbSetting(tdb, VCF_SAMPLE_COLOR_FILE);
if (tdbSetting && strchr(tdbSetting, ' '))
    {
    char *hapMethod = cartOrTdbString(cart, tdb, VCF_HAP_METHOD_VAR, VCF_DEFAULT_HAP_METHOD);
    printf("<div id='sampleColorContainer'%s>\n",
           startsWithWord(VCF_HAP_METHOD_TREE_FILE, hapMethod) ? "" : " style='display: none;'");
    printf("<b>Sample coloring scheme for tree:</b><br>\n");
    char *setting = cartOrTdbString(cart, tdb, VCF_SAMPLE_COLOR_FILE, tdbSetting);
    char *options[16];
    int optionCount = chopLine(tdbSetting, options);
    char *labels[optionCount];
    char *values[optionCount];
    int i;
    for (i = 0;  i < optionCount;  i++)
        {
        char *eq = strchr(options[i], '=');
        if (eq)
            {
            *eq = '\0';
            labels[i] = options[i];
            replaceChar(options[i], '_', ' ');
            values[i] = eq+1;
            }
        else
            {
            labels[i] = values[i] = options[i];
            }
        }
    char *selected = strchr(setting, ' ') ? values[0] : setting;
    char varName[1024];
    safef(varName, sizeof varName, "%s." VCF_SAMPLE_COLOR_FILE, name);
    cgiMakeDropListWithVals(varName, labels, values, optionCount, selected);
    puts("</div>");
    }
}

static void vcfCfgHapClusterTreeAngle(struct cart *cart, struct trackDb *tdb, char *name,
				   boolean parentLevel)
/* Let the user choose branch shape. */
{
// This option applies only to center-weighted clustering; don't show option when some other
// method is selected.
char *hapMethod = cartOrTdbString(cart, tdb, VCF_HAP_METHOD_VAR, VCF_DEFAULT_HAP_METHOD);
printf("<div id='leafShapeContainer'%s>\n",
       differentString(hapMethod, VCF_HAP_METHOD_CENTER_WEIGHTED) ? " style='display: none;'" : "");
printf("<B>%s clustering tree leaf shape:</B><BR>\n", vcfHaplotypeOrSample(cart));
char *treeAngle = cartOrTdbString(cart, tdb, VCF_HAP_TREEANGLE_VAR, VCF_DEFAULT_HAP_TREEANGLE);
char varName[1024];
safef(varName, sizeof(varName), "%s." VCF_HAP_TREEANGLE_VAR, name);
cgiMakeRadioButton(varName, VCF_HAP_TREEANGLE_TRIANGLE,
		   sameString(treeAngle, VCF_HAP_TREEANGLE_TRIANGLE));
printf("draw branches whose samples are all identical as &lt;<BR>\n");
cgiMakeRadioButton(varName, VCF_HAP_TREEANGLE_RECTANGLE,
		   sameString(treeAngle, VCF_HAP_TREEANGLE_RECTANGLE));
printf("draw branches whose samples are all identical as [<BR>\n");
puts("</div>");
}

static void vcfCfgHapClusterHeight(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff,
				   char *name, boolean parentLevel)
/* Let the user specify a height for the track. */
{
if (vcff != NULL && vcff->genotypeCount > 1)
    {
    printf("<B>%s sorting display height:</B> \n", vcfHaplotypeOrSample(cart));
    int cartHeight = cartOrTdbInt(cart, tdb, VCF_HAP_HEIGHT_VAR, VCF_DEFAULT_HAP_HEIGHT);
    char varName[1024];
    safef(varName, sizeof(varName), "%s." VCF_HAP_HEIGHT_VAR, name);
    cgiMakeIntVarInRange(varName, cartHeight, "Height (in pixels) of track", 5, "4", "10000");
    puts("<BR>");
    }
}

static void vcfCfgHapCluster(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff,
			     char *name, boolean parentLevel)
/* Show controls for haplotype-sorting display, which only makes sense to do when
 * the VCF file describes multiple genotypes. */
{
char *hapOrSample = vcfHaplotypeOrSample(cart);
printf("<H3>%s sorting display</H3>\n", hapOrSample);
vcfCfgHapClusterEnable(cart, tdb, name, parentLevel);
vcfCfgHaplotypeMethod(cart, tdb, name, parentLevel, vcff);
vcfCfgHapClusterTreeAngle(cart, tdb, name, parentLevel);
vcfCfgHapClusterSampleColor(cart, tdb, name, parentLevel);
vcfCfgHapClusterColor(cart, tdb, name, parentLevel);
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
if (slCount(vcff->filterDefs) > 1)
    {
    jsMakeCheckboxGroupSetClearButton(cartVar, TRUE);
    puts("&nbsp;");
    jsMakeCheckboxGroupSetClearButton(cartVar, FALSE);
    }
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

static char *getChildSample(struct trackDb *tdb)
/* Return just the VCF sample name of the phased trio child setting */
{
char *childSampleMaybeAlias = cloneString(trackDbLocalSetting(tdb, VCF_PHASED_CHILD_SAMPLE_SETTING));
char *pt = strchr(childSampleMaybeAlias, '|');
if (pt != NULL)
    *pt = '\0';
return childSampleMaybeAlias;
}

static struct slPair *vcfPhasedGetSamplesFromTdb(struct trackDb *tdb, boolean hideOtherSamples)
/* Get the different VCF Phased Trio setings out of trackDb onto a list */
{
// cloneString here because we will be munging the result if there are alternate labels
char *childSampleMaybeAlias = cloneString(trackDbLocalSetting(tdb, VCF_PHASED_CHILD_SAMPLE_SETTING));
char *parentSamplesMaybeAlias = cloneString(trackDbLocalSetting(tdb, VCF_PHASED_PARENTS_SAMPLE_SETTING));
char *samples[VCF_PHASED_MAX_OTHER_SAMPLES+1]; // for now only allow at most two parents
int numOthers = 0;
if (parentSamplesMaybeAlias && !hideOtherSamples)
    {
    numOthers = chopCommas(cloneString(parentSamplesMaybeAlias), samples);
    if (numOthers > VCF_PHASED_MAX_OTHER_SAMPLES)
        {
        warn("More than %d other samples specified for phased trio", VCF_PHASED_MAX_OTHER_SAMPLES);
        numOthers = VCF_PHASED_MAX_OTHER_SAMPLES;
        }
    // shove child into middle of array, and if there are two parents, scoot the second one to the end
    int lastParentIx = VCF_PHASED_MAX_OTHER_SAMPLES - 1;
    if (samples[lastParentIx] != NULL)
        samples[VCF_PHASED_MAX_OTHER_SAMPLES] = cloneString(samples[lastParentIx]);
    samples[lastParentIx] = cloneString(childSampleMaybeAlias);
    }
else
    samples[0] = cloneString(childSampleMaybeAlias);

boolean gotAlias = strchr(samples[0], '|') != NULL; // default to whatever is first
struct slPair *ret = NULL;
int i;
for (i = 0; i < numOthers+1; i++)
    {
    char *val = strchr(samples[i], '|');
    boolean foundAlias = val != NULL;
    if (val != NULL)
        {
        if (foundAlias != gotAlias)
            errAbort("Either all samples have aliases or none.");
        else
            *val++ = 0;
        }
    char *name = samples[i];
    struct slPair *temp = slPairNew(cloneString(name), cloneString(val));
    slAddHead(&ret, temp);
    }
slReverse(&ret);
return ret;
}

struct slPair *vcfPhasedGetSampleOrder(struct cart *cart, struct trackDb *tdb, boolean parentLevel, boolean hideOtherSamples)
/* Parse out a trio sample order from either trackDb or the cart.
 * If the trackName.sortChildBelow cart variable is true, then ensure
 * the vcfChildSample sample is last in the order, otherwise, use what's
 * in the trackName.vcfSampleOrder cart variable. */
{
char sampleOrderVar[1024];
safef(sampleOrderVar, sizeof(sampleOrderVar), "%s.%s", tdb->track, VCF_PHASED_SAMPLE_ORDER_VAR);
char *cartOrder = cartOptionalString(cart, sampleOrderVar);
boolean childBelow = cartUsualBooleanClosestToHome(cart, tdb, parentLevel, VCF_PHASED_CHILD_BELOW_VAR, FALSE);
struct slPair *tdbOrder = vcfPhasedGetSamplesFromTdb(tdb, hideOtherSamples);
if (!hideOtherSamples)
    {
    // if the user used drag and drop to reorder the trios then that takes precedence
    // over the childBelow checkbox
    if (cartOrder != NULL)
        {
        struct slName *name, *fromCart = slNameListFromComma(cartOrder);
        struct slPair *ret = NULL;
        for (name = fromCart; name != NULL; name = name->next)
            {
            struct slPair *temp = slPairFind(tdbOrder, name->name);
            struct slPair *toAdd = slPairNew(temp->name, temp->val);
            slAddHead(&ret, toAdd);
            }
        slReverse(&ret);
        return ret;
        }
    else if (childBelow)
        {
        char *childName = getChildSample(tdb);
        struct slPair *ret = NULL, *child = NULL, *temp = NULL;
        for (temp = tdbOrder; temp != NULL; temp = temp->next)
            {
            struct slPair *toAdd = slPairNew(temp->name, temp->val);
            if (sameString(temp->name, childName))
                child = toAdd;
            else
                slAddHead(&ret, toAdd);
            }
        if (child)
            slAddHead(&ret, child);
        slReverse(&ret);
        return ret;
        }
    }
// we're hiding the parents OR (we unchecked the childBelow checkbox AND we didn't drag reorder)
return tdbOrder;
}

static boolean hasSampleAliases(struct trackDb *tdb)
/* Check whether trackDb has aliases for the sample names  */
{
struct slPair *nameVals = vcfPhasedGetSamplesFromTdb(tdb,FALSE);
return nameVals->val != NULL;
}

static void vcfPhasedSampleSortUi(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff, char *name,
                                    boolean parentLevel)
/* Put up the UI for sorting the samples */
{
struct dyString *sortOrder = dyStringNew(0);
struct slPair *pair, *tdbOrder = vcfPhasedGetSampleOrder(cart, tdb, parentLevel, FALSE);
if (slCount(tdbOrder) == 1) // no sorting if there are no parents
    return;
char childBelowSortOrder[1024];
safef(childBelowSortOrder, sizeof(childBelowSortOrder), "%s.%s", name, VCF_PHASED_CHILD_BELOW_VAR);
boolean isBelowChecked = cartUsualBooleanClosestToHome(cart, tdb, parentLevel, VCF_PHASED_CHILD_BELOW_VAR, FALSE);
printf("<b>Show child haplotypes below parents:</b>\n");
cgiMakeCheckBox(childBelowSortOrder, isBelowChecked);
char *infoText = "Check this box to sort the child haplotypes below the parents, leave unchecked"
    " to use the default sort order of the child in the middle. Click into each subtrack to arbitrarily"
    " order the samples which overrides this setting.";
printInfoIcon(infoText);
printf("<br>");
if (!parentLevel)
    {
    printf("<b>or:</b><br>\n");
    printf("<b>Drag to change order:</b>\n");
    printf("<div>\n");
    printf("<table id=\"%s_table\" class=\"tableWithDragAndDrop\">\n", tdb->track);
    for (pair = tdbOrder; pair != NULL; pair = pair->next)
        {
        char id[256];
        safef(id, sizeof(id), "%s_drag", pair->name);
        printf("<tr id=\"%s_row\" class=\"trDraggable\"><td id=\"%s\" class=\"dragHandle\">%s - %s</td></tr>\n", pair->name, id, pair->name, (char *)pair->val);
        dyStringPrintf(sortOrder, "%s,", pair->name);
        }
    printf("</table>\n");
    printf("</div>\n");
    printf("<input type=\"hidden\" name=\"%s.%s\" value=\"%s\">",tdb->track, VCF_PHASED_SAMPLE_ORDER_VAR, dyStringCannibalize(&sortOrder));
    // add the hidden variable for setting the order and the javascript to change it
    jsInlineF(""
    "dragReorder.init();\n"
    "var imgTable = $(\"#%s_table\");\n"
    "if ($(imgTable).length > 0) {\n"
    "   $(imgTable).tableDnD({\n"
    "       onDragClass: \"trDrag\",\n"
    "       dragHandle: \"dragHandle\",\n"
    "       scrollAmount: 40,\n"
    "       onDragStart: function(ev, table, row) {\n"
    "           mouse.saveOffset(ev);\n"
    "           table.tableDnDConfig.dragObjects = [ row ]; // defaults to just the one\n"
    "       },\n"
    "       onDrop: function(table, row, dragStartIndex) {\n"
    "           if ($(row).attr('rowIndex') !== dragStartIndex) {\n"
    "               // NOTE Even if dragging a contiguous set of rows,\n"
    "               // still only need to check the one under the cursor.\n"
    "               if (dragReorder.setOrder) {\n"
    "                   dragReorder.setOrder(table);\n"
    "               }\n"
    "               // save the order of the samples into the input variable named above\n"
    "               var newVal = \"\";\n"
    "               var inp = $(\"input[name='%s.%s']\")[0];\n"
    "               for (i = 0; i < table.rows.length; i++) {\n"
    "                   newVal += table.rows[i].id.slice(0,-4) + \",\";\n"
    "               }\n"
    "               if (newVal.slice(-1) === \",\") {\n"
    "                   newVal = newVal.slice(0,-1);\n"
    "               }\n"
    "               inp.value = newVal;\n"
    "           }\n"
    "       }\n"
    "   });\n"
    "}\n"
    "", tdb->track, tdb->track, VCF_PHASED_SAMPLE_ORDER_VAR);
    }
}

static void vcfCfgPhasedTrioUi(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff, char *name,
                                boolean parentLevel)
/* Put up the phased trio specific config settings */
{
//if (!parentLevel) // don't put up this display at the composite level
vcfPhasedSampleSortUi(cart, tdb, vcff, name, parentLevel);
if (hasSampleAliases(tdb))
    {
    printf("<b>Label samples by:</b>");
    char defaultLabel[1024], aliasLabel[1024];
    safef(defaultLabel, sizeof(defaultLabel), "%s.%s", name, VCF_PHASED_DEFAULT_LABEL_VAR);
    safef(aliasLabel, sizeof(aliasLabel), "%s.%s", name, VCF_PHASED_ALIAS_LABEL_VAR);
    boolean isDefaultChecked = cartUsualBooleanClosestToHome(cart, tdb, parentLevel, VCF_PHASED_DEFAULT_LABEL_VAR, FALSE);
    boolean isAliasChecked = cartUsualBooleanClosestToHome(cart, tdb, parentLevel, VCF_PHASED_ALIAS_LABEL_VAR, TRUE);
    cgiMakeCheckBox(defaultLabel, isDefaultChecked);
    printf("VCF file sample names &nbsp;");
    cgiMakeCheckBox(aliasLabel, isAliasChecked);
    printf("Family Labels");
    printf("<br>");
    }
if (trackDbSetting(tdb,VCF_PHASED_PARENTS_SAMPLE_SETTING))
    {
    printf("<b>Hide parent sample(s)");
    char hideVarName[1024];
    safef(hideVarName, sizeof(hideVarName), "%s.%s", name, VCF_PHASED_HIDE_OTHER_VAR);
    boolean hidingOtherSamples = cartUsualBooleanClosestToHome(cart, tdb, parentLevel, VCF_PHASED_HIDE_OTHER_VAR, FALSE);
    cgiMakeCheckBox(hideVarName, hidingOtherSamples);
    }
printf("<br>");
printf("Allele coloring scheme:");
printf("<br>");
char *colorBy = cartOrTdbString(cart, tdb, VCF_PHASED_COLORBY_VAR, VCF_PHASED_COLORBY_DEFAULT);
char varName[1024];
safef(varName, sizeof(varName), "%s.%s", name, VCF_PHASED_COLORBY_VAR);
cgiMakeRadioButton(varName, VCF_PHASED_COLORBY_DEFAULT, sameString(colorBy, VCF_PHASED_COLORBY_DEFAULT));
printf("No color<br>");
char *geneTrack = cartOrTdbString(cart, tdb, "geneTrack", NULL);
if (isNotEmpty(geneTrack))
    {
    cgiMakeRadioButton(varName, VCF_PHASED_COLORBY_FUNCTION, sameString(colorBy, VCF_PHASED_COLORBY_FUNCTION));
    printf("predicted functional affect: ");
    printf("reference alleles invisible, alternate alleles in "
           "<span style='color:red'>red</span> for non-synonymous, "
           "<span style='color:green'>green</span> for synonymous, "
           "<span style='color:blue'>blue</span> for UTR/noncoding, "
           "black otherwise<BR>\n");
    }
cgiMakeRadioButton(varName, VCF_PHASED_COLORBY_DE_NOVO, sameString(colorBy, VCF_PHASED_COLORBY_DE_NOVO));
printf("predicted de novo child mutations <span style='color:red'>red</span>");
char *deNovoInfoText = "Check this box to color child variants red if they are unique to the child";
printInfoIcon(deNovoInfoText);
printf("<br>");
cgiMakeRadioButton(varName, VCF_PHASED_COLORBY_MENDEL_DIFF, sameString(colorBy, VCF_PHASED_COLORBY_MENDEL_DIFF));
printf("child variants that are inconsistent with phasing <span style='color:red'>red</span>");
char *phasedInfoText = "Check this box to color child variants red if they do not agree with the implied "
    "parental transmitted allele at this location. This configuration is only available when parent "
    "haplotypes are displayed.";
printInfoIcon(phasedInfoText);
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
    boolean doVcfFilterUi = cartOrTdbBoolean(cart, tdb, VCF_DO_FILTER_UI, TRUE);
    boolean doVcfQualUi = cartOrTdbBoolean(cart, tdb, VCF_DO_QUAL_UI, TRUE);
    boolean doVcfMafUi = cartOrTdbBoolean(cart, tdb, VCF_DO_MAF_UI, TRUE);
    if (vcff->genotypeCount > 1 && !sameString(tdb->type, "vcfPhasedTrio"))
        {
        vcfCfgHapCluster(cart, tdb, vcff, name, parentLevel);
        }
    if (sameString(tdb->type, "vcfPhasedTrio"))
        {
        vcfCfgPhasedTrioUi(cart, tdb, vcff, name, parentLevel);
        }
    if (differentString(tdb->track,"evsEsp6500") && (doVcfFilterUi || doVcfQualUi))
        {
        puts("<H3>Filters</H3>");
        if (doVcfQualUi)
            vcfCfgMinQual(cart, tdb, vcff, name, parentLevel);
        if (doVcfFilterUi)
            vcfCfgFilterColumn(cart, tdb, vcff, name, parentLevel);
        }
    if (doVcfMafUi)
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

