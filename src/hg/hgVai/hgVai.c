/* hgVai - Variant Annotation Integrator. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
#include "cartTrackDb.h"
#include "genbank.h"
#include "hgConfig.h"
#include "hgHgvs.h"
#include "hui.h"
#include "grp.h"
#include "hCommon.h"
#include "hgFind.h"
#include "hPrint.h"
#include "jsHelper.h"
#include "memalloc.h"
#include "textOut.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "twoBit.h"
#include "gpFx.h"
#include "bigGenePred.h"
#include "udc.h"
#include "knetUdc.h"
#include "md5.h"
#include "regexHelper.h"
#include "hAnno.h"
#include "annoGratorQuery.h"
#include "annoGratorGpVar.h"
#include "annoFormatVep.h"
#include "annoStreamBigBed.h"
#include "annoStreamDb.h"
#include "annoStreamVcf.h"
#include "windowsToAscii.h"
#include "obscure.h"
#include "chromAlias.h"

#include "libifyMe.h"

#define GENCODE_TAG_DOC_URL "\"http://www.gencodegenes.org/gencode_tags.html\""
#define GENCODE_BASIC_DOC_URL "\"http://www.gencodegenes.org/faq.html\""
#define REFSEQ_STATUS_DOC_URL "\"https://www.ncbi.nlm.nih.gov/books/NBK21091/table/ch18.T.refseq_status_codes\""
#define APPRIS_DOC_URL "\"http://appris.bioinfo.cnio.es/#/help/database\""

#define HGVS_MUST_USE_ACC "Note: HGVS terms must use versioned transcript or genomic accessions " \
    "(e.g. NM_000023.3, NC_000012.11, ENST00000000233.9), not gene symbols."

/* Global Variables */
struct cart *cart;		/* CGI and other variables */
struct hash *oldVars = NULL;	/* The cart before new cgi stuff added. */
char *genome = NULL;		/* Name of genome - mouse, human, etc. */
char *database = NULL;		/* Current genome database - hg17, mm5, etc. */
char *regionType = NULL;	/* genome, ENCODE pilot regions, or specific position range. */
struct grp *fullGroupList = NULL;	/* List of all groups. */
struct trackDb *fullTrackList = NULL;	/* List of all tracks in database. */
static struct pipeline *compressPipeline = (struct pipeline *)NULL;


// Null terminated list of CGI Variables we don't want to save permanently:
char *excludeVars[] = {"Submit", "submit", "hgva_startQuery", NULL,};

#define hgvaRange "position"
#define hgvaRegionType "hgva_regionType"
#define hgvaRegionTypeEncode "encode"
#define hgvaRegionTypeGenome "genome"
#define hgvaRegionTypeRange "range"
#define hgvaPositionContainer "positionContainer"
#define hgvsTrashSubDir "hgv"
// auto-generated variants as input
#define hgvaSampleVariants "hgva_internally_generated_sample_variants"
#define hgvaSampleVariantsLabel "Artificial Example Variants"
// dbSNP rsIDs as variant inputs:
#define hgvaUseVariantIds "hgva_useVariantIds"
#define hgvaVariantIdsLabel "Variant Identifiers"
#define hgvaVariantIds "hgva_variantIds"
#define hgvaUseVariantFileOrUrl "hgva_useVariantFileOrUrl"
#define hgvaVariantPasteContainer "variantPasteContainer"
// A local file or URL as variant input:
#define hgvaVariantFileOrUrlLabel "from file or URL"
#define hgvaVariantFileOrUrl "hgva_variantFileOrUrl"
#define hgvaVariantFileOrUrlType "hgva_variantFileOrUrlType"
// HGVS terms as variant inputs:
#define hgvaUseHgvs "hgva_useHgvs"
#define hgvaHgvsLabel "HGVS terms"
#define hgvaHgvs "hgva_hgvs"
#define hgvaHgvsPasteContainer "hgvsPasteContainer"

void addSomeCss()
/*#*** This should go in a .css file of course. */
{
printf("<style>\n"
	"div.sectionLite { border-width: 1px; border-color: #"HG_COL_BORDER"; border-style: solid;"
	"  background-color: #"HG_COL_INSIDE"; padding-left: 10px; padding-right: 10px; "
	"  padding-top: 8px; padding-bottom: 5px; margin-top: 5px; margin-bottom: 5px }\n"
	".sectionLiteHeader { font-weight: bold; font-size:larger; color:#000000;"
	"  text-align:left; vertical-align:bottom; white-space:nowrap; }\n"
	"div.sectionLiteHeader.noReorderRemove { padding-bottom:5px; }\n"
	"div.sourceFilter { padding-top: 5px; padding-bottom: 5px }\n"
	"</style>\n");
}


INLINE void startCollapsibleSection(char *sectionSuffix, char *title, boolean onByDefault)
// Wrap shared args to jsBeginCollapsibleSectionFontSize
{
jsBeginCollapsibleSectionFontSize(cart, "hgva", sectionSuffix, title, onByDefault, "1.1em");
}

INLINE void startSmallCollapsibleSection(char *sectionSuffix, char *title, boolean onByDefault)
// Wrap shared args to jsBeginCollapsibleSectionFontSize
{
jsBeginCollapsibleSectionFontSize(cart, "hgva", sectionSuffix, title, onByDefault, "0.9em");
}

#define endCollapsibleSection jsEndCollapsibleSection


INLINE void printOption(char *val, char *selectedVal, char *label)
/* For rolling our own select without having to build conditional arrays/lists. */
{
printf("<OPTION VALUE='%s'%s>%s\n", val, (sameString(selectedVal, val) ? " SELECTED" : ""), label);
}

void printRegionListHtml(char *db)
/* Make a dropdown choice of region type, with position input box that appears if needed.
 * Unlike hgTables, don't bother with ENCODE pilot regions -- unless someone misses it.
 * Return the selected region type. */
{
printf("<SELECT ID='"hgvaRegionType"' NAME='"hgvaRegionType"'>\n");
jsOnEventById("change", hgvaRegionType, "hgva.changeRegion();");
printOption(hgvaRegionTypeGenome, regionType, "genome");
printOption(hgvaRegionTypeRange, regionType, "position or search term");
printf("</SELECT>");
}

void topLabelSpansStart(char *label)
{
printf("<span style='display: inline-block; padding-right: 5px;'>"
       "<span style='display: block;'>%s</span>\n"
       "<span style='display: block; padding-bottom: 5px;'>\n", label);
}

void topLabelSpansEnd()
{
printf("</span></span>");
}

char *makePositionInput()
/* Return HTML for the position input. */
{
struct dyString *dy = dyStringCreate("<INPUT TYPE=TEXT NAME=\"%s\" SIZE=%d VALUE=\"%s\">",
				     hgvaRange, 45,
				     addCommasToPos(NULL, cartString(cart, hgvaRange)));
return dyStringCannibalize(&dy);
}

void printCtAndHubButtons()
/* Print a div with buttons for hgCustom and hgHubConnect */
{
boolean hasCustomTracks = customTracksExist(cart, NULL);
puts("<div style='padding-top: 5px; padding-bottom: 5px'>");
hOnClickButton("prtCtHub_CtBut", "document.customTrackForm.submit(); return false;",
	       hasCustomTracks ? CT_MANAGE_BUTTON_LABEL : CT_ADD_BUTTON_LABEL);
printf(" ");
if (hubConnectTableExists())
    hOnClickButton("prtCtHub_TrkHub", "document.trackHubForm.submit(); return false;", "track hubs");
nbSpaces(3);
printf("To reset <B>all</B> user cart settings (including custom tracks), \n"
       "<A HREF=\"cartReset?destination=%s\">click here</A>.\n",
       cgiScriptName());
puts("</div>");
}

void hgGatewayCladeGenomeDb()
/* Make a row of labels and row of buttons like hgGateway, but not using tables. */
{
jsIncludeAutoCompleteLibs();
char *searchPlaceholder = "Search any species, genome or assembly name";
char *searchBarId = "genomeSearch";
printf("<div class='searchCell'>\n");
printGenomeSearchBar(searchBarId, searchPlaceholder, NULL, TRUE, "Change selected genome:", NULL);
jsInlineF(
    "function hgVaiSelect(selectEle, item) {\n"
    "   selectEle.innerHTML = item.label;\n"
    "   setCartVar(\"db\", item.genome);\n"
    "}\n\n"
    "document.addEventListener(\"DOMContentLoaded\", () => {\n"
    "    // bind the actual <select> to the function hgVaiSelect, that way\n"
    "    // initSpeciesAutoCompleteDropdown can call the function\n"
    "    let selectEle = document.getElementById(\"genomeLabel\");\n"
    "    let boundSelect = hgVaiSelect.bind(null, selectEle);\n"
    "    initSpeciesAutoCompleteDropdown('%s', boundSelect);\n"
    "    // make the search button trigger the autocomplete manually\n"
    "    let btn = document.getElementById(\"%sButton\");\n"
    "    btn.addEventListener(\"click\", () => {\n"
    "        let val = document.getElementById(\"%s\").value;\n"
    "        $(\"[id=\'%s\']\").autocompleteCat(\"search\", val);\n"
    "    });\n"
    "});\n"
    , searchBarId, searchBarId, searchBarId, searchBarId
);
printf("<div class='searchCell'>\n");
char *dbLabel = getCurrentGenomeLabel(database);
printf("Current Genome: <span id='genomeLabel'>%s</span>\n", dbLabel);
printf("</div>\n");
puts("<BR>");
topLabelSpansStart("region to annotate");
printRegionListHtml(database);
topLabelSpansEnd();
topLabelSpansStart("");
// Yet another span, for hiding/showing position input and lookup button:
printf("<span id='"hgvaPositionContainer"'%s>\n",
       differentString(regionType, hgvaRegionTypeRange) ? " style='display: none;'" : "");
puts(makePositionInput());
printf("</span>\n");
topLabelSpansEnd();
puts("<BR>");
}

void printAssemblySection()
/* Print assembly-selection stuff.
 * Redrawing the whole page when the assembly changes seems fine to me. */
{
//#*** ---------- More copied verbatim, from hgTables/mainPage.c: -----------
/* Hidden form - for benefit of javascript. */
    {
    static char *saveVars[] = {
      "clade", "org", "db", hgvaRange, hgvaRegionType };
    jsCreateHiddenForm(cart, cgiScriptName(), saveVars, ArraySize(saveVars));
    }

/* Hidden form for jumping to custom tracks CGI. */
printf("<FORM ACTION='%s' NAME='customTrackForm' ID='customTrackForm'>", hgCustomName());
cartSaveSession(cart);
printf("</FORM>\n");

/* Hidden form for jumping to track hub manager CGI. */
printf("<FORM ACTION='%s' NAME='trackHubForm'>", hgHubConnectName());
//#*** well, almost verbatim... lib version should use cgiScriptName.
cartSaveSession(cart);
printf("</FORM>\n");

printf("<FORM ACTION=\"%s\" NAME=\"mainForm\" ID=\"mainForm\" METHOD=%s>\n",
	cgiScriptName(), cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);

//#*** ------------------ end verbatim ---------------

printf("<div class='sectionLiteHeader noReorderRemove'>"
       "Select Genome Assembly and Region</div>\n");

/* Print clade, genome and assembly line. */
hgGatewayCladeGenomeDb();

//#*** No longer ending form here...
}

void factorSourceListInputProperties(struct trackDb *tdb, struct slName **retFactorList,
			     struct slName **retCellTypeList, struct slName **retTreatmentList)
/* Get a list of factor names used in factorSource track. */
{
char *inputsTable = trackDbSetting(tdb, "inputTrackTable");
if (isEmpty(inputsTable))
    errAbort("track %s does not have an inputTrackTable setting", tdb->track);
char query[2048];
struct sqlConnection *conn = hAllocConn(database);
if (retFactorList && retCellTypeList && retTreatmentList)
    {
    sqlSafef(query, sizeof(query), "select distinct(factor) from %s order by factor", inputsTable);
    *retFactorList = sqlQuickList(conn, query);
    sqlSafef(query, sizeof(query), "select distinct(cellType) from %s order by cellType",
	     inputsTable);
    *retCellTypeList = sqlQuickList(conn, query);
    sqlSafef(query, sizeof(query), "select distinct(treatment) from %s order by treatment",
	     inputsTable);
    *retTreatmentList = sqlQuickList(conn, query);
    }
else
    errAbort("factorSourceListInputProperties: ret args must be non-NULL.");
hFreeConn(&conn);
}

void printMultiselect(char *track, char *label, char *var, struct slName *optionList)
/* Print a label and multi-select, with hgva_track_var as cart var, with options in optionList,
 * marking as selected any options stored in cart. */
{
printf("%s: ", label);
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "hgva_filter_%s_%s", track, var);
struct hash *selHash = NULL;
if (cartListVarExists(cart, cartVar))
    selHash = hashFromSlNameList(cartOptionalSlNameList(cart, cartVar));
printf("<select id=\"%s\" name=\"%s\" style=\"display: none; font-size:.9em;\" "
       "class=\"filterBy\" multiple>\n", cartVar, cartVar);
char *selected = "";
if (!selHash || hashLookup(selHash, "All"))
    selected = " selected";
printf("<option%s>All</option>", selected);
struct slName *option;
for (option = optionList;  option != NULL;  option = option->next)
    {
    selected = "";
    if (selHash && hashLookup(selHash, option->name))
	selected = " selected";
    printf("<option%s>%s</option>", selected, option->name);
    }
printf("</select>\n");
char shadowVar[1024];
safef(shadowVar, sizeof(shadowVar), "%s%s", cgiMultListShadowPrefix(), cartVar);
cgiMakeHiddenVar(shadowVar, "1");
jsInlineF("$(document).ready(function(){ ddcl.setup($('#%s')[0]); });\n", cartVar);
}

void printFilterOptions(struct trackDb *tdb)
/* Print a collapsible filter section for tdb, with controls depending on tdb->type. */
{
char sectionName[512], cartVar[512];
safef(sectionName, sizeof(sectionName), "%s_filter", tdb->track);
if (sameString(tdb->type, "factorSource"))
    {
    puts("<TABLE>");
    startSmallCollapsibleSection(sectionName, "filter items", FALSE);
    struct slName *factorOptions = NULL, *cellTypeOptions = NULL, *treatmentOptions = NULL;
    factorSourceListInputProperties(tdb, &factorOptions, &cellTypeOptions,
				    &treatmentOptions);
    printMultiselect(tdb->track, "factor", "name", factorOptions);
    printMultiselect(tdb->track, "cell type", "cellType", cellTypeOptions);
    printMultiselect(tdb->track, "treatment", "treatment", treatmentOptions);
    puts("<BR>");
    puts("minimum peak score [0-1000]: ");
    safef(cartVar, sizeof(cartVar), "hgva_filter_%s_score", tdb->track);
    char *defaultScore = cartUsualString(cart, cartVar, "0");
    printf("<input type=text name=\"%s\" size=12 value=\"%s\"><BR>",
	   cartVar, defaultScore);
    // The dimensions of ui-dropdownchecklist multiselects are not correct when
    // the item is hidden.  So, when this filter section is made visible, reinit them.
    jsInlineF(
	   "$(function(){\n"
	   "$('tr[id^=\"%s-\"]').bind('show',\n"
	   "  function(jqev) { \n"
	   "    var $multisels = $(jqev.target).find('.filterBy');\n"
	   "    var multiselElList = $multisels.each(function(ix, el){ return el; });\n"
	   "    ddcl.reinit(multiselElList);"
	   "  });\n"
	   "});\n"
	   , sectionName);
    puts("</TABLE>");
    endCollapsibleSection();
    }
if (startsWith("bed 5", tdb->type)) //#*** TODO: detect bed# properly
    {
    puts("<TABLE>");
    startSmallCollapsibleSection(sectionName, "filter items", FALSE);
    //#*** Also watch out for noScoreFilter or whatever it's called
    puts("minimum peak score [0-1000]: ");
    safef(cartVar, sizeof(cartVar), "hgva_filter_%s_score", tdb->track);
    char *defaultScore = cartUsualString(cart, cartVar, "0");
    printf("<input type=text name=\"%s\" size=12 value=\"%s\"><BR>",
	   cartVar, defaultScore);
    puts("</TABLE>");
    endCollapsibleSection();
    }
}


typedef boolean TdbFilterFunction(struct trackDb *tdb, void *filterData);
/* Return TRUE if tdb passes filter criteria. */

void rFilterTrackList(struct trackDb *trackList, struct slRef **pPassingRefs,
		      TdbFilterFunction *filterFunc, void *filterData)
/* Recursively apply filterFunc and filterData to all tracks in trackList and
 * their subtracks. Add an slRef to pPassingRefs for each track/subtrack that passes.
 * Caller should slReverse(pPassingRefs) when recursion is all done. */
{
struct trackDb *tdb;
for (tdb = trackList;  tdb != NULL;  tdb = tdb->next)
    {
    if (tdb->subtracks != NULL)
	rFilterTrackList(tdb->subtracks, pPassingRefs, filterFunc, filterData);
    if (filterFunc(tdb, filterData))
	slAddHead(pPassingRefs, slRefNew(tdb));
    }
}

