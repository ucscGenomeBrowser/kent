/* vcfUi - Variant Call Format user interface controls that are shared
 * between more than one CGI. */

#include "common.h"
#include "cheapcgi.h"
#include "errCatch.h"
#include "hCommon.h"
#include "hui.h"
#include "vcf.h"
#include "vcfUi.h"
#if (defined USE_TABIX && defined KNETFILE_HOOKS)
#include "knetUdc.h"
#include "udc.h"
#endif//def USE_TABIX && KNETFILE_HOOKS

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

void vcfCfgHaplotypeCenter(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff,
			   char *thisName, char *thisChrom, int thisPos, char *formName)
/* If vcff has genotype data, show status and controls for choosing the center variant
 * for haplotype clustering/sorting in hgTracks. */
{
if (vcff != NULL && vcff->genotypeCount > 1)
    {
// cluster haplotypes or just show allele summary?
//   if clustering haplotypes:
//      track height?
//      thicken lines?
//      outline center variant?
//      choose center variant?  [hgc option to make this variant's coords the center of viewed region?]
//      color haplotypes by red/blue or allele bases?
    printf("<TABLE cellpadding=0><TR><TD>"
	   "<B>Haplotype sorting:</B> using ");
    char cartVar[1024];
    safef(cartVar, sizeof(cartVar), "%s.centerVariantChrom", tdb->track);
    char *centerChrom = cartOptionalString(cart, cartVar);
    if (isEmpty(centerChrom))
	{
	// Unspecified in cart -- describe the default action
	printf(VCF_HAPLOSORT_DEFAULT_DESC " as anchor.</TD></TR>\n");
	if (isNotEmpty(thisChrom))
	    {
	    // but we do have a candidate, so offer to make it the center:
	    puts("<TR><TD>");
	    vcfCfgHaplotypeCenterHiddens(tdb->track, thisName, thisChrom, thisPos);
	    char label[256];
	    safef(label, sizeof(label), "Use %s", nameOrDefault(thisName, "this variant"));
	    cgiMakeButton("submit", label);
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
	cgiMakeHiddenVar(cartVar, "");
	safef(cartVar, sizeof(cartVar), "%s.centerVariantPos", tdb->track);
	int centerPos = cartInt(cart, cartVar);
	safef(cartVar, sizeof(cartVar), "%s.centerVariantName", tdb->track);
	char *centerName = cartString(cart, cartVar);
	if (isNotEmpty(thisChrom))
	    {
	    // These form inputs are for either "use me" or clear:
	    vcfCfgHaplotypeCenterHiddens(tdb->track, thisName, thisChrom, thisPos);
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
		cgiMakeButton("submit", label);
		printf(" as anchor</TD></TR>\n");
		}
	    }
	else
	    {
	    // Form inputs (in case the clear button is clicked)
	    vcfCfgHaplotypeCenterHiddens(tdb->track, centerName, centerChrom, centerPos);
	    printf("%s at %s:%d as anchor.</TD></TR>\n",
		   nameOrDefault(centerName, "variant"), centerChrom, centerPos+1);
	    }
	// Make a clear button that modifies the hiddens using onClick
	puts("<TR><TD>");
	struct dyString *onClick = dyStringNew(0);
	dyStringPrintf(onClick, "updateOrMakeNamedVariable(%s, '%s.centerVariantChrom', ''); ",
		       formName, tdb->track);
	dyStringPrintf(onClick, "updateOrMakeNamedVariable(%s, '%s.centerVariantName', ''); ",
		       formName, tdb->track);
	dyStringPrintf(onClick, "updateOrMakeNamedVariable(%s, '%s.centerVariantPos', 0);",
		       formName, tdb->track);
	dyStringPrintf(onClick, "document.%s.submit(); return false;", formName);
	cgiMakeButtonWithOnClick("submit", "Clear selection", NULL, onClick->string);
	printf(" (use " VCF_HAPLOSORT_DEFAULT_DESC ")</TD></TR>\n");
	}
    puts("</TABLE>");
    }
}

//TODO: share this code w/hgTracks, hgc in hg/lib/vcfFile.c
static struct vcfFile *vcfHopefullyOpenHeader(struct cart *cart, struct trackDb *tdb)
/* Defend against network errors and return the vcfFile object with header data, or NULL. */
{
#if (defined USE_TABIX && defined KNETFILE_HOOKS)
knetUdcInstall();
if (udcCacheTimeout() < 300)
    udcSetCacheTimeout(300);
#endif//def USE_TABIX && KNETFILE_HOOKS
char *db = cartString(cart, "db");
struct sqlConnection *conn = hAllocConnTrack(db, tdb);
char *fileOrUrl = bbiNameFromSettingOrTable(tdb, conn, tdb->table);
hFreeConn(&conn);
int vcfMaxErr = 100;
struct vcfFile *vcff = NULL;
/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    vcff = vcfTabixFileMayOpen(fileOrUrl, NULL, 0, 0, vcfMaxErr);
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

void vcfCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed)
/* VCF: Variant Call Format.  redmine #3710 */
{
boxed = cfgBeginBoxAndTitle(tdb, boxed, title);
printf("<TABLE%s><TR><TD>", boxed ? " width='100%'" : "");
struct vcfFile *vcff = vcfHopefullyOpenHeader(cart, tdb);
vcfCfgHaplotypeCenter(cart, tdb, vcff, NULL, NULL, 0, "mainForm");
// filter:
//   by qual column
//   by filter column
// color bases like pgSnp or some better palette?


printf("</TD></TR></TABLE>");

if (!boxed && fileExists(hHelpFile("hgVcfTrackHelp")))
    printf("<P><A HREF=\"../goldenPath/help/hgVcfTrackHelp.html\" TARGET=_BLANK>VCF "
	   "configuration help</A></P>");
cfgEndBox(boxed);
}