void tdbFilterGroupTrack(struct trackDb *fullTrackList, struct grp *fullGroupList,
			 TdbFilterFunction *filterFunc, void *filterData,
			 struct slRef **retGroupRefList, struct slRef **retTrackRefList)
/* Apply filterFunc and filterData to each track in fullTrackList and all subtracks;
 * make an slRef list of tracks that pass the filter (retTrackRefList) and an slRef
 * list of groups to which the passing tracks belong (retGroupRefList). */
{
struct slRef *trackRefList = NULL;
rFilterTrackList(fullTrackList, &trackRefList, filterFunc, filterData);
slReverse(&trackRefList);
if (retTrackRefList != NULL)
    *retTrackRefList = trackRefList;
if (retGroupRefList != NULL)
    {
    // Which groups have tracks that passed the filter?
    struct hash *hash = hashNew(0);
    struct slRef *trackRef;
    for (trackRef = trackRefList;  trackRef != NULL;  trackRef = trackRef->next)
	{
	struct trackDb *tdb = trackRef->val;
	hashAddInt(hash, tdb->grp, TRUE);
	}
    struct slRef *groupRefList = NULL;
    struct grp *grp;
    for (grp = fullGroupList;  grp != NULL;  grp = grp->next)
	{
	if (hashIntValDefault(hash, grp->name, FALSE))
	    slAddHead(&groupRefList, slRefNew(grp));
	}
    hashFree(&hash);
    slReverse(&groupRefList);
    *retGroupRefList = groupRefList;
    }
}

boolean isVariantCustomTrack(struct trackDb *tdb, void *filterData)
/* This is a TdbFilterFunction to get custom or hub tracks with type pgSnp or vcf(Tabix). */
{
return ((sameString(tdb->grp, "user") || isHubTrack(tdb->track)) &&
	(sameString(tdb->type, "pgSnp") || startsWith("vcf", tdb->type)));
}

void selectVariants()
/* Offer selection of user's variant custom tracks, example variants, pasted input etc. */
{
#define PGSNP_OR_VCF "<A HREF='../FAQ/FAQformat.html#format10' TARGET=_BLANK>pgSnp</A> or " \
       "<A HREF='../goldenPath/help/vcf.html' TARGET=_BLANK>VCF</A>"

printf("<div class='sectionLiteHeader'>Select Variants</div>\n");
/* Check for variant custom tracks.  If there are none, tell the user that they should add one. */
struct slRef *varTrackList = NULL, *varGroupList = NULL;
tdbFilterGroupTrack(fullTrackList, fullGroupList, isVariantCustomTrack, NULL,
		    &varGroupList, &varTrackList);
if (varTrackList == NULL)
    {
    printf("Your session doesn't have any custom tracks or hub tracks in " PGSNP_OR_VCF
           " format.\n");
    hOnClickButton("selVar_AddPgpVcfCt", "return hgva.goToAddCustomTrack();",
                   "add pgSnp or VCF custom track");
    if (hubConnectTableExists())
        {
        nbSpaces(2);
        hOnClickButton("selVar_AddTrkHub", "document.trackHubForm.submit(); return false;", "add track hub");
        }
    puts("<BR>");
    }
else if (slCount(varTrackList) > 1)
    {
    printf("If you have more than one custom track or hub track in "
           PGSNP_OR_VCF " format, please select the one you wish to annotate:<BR>\n");
    }
printf("<B>variants: </B>");
printf("<SELECT ID='hgva_variantTrack' NAME='hgva_variantTrack'>\n");
jsOnEventById("change", "hgva_variantTrack", "hgva.changeVariantSource();");
char *selected = cartUsualString(cart, "hgva_variantTrack", "");
struct slRef *ref;
for (ref = varTrackList;  ref != NULL;  ref = ref->next)
    {
    struct trackDb *tdb = ref->val;
    printOption(tdb->track, selected, tdb->longLabel);
    }
printOption(hgvaSampleVariants, selected, hgvaSampleVariantsLabel);
printOption(hgvaUseHgvs, selected, hgvaHgvsLabel);
struct trackDb *snpTdb = hFindLatestSnpTrack(database, NULL, &fullTrackList);
// We can convert rsIds from snpNNN tables (but not ancient "snp" - no refUCSC column) into VCF
boolean hasSnps = (snpTdb != NULL && differentString(snpTdb->table, "snp"));
if (hasSnps)
    printOption(hgvaUseVariantIds, selected, hgvaVariantIdsLabel);
printf("</SELECT><BR>\n");

printf("<div id='"hgvaHgvsPasteContainer"'%s>\n",
       differentString(selected, hgvaUseHgvs) ? " style='display: none;'" : "");
printf("Enter HGVS terms: one term per line; blank lines and comment lines beginning with '#' "
       "are ignored.<BR>\n"
       HGVS_MUST_USE_ACC"<br>\n");
char *oldPasted = cartUsualString(cart, hgvaHgvs, "");
cgiMakeTextArea(hgvaHgvs, oldPasted, 10, 70);
puts("</div>");

if (hasSnps)
    {
    printf("<div id='"hgvaVariantPasteContainer"'%s>\n",
	   differentString(selected, hgvaUseVariantIds) ? " style='display: none;'" : "");
    printf("Enter dbSNP rs# identifiers separated by whitespace or commas:<BR>\n");
    char *oldPasted = cartUsualString(cart, hgvaVariantIds, "");
    cgiMakeTextArea(hgvaVariantIds, oldPasted, 10, 70);
    puts("</div>");
    }

printf("<B>maximum number of variants to be processed:</B>\n");
char *curLimit = cartUsualString(cart, "hgva_variantLimit", "10000");
char *limitLabels[] = { "10", "100", "1,000", "10,000", "100,000" };
char *limitValues[] = { "10", "100", "1000", "10000", "100000" };
cgiMakeDropListWithVals("hgva_variantLimit", limitLabels, limitValues, ArraySize(limitLabels),
			curLimit);
printCtAndHubButtons();
puts("<BR>");
}

boolean isGeneTrack(struct trackDb *tdb, void *filterData)
/* This is a TdbFilterFunction to get genePred tracks. */
{
return (startsWith("genePred", tdb->type) || sameString("bigGenePred", tdb->type));
}

char *selectGenes()
/* Let user select a gene predictions track; return NULL if there are no genePred tracks. */
{
struct slRef *trackRefList = NULL;
tdbFilterGroupTrack(fullTrackList, fullGroupList, isGeneTrack, NULL, NULL, &trackRefList);
boolean gotGP = (trackRefList != NULL);
if (!gotGP)
    warn("This assembly (%s) has no gene prediction tracks, "
	 "so the VAI will not be able to annotate it.", database);
printf("<div class='sectionLiteHeader'>Select Genes</div>\n");
if (gotGP)
    printf("The gene predictions selected here will be used ");
else
    printf("Gene predictions are required in order ");
printf("to determine the effect of "
       "each variant on genes, for example intronic, missense, splice site, intergenic etc.");
if (!gotGP)
    printf(" Since this assembly has no gene prediction tracks, "
	   "the VAI can't provide functional annotations. "
	   "Please select a different genome.<BR>");
printf("<BR>\n");
if (! gotGP)
    return NULL;
char *firstTrack = ((struct trackDb *)(trackRefList->val))->track;
char *cartGeneTrack = cartOptionalString(cart, "hgva_geneTrack");
if (isNotEmpty(cartGeneTrack))
    {
    // Make sure it's actually in trackRefList (might have been carried over from other db)
    boolean exists = FALSE;
    struct slRef *ref;
    for (ref = trackRefList;  ref != NULL;  ref = ref->next)
        {
        struct trackDb *tdb = ref->val;
        if (sameString(cartGeneTrack, tdb->track))
            {
            exists = TRUE;
            break;
            }
        }
    if (!exists)
        cartGeneTrack = NULL;
    }
char *selected = isNotEmpty(cartGeneTrack) ? cartGeneTrack : firstTrack;
//#*** should show more info about each track... button to pop up track desc?

if (gotGP)
    {
    printf("<SELECT ID='hgva_geneTrack' NAME='hgva_geneTrack'>\n");
    jsOnEventById("change", "hgva_geneTrack", "hgva.changeGeneSource();");
    struct slRef *ref;
    for (ref = trackRefList;  ref != NULL;  ref = ref->next)
	{
	struct trackDb *tdb = ref->val;
    if (tdb->subtracks == NULL)
	printOption(tdb->track, selected, tdb->longLabel);
	}
    puts("</SELECT><BR>");
    }
return selected;
}

//#*** We really need a dbNsfp.[ch]:
enum PolyPhen2Subset { noSubset, HDIV, HVAR };

char *formatDesc(char *url, char *name, char *details, boolean doHtml)
/* Return a description with URL for name plus extra details.  If doHtml,
 * wrap URL in <A ...>...</A>. */
{
char desc[1024];
if (doHtml)
    safef(desc, sizeof(desc), "<A HREF='%s' TARGET=_BLANK>%s</A> %s",
	  url, name, details);
else
    safef(desc, sizeof(desc), "(%s) %s %s",
	  url, name, details);
return cloneString(desc);
}

char *dbNsfpDescFromTableName(char *tableName, enum PolyPhen2Subset subset, boolean doHtml)
/* Return a description for table name... if these become tracks, use tdb of course. */
{
if (sameString(tableName, "dbNsfpSift"))
    return formatDesc("http://sift.bii.a-star.edu.sg/", "SIFT",
		      "(D = damaging, T = tolerated)", doHtml);
else if (sameString(tableName, "dbNsfpPolyPhen2"))
    {
    if (subset == HDIV)
	return formatDesc("http://genetics.bwh.harvard.edu/pph2/", "PolyPhen-2",
			  "with HumDiv training set "
			  "(D = probably damaging, P = possibly damaging, B = benign)", doHtml);
    else if (subset == HVAR)
	return formatDesc("http://genetics.bwh.harvard.edu/pph2/", "PolyPhen-2",
			  "with HumVar training set "
			  "(D = probably damaging, P = possibly damaging, B = benign)", doHtml);
    else
	errAbort("dbNsfpDescFromTableName: invalid PolyPhen2 subset type (%d)", subset);
    }
else if (sameString(tableName, "dbNsfpMutationTaster"))
	return formatDesc("http://www.mutationtaster.org/", "MutationTaster",
			  "(A = disease causing automatic, D = disease causing, "
			  "N = polymorphism, P = polymorphism automatic)", doHtml);
else if (sameString(tableName, "dbNsfpMutationAssessor"))
	return formatDesc("http://mutationassessor.org/", "MutationAssessor",
			  "(high or medium: predicted functional; "
			  "low or neutral: predicted non-functional)", doHtml);
else if (sameString(tableName, "dbNsfpLrt"))
	return formatDesc("http://www.genetics.wustl.edu/jflab/lrt_query.html",
			  "Likelihood ratio test (LRT)",
			  "(D = deleterious, N = Neutral, U = unknown)", doHtml);
else if (sameString(tableName, "dbNsfpVest"))
    return formatDesc("https://www.ncbi.nlm.nih.gov/pmc/articles/PMC3665549/",
                      "Variant Effect Scoring Tool (VEST)",
                      "(scores [0-1] predict confidence that a change is deleterious", doHtml);
else if (sameString(tableName, "dbNsfpGerpNr"))
	return formatDesc("http://mendel.stanford.edu/SidowLab/downloads/gerp/index.html",
			  "GERP++", "Neutral Rate (NR)", doHtml);
else if (sameString(tableName, "dbNsfpGerpRs"))
	return formatDesc("http://mendel.stanford.edu/SidowLab/downloads/gerp/index.html",
			  "GERP++", "Rejected Substitutions (RS)", doHtml);
else if (sameString(tableName, "dbNsfpInterPro"))
	return formatDesc("http://www.ebi.ac.uk/interpro/", "InterPro", "protein domains", doHtml);
return NULL;
}

struct slName *findDbNsfpTables()
/* See if this database contains dbNSFP tables. */
{
if (startsWith(hubTrackPrefix, database))
    return NULL;
struct sqlConnection *conn = hAllocConn(database);
struct slName *dbNsfpTables = sqlListTablesLike(conn, "dbNsfp%");
hFreeConn(&conn);
return dbNsfpTables;
}

void printDbNsfpSource(char *table, enum PolyPhen2Subset subset)
/* If we know what to do with table, make a checkbox with descriptive label. */
{
char *description = dbNsfpDescFromTableName(table, subset, TRUE);
if (description != NULL)
    {
    char cartVar[512];
    if (subset == HDIV)
	safef(cartVar, sizeof(cartVar), "hgva_track_%s_%s:HDIV", database, table);
    else if (subset == HVAR)
	safef(cartVar, sizeof(cartVar), "hgva_track_%s_%s:HVAR", database, table);
    else
	safef(cartVar, sizeof(cartVar), "hgva_track_%s_%s", database, table);
    boolean defaultChecked = (sameString("dbNsfpSift", table) ||
			      sameString("dbNsfpPolyPhen2", table));
    cartMakeCheckBox(cart, cartVar, defaultChecked);
    printf("%s<BR>\n", description);
    }
}

void selectDbNsfp(struct slName *dbNsfpTables)
/* Let user select scores/predicitions from various tools collected by dbNSFP. */
{
if (dbNsfpTables == NULL)
    return;
startCollapsibleSection("dbNsfp", "Database of Non-synonymous Functional Predictions (dbNSFP)",
			TRUE);
//#*** hardcoded version info... we need metadata (#11462)
char *dbNsfpVersion = "3.1a";
char *txVersion = "Gencode release 22 (Ensembl 79, Mar. 2015)";
char *refYear = "2015";
char *refUrl = "http://onlinelibrary.wiley.com/doi/10.1002/humu.22932/abstract";
// For the time being hg19 is still on version 2.0:
if (sameString(database, "hg19"))
    {
    dbNsfpVersion = "2.0";
    txVersion = "Gencode release 9 (Ensembl 64, Dec. 2011)";
    refYear = "2013";
    refUrl = "http://onlinelibrary.wiley.com/doi/10.1002/humu.22376/abstract";
    }

printf("<A HREF='https://sites.google.com/site/jpopgen/dbNSFP' TARGET=_BLANK>dbNSFP</A> "
       "(<A HREF='%s' "
       "TARGET=_BLANK>Liu <em>et al.</em> %s</A>) "
       "release %s "
       "provides pre-computed scores and predictions of functional significance "
       "from a variety of tools.  Every possible coding change to transcripts in "
       "%s gene predictions "
       "has been evaluated.  "
       "<em>Note: This may not encompass all transcripts in your "
       "selected gene set.</em><BR>\n", refUrl, refYear, dbNsfpVersion, txVersion);
//#*** Another cheap hack: reverse alph order happens to be what we want (until VEST??),
//#*** but priorities would be cleaner:
slReverse(&dbNsfpTables);
jsMakeSetClearContainer();
struct slName *table;
for (table = dbNsfpTables;  table != NULL;  table = table->next)
    {
    if (sameString(table->name, "dbNsfpPolyPhen2"))
	{
	printDbNsfpSource(table->name, HDIV);
	printDbNsfpSource(table->name, HVAR);
	}
    else
	printDbNsfpSource(table->name, 0);
    }
jsEndContainer();
puts("<BR>");
endCollapsibleSection();
}

boolean findSnpBed4(char *suffix, char **retFileName, struct trackDb **retTdb)
/* If we can find the latest snpNNNsuffix table, or better yet a bigBed file for it (faster),
 * set the appropriate ret* and return TRUE, otherwise return FALSE. */
{
struct trackDb *tdb = hFindLatestSnpTrack(database, suffix, &fullTrackList);
if (tdb == NULL)
    return FALSE;
boolean foundIt = FALSE;
// Do we happen to have a bigBed version?  Better yet, bed4 only for current uses:
char origFileName[HDB_MAX_PATH_STRING];
safef(origFileName, sizeof(origFileName), "/gbdb/%s/vai/%s.bed4.bb", database, tdb->table);
char* fileName = hReplaceGbdb(origFileName);
if (fileExists(fileName))
    {
    if (retFileName != NULL)
	*retFileName = fileName;
    foundIt = TRUE;
    }
else
    {
    // Not bed4; try just .bb:
    freez(&fileName);
    safef(origFileName, sizeof(origFileName), "/gbdb/%s/vai/%s.bb", database, tdb->table);
    fileName = hReplaceGbdb(origFileName);
    if (fileExists(fileName))
	{
	if (retFileName != NULL)
	    *retFileName = fileName;
	foundIt = TRUE;
	}
    }
if (retTdb == NULL)
    return foundIt;
else
    *retTdb = tdb;
return TRUE;
}

void selectDbSnp(boolean gotSnp)
/* Offer to include rsID (and other fields, or leave that for advanced output??) if available */
{
if (!gotSnp)
    return;
startCollapsibleSection("dbSnp", "Known variation", TRUE);
cartMakeCheckBox(cart, "hgva_rsId", TRUE);
printf("Include <A HREF='https://www.ncbi.nlm.nih.gov/projects/SNP/' TARGET=_BLANK>dbSNP</A> "
       "rs# ID if one exists<BR>\n");
puts("<BR>");
endCollapsibleSection();
}

#define GENCODE_PREFIX "wgEncodeGencode"

struct slName *getGencodeTagVersions()
/* Return a list of version strings from the ends of wgEncodeGencodeTag% tables. */
{
static struct slName *tagVersions = NULL;
if (tagVersions == NULL && !startsWith(hubTrackPrefix, database))
    {
    struct sqlConnection *conn = hAllocConn(database);
    char query[1024];
    sqlSafef(query, sizeof query, "show tables like '"GENCODE_PREFIX"Tag%%'");
    struct slName *tagTables = sqlQuickList(conn, query);
    int offset = strlen(GENCODE_PREFIX"Tag");
    struct slName *tt;
    for (tt = tagTables;  tt != NULL;  tt = tt->next)
        slAddHead(&tagVersions, slNameNew(tt->name + offset));
    hFreeConn(&conn);
    }
return slNameCloneList(tagVersions);
}

boolean knownGeneHasGencodeTags()
/* Return TRUE if this database has knownToTag for knownGene. */
{
return hTableExists(database, "knownGene") && hTableExists(database, "knownToTag");
}

boolean hasGencodeTags()
/* Return TRUE if GENCODE tags can be associated with some gene track in database. */
{
return knownGeneHasGencodeTags() || (getGencodeTagVersions() != NULL);
}

void genbankGetDbTable(char *db, char *gbTable, char **retGenbankDb, char **retTableName)
/* Genbank tables (e.g. gbCdnaInfoTable) may now include a 'db.' prefix, e.g.
 * hgFixed.gbCdnaInfo.  If so, separate out the database and table name.  If gbTable
 * does not have a 'db.' prefix then use the configured genbankDb if given, else db. */
{
char *genbankDb = cfgOptionEnv("GENBANKDB", "genbankDb");
char *tableName = gbTable;
char tableCopy[strlen(gbTable)+1];
safecpy(tableCopy, sizeof(tableCopy), gbTable);
char *words[2];
int wordCount = chopByChar(tableCopy, '.', words, ArraySize(words));
if (wordCount == 2)
    {
    genbankDb = words[0];
    tableName = words[1];
    }
else if (isEmpty(genbankDb))
    genbankDb = db;
if (retGenbankDb)
    *retGenbankDb = cloneString(genbankDb);
if (retTableName)
    *retTableName = cloneString(tableName);
}

boolean genbankTableExists(char *db, char *gbTable)
/* Return TRUE if table (which may or may not be prefixed by genbankDb) exists in
 * genbankDb or db. */
{
char *genbankDb=NULL, *tableName=NULL;
genbankGetDbTable(db, gbTable, &genbankDb, &tableName);
return hTableExists(genbankDb, tableName);
}

boolean hasTxStatus()
/* Return TRUE if any gene track in database has some kind of transcript status info
 * like knownCanonical, GENCODE tags and/or RefSeq status. */
{
if (hasGencodeTags())
    return TRUE;
if (hTableExists(database, "knownGene") && hTableExists(database, "knownCanonical"))
    return TRUE;
if (hTableExists(database, "refGene") && genbankTableExists(database, refSeqStatusTable))
    return TRUE;
return FALSE;
}

char *getLatestGencodeVersion(struct slName *versionList)
/* Return the numerically largest version found in versionList. */
{
int maxVersionNum = -1;
char *maxVersion = NULL;
struct slName *version;
for (version = versionList;  version != NULL;  version = version->next)
    {
    int versionNum = atoi(skipToNumeric(version->name));
    if (versionNum > maxVersionNum)
        {
        maxVersionNum = versionNum;
        maxVersion = version->name;
        }
    }
return cloneString(maxVersion);
}

INLINE char *gencodeTableName(char *suffix, char *version, char *buf, size_t bufSize)
/* Write wgEncodeGencode<suffix><version> into buf.  Return buf for convenience. */
{
safef(buf, bufSize, GENCODE_PREFIX"%s%s", suffix, version);
return buf;
}

boolean refGeneHasGencodeTags(struct slName *gencodeVersionList)
/* Return TRUE if this database has a wgEncodeGencodeRefSeq table as well as a ...Tag table. */
{
char *version = getLatestGencodeVersion(gencodeVersionList);
char table[PATH_LEN];
return hTableExists(database, gencodeTableName("RefSeq", version, table, sizeof(table)));
}

boolean startsWithGencodeGene(char *geneTrack)
/* Return TRUE if geneTrack starts with wgEncodeGencode{Basic,Comp,PseudoGene}.
 * (There are other GENCODE genepred tracks that don't have tags associated with them.) */
{
return (startsWith(GENCODE_PREFIX"Basic", geneTrack) ||
        startsWith(GENCODE_PREFIX"Comp", geneTrack) ||
        startsWith(GENCODE_PREFIX"PseudoGene", geneTrack));
}

boolean isGencodeWithVersion(char *geneTrack, struct slName *versionList)
/* Return TRUE if geneTrack looks like a Gencode gene track for a supported version. */
{
if (! startsWithGencodeGene(geneTrack))
    return FALSE;
struct slName *v;
for (v = versionList;  v != NULL;  v = v->next)
    {
    if (endsWith(geneTrack, v->name))
        return TRUE;
    }
return FALSE;
}

void selectTxStatus(boolean hasTxStatus, char *geneTrack)
/* Offer to include transcript status, e.g. whether it is in knownCanonical or has GENCODE tags.
 * This makes one div per category of txStatus info; each div is visible only if its info is
 * applicable to the selected gene track.  If no divs are visible, display a message that
 * there's nothing for the currently selected gene track. */
{
if (! hasTxStatus)
    return;
startCollapsibleSection("txStatus", "Transcript status", FALSE);
boolean somethingIsVisible = FALSE;
if (hasGencodeTags())
    {
    struct slName *versionList = getGencodeTagVersions();
    char *maybeKnownGene = knownGeneHasGencodeTags() ? "knownGene" : "";
    char *maybeRefGene = refGeneHasGencodeTags(versionList) ? "refGene" : "";
    char *maybeEnsGene = "";
    char *versions = "";
    if (versionList != NULL)
        {
        if (hTableExists(database, "ensGene"))
            maybeEnsGene = "ensGene";
        versions = slNameListToString(versionList, ' ');
        }
    boolean isVisible = (sameString(geneTrack, maybeKnownGene) ||
                         sameString(geneTrack, maybeEnsGene) ||
                         sameString(geneTrack, maybeRefGene) ||
                         isGencodeWithVersion(geneTrack, versionList));
    somethingIsVisible |= isVisible;
    printf("<div class=\"txStatus %s %s %s %s\" style=\"display: %s;\">",
           maybeKnownGene, maybeRefGene, maybeEnsGene, versions,
           isVisible ? "block" : "none");
    cartMakeCheckBox(cart, "hgva_txStatus_gencode", FALSE);
    puts("Include the <A HREF=" GENCODE_TAG_DOC_URL " TARGET=_BLANK>GENCODE tags</A> (if any) "
         "associated with each transcript.<BR>");
    puts("</div>");
    }
if (hTableExists(database, "knownGene") && hTableExists(database, "knownCanonical"))
    {
    boolean isVisible = sameString(geneTrack, "knownGene");
    somethingIsVisible |= isVisible;
    printf("<div class=\"txStatus knownGene\" style=\"display: %s;\">",
           isVisible ? "block" : "none");
    cartMakeCheckBox(cart, "hgva_txStatus_knownCanonical", FALSE);
    char *desc = hTableExists(database, "knownToTag") ?
        "based on <A HREF=" APPRIS_DOC_URL " TARGET=_BLANK>"
        "APPRIS</A> status or inclusion in "
        "<A HREF=" GENCODE_BASIC_DOC_URL " TARGET=_BLANK>GENCODE Basic</A> subset: "
        "principal &gt; alternative &gt; basic &gt; longest isoform" :
        "generally the longest isoform of a gene";
    printf("Indicate whether each transcript is 'canonical' (%s).<BR>\n", desc);
    puts("</div>");
    }
if (hTableExists(database, "refGene") && genbankTableExists(database, refSeqStatusTable))
    {
    boolean isVisible = sameString(geneTrack, "refGene");
    somethingIsVisible |= isVisible;
    printf("<div class=\"txStatus refGene\" style=\"display: %s;\">",
           isVisible ? "block" : "none");
    cartMakeCheckBox(cart, "hgva_txStatus_refSeqStatus", FALSE);
    puts("Include the "
         "<A HREF=" REFSEQ_STATUS_DOC_URL " "
         "TARGET=_BLANK>RefSeq status</A> of each transcript.<BR>");
    puts("</div>");
    }
printf("<div class=\"noTxStatus\" style=\"display: %s;\">",
       somethingIsVisible ? "none" : "block");
puts("No transcript status data are available for the selected gene track.");
puts("</div>");
puts("<BR>");
endCollapsibleSection();
}

static boolean canDoHgvsOut(char *geneTrack)
/* Return TRUE if we're able to make HGVS output terms for transcripts in geneTrack. */
{
return (sameString(geneTrack, "refGene") || startsWith("ncbiRefSeq", geneTrack) ||
        startsWith("wgEncodeGencodeBasic", geneTrack) ||
        startsWith("wgEncodeGencodeComp", geneTrack));
}

static void selectHgvsOut(char *geneTrack)
/* Offer HGVS output choices if RefSeq Genes are selected */
{
startCollapsibleSection("hgvsOut", "HGVS variant nomenclature", TRUE);
printf("The <a href='http://www.hgvs.org/' target=_blank>Human Genome Variation Society (HGVS)</a> "
       "has established a "
       "<a href='http://varnomen.hgvs.org/' target=_blank>sequence variant nomenclature</a>, "
       "an international standard used to report variation in "
       "genomic, transcript and protein sequences.<br>\n");
boolean hgvsOk = canDoHgvsOut(geneTrack);
printf("<div id=\"hgvsOptions\" style=\"display: %s;\">", hgvsOk ? "block" : "none");
cartMakeCheckBox(cart, "hgva_hgvsG", FALSE);
printf("Include HGVS genomic (g.) terms in output<br>\n");
cartMakeCheckBox(cart, "hgva_hgvsCN", FALSE);
printf("Include HGVS coding (c.) terms if applicable, otherwise noncoding (n.) terms, in output"
       "<br>\n");
cartMakeCheckBox(cart, "hgva_hgvsP", FALSE);
printf("Include HGVS protein (p.) terms (if applicable) in output<br>\n");
cartMakeCheckBox(cart, "hgva_hgvsPAddParens", FALSE);
printf("When including HGVS protein (p.) terms, add parentheses around changes to emphasize "
       "that they are predictions<br>\n");
cartMakeCheckBox(cart, "hgva_hgvsBreakDelIns", FALSE);
printf("For variants that involve both a deletion and insertion, "
       "including multi-nucleotide variants, "
       "include the deleted sequence (e.g. show \"delAGinsTT\" instead of only \"delinsTT\")"
       "<br>\n");
puts("</div>");
printf("<div id=\"noHgvs\" style=\"display: %s;\">",
       hgvsOk ? "none" : "block");
printf("Select RefSeq Genes or an official GENCODE release "
       "(\"Basic Gene Annotation Set from GENCODE...\" "
       "or \"Comprehensive Gene Annotation Set...\") "
       "in the \"Select Genes\" section above in order to make options appear.\n");
puts("</div>");
puts("<br>");
endCollapsibleSection();
}

boolean isHg19RegulatoryTrack(struct trackDb *tdb, void *filterData)
/* For now, just look for a couple specific tracks by tableName. */
{
//#*** NEED METADATA
return (sameString("wgEncodeRegDnaseClusteredV3", tdb->table) ||
	sameString("wgEncodeRegTfbsClusteredV3", tdb->table));
}

boolean isHg38RegulatoryTrack(struct trackDb *tdb, void *filterData)
/* For now, just look for a couple specific tracks by tableName. */
{
//#*** NEED METADATA
return (sameString("wgEncodeRegDnaseClustered", tdb->table) ||
	sameString("wgEncodeRegTfbsClusteredV3", tdb->table));
}

struct slRef *findRegulatoryTracks()
/* Look for the very limited set of Regulation tracks that hgVai offers. */
{
struct slRef *regTrackRefList = NULL;
if (sameString(database, "hg19"))
    tdbFilterGroupTrack(fullTrackList, fullGroupList, isHg19RegulatoryTrack,
                        NULL, NULL, &regTrackRefList);
else if (sameString(database, "hg38"))
    tdbFilterGroupTrack(fullTrackList, fullGroupList, isHg38RegulatoryTrack,
                        NULL, NULL, &regTrackRefList);
return regTrackRefList;
}

void selectRegulatory()
/* If trackRefList is non-NULL, make a section with a checkbox for each track,
 * labelled with longLabel, and optionally some filtering options. */
{
struct slRef *trackRefList = findRegulatoryTracks();
if (trackRefList != NULL)
    {
    puts("<BR>");
    printf("<div class='sectionLiteHeader'>Select Regulatory Annotations</div>\n");
    printf("The annotations in this section provide predicted regulatory regions "
	   "based on various experimental data.  "
	   "When a variant overlaps an annotation selected here, the consequence term "
	   "<A HREF=\"http://sequenceontology.org/browser/current_release/term/SO:0001566\" "
	   "TARGET=_BLANK>regulatory_region_variant</A> will be assigned.  "
	   "Follow the links to description pages that explain how each dataset was "
	   "constructed.  "
	   "Some datasets cover a significant portion of the genome and it may be "
	   "desirable to filter these annotations by cell type and/or score in order "
	   "to avoid an overabundance of hits.  ");
    // Use a table with checkboxes in one column and label/other stuff that depends on
    // checkbox in other column.
    puts("<TABLE>");
    struct slRef *ref;
    for (ref = trackRefList;  ref != NULL;  ref = ref->next)
	{
	struct trackDb *tdb = ref->val;
	char cartVar[512];
	safef(cartVar, sizeof(cartVar), "hgva_track_%s_%s", database, tdb->track);
	puts("<TR><TD valign=top>");
	cartMakeCheckBox(cart, cartVar, FALSE);
	puts("</TD><TD>");
	struct trackDb *topTdb = trackDbTopLevelSelfOrParent(tdb);
	printf("<A HREF=\"%s?%s&g=%s\">%s</A><BR>\n", hgTrackUiName(), cartSidUrlString(cart),
	       topTdb->track, tdb->longLabel);
	printFilterOptions(tdb);
	puts("</TD></TR>");
	}
    puts("</TABLE>");
    }
}

boolean isConsElTrack(struct trackDb *tdb, void *filterData)
/* This is a TdbFilterFunction to get "phastConsNwayElements" tracks. */
{
return (startsWith("phastCons", tdb->table) && stringIn("Elements", tdb->table));
}

boolean isConsScoreTrack(struct trackDb *tdb, void *filterData)
/* This is a TdbFilterFunction to get tracks that start with "phastCons" (no Elements)
 * or "phyloP". */
{
return ((startsWith("phastCons", tdb->table) && !stringIn("Elements", tdb->table))
	|| startsWith("phyloP", tdb->table));
}

void findCons(struct slRef **retElTrackRefList, struct slRef **retScoreTrackRefList)
/* See if this database has Conserved Elements and/or Conservation Scores */
{
tdbFilterGroupTrack(fullTrackList, fullGroupList, isConsElTrack,
		    NULL, NULL, retElTrackRefList);
tdbFilterGroupTrack(fullTrackList, fullGroupList, isConsScoreTrack,
		    NULL, NULL, retScoreTrackRefList);
}

boolean trackNameMatches(struct trackDb *tdb, void *filterData)
/* This is a TdbFilterFunction to get track(s) whose tdb->track matches name (filterData). */
{
char *name = filterData;
return sameString(tdb->track, name);
}

struct slRef *findTrackRefByName(char *name)
/* Return a reference to the named track, if found. */
{
struct slRef *trackRefList = NULL;
tdbFilterGroupTrack(fullTrackList, fullGroupList, trackNameMatches, name, NULL, &trackRefList);
return trackRefList;
}

void trackCheckBoxSection(char *sectionSuffix, char *title, struct slRef *trackRefList)
/* If trackRefList is non-NULL, make a collapsible section with a checkbox for each track,
 * labelled with longLabel. */
{
if (trackRefList != NULL)
    {
    startCollapsibleSection(sectionSuffix, title, FALSE);
    struct slRef *ref;
    for (ref = trackRefList;  ref != NULL;  ref = ref->next)
	{
	struct trackDb *tdb = ref->val;
	char cartVar[512];
	safef(cartVar, sizeof(cartVar), "hgva_track_%s_%s", database, tdb->track);
	cartMakeCheckBox(cart, cartVar, FALSE);
	struct trackDb *topTdb = trackDbTopLevelSelfOrParent(tdb);
	printf("<A HREF=\"%s?%s&g=%s\">%s</A><BR>\n", hgTrackUiName(), cartSidUrlString(cart),
	       topTdb->track, tdb->longLabel);
	}
    puts("<BR>");
    endCollapsibleSection();
    }
}

void selectAnnotations(char *geneTrack)
/* Beyond predictions of protein-coding effect, what other basic data can we integrate? */
{
struct slName *dbNsfpTables = findDbNsfpTables();
boolean gotSnp = findSnpBed4("", NULL, NULL);
struct slRef *elTrackRefList = NULL, *scoreTrackRefList = NULL;
findCons(&elTrackRefList, &scoreTrackRefList);
struct slRef *cosmicTrackRefList = findTrackRefByName("cosmic");
boolean hasTxStat = hasTxStatus();
if (dbNsfpTables == NULL && !gotSnp && elTrackRefList == NULL && scoreTrackRefList == NULL &&
    cosmicTrackRefList == NULL && !hasTxStat)
    return;
puts("<BR>");
printf("<div class='sectionLiteHeader'>Select More Annotations (optional)</div>\n");
// Make wrapper table for collapsible sections:
puts("<TABLE border=0 cellspacing=5 cellpadding=0 style='padding-left: 10px;'>");
selectDbNsfp(dbNsfpTables);
selectTxStatus(hasTxStat, geneTrack);
selectHgvsOut(geneTrack);
selectDbSnp(gotSnp);
trackCheckBoxSection("Cosmic", "COSMIC", cosmicTrackRefList);
trackCheckBoxSection("ConsEl", "Conserved elements", elTrackRefList);
trackCheckBoxSection("ConsScore", "Conservation scores", scoreTrackRefList);
puts("</TABLE>");
}

void selectFiltersFunc()
/* Options to restrict variants based on gene region/soTerm from gpFx */
{
startCollapsibleSection("filtersFunc", "Functional role", FALSE);
printf("Include variants annotated as<BR>\n");
jsMakeSetClearContainer();
cartMakeCheckBox(cart, "hgva_include_intergenic", TRUE);
printf("intergenic<BR>\n");
cartMakeCheckBox(cart, "hgva_include_upDownstream", TRUE);
printf("upstream/downstream of gene<BR>\n");
cartMakeCheckBox(cart, "hgva_include_nmdTranscript", TRUE);
printf("in transcript already subject to nonsense-mediated decay (NMD)<BR>\n");
cartMakeCheckBox(cart, "hgva_include_exonLoss", TRUE);
printf("exon loss caused by deletion<BR>\n");
cartMakeCheckBox(cart, "hgva_include_utr", TRUE);
printf("5' or 3' UTR<BR>\n");
cartMakeCheckBox(cart, "hgva_include_cdsSyn", TRUE);
printf("CDS - synonymous coding change<BR>\n");
cartMakeCheckBox(cart, "hgva_include_cdsNonSyn", TRUE);
printf("CDS - non-synonymous (missense, stop gain/loss, frameshift etc)<BR>\n");
cartMakeCheckBox(cart, "hgva_include_intron", TRUE);
printf("intron<BR>\n");
cartMakeCheckBox(cart, "hgva_include_splice", TRUE);
printf("splice site or splice region<BR>\n");
cartMakeCheckBox(cart, "hgva_include_nonCodingExon", TRUE);
printf("exon of non-coding gene<BR>\n");
cartMakeCheckBox(cart, "hgva_include_noVariation", TRUE);
printf("\"variants\" for which no alternate allele has been observed<BR>\n");
struct slRef *regTrackRefList = findRegulatoryTracks();
if (regTrackRefList != NULL)
    {
    cartMakeCheckBox(cart, "hgva_include_regulatory", TRUE);
    printf("regulatory element (note: these are detected only if one or more tracks "
	   "are selected in Regulatory regions above)<BR>\n");
    }
jsEndContainer();
puts("<BR>");
endCollapsibleSection();
}

void selectFiltersKnownVar()
/* Options to restrict output based on overlap with known variants. */
{
boolean gotCommon = findSnpBed4("Common", NULL, NULL);
boolean gotMult = findSnpBed4("Mult", NULL, NULL);
if (!gotCommon && !gotMult)
    return;
startCollapsibleSection("filtersVar", "Known variation", FALSE);
if (gotMult)
    {
    cartMakeCheckBox(cart, "hgva_include_snpMult", TRUE);
    printf("Include variants even if they are co-located with variants that have been mapped to "
	   "multiple genomic locations by dbSNP<BR>\n");
    }
if (gotCommon)
    {
    cartMakeCheckBox(cart, "hgva_include_snpCommon", TRUE);
    printf("Include variants even if they are co-located with \"common\" variants "
	   "(uniquely mapped variants with global minor allele frequency (MAF) "
	   "of at least 1%% according to dbSNP)<BR>\n");
    }
puts("<BR>");
endCollapsibleSection();
}

void selectFiltersCons()
/* Options to restrict output based on overlap with conserved elements. */
{
struct slRef *elTrackRefList = NULL;
tdbFilterGroupTrack(fullTrackList, fullGroupList, isConsElTrack, NULL, NULL, &elTrackRefList);
if (elTrackRefList == NULL)
    return;
startCollapsibleSection("filtersCons", "Conservation", FALSE);
// Use a little table to indent radio buttons (if we do those) past checkbox:
puts("<TABLE><TR><TD>");
cartMakeCheckBox(cart, "hgva_require_consEl", FALSE);
printf("</TD><TD>Include only the variants that overlap:");
if (slCount(elTrackRefList) == 1)
    {
    struct trackDb *tdb = elTrackRefList->val;
    printf(" %s<BR>\n", tdb->longLabel);
    cgiMakeHiddenVar("hgva_require_consEl_track", tdb->track);
    puts("</TD></TR></TABLE>");
    }
else
    {
    puts("</TD></TR>");
    char *selected = cartUsualString(cart, "hgva_require_consEl_track", "");
    struct slRef *ref;
    for (ref = elTrackRefList;  ref != NULL;  ref = ref->next)
	{
	printf("<TR><TD></TD><TD>");
	struct trackDb *tdb = ref->val;
	cgiMakeOnEventRadioButtonWithClass("hgva_require_consEl_track", tdb->track,
				  sameString(tdb->track, selected),
	  NULL, "click", "setCheckboxList('hgva_require_consEl', true);");
	printf("%s</TD></TR>\n", tdb->longLabel);
	}
    puts("</TABLE>");
    }
endCollapsibleSection();
}

void selectFilters()
/* Options to restrict output to variants with specific properties */
{
puts("<BR>");
printf("<div class='sectionLiteHeader'>Define Filters</div>\n");
puts("<TABLE border=0 cellspacing=5 cellpadding=0 style='padding-left: 10px;'>");
selectFiltersFunc();
selectFiltersKnownVar();
selectFiltersCons();
puts("</TABLE>");
}

void selectOutput()
/* Just VEP text for now... should also have VEP HTML with limited # lines too.
 * Next: custom track with same subset of info as we would stuff in VEP */
{
puts("<BR>");
printf("<div class='sectionLiteHeader'>Configure Output</div>\n");
printf("<B>output format: </B>");
char *selected = cartUsualString(cart, "hgva_outFormat", "vepTab");
printf("<SELECT ID='hgva_outFormat' NAME='hgva_outFormat'>\n");
printOption("vepTab", selected, "Variant Effect Predictor (tab-separated text)");
printOption("vepHtml", selected, "Variant Effect Predictor (HTML)");
printf("</SELECT><BR>\n");
char *compressType = cartUsualString(cart, "hgva_compressType", textOutCompressNone);
char *fileName = cartUsualString(cart, "hgva_outFile", "");
printf("<B>output file:</B>&nbsp;");
cgiMakeTextVar("hgva_outFile", fileName, 29);
printf("&nbsp;(leave blank to keep output in browser)<BR>\n");
printf("<B>file type returned:&nbsp;</B>");
cgiMakeRadioButton("hgva_compressType", textOutCompressNone,
		   sameWord(textOutCompressNone, compressType));
printf("&nbsp;plain text&nbsp&nbsp");
cgiMakeRadioButton("hgva_compressType", textOutCompressGzip,
		   sameWord(textOutCompressGzip, compressType));
printf("&nbsp;gzip compressed (ignored if output file is blank)");
puts("<BR>");
}

void submitAndDisclaimer()
{
puts("<div id=disclaimerDialog title='NOTE'>");
puts("This tool is for research use only. While this tool is open to the "
     "public, users seeking information about a personal medical or genetic "
     "condition are urged to consult with a qualified physician for "
     "diagnosis and for answers to personal questions.");
puts("</div><BR>");
printf("<div><img id='loadingImg' src='../images/loading.gif' />\n");
printf("<span id='loadingMsg'></span></div>\n");
cgiMakeOnClickButton("subDisclmAgrd","hgva.submitQueryIfDisclaimerAgreed();", "Get results");
puts("<BR><BR>");
}

/*
 * When user clicks submit, we need to roll a JSON querySpec from form selections,
 * and show data from a submission to hgAnnoGrator.  redirect from this CGI?
 * or have javascript submit directly?
 * * primary: variants, from custom track
 * * if there are genes, those w/annoGratorGpVar
 * * if there are {dbSNP, dbNsfp, regulatory, cons} selections, grator for each of those
 * * vep output config
 *
 * If we get bold & offer 1000Genomes VCF, will def. need handling of split chroms.
 * Are we really going to offer genome-wide in hgVai?
 * Up-front limit on #rows of input ?
 *
 * Eventually, we might want a FormatVep that produces structs that are passed
 * forward to multiple output writers... I would want to send it lots of gratorData
 * like a formatter, but it would produce rows like an annoGrator.
 * Maybe annoGrators should accept a bunch of input rows like formatters?
 * or would this grator wrap all the input grators inside?
 */

void doMainPage()
/* Print out initial HTML of control page. */
{
jsInit();
webIncludeResourceFile("jquery-ui.css");
webIncludeResourceFile("ui.dropdownchecklist.css");
boolean alreadyAgreed = cartUsualBoolean(cart, "hgva_agreedToDisclaimer", FALSE);
jsInlineF(
    "$(document).ready(function() { hgva.disclaimer.init(%s, hgva.userClickedAgree); });\n"
    , alreadyAgreed ? "true" : "false");
addSomeCss();
printAssemblySection();

puts("<BR>");
// Make wrapper table for collapsible sections:
selectVariants();
char *geneTrack = selectGenes();
if (geneTrack != NULL)
    {
    selectRegulatory();
    selectAnnotations(geneTrack);
    selectFilters();
    selectOutput();
    submitAndDisclaimer();
    }
printf("</FORM>");

jsReloadOnBackButton(cart);

webNewSection("Using the Variant Annotation Integrator");
webIncludeHelpFileSubst("hgVaiHelpText", cart, FALSE);
jsIncludeFile("jquery-ui.js", NULL);
jsIncludeFile("hgVai.js", NULL);
jsIncludeFile("ui.dropdownchecklist.js", NULL);
jsIncludeFile("ddcl.js", NULL);
}

void doUi()
/* Set up globals and make web page */
{
cartWebStart(cart, database, "Variant Annotation Integrator");
doMainPage();
cartWebEnd();
/* Save variables. */
cartCheckout(&cart);
}

void checkVariantTrack(struct trackDb *tdb)
/* variantTrack should be either pgSnp or VCF. */
{
if (! sameString(tdb->type, "pgSnp") &&
    ! startsWith("vcf", tdb->type))
    {
    errAbort("Expected variant track '%s' to be type pgSnp, vcf or vcfTabix, but it's '%s'",
	     tdb->track, tdb->type);
    }
}

char *fileNameFromTable(char *table)
/* Get fileName from a bigData table (for when we don't have a trackDb, just table). */
{
struct sqlConnection *conn = hAllocConn(database);
char query[512];
sqlSafef(query, sizeof(query), "select fileName from %s", table);
char *fileName = sqlQuickString(conn, query);
hFreeConn(&conn);
char *fileNameRewrite = hReplaceGbdb(fileName);
freez(&fileName);
return fileNameRewrite;
}

void textOpen()
/* Start serving up plain text, possibly via a pipeline to gzip. */
{
char *fileName = textOutSanitizeHttpFileName(cartUsualString(cart, "hgva_outFile", ""));
char *compressType = cartUsualString(cart, "hgva_compressType", textOutCompressGzip);
compressPipeline = textOutInit(fileName, compressType, NULL);
}

void setGpVarFuncFilter(struct annoGrator *gpVarGrator)
/* Use cart variables to configure gpVarGrator's filtering by functional category. */
{
struct annoGratorGpVarFuncFilter aggvFuncFilter;
ZeroVar(&aggvFuncFilter);
aggvFuncFilter.intergenic = cartUsualBoolean(cart, "hgva_include_intergenic", TRUE);
aggvFuncFilter.upDownstream = cartUsualBoolean(cart, "hgva_include_upDownstream", TRUE);
aggvFuncFilter.nmdTranscript = cartUsualBoolean(cart, "hgva_include_nmdTranscript", TRUE);
aggvFuncFilter.exonLoss = cartUsualBoolean(cart, "hgva_include_exonLoss", TRUE);
aggvFuncFilter.utr = cartUsualBoolean(cart, "hgva_include_utr", TRUE);
aggvFuncFilter.cdsSyn = cartUsualBoolean(cart, "hgva_include_cdsSyn", TRUE);
aggvFuncFilter.cdsNonSyn = cartUsualBoolean(cart, "hgva_include_cdsNonSyn", TRUE);
aggvFuncFilter.intron = cartUsualBoolean(cart, "hgva_include_intron", TRUE);
aggvFuncFilter.splice = cartUsualBoolean(cart, "hgva_include_splice", TRUE);
aggvFuncFilter.nonCodingExon = cartUsualBoolean(cart, "hgva_include_nonCodingExon", TRUE);
aggvFuncFilter.noVariation = cartUsualBoolean(cart, "hgva_include_noVariation", TRUE);
annoGratorGpVarSetFuncFilter(gpVarGrator, &aggvFuncFilter);
}

static void setHgvsOutOptions(struct annoGrator *gpVarGrator, char *geneTrack,
                              struct annoFormatter *vepOut)
/* Use cart variables to configure gpVarGrator's HGVS output. */
{
uint hgvsOutOptions = 0;
if (canDoHgvsOut(geneTrack))
    {
    if (cartUsualBoolean(cart, "hgva_hgvsG", FALSE))
        hgvsOutOptions |= HGVS_OUT_G;
    if (cartUsualBoolean(cart, "hgva_hgvsCN", FALSE))
        hgvsOutOptions |= HGVS_OUT_CN;
    if (cartUsualBoolean(cart, "hgva_hgvsP", FALSE))
        hgvsOutOptions |= HGVS_OUT_P;
    if (cartUsualBoolean(cart, "hgva_hgvsPAddParens", FALSE))
        hgvsOutOptions |= HGVS_OUT_P_ADD_PARENS;
    if (cartUsualBoolean(cart, "hgva_hgvsBreakDelIns", FALSE))
        hgvsOutOptions |= HGVS_OUT_BREAK_DELINS;
    }
annoGratorGpVarSetHgvsOutOptions(gpVarGrator, hgvsOutOptions);
annoFormatVepSetHgvsOutOptions(vepOut, hgvsOutOptions);
}

struct annoGrator *gratorForSnpBed4(struct hash *gratorsByName, char *suffix,
				    struct annoAssembly *assembly, char *chrom,
				    enum annoGratorOverlap overlapRule,
				    char **retDescription)
/* Look up snpNNNsuffix; if we find it, return a grator (possibly for a bigBed 4 file),
 * otherwise return NULL. */
{
char *fileName = NULL;
struct trackDb *tdb = NULL;
if (! findSnpBed4(suffix, &fileName, &tdb))
    return NULL;
struct annoGrator *grator = NULL;
// First look in gratorsByName to see if this grator has already been constructed:
if (tdb != NULL)
    {
    grator = hashFindVal(gratorsByName, tdb->table);
    if (retDescription)
	*retDescription = cloneString(tdb->longLabel);
    }
if (grator != NULL)
    {
    // Set its overlap behavior and we're done.
    grator->setOverlapRule(grator, overlapRule);
    return grator;
    }
// If not in gratorsByName, then attempt to construct it here:
if (fileName != NULL)
    grator = hAnnoGratorFromBigFileUrl(fileName, NULL, assembly, ANNO_NO_LIMIT, overlapRule);
else
    grator = hAnnoGratorFromTrackDb(assembly, tdb->table, tdb, chrom, ANNO_NO_LIMIT,
                                    NULL, overlapRule, NULL);
if (grator != NULL)
    hashAdd(gratorsByName, tdb->table, grator);
return grator;
}

char *tableNameFromSourceName(char *sourceName)
/* Strip sourceName (file path or db.table) to just the basename or table name. */
{
char *tableName = cloneString(sourceName);
char *p = strrchr(tableName, '/');
if (p != NULL)
    {
    // file path; strip to basename
    char dir[PATH_LEN], name[FILENAME_LEN], extension[FILEEXT_LEN];
    splitPath(tableName, dir, name, extension);
    safecpy(tableName, strlen(tableName)+1, name);
    }
else
    {
    // database.table -- skip database.
    p = strchr(tableName, '.');
    if (p != NULL)
	tableName = p+1;
    }
return tableName;
}

char *tagFromTableName(char *tableName, char *suffix)
/* Generate a tag for VEP's extras column or VCF's info column. */
{
char *p = strstr(tableName, "dbNsfp");
if (p != NULL)
    tableName = p + strlen("dbNsfp");
int suffixLen = (suffix == NULL) ? 0 : strlen(suffix);
int tagSize = strlen(tableName) + suffixLen + 1;
char *tag = cloneStringZ(tableName, tagSize);
if (isNotEmpty(suffix))
    safecat(tag, tagSize, suffix);
touppers(tag);
// Some custom shortenings, to avoid very long tag names:
(void)strSwapStrs(tag, tagSize, "POLYPHEN", "PP");
(void)strSwapStrs(tag, tagSize, "MUTATION", "MUT");
(void)strSwapStrs(tag, tagSize, "PHYLOP", "PHP");
(void)strSwapStrs(tag, tagSize, "PHASTCONS", "PHC");
(void)strSwapStrs(tag, tagSize, "ELEMENTS", "EL");
(void)strSwapStrs(tag, tagSize, "PRIMATES", "PRIM");
(void)strSwapStrs(tag, tagSize, "PLACENTAL", "PLAC");
if (regexMatch(tag, "^PH.*[0-9]WAY"))
    (void)strSwapStrs(tag, tagSize, "WAY", "W");
(void)strSwapStrs(tag, tagSize, "WGENCODEREGDNASECLUSTERED", "DNASE");
(void)strSwapStrs(tag, tagSize, "WGENCODEREGTFBSCLUSTERED", "TFBS");
return tag;
}

enum PolyPhen2Subset stripSubsetFromTrackName(char *trackName)
/* trackName may have a _suffix for a subset of PolyPhen2; convert that to enum
 * and zero out the suffix so we have the real trackName. */
{
enum PolyPhen2Subset subset = noSubset;
char *p = strchr(trackName, ':');
if (p != NULL)
    {
    if (sameString(p+1, "HDIV"))
	subset = HDIV;
    else if (sameString(p+1, "HVAR"))
	subset = HVAR;
    else
	errAbort("unrecognized suffix in track_suffix '%s'", trackName);
    *p = '\0';
    }
return subset;
}

void updateGratorListAndVepExtra(struct annoGrator *grator, struct annoGrator **pGratorList,
				 struct annoFormatter *vepOut, enum PolyPhen2Subset subset,
				 char *column, char *description, boolean isReg)
/* If grator is non-NULL, add it to gratorList and vepOut's list of items for EXTRAs column. */
{
if (grator == NULL)
    return;
slAddHead(pGratorList, grator);
if (vepOut != NULL)
    {
    char *tableName = tableNameFromSourceName(grator->streamer.name);
    char *suffix = NULL;
    if (subset == HDIV)
	suffix = "HDIV";
    else if (subset == HVAR)
	suffix = "HVAR";
    char *tag = tagFromTableName(tableName, suffix);
    if (isEmpty(description))
	description = grator->streamer.name;
    if (isReg)
	annoFormatVepAddRegulatory(vepOut, (struct annoStreamer *)grator, tag, description, column);
    else
	annoFormatVepAddExtraItem(vepOut, (struct annoStreamer *)grator, tag, description, column,
                                  FALSE);
    }
}

INLINE void updateGratorList(struct annoGrator *grator, struct annoGrator **pGratorList)
/* If grator is non-NULL, add it to gratorList. */
{
updateGratorListAndVepExtra(grator, pGratorList, NULL, 0, NULL, NULL, FALSE);
}

void addDbNsfpSeqChange(char *trackName, struct annoAssembly *assembly, struct hash *gratorsByName,
			struct annoGrator **pGratorList)
// If the user has selected dbNsfp* data, we also need the underlying dbNsfpSeqChange
// data, so annoFormatVep can tell whether the variant and gpFx are consistent with the
// variant and transcript that dbNsfp used to calculate scores.
{
//#*** Yet another place where we need metadata:
char *seqChangeTable = "dbNsfpSeqChange";
if (hashFindVal(gratorsByName, seqChangeTable) == NULL)
    {
    char *fileName = fileNameFromTable(seqChangeTable);
    if (fileName == NULL)
	errAbort("'%s' requested, but I can't find fileName for %s",
		 trackName, seqChangeTable);
    struct annoGrator *grator = hAnnoGratorFromBigFileUrl(fileName, NULL, assembly, ANNO_NO_LIMIT,
                                                          agoNoConstraint);
    updateGratorList(grator, pGratorList);
    hashAdd(gratorsByName, seqChangeTable, grator);
    }
}

static struct dyString *dyInfo = NULL;

struct hash *getTrackFilterVars(char *track)
/* Return a hash of filter variable names (cart variable suffixes) to slName lists of values. */
{
char filterPrefix[512];
safef(filterPrefix, sizeof(filterPrefix), "hgva_filter_%s_", track);
struct slPair *filterVars = cartVarsWithPrefix(cart, filterPrefix), *var;
int prefixLen = strlen(filterPrefix);
struct hash *varHash = hashNew(0);
for (var = filterVars;  var != NULL;  var = var->next)
    {
    char *varName = var->name+prefixLen;
    char *val = var->val;
    struct hashEl *hel = hashLookup(varHash, varName);
    if (hel != NULL)
	slNameAddHead((struct slName **)(&hel->val), val);
    else
	hashAdd(varHash, varName, slNameNew(val));
    }
return varHash;
}

INLINE boolean isNotAll(struct slName *valList)
/* Return TRUE unless valList has one element with name "All" (for multiselects). */
{
if (slCount(valList) == 1 && sameString(valList->name, "All"))
    return FALSE;
return TRUE;
}

void factorSourceGratorAddFilter(struct annoGrator *grator, char *name, struct slName *valList)
/* Add filter to factorSource grator. */
//#*** Do these smarts belong here in hgVai?  Probably not -- should be an hg/lib module with
//#*** UI/metadata smarts.
{
struct annoStreamer *gStreamer = (struct annoStreamer *)grator;
struct annoFilter *filter = NULL;
if (sameString(name, "name") || sameString(name, "cellType") || sameString(name, "treatment"))
    {
    if (valList && isNotAll(valList))
	filter = annoFilterFromAsColumn(gStreamer->asObj, name, afMatch, valList);
    }
else if (sameString(name, "score"))
    filter = annoFilterFromAsColumn(gStreamer->asObj, name, afGTE, valList);
else
    errAbort("Unrecognized filter name '%s' for %s, type=factorSource", name, gStreamer->name);
if (filter)
    gStreamer->addFilters(gStreamer, filter);
}

void bed5AddFilter(struct annoGrator *grator, char *name, struct slName *valList)
/* Add filter to bed 5 grator. */
{
struct annoStreamer *gStreamer = (struct annoStreamer *)grator;
struct annoFilter *filter = NULL;
if (sameString(name, "name"))
    {
    if (valList && isNotAll(valList))
	filter = annoFilterFromAsColumn(gStreamer->asObj, name, afMatch, valList);
    }
else if (sameString(name, "score"))
    filter = annoFilterFromAsColumn(gStreamer->asObj, name, afGTE, valList);
else
    errAbort("Unrecognized filter name '%s' for %s, type=bed 5", name, gStreamer->name);
if (filter)
    gStreamer->addFilters(gStreamer, filter);
}

void addFiltersToGrator(struct annoGrator *grator, struct trackDb *tdb)
/* Look for filter variables in the cart and add filters to grator accordingly. */
{
struct hash *varHash = getTrackFilterVars(tdb->track);
struct hashEl *hel, *helList = hashElListHash(varHash);
for (hel = helList;  hel != NULL;  hel = hel->next)
    {
    char *filterName = hel->name;
    struct slName *valList = hel->val;
//#*** Need a much better way to dispatch...
    if (sameString("factorSource", tdb->type))
	factorSourceGratorAddFilter(grator, filterName, valList);
    else if (startsWith("bed 5", tdb->type))
	bed5AddFilter(grator, filterName, valList);
    else
	dyStringPrintf(dyInfo, "Ignoring %s filter %s\n", tdb->track, filterName);
    }
hashFree(&varHash);
}

void addOutputTracks(struct annoGrator **pGratorList, struct hash *gratorsByName,
		     struct annoFormatter *vepOut, struct annoAssembly *assembly, char *chrom,
		     boolean doHtml, boolean *retHasRegulatory)
// Construct grators for tracks selected to appear in EXTRAS column
{
boolean includeReg = cartUsualBoolean(cart, "hgva_include_regulatory", TRUE);
boolean haveReg = FALSE;
char trackPrefix[128];
safef(trackPrefix, sizeof(trackPrefix), "hgva_track_%s_", database);
int trackPrefixLen = strlen(trackPrefix);
struct slPair *trackVar, *trackVars = cartVarsWithPrefix(cart, trackPrefix);
for (trackVar = trackVars;  trackVar != NULL;  trackVar = trackVar->next)
    {
    char *val = trackVar->val;
    if (! (sameWord(val, "on") || atoi(val) > 0))
	continue;
    char *trackName = trackVar->name + trackPrefixLen;
    if (sameString(trackName, "dbNsfpPolyPhen2"))
	// PolyPhen2 must have a suffix now -- skip obsolete cartVar from existing carts
	continue;
    struct annoGrator *grator = hashFindVal(gratorsByName, trackName);
    if (grator != NULL)
	// We already have this as a grator:
	continue;
    enum PolyPhen2Subset subset = noSubset;
    char *description = NULL;
    char *column = NULL;
    boolean isReg = FALSE;
    if (startsWith("dbNsfp", trackName))
	{
	// trackName for PolyPhen2 has a suffix for subset -- strip it if we find it:
	subset = stripSubsetFromTrackName(trackName);
	description = dbNsfpDescFromTableName(trackName, subset, doHtml);
	addDbNsfpSeqChange(trackName, assembly, gratorsByName, pGratorList);
	char *fileName = fileNameFromTable(trackName);
	if (fileName != NULL)
	    grator = hAnnoGratorFromBigFileUrl(fileName, NULL, assembly, ANNO_NO_LIMIT, agoNoConstraint);
	}
    else
	{
	struct trackDb *tdb = tdbForTrack(database, trackName, &fullTrackList);
	if (tdb != NULL)
	    {
	    grator = hAnnoGratorFromTrackDb(assembly, tdb->table, tdb, chrom, ANNO_NO_LIMIT, NULL,
                                            agoNoConstraint, NULL);
	    if (grator != NULL)
		{
		//#*** Need something more sophisticated but this works for our
		//#*** limited selection of extra tracks:
		if (asColumnFind(grator->streamer.asObj, "name") != NULL)
		    column = "name";
		addFiltersToGrator(grator, tdb);
		}
	    description = tdb->longLabel;
	    isReg = (includeReg &&
                     ((sameString(database, "hg19") && isHg19RegulatoryTrack(tdb, NULL)) ||
                      (sameString(database, "hg38") && isHg38RegulatoryTrack(tdb, NULL))));
	    }
	}
    haveReg |= isReg;
    updateGratorListAndVepExtra(grator, pGratorList, vepOut, subset, column, description, isReg);
    if (grator != NULL)
	hashAdd(gratorsByName, trackName, grator);
    }
if (retHasRegulatory)
    *retHasRegulatory = haveReg;
}

void addFilterTracks(struct annoGrator **pGratorList, struct hash *gratorsByName,
		     struct annoAssembly *assembly, char *chrom)
// Add grators for filters (not added to vepOut):
{
if (!cartUsualBoolean(cart, "hgva_include_snpCommon", TRUE))
    {
    struct annoGrator *grator = gratorForSnpBed4(gratorsByName, "Common", assembly, chrom,
						 agoMustNotOverlap, NULL);
    updateGratorList(grator, pGratorList);
    }

if (!cartUsualBoolean(cart, "hgva_include_snpMult", TRUE))
    {
    struct annoGrator *grator = gratorForSnpBed4(gratorsByName, "Mult", assembly, chrom,
						 agoMustNotOverlap, NULL);
    updateGratorList(grator, pGratorList);
    }

if (cartUsualBoolean(cart, "hgva_require_consEl", FALSE))
    {
    char *consElTrack = cartString(cart, "hgva_require_consEl_track");
    struct annoGrator *grator = hashFindVal(gratorsByName, consElTrack);
    if (grator == NULL)
	{
	struct trackDb *tdb = tdbForTrack(database, consElTrack, &fullTrackList);
	if (tdb != NULL)
	    grator = hAnnoGratorFromTrackDb(assembly, tdb->table, tdb, chrom, ANNO_NO_LIMIT, NULL,
                                            agoMustOverlap, NULL);
	updateGratorList(grator, pGratorList);
	}
    else
	grator->setOverlapRule(grator, agoMustOverlap);
    }
}

static void getCartPosOrDie(char **retChrom, uint *retStart, uint *retEnd)
/* Get chrom:start-end from cart, errAbort if any problems. */
{
char *position = windowsToAscii(cartString(cart, hgvaRange));
if (! parsePosition(position, retChrom, retStart, retEnd))
    errAbort("Expected position to be chrom:start-end but got '%s'", position);
}

static char *sampleVariantsPath(struct annoAssembly *assembly, char *geneTrack)
/* Construct a path for a trash file of contrived example variants for this assembly
 * and gene track. */
{
char *chrom = NULL;
uint start = 0, end = 0;
getCartPosOrDie(&chrom, &start, &end);
mkdirTrashDirectory(hgvsTrashSubDir);
struct dyString *dy = dyStringCreate("%s/%s/%s_%s_%s_%u-%u.vcf",
				     trashDir(), hgvsTrashSubDir, assembly->name, geneTrack,
				     chrom, start, end);
return dyStringCannibalize(&dy);
}

static char *makeMinimalVcfHeader(char *db, char *headerDefs)
/* Return a string containing a simple no-genotypes VCF header.  db must be non-NULL.
 * headerDefs can be NULL or empty or line(s) starting with '##' and ending w/'\n' */
{
struct dyString *dy = dyStringCreate("##fileformat=VCFv4.2\n"
                                     "##reference=%s\n"
                                     "%s"
                                     "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n",
                                     db, (headerDefs ? headerDefs : ""));
return dyStringCannibalize(&dy);
}

static void writeMinimalVcfHeader(FILE *f, char *db, char *headerDefs)
/* Write header for VCF with no genotype data. db must be non-NULL.
 * headerDefs can be NULL or empty or line(s) starting with '##' and ending w/'\n' */
{
char *headerString = makeMinimalVcfHeader(db, headerDefs);
fputs(headerString, f);
freeMem(headerString);
}

static void mutateBase(char *pBase)
/* Change *pBase into a different base. */
{
static char *bases = "ACGT";
char c;
// In real life the probabilities aren't even, but this is easier.
while ((c = bases[(uint)(floor(drand48() * 4.0))]) == *pBase)
    ;
*pBase = c;
}

static void writeMinimalVcfRow(FILE *f, struct vcfRecord *rec)
/* Write a minimalist VCF row (coords, name, alleles and placeholders). */
{
// VCF columns: #CHROM	POS	ID	REF	ALT	QUAL	FILTER	INFO
fprintf(f, "%s\t%u\t%s\t%s\t",
	rec->chrom, rec->chromStart+1, rec->name, rec->alleles[0]);
// Comma-separated alt alleles:
int i;
for (i = 1;  i < rec->alleleCount;  i++)
    fprintf(f, "%s%s",  (i > 1 ? "," : ""), rec->alleles[i]);
// Placeholder qual, filter, info.
fprintf(f, "\t.\t.\t.\n");
}

static void writeMinimalVcfRowFromBed(FILE *f, struct bed4 *bed, struct dnaSeq *seq, uint seqOffset)
/* Write a minimalist VCF row with a mutation of the reference sequence at the given
 * position. */
{
uint indelBase = 0, start = bed->chromStart, end = bed->chromEnd;
if (end != start+1)
    {
    // VCF treats indels in a special way: pos is the base to the left of the indel,
    // and both ref and alt alleles include the base to the left of the indel.
    indelBase = 1;
    }
// Get reference allele sequence:
uint refAlLen = end - start + indelBase;
char ref[refAlLen+1];
uint seqStart = start - indelBase - seqOffset;
safencpy(ref, sizeof(ref), seq->dna+seqStart, refAlLen);
touppers(ref);
// Alternate allele seq needs extra room in case of single-base insertion:
char alt[refAlLen+2];
alt[0] = ref[0];
alt[1] = '\0';
if (start == end)
    // insertion: alt gets extra base
    safecpy(alt+1, sizeof(alt)-1, "A");
else if (end == start+1)
    // single nucleotide variant
    mutateBase(alt);
// else deletion: leave alt truncated after the shared base to left of indel

// VCF columns: #CHROM	POS	ID	REF	ALT	QUAL	FILTER	INFO
fprintf(f, "%s\t%u\t%s\t%s\t%s\t.\t.\t.\n",
	bed->chrom, start+1-indelBase, bed->name, ref, alt);
}

static void writeArbitraryVariants(FILE *f, struct annoAssembly *assembly)
/* Concoct some variants at an arbitrary position on some assembly sequence.
 * We shouldn't ever get here unless the selected geneTrack is empty. */
{
char *chrom = "NULL";
uint start = 0, end = 0;
getCartPosOrDie(&chrom, &start, &end);
struct bed4 *bed = bed4New(chrom, start, start, "ex_ins");
struct dnaSeq *seq = twoBitReadSeqFragLower(assembly->tbf, chrom, start-1, start+100);
writeMinimalVcfRowFromBed(f, bed, seq, start-1);
dnaSeqFree(&seq);
}

static void addGpFromRow(struct genePred **pGpList, struct annoRow *row,
			 boolean *pNeedCoding, boolean *pNeedNonCoding, boolean isBig)
/* If row is coding and we need a coding gp, add it to pGpList and update pNeedCoding;
 * likewise for noncoding. */
{
struct genePred *gp = isBig ? (struct genePred *)genePredFromBigGenePredRow(row->data) : genePredLoad(row->data);
if (gp->cdsStart != gp->cdsEnd && *pNeedCoding)
    {
    slAddHead(pGpList, gp);
    *pNeedCoding = FALSE;
    }
else if (gp->cdsStart == gp->cdsEnd && *pNeedNonCoding)
    {
    slAddHead(pGpList, gp);
    *pNeedNonCoding = FALSE;
    }
}

static void addGpFromPos(struct annoStreamer *geneStream, char *chrom, uint start, uint end,
			 struct genePred **pGpList, boolean *pNeedCoding, boolean *pNeedNonCoding,
			 struct lm *lm, boolean isBig)
/* Get up to 10 rows from position; if we find one coding and one non-coding gene,
 * add them to pGpList and update pNeed* accordingly. */
{
geneStream->setRegion(geneStream, chrom, start, end);
int rowCount = 0;
struct annoRow *row = NULL;
while (rowCount < 10 && (*pNeedCoding || *pNeedNonCoding)
       && (row = geneStream->nextRow(geneStream, NULL, 0, lm)) != NULL)
    addGpFromRow(pGpList, row, pNeedCoding, pNeedNonCoding, isBig);
}

static struct genePred *genesFromPosition(struct annoStreamer *geneStream, boolean isBig,
					  boolean *retGotCoding, boolean *retGotNonCoding)
/* Try to get a coding and non-coding gene from geneStream at cart position.
 * If there are none, try whole chrom; if none there, first in assembly. */
{
struct genePred *gpList = NULL;
struct lm *lm = lmInit(0);
char *chrom = NULL;
uint start = 0, end = 0;
getCartPosOrDie(&chrom, &start, &end);
boolean needCoding = TRUE, needNonCoding = TRUE;
// First, look for both coding and noncoding genes at cart position:
addGpFromPos(geneStream, chrom, start, end, &gpList, &needCoding, &needNonCoding, lm, isBig);
// If that didn't do it, try querying whole chrom
if (needCoding || needNonCoding)
    addGpFromPos(geneStream, chrom, 0, 0, &gpList, &needCoding, &needNonCoding, lm, isBig);
// If we still haven't found either coding or non-coding on the cart's current chrom,
// try whole genome:
if (needCoding && needNonCoding)
    addGpFromPos(geneStream, NULL, 0, 0, &gpList, &needCoding, &needNonCoding, lm, isBig);
slSort(&gpList, genePredCmp);
lmCleanup(&lm);
if (retGotCoding)
    *retGotCoding = !needCoding;
if (retGotNonCoding)
    *retGotNonCoding = !needNonCoding;
return gpList;
}

static void addSnv(struct bed4 **pBedList, char *chrom, uint start, char *name)
/* Add a single-nucleotide bed4 item. */
{
slAddHead(pBedList, bed4New(chrom, start, start+1, name));
}

// Stuff within this range (see gpFx.h) is considered upstream/downstream of a transcript:
#define UPSTREAMFUDGE GPRANGE

static struct bed4 *sampleVarBedFromGenePred(struct genePred *gp, struct annoAssembly *assembly,
					     uint txSeqStart, uint txSeqEnd, boolean exonOnly)
/* Construct imaginary variants that hit various parts of the transcript
 * described in gp, using reference base values from assembly. */
{
uint basesLeft = gp->txStart - txSeqStart, basesRight = txSeqEnd - gp->txEnd;
struct bed4 *bedList = NULL;
if (!exonOnly && basesLeft > UPSTREAMFUDGE)
    // SNV, intergenic to the left
    addSnv(&bedList, gp->chrom, txSeqStart, "ex_igLeft");
if (!exonOnly && basesLeft > 0)
    // SNV, up/downstream to the left
    addSnv(&bedList, gp->chrom, gp->txStart - 1, "ex_updownLeft");
if (!exonOnly && gp->exonCount > 1)
    {
    // SNV, intron
    uint start = (gp->exonEnds[0] + gp->exonStarts[1]) / 2;
    addSnv(&bedList, gp->chrom, start, "ex_intron");
    // SNV, splice
    addSnv(&bedList, gp->chrom, gp->exonStarts[1] - 2, "ex_splice");
    }
boolean isCoding = (gp->cdsStart != gp->cdsEnd);
if (isCoding)
    {
    if (gp->txStart < gp->cdsStart)
	// SNV, UTR to the left
	addSnv(&bedList, gp->chrom, gp->txStart + 1, "ex_utrLeft");
    int cdsExon = 0;
    while (gp->cdsStart > gp->exonEnds[cdsExon] && cdsExon < gp->exonCount)
	cdsExon++;
    uint start = gp->cdsStart + 2;
    // single-base insertion, leftmost codon
    slAddHead(&bedList, bed4New(gp->chrom, start, start, "ex_codonLeftIns"));
    // 3-base deletion leftmost codon
    slAddHead(&bedList, bed4New(gp->chrom, start, start+3, "ex_codonLeftDel"));
    // SNV, leftmost codon
    addSnv(&bedList, gp->chrom, start+1, "ex_codonLeft");
    if (gp->txEnd > gp->cdsEnd)
	// SNV, UTR to the right
	addSnv(&bedList, gp->chrom, gp->txEnd - 1, "ex_utrRight");
    }
else
    {
    // noncoding exon variant
    uint start = (gp->exonStarts[0] + gp->exonEnds[0]) / 2;
    addSnv(&bedList, gp->chrom, start, "ex_nce");
    }
if (!exonOnly && basesRight > 0)
    // SNV, up/downstream to the left
    addSnv(&bedList, gp->chrom, gp->txEnd + 1, "ex_updownRight");
return bedList;
}

// margin for intergenic variants around transcript:
#define IGFUDGE 5

static struct annoStreamer *makeSampleVariantsStreamer(struct annoAssembly *assembly,
						       struct trackDb *geneTdb, int maxOutRows)
/* Construct a VCF file of example variants for db (unless it already exists)
 * and return an annoStreamer for that file.  If possible, make the variants hit a gene. */
{
char *sampleFile = sampleVariantsPath(assembly, geneTdb->track);
boolean forceRebuild = cartUsualBoolean(cart, "hgva_rebuildSampleVariants", FALSE);
if (! fileExists(sampleFile) || forceRebuild)
    {
    struct annoStreamer *geneStream = hAnnoStreamerFromTrackDb(assembly, geneTdb->table, geneTdb,
                                                               NULL, ANNO_NO_LIMIT, NULL);
    boolean isBig = sameString(geneTdb->type, "bigGenePred");
    boolean gotCoding = FALSE, gotNonCoding = FALSE;
    struct genePred *gpList = genesFromPosition(geneStream, isBig, &gotCoding, &gotNonCoding);
    FILE *f = mustOpen(sampleFile, "w");
    writeMinimalVcfHeader(f, assembly->name, NULL);
    if (gpList == NULL)
	{
	warn("Unable to find any gene transcripts in '%s' (%s)",
	     geneTdb->shortLabel, geneTdb->track);
	writeArbitraryVariants(f, assembly);
	}
    else
	{
	struct bed4 *bedList = NULL;
	uint chromSize = annoAssemblySeqSize(assembly, gpList->chrom);
	uint minSeqStart = chromSize, maxSeqEnd = 0;
	struct genePred *gp;
	for (gp = gpList;  gp != NULL;  gp = gp->next)
	    {
	    int txSeqStart = gp->txStart - (UPSTREAMFUDGE + IGFUDGE);
	    uint txSeqEnd = gp->txEnd + UPSTREAMFUDGE + IGFUDGE;
	    if (txSeqStart < 0)
		txSeqStart = 0;
	    if (txSeqEnd > chromSize)
		txSeqEnd = chromSize;
	    if (txSeqStart < minSeqStart)
		minSeqStart = txSeqStart;
	    if (txSeqEnd > maxSeqEnd)
		maxSeqEnd = txSeqEnd;
	    // If we got a coding gp, but this gp is non-coding, just do its exon variant:
	    boolean exonOnly = gotCoding && (gp->cdsStart == gp->cdsEnd);
	    slCat(&bedList, sampleVarBedFromGenePred(gp, assembly, txSeqStart, txSeqEnd, exonOnly));
	    }
	slSort(&bedList, bedCmp);
	struct dnaSeq *txSeq = twoBitReadSeqFragLower(assembly->tbf, gpList->chrom,
						      minSeqStart, maxSeqEnd);
	struct bed4 *bed;
	for (bed = bedList;  bed != NULL;  bed = bed->next)
	    {
	    writeMinimalVcfRowFromBed(f, bed, txSeq, minSeqStart);
	    }
	dnaSeqFree(&txSeq);
	bed4FreeList(&bedList);
	}
    geneStream->close(&geneStream);
    carefulClose(&f);
    }
return annoStreamVcfNew(sampleFile, NULL, FALSE, assembly, maxOutRows);
}

static char *md5TrashPath(struct annoAssembly *assembly, char *string)
/* Use the md5sum of a string to make a hopefully unique trash filename. */
{
char *md5sum = md5HexForString(string);
mkdirTrashDirectory(hgvsTrashSubDir);
struct dyString *dy = dyStringCreate("%s/%s/%s_%s.vcf", trashDir(), hgvsTrashSubDir,
                                     assembly->name, md5sum);
return dyStringCannibalize(&dy);
}

static struct slName *hashListNames(struct hash *hash)
/* Return a list of all element names in the hash (if any). */
{
struct slName *list = NULL;
struct hashCookie cookie = hashFirst(hash);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    slAddHead(&list, slNameNew(hel->name));
return list;
}

static char *encloseInAngleBracketsDbSnp(char *stringIn)
/* Return a string that has <dbSNP: stringIn>, with spaces replaced by '_'s. */
{
int stringInLen = strlen(stringIn);
int stringOutLen = stringInLen + strlen("<dbSNP:>") + 1;
char *stringOut = needMem(stringOutLen);
safef(stringOut, stringOutLen, "<dbSNP:%s>", stringIn);
subChar(stringOut, ' ', '_');
return stringOut;
}

// dbSNP named alleles have many ways to describe a deletion from the reference,
// for example "LARGEDELETION", "LARGE DELETION", "... DELETED", "... DEL":
static const char *dbSnpDelRegex = "^\\(.*(DELET.*| DEL)\\)$";

static char **altAlsFromObserved(char *refAl, int obsCount, char **obsWords, boolean minusStrand,
                                 int *retAltAlCount, boolean *retNeedLeftBase)
/* Given a non-symbolic reference allele and an array of observed alleles,
 * return an array of +-strand alleles that are not the same as the reference.
 * If any allele is "-" (deleted, zero-length), then set retNeedLeftBase to TRUE
 * because in this case VCF requires that the reference base to the left of the indel
 * must be added to all alleles, and the start coord also moves one base to the left.
 * Also, if any alt allele is symbolic, padding is required.
 * Note: this trashes obsAls.  Resulting array can be freed but not its contents. */
{
boolean obsHasDeletion = FALSE;
int i;
for (i = 0;  i < obsCount;  i++)
    if (sameString(obsWords[i], "-"))
	{
	obsHasDeletion = TRUE;
	break;
	}
char **altAls;
AllocArray(altAls, obsCount);
int altCount = 0;
boolean needLeftBase = isEmpty(refAl) || sameString(refAl, "-");
for (i = 0;  i < obsCount;  i++)
    {
    char *altAl = obsWords[i];
    if (sameString(altAl, "-"))
        altAl[0] = '\0';
    int altAlLen = strlen(altAl);
    if (minusStrand && isAllNt(altAl, altAlLen))
	reverseComplement(altAl, altAlLen);
    if (differentString(altAl, refAl))
	{
	if (isEmpty(altAl))
	    {
	    needLeftBase = TRUE;
	    }
	else
	    {
	    // It would be nice to expand the "(CA)11/12/14/15/16/17/18/19/20" syntax of
	    // some dbSNP observed's.  What are these?: "(D1S243)", "(D1S2870)"
	    // Unfortunately for observed="lengthTooLong" we just can't get the correct allele
	    // sequence. (76,130 of those in snp138)
	    // Hmmm, I guess we could at least stick in the right number of N's if we can
	    // parse "(245 BP INSERTION)".  (2403 rows rlike "[0-9]+ BP ?INSERTION" in snp138)
	    if (!isAllNt(altAl, altAlLen))
		{
		// Symbolic allele: left base required, and enclose it in <dbSNP:>'s.
		// But if it's one of dbSNP's LARGEDELETION kind of alleles, that is redundant
		// with the reference allele, so if we know there is already a "-" allele,
		// skip it.
		if (obsHasDeletion && regexMatch(altAl, dbSnpDelRegex))
		    continue;
		needLeftBase = TRUE;
		altAl = encloseInAngleBracketsDbSnp(altAl);
		}
	    altAls[altCount] = altAl;
	    }
        altAls[altCount] = altAl;
	altCount++;
	}
    }
*retAltAlCount = altCount;
*retNeedLeftBase = needLeftBase;
return altAls;
}

static char **parseDbSnpAltAlleles(char *refAl, char *obsAls, int alleleFreqCount, char *alleles,
				   boolean minusStrand,
                                   int *retAltAlCount, boolean *retNeedLeftBase)
/* Given a non-symbolic reference allele and slash-sep observed alleles from dbSNP,
 * return an array of +-strand alleles that are not the same as the reference.
 * If obsAls is "lengthTooLong" but alleleFreqCount is at least 2 and alleles is a comma-sep
 * list of nucleotide bases, use alleles instead.
 * If any allele is "-" (deleted, zero-length), then set retNeedLeftBase to TRUE
 * because in this case VCF requires that the reference base to the left of the indel
 * must be added to all alleles, and the start coord also moves one base to the left.
 * Also, if any alt allele is symbolic, padding is required.
 * Note: this trashes obsAls.  Resulting array can be freed but not its contents. */
{
int obsCount = 0;
int maxWords = 1000;
char *obsWords[maxWords];
if (sameString(obsAls, "lengthTooLong") && alleleFreqCount > 1)
    {
    obsCount = countChars(alleles, ',');
    if (obsCount > maxWords)
        errAbort("parseDbSnpAltAlleles: too many alleles (%d) in observed column", obsCount);
    chopByChar(alleles, ',', obsWords, obsCount);
    }
else
    {
    obsCount = countChars(obsAls, '/') + 1;
    if (obsCount > maxWords)
        errAbort("parseDbSnpAltAlleles: too many alleles (%d) in observed column", obsCount);
    chopByChar(obsAls, '/', obsWords, obsCount);
    }
return altAlsFromObserved(refAl, obsCount, obsWords, minusStrand, retAltAlCount, retNeedLeftBase);
}

char *firstNCommaSep(struct slName *nameList, int n)
/* Return a comma-separated string with the first n names in nameList. */
{
struct dyString *dy = dyStringNew(0);
int i;
struct slName *el;
for (i=0, el=nameList;  i < 5 && el != NULL;  i++, el = el->next)
    {
    if (i > 0)
	dyStringAppend(dy, ", ");
    dyStringPrintf(dy, "'%s'", el->name);
    }
return dyStringCannibalize(&dy);
}

//#*** Variant ID-matching should be metadata-driven too.  termRegex -> data source.
static const char *rsIdRegex = "^rs[0-9]+$";

static void rsIdsToVcfRecords(struct annoAssembly *assembly, struct slName *rsIds,
			      struct vcfFile *vcff, struct vcfRecord **pRecList,
                              FILE *warnF)
/* If possible, look up coordinates and alleles of dbSNP rs IDs. */
{
if (rsIds == NULL)
    return;
struct trackDb *tdb = hFindLatestSnpTrack(database, NULL, &fullTrackList);
if (tdb == NULL)
    return;
struct sqlConnection *conn = hAllocConn(assembly->name);
// Build a 'name in (...)' query, and build a hash of IDs so we can test whether all were found
struct dyString *dq = sqlDyStringCreate("select chrom, chromStart, chromEnd, name, strand, "
					"refUCSC, observed, alleleFreqCount, alleles "
                                        "from %s where name in (",
					tdb->table);
struct hash *idHash = hashNew(0);
struct slName *id;
for (id = rsIds;  id != NULL;  id = id->next)
    {
    tolowers(id->name);
    if (id != rsIds)
	sqlDyStringPrintf(dq, ",");
    sqlDyStringPrintf(dq, "'%s'", id->name);
    hashStoreName(idHash, id->name);
    }
sqlDyStringPrintf(dq, ");");
struct sqlResult *sr = sqlGetResult(conn, dq->string);
// Construct a minimal VCF row to make a vcfRecord for each variant.
char *vcfRow[9];
vcfRow[5] = vcfRow[6] = vcfRow[7] = "."; // placeholder for qual, filter, info
// It would be cool to someday add snpNNN's exceptions column to the filter or info.
struct dyString *dyAltAlStr = dyStringNew(0);
int i;
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *chrom = row[0];
    uint chromStart = atoll(row[1]);
    uint chromEnd = atoll(row[2]);
    char *name = row[3];
    char *strand = row[4];
    char refAl[max(strlen(row[5]), chromEnd-chromStart) + 2];
    safecpy(refAl, sizeof(refAl), row[5]);
    if (sameString(refAl, "-"))
	refAl[0] = '\0';
    else if (! isAllNt(refAl, strlen(refAl)))
	{
	// refUCSC is symbolic like "( 2548bp insertion )" -- just use the actual reference
	// sequence for now, to keep things simple downstream.
	//#*** Need something more efficient, like a sequence cache inside assembly!
	struct dnaSeq *seq = twoBitReadSeqFragLower(assembly->tbf, chrom, chromStart, chromEnd);
	safecpy(refAl, sizeof(refAl), seq->dna);
	toUpperN(refAl, seq->size);
	dnaSeqFree(&seq);
	}
    char *obsAls = row[6];
    int alFCount = atoi(row[7]);
    char *alFAls = row[8];
    int altAlCount = 0;
    boolean needLeftBase = FALSE;
    char **altAls = parseDbSnpAltAlleles(refAl, obsAls, alFCount, alFAls, sameString(strand, "-"),
					 &altAlCount, &needLeftBase);
    needLeftBase |= (chromStart == chromEnd);  // should be redundant, but just in case.
    // variantProjector doesn't accept symbolic alleles (aside from VCF <DEL>),
    // so filter those out and warn:
    int j = 0;
    struct dyString *noSym = NULL;
    char *filteredAltAls[altAlCount];
    for (i = 0;  i < altAlCount;  i++)
        {
        if (!isAllNt(altAls[i], strlen(altAls[i])))
            {
            if (noSym == NULL)
                noSym = dyStringCreate("%s has symbolic allele(s) e.g. '%s', ignoring.  ",
                                       name, altAls[i]);
            }
        else
            filteredAltAls[j++] = altAls[i];
        }
    if (noSym != NULL)
        {
        fputs(noSym->string, warnF);
        dyStringFree(&noSym);
        }
    altAlCount = j;
    altAls = filteredAltAls;
    hashRemove(idHash, name);
    uint vcfStart = chromStart + 1;
    dyStringClear(dyAltAlStr);
    // If this is a non-symbolic indel, we'll have to move start one base to the left
    // and add the reference base to the left of the actual alleles
    if (needLeftBase)
	{
	vcfStart--;
	//#*** Need something more efficient, like a sequence cache inside assembly!
	struct dnaSeq *seq = twoBitReadSeqFragLower(assembly->tbf, chrom,
						    chromStart-1, chromStart);
	toUpperN(seq->dna, 1);
	char leftBase = seq->dna[0];
	dnaSeqFree(&seq);
	char refAlPlus[strlen(refAl)+2];
	safef(refAlPlus, sizeof(refAlPlus), "%c%s", leftBase, refAl);
	safecpy(refAl, sizeof(refAl), refAlPlus);
	for (i = 0;  i < altAlCount;  i++)
	    {
	    if (i > 0)
		dyStringPrintf(dyAltAlStr, ",");
            dyStringPrintf(dyAltAlStr, "%c%s", leftBase, altAls[i]);
	    }
	}
    else
	{
	// Not an indel, just make comma-sep string of alt alleles.
	for (i = 0;  i < altAlCount;  i++)
	    {
	    if (i > 0)
		dyStringPrintf(dyAltAlStr, ",");
	    dyStringPrintf(dyAltAlStr, "%s", altAls[i]);
	    }
	}
    if (altAlCount == 0)
        dyStringPrintf(dyAltAlStr, ".");
    char vcfStartStr[64];
    safef(vcfStartStr, sizeof(vcfStartStr), "%d", vcfStart);
    vcfRow[0] = chrom;
    vcfRow[1] = vcfStartStr;
    vcfRow[2] = name;
    vcfRow[3] = refAl;
    vcfRow[4] = dyAltAlStr->string;
    struct vcfRecord *rec = vcfRecordFromRow(vcff, vcfRow);
    slAddHead(pRecList, rec);
    }
dyStringFree(&dyAltAlStr);
// Check for IDs not found
struct slName *notFoundIds = hashListNames(idHash);
if (notFoundIds != NULL)
    {
    char *namesNotFound = firstNCommaSep(notFoundIds, 5);
    fprintf(warnF, "%d rs# IDs not found, e.g. %s",
            slCount(notFoundIds), namesNotFound);
    freeMem(namesNotFound);
    }
slNameFreeList(&notFoundIds);
}

static struct vcfRecord *parseVariantIds(struct annoAssembly *assembly, char *variantIdText,
					 FILE *warnF)
/* Return a sorted list of minimal vcfRecords (coords, name, ref and alt alleles)
 * corresponding to each pasted variant. */
{
struct vcfRecord *recList = NULL;
char *p = cloneString(variantIdText), *id;
struct slName *rsIds = NULL, *unknownIds = NULL;
subChar(p, ',', ' ');
struct lineFile *lf = lineFileOnString("rs# IDs", TRUE, p);
char *line = NULL;
while (lineFileNextReal(lf, &line))
    {
    while ((id = nextWord(&line)) != NULL)
        {
        if (regexMatchNoCase(id, rsIdRegex))
            slNameAddHead(&rsIds, id);
        else
            slNameAddHead(&unknownIds, id);
        }
    }
lineFileClose(&lf);
if (unknownIds != NULL)
    {
    slReverse(&unknownIds);
    char *firstUnknownIds = firstNCommaSep(unknownIds, 5);
    fprintf(warnF, "%d variant identifiers are unrecognized, e.g. %s.  ",
            slCount(unknownIds), firstUnknownIds);
    freeMem(firstUnknownIds);
    }
struct vcfFile *vcff = vcfFileNew();
rsIdsToVcfRecords(assembly, rsIds, vcff, &recList, warnF);
slSort(&recList, vcfRecordCmp);
slNameFreeList(&rsIds);
slNameFreeList(&unknownIds);
return recList;
}

static void adjustRangeForVariants(struct vcfRecord *recList, char **pChrom,
				   uint *pStart, uint *pEnd)
/* Given a *sorted* list of VCF records, look at the first and last records and if
 * they fall outside of the range, expand the range to include them. */
{
if (pChrom != NULL && *pChrom != NULL && recList != NULL)
    {
    // We have a non-empty sorted list of records, and search is not genome-wide;
    // look at first and last variants to see if they're not already included in search range.
    struct vcfRecord *first = recList, *last = recList;
    while (last->next)
	last = last->next;
    if (differentString(*pChrom, first->chrom) || differentString(*pChrom, last->chrom))
	{
	// Variants span multiple chroms; force genome-wide search.
	*pChrom = NULL;
	*pStart = *pEnd = 0;
	}
    else
	{
	// Variant(s) are on the same chrom as search range; check starts and ends.
	if (*pStart > first->chromEnd)
	    *pStart = first->chromEnd;
	if (*pEnd < last->chromStart)
	    *pEnd = last->chromStart;
	}
    }
}

static void slAddFileContentsTail(struct slName **pSlNameList, char *filename)
/* If filename exists and is nonempty, add its contents as a new slName at end of list. */
{
char *contents = NULL;
size_t contentSize = 0;
if (fileExists(filename))
    readInGulp(filename, &contents, &contentSize);
if (isNotEmpty(contents))
    slAddTail(pSlNameList, slNameNew(contents));
freeMem(contents);
}

static struct annoStreamer *makeVariantIdStreamer(struct annoAssembly *assembly, int maxOutRows,
						  char **pChrom, uint *pStart, uint *pEnd,
						  struct slName **pCommentList)
/* Translate user's pasted/uploaded variant IDs into minimal VCF if possible.
 * Return a VCF streamer for those, and if the current search position is too narrow
 * to include all of the variants, widen it as necessary. */
{
// Hash variant text to get trash filename.  Use if exists, otherwise build it.
char *variantIds = cartString(cart, hgvaVariantIds);
char *varFile = md5TrashPath(assembly, variantIds);
char warnFile[strlen(varFile)+6];
safef(warnFile, sizeof(warnFile), "%s.warn", varFile);
boolean forceRebuild = cartUsualBoolean(cart, "hgva_rebuildVariantIds", FALSE);
if (! fileExists(varFile) || forceRebuild)
    {
    FILE *warnF = mustOpen(warnFile, "w");
    struct vcfRecord *varList = parseVariantIds(assembly, variantIds, warnF);
//#*** If no variants were recognized, we should probably show main page with a warning.
    adjustRangeForVariants(varList, pChrom, pStart, pEnd);
    FILE *f = mustOpen(varFile, "w");
    writeMinimalVcfHeader(f, assembly->name, NULL);
    struct vcfRecord *var;
    for (var = varList;  var != NULL;  var = var->next)
	writeMinimalVcfRow(f, var);
    carefulClose(&f);
    carefulClose(&warnF);
    }
slAddFileContentsTail(pCommentList, warnFile);
return annoStreamVcfNew(varFile, NULL, FALSE, assembly, maxOutRows);
}

static struct vcfRecord *parseHgvs(struct vcfFile *vcff, struct annoAssembly *assembly,
                                   char *hgvsTerms, FILE *warnF)
/* Return a sorted list of vcfRecords . */
{
struct vcfRecord *recList = NULL;
struct slName *failedTerms = NULL;
struct dyString *dyError = dyStringNew(0);
struct lineFile *lf = lineFileOnString("user-provided HGVS terms", TRUE, cloneString(hgvsTerms));
char *term = NULL;
boolean notVersionedAcc = FALSE;
while (lineFileNextReal(lf, &term))
    {
    eraseTrailingSpaces(term);
    dyStringClear(dyError);
    struct vcfRow *vcfRow = hgvsToVcfRow(assembly->name, term, FALSE, dyError);
    if (vcfRow)
        {
        char *row[8];
        row[0] = vcfRow->chrom;
        char posStr[64];
        safef(posStr, sizeof(posStr), "%d", vcfRow->pos);
        row[1] = posStr;
        row[2] = vcfRow->id;
        row[3] = vcfRow->ref;
        row[4] = vcfRow->alt;
        row[5] = ".";
        row[6] = vcfRow->filter;
        row[7] = vcfRow->info;
        slAddHead(&recList, vcfRecordFromRow(vcff, row));
        }
    else
        {
        if (!regexMatch(term, "^[A-Z_0-9]+\\.[0-9]+[: ]"))
            notVersionedAcc = TRUE;
	slNameAddHead(&failedTerms, dyStringContents(dyError));
        }
    }
if (failedTerms != NULL)
    {
    slReverse(&failedTerms);
    char *firstUnknownIds = firstNCommaSep(failedTerms, 5);
    fprintf(warnF, "%d HGVS terms could not be parsed and/or mapped to %s, e.g. %s.  ",
            slCount(failedTerms), assembly->name, firstUnknownIds);
    if (notVersionedAcc)
        fputs(HGVS_MUST_USE_ACC, warnF);
    freeMem(firstUnknownIds);
    }
slSort(&recList, vcfRecordCmp);
slNameFreeList(&failedTerms);
dyStringFree(&dyError);
lineFileClose(&lf);
return recList;
}

static struct annoStreamer *makeHgvsStreamer(struct annoAssembly *assembly, int maxOutRows,
                                             char **pChrom, uint *pStart, uint *pEnd,
                                             struct slName **pCommentList)
/* Translate user's pasted/uploaded variant IDs into minimal VCF if possible.
 * Return a VCF streamer for those., and if the current search position is too narrow
 * to include all of the variants, widen it as necessary. */
{
// Hash HGVS input to get trash filename.  Use if exists, otherwise build it.
char *hgvsTerms = cartString(cart, hgvaHgvs);
char *varFile = md5TrashPath(assembly, hgvsTerms);
char warnFile[strlen(varFile)+6];
safef(warnFile, sizeof(warnFile), "%s.warn", varFile);
boolean forceRebuild = cartUsualBoolean(cart, "hgva_rebuildHgvs", FALSE);
if (! fileExists(varFile) || forceRebuild)
    {
    char *headerString = makeMinimalVcfHeader(assembly->name, HGVS_VCF_HEADER_DEFS);
    struct vcfFile *vcff = vcfFileFromHeader("hgVaiHgvs", headerString, VCF_IGNORE_ERRS);
    FILE *warnF = mustOpen(warnFile, "w");
    struct vcfRecord *varList = parseHgvs(vcff, assembly, hgvsTerms, warnF);
//#*** If no HGVS terms could be mapped, we should probably show main page with a warning.
    adjustRangeForVariants(varList, pChrom, pStart, pEnd);
    FILE *f = mustOpen(varFile, "w");
    writeMinimalVcfHeader(f, assembly->name, HGVS_VCF_HEADER_DEFS);
    struct vcfRecord *var;
    for (var = varList;  var != NULL;  var = var->next)
	writeMinimalVcfRow(f, var);
    carefulClose(&f);
    carefulClose(&warnF);
    vcfFileFree(&vcff);
    }
slAddFileContentsTail(pCommentList, warnFile);
return annoStreamVcfNew(varFile, NULL, FALSE, assembly, maxOutRows);
}

static struct trackDb *getVariantTrackDb(char *variantTrack)
/* Get a tdb for the user's selected variant track, or warn and return NULL
 * (main page will be displayed) */
{
struct trackDb *varTdb = tdbForTrack(database, variantTrack, &fullTrackList);
if (varTdb == NULL)
    {
    if (isHubTrack(variantTrack))
	warn("Can't find hub track '%s'", variantTrack);
    else
	warn("Can't find tdb for variant track '%s'", variantTrack);
    }
else
    checkVariantTrack(varTdb);
return varTdb;
}

static char *gencodeVersionFromTrack(char *track)
/* If track is a GENCODE table, find and return a pointer to the version at the end;
 * otherwise return NULL. */
{
if (startsWithGencodeGene(track))
    {
    char *v = strrchr(track, 'V');
    return v;
    }
return NULL;
}

static char *gencodeTagTableForTrack(char *db, char *track)
/* If there is a wgEncodeGencodeTag<version> table that can be associated with track,
 * return it; otherwise return NULL. */
{
struct slName *versionList = getGencodeTagVersions();
if (startsWithGencodeGene(track))
    {
    char *version = gencodeVersionFromTrack(track);
    if (version != NULL)
        {
        char table[PATH_LEN];
        return cloneString(gencodeTableName("Tag", version, table, sizeof(table)));
        }
    }
else if (sameString(track, "refGene") && refGeneHasGencodeTags(versionList))
    {
    char *version = getLatestGencodeVersion(versionList);
    char table[PATH_LEN];
    if (hTableExists(db, gencodeTableName("RefSeq", version, table, sizeof(table))))
        return cloneString(gencodeTableName("Tag", version, table, sizeof(table)));
    }
else if (sameString(track, "knownGene") && knownGeneHasGencodeTags())
    {
    if (hTableExists(db, "knownToTag"))
        return cloneString("knownToTag");
    }
return NULL;
}

static struct joinerDtf *getTxStatusExtras(char *db, char *track)
// Several kinds of transcript status may be enabled in the cart; if any are enabled,
// and if they apply to track, return the tables & fields to be joined with the track.
{
struct joinerDtf *txStatusExtras = NULL;
if (cartUsualBoolean(cart, "hgva_txStatus_gencode", FALSE))
    {
    char *gencodeTagTable = gencodeTagTableForTrack(db, track);
    if (gencodeTagTable != NULL)
        {
        char *field = "tag";
        if (sameString("knownToTag", gencodeTagTable))
            field = "value";
        slAddHead(&txStatusExtras, joinerDtfNew(db, gencodeTagTable, field));
        }
    }
if (cartUsualBoolean(cart, "hgva_txStatus_knownCanonical", FALSE) &&
    sameString(track, "knownGene") &&
    hTableExists(db, "knownCanonical"))
    {
    slAddHead(&txStatusExtras, joinerDtfNew(db, "knownCanonical", "transcript"));
    }
if (cartUsualBoolean(cart, "hgva_txStatus_refSeqStatus", FALSE) &&
    sameString(track, "refGene") &&
    genbankTableExists(db, refSeqStatusTable))
    {
    char *genbankDb=NULL, *tableName=NULL;
    genbankGetDbTable(db, refSeqStatusTable, &genbankDb, &tableName);
    slAddHead(&txStatusExtras, joinerDtfNew(genbankDb, tableName, "status"));
    }
return txStatusExtras;
}

static void configAddDtf(struct dyString *dy, struct joinerDtf *dtf, boolean *pIsFirst)
/* Add a JSON object with [db].table and (list of one) field. */
{
if (! *pIsFirst)
    dyStringAppend(dy, ", ");
// The convention for related tables in same db as track is to prepend "." instead of "<db>.":
char *db = (sameString(dtf->database, database) ? "" : dtf->database);
dyStringPrintf(dy, "{ \"table\": \"%s.%s\", \"fields\": [\"%s\"] }", db, dtf->table, dtf->field);
*pIsFirst = FALSE;
}

static void configAddTableField(struct dyString *dy, char *table, char *field, boolean *pIsFirst)
/* Add a JSON object with current database, table and (list of one) field. */
{
struct joinerDtf dtf;
dtf.database = database;
dtf.table = table;
dtf.field = field;
configAddDtf(dy, &dtf, pIsFirst);
}

static struct jsonElement *configForStreamer(char *db, struct trackDb *tdb,
                                             struct joinerDtf *txStatusExtras)
/* Add VAI-specific config options, if applicable. */
{
struct jsonElement *config = NULL;
char *track = tdb->track;
struct dyString *dyConfig = dyStringCreate("{ \"naForMissing\": false,"
                                           "  \"relatedTables\": [ ");
boolean isFirst = TRUE;
// If track is sql-based knownGene and we have kgXref, then add kgXref.geneSymbol after
// the columns of knownGene.
if (sameString(track, "knownGene") &&
    !isCustomTrack(track) && !isHubTrack(track) &&
    !trackDbSetting(tdb, "bigDataUrl") &&
    hTableExists(db, "kgXref"))
    {
    configAddTableField(dyConfig, "kgXref", "geneSymbol", &isFirst);
    }
struct joinerDtf *txStatDtf;
for (txStatDtf = txStatusExtras;  txStatDtf != NULL;  txStatDtf = txStatDtf->next)
    configAddDtf(dyConfig, txStatDtf, &isFirst);

// If any of the above apply, close the relatedTables list and config object
// and parse into jsonElements.
if (! isFirst)
    {
    dyStringAppend(dyConfig, " ] }");
    config = jsonParse(dyConfig->string);
    dyStringFree(&dyConfig);
    }
return config;
}

static void adjustGpVarOverlapRule(struct annoGrator *gpVarGrator, boolean haveRegulatory)
/* If we're able to detect regulatory elements, and want to keep those annotations, loosen up
 * gpVarGrator's overlap rule from the default (must overlap). */
{
if (haveRegulatory && cartUsualBoolean(cart, "hgva_include_regulatory", TRUE))
    gpVarGrator->setOverlapRule(gpVarGrator, agoNoConstraint);
}

static void addTxStatusExtras(struct annoFormatter *vepOut, char *geneTrack,
                              struct annoGrator *gpVarGrator,
                              struct joinerDtf *txStatusExtras)
/* Given a list of tables and fields that will be joined with geneTrack to provide transcript
 * status info, configure vepOut to put them in the EXTRAs column. */
{
struct joinerDtf *txStatDtf;
for (txStatDtf = txStatusExtras;  txStatDtf != NULL;  txStatDtf = txStatDtf->next)
    {
    char *tag = NULL, *description = NULL;
    boolean isBoolean = FALSE;
    // Double-check that we're not joining in some table from a different assembly database:
    if (differentString(txStatDtf->database, database) &&
        differentString(txStatDtf->database, "hgFixed"))
        errAbort("addTxStatusExtras: Expected db=%s or hgFixed in txStatDtf but got %s",
                 database, txStatDtf->database);
    if ((startsWith(GENCODE_PREFIX"Tag", txStatDtf->table) &&
         sameString(txStatDtf->field, "tag")) ||
        (sameString(txStatDtf->table, "knownToTag") &&
         sameString(txStatDtf->field, "value")))
        {
        tag = "GENCODE_TAG";
        description = "<A HREF=" GENCODE_TAG_DOC_URL " "
            "TARGET=_BLANK>GENCODE tags</A> for the transcript";
        }
    else if (sameString(txStatDtf->table, "knownCanonical") &&
             sameString(txStatDtf->field, "transcript"))
        {
        tag = "CANONICAL";
        description = "If present, the transcript is the 'canonical' transcript of the gene "
            "(generally the longest isoform of the gene)";
        isBoolean = TRUE;
        }
    else if (sameString(txStatDtf->table, "refSeqStatus") &&
             sameString(txStatDtf->field, "status"))
        {
        tag = "REFSEQ_STATUS";
        description = "<A HREF=" REFSEQ_STATUS_DOC_URL " "
         "TARGET=_BLANK>RefSeq status</A> of the transcript";
        }
    else
        {
        errAbort("addTxStatusExtras: Unrecognized {table,field}: {%s,%s}",
                 txStatDtf->table, txStatDtf->field);
        }
    char *column = annoStreamDbColumnNameFromDtf(database, geneTrack, txStatDtf);
    annoFormatVepAddExtraItem(vepOut, (struct annoStreamer *)gpVarGrator,
                              tag, description, column, isBoolean);
    }
}

static char *getHubPrefix(char *dbOrTrack)
/* Extract the hub_number_ prefix from a db or track identifier, allocate & return. */
{
if (!startsWith(hubTrackPrefix, dbOrTrack))
    {
    errAbort("getHubPrefix: '%s' does not start with the expected prefix '%s'", dbOrTrack, hubTrackPrefix);
    }
uint hubNumber = atoll(dbOrTrack + strlen(hubTrackPrefix));
struct dyString *dy = dyStringCreate("%s%u_", hubTrackPrefix, hubNumber);
return dyStringCannibalize(&dy);
}

void doQuery()
/* Translate simple form inputs into anno* components and execute query. */
{
dyInfo = dyStringNew(0);
char *chrom = NULL;
uint start = 0, end = 0;
if (sameString(regionType, hgvaRegionTypeRange))
    getCartPosOrDie(&chrom, &start, &end);
struct annoAssembly *assembly = hAnnoGetAssembly(database);
boolean isCommandLine = (cgiOptionalString("cgiSpoof") != NULL);

char *geneTrack = cartString(cart, "hgva_geneTrack");
struct trackDb *geneTdb = tdbForTrack(database, geneTrack, &fullTrackList);
if (geneTdb == NULL)
    {
    if (startsWith(hubTrackPrefix, database) && !startsWith(hubTrackPrefix, geneTrack))
        {
        // geneTrack was provided without the hub_ prefix, try adding prefix and looking it up
        char *hubNumberPrefix = getHubPrefix(database);
        struct dyString *dy = dyStringCreate("%s%s", hubNumberPrefix, geneTrack);
        geneTdb = tdbForTrack(database, dy->string, &fullTrackList);
        if (geneTdb != NULL)
            geneTrack = dyStringCannibalize(&dy);
        }
    if (geneTdb == NULL)
        {
        warn("Can't find tdb for gene track %s", geneTrack);
        if (! isCommandLine)
            doUi();
        return;
        }
    }

int maxVarRows = cartUsualInt(cart, "hgva_variantLimit", 10);
struct annoStreamer *primary = NULL;
char *primaryLongLabel = NULL;
char *variantTrack = cartString(cart, "hgva_variantTrack");
struct slName *commentList = NULL;
if (sameString(variantTrack, hgvaSampleVariants))
    {
    primary = makeSampleVariantsStreamer(assembly, geneTdb, maxVarRows);
    primaryLongLabel = hgvaSampleVariantsLabel;
    // Sample variants can't always be made within the currently selected position range,
    // so just for these, force search to be genome-wide.
    chrom = NULL;
    start = 0;
    end = 0;
    }
else if (sameString(variantTrack, hgvaUseVariantIds))
    {
    // Override search position if cart position doesn't include all variants:
    primary = makeVariantIdStreamer(assembly, maxVarRows, &chrom, &start, &end, &commentList);
    primaryLongLabel = hgvaVariantIdsLabel;
    }
else if (sameString(variantTrack, hgvaUseHgvs))
    {
    primary = makeHgvsStreamer(assembly, maxVarRows, &chrom, &start, &end, &commentList);
    primaryLongLabel = hgvaHgvsLabel;
    }
else if (sameString(variantTrack, hgvaUseVariantFileOrUrl))
    {
    char *fileOrUrl = cartString(cart, hgvaVariantFileOrUrl);
    char *type = cartOptionalString(cart, hgvaVariantFileOrUrlType);
    primary = hAnnoStreamerFromBigFileUrl(fileOrUrl, NULL, assembly, maxVarRows, type);
    primaryLongLabel = hgvaVariantFileOrUrlLabel;
    }
else
    {
    struct trackDb *varTdb = getVariantTrackDb(variantTrack);
    if (varTdb == NULL)
	{
        if (! isCommandLine)
            doUi();
	return;
	}
    primary = hAnnoStreamerFromTrackDb(assembly, varTdb->table, varTdb, chrom, maxVarRows, NULL);
    primaryLongLabel = varTdb->longLabel;
    }

enum annoGratorOverlap geneOverlapRule = agoMustOverlap;
struct joinerDtf *txStatusExtras = getTxStatusExtras(database, geneTrack);
struct jsonElement *gpConfig = configForStreamer(database, geneTdb, txStatusExtras);
struct annoGrator *gpVarGrator = hAnnoGratorFromTrackDb(assembly, geneTdb->table, geneTdb, chrom,
                                                        ANNO_NO_LIMIT, primary->asObj,
                                                        geneOverlapRule, gpConfig);
setGpVarFuncFilter(gpVarGrator);

// Some grators may be used as both filters and output values. To avoid making
// multiple grators for the same source, hash them by trackName:
struct hash *gratorsByName = hashNew(8);

struct annoGrator *snpGrator = NULL;
char *snpDesc = NULL;
if (cartUsualBoolean(cart, "hgva_rsId", TRUE))
    snpGrator = gratorForSnpBed4(gratorsByName, "", assembly, chrom, agoNoConstraint, &snpDesc);

// Now construct gratorList in the order in which annoFormatVep wants to see them,
// i.e. first the gpVar, then the snpNNN, then whatever else:
struct annoGrator *gratorList = NULL;
slAddHead(&gratorList, gpVarGrator);
if (snpGrator != NULL)
    slAddHead(&gratorList, snpGrator);

// Text or HTML output?
char *outFormat = cartUsualString(cart, "hgva_outFormat", "vepTab");
boolean doHtml = sameString(outFormat, "vepHtml");

// Initialize VEP formatter:
struct annoFormatter *vepOut = annoFormatVepNew("stdout", doHtml,
						primary, primaryLongLabel,
						(struct annoStreamer *)gpVarGrator,
						geneTdb->longLabel,
						(struct annoStreamer *)snpGrator,
						snpDesc, assembly);
addTxStatusExtras(vepOut, geneTrack, gpVarGrator, txStatusExtras);
boolean haveRegulatory = FALSE;
addOutputTracks(&gratorList, gratorsByName, vepOut, assembly, chrom, doHtml, &haveRegulatory);
adjustGpVarOverlapRule(gpVarGrator, haveRegulatory);
setHgvsOutOptions(gpVarGrator, geneTdb->track, vepOut);

addFilterTracks(&gratorList, gratorsByName, assembly, chrom);

slReverse(&gratorList);

if (doHtml)
    {
    webStart(cart, database, "Annotated Variants in VEP/HTML format");
    }
else if (! isCommandLine)
    {
    // Undo the htmlPushEarlyHandlers() because after this point they make ugly text:
    popWarnHandler();
    popAbortHandler();
    textOpen();
    webStartText();
    }
if (isNotEmpty(dyInfo->string))
    puts(dyInfo->string);
struct annoGratorQuery *query = annoGratorQueryNew(assembly, primary, gratorList, vepOut);
struct slName *comment;
for (comment = commentList;  comment != NULL;  comment = comment->next)
    vepOut->comment(vepOut, comment->name);
if (chrom != NULL)
    annoGratorQuerySetRegion(query, chrom, start, end);
annoGratorQueryExecute(query);
annoGratorQueryFree(&query);

if (doHtml)
    webEnd();
else if (! isCommandLine)
    textOutClose(&compressPipeline, NULL);
}

struct hgPositions *lookupPosition(struct dyString *dyWarn)
/* Look up position (aka range) if need be.  Return a container of matching tables and positions.
 * Warnings/errors are appended to dyWarn. */
{
char *range = windowsToAscii(cloneString(cartUsualString(cart, hgvaRange, "")));
range = trimSpaces(range);
if (isEmpty(range))
    range = hDefaultPos(database);
struct hgPositions *hgp = hgFindSearch(cart, &range, NULL, NULL, NULL, cgiScriptName(), dyWarn, NULL);
cartSetString(cart, hgvaRange, range);
return hgp;
}

int main(int argc, char *argv[])
/* Process command line. */
{
long enteredMainTime = clock1000();
if (hIsPrivateHost())
    pushCarefulMemHandler(LIMIT_2or6GB);

cgiSpoof(&argc, argv);
boolean isCommandLine = (cgiOptionalString("cgiSpoof") != NULL);
if (!isCommandLine)
    htmlPushEarlyHandlers(); /* Make errors legible during initialization. */
oldVars = hashNew(10);
boolean startQuery = (cgiUsualString("hgva_startQuery", NULL) != NULL);
if (startQuery)
    {
    if (isCommandLine)
        // No HTTP header for command-line use.
        cart = cartForSession(hUserCookie(), excludeVars, oldVars);
    else
        cart = cartAndCookieNoContent(hUserCookie(), excludeVars, oldVars);
    }
else
    cart = cartAndCookie(hUserCookie(), excludeVars, oldVars);

// Try to deal with virt chrom position used by hgTracks.
if (startsWith(    MULTI_REGION_CHROM, cartUsualString(cart, "position", ""))
 || startsWith(OLD_MULTI_REGION_CHROM, cartUsualString(cart, "position", "")))
    cartSetString(cart, "position", cartUsualString(cart, "nonVirtPosition", ""));

/* Set up global variables. */
getDbAndGenome(cart, &database, &genome, oldVars);
chromAliasSetup(database);
initGenbankTableNames(database);
regionType = cartUsualString(cart, hgvaRegionType, hgvaRegionTypeGenome);
if (isEmpty(cartOptionalString(cart, hgvaRange)))
    cartSetString(cart, hgvaRange, hDefaultPos(database));

int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
knetUdcInstall();

char *range = trimSpaces(windowsToAscii(cartUsualString(cart, hgvaRange, "")));
cartSetLastPosition(cart, range, oldVars);
cartTrackDbInit(cart, &fullTrackList, &fullGroupList, TRUE);
struct dyString *dyWarn = dyStringNew(0);
boolean noShort = (cartOptionalString(cart, "noShort") != NULL); // is this the second page of results
struct hgPositions *hgp = lookupPosition(dyWarn);
if (hgp->singlePos && isEmpty(dyWarn->string) && !noShort)
    {
    if (startQuery)
	doQuery();
    else if (! isCommandLine)
	doUi();
    }
else
    {
    if (startQuery)
        // Need Content-type (html)
        cartWebStartHeader(cart, database, "Variant Annotation Integrator");
    else
        cartWebStart(cart, database, "Variant Annotation Integrator");
    if (isNotEmpty(dyWarn->string))
        {
        if (noShort) // we're on the second page of results
            hgp->posCount = 0;  // hgFindSearch gives us a bogus hgp if the warn string is set
        else
            warn("%s", dyWarn->string);
        }
    if ((hgp->posCount > 1) || noShort) // if we're on the second page we want to put out HTML even if there are no results.
        {
        hgPositionsHtml(database, hgp, hgVaiName(), cart);
        cartSetString(cart, hgvaRange, range); // we need to reset the position because lookupPosition above trashes it
        }
    else
        doMainPage();
    cartWebEnd();
    }

cartCheckout(&cart);
if (! isCommandLine)
    cgiExitTime("hgVai", enteredMainTime);
return 0;
}
