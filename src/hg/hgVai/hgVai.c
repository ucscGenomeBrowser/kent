/* hgVai - Variant Annotation Integrator. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
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
#include "udc.h"
#include "knetUdc.h"
#include "annoGratorQuery.h"
#include "annoGratorGpVar.h"
#include "annoFormatVep.h"
#include "annoStreamBigBed.h"

#include "libifyMe.h"

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
char *excludeVars[] = {"Submit", "submit", NULL,};

#define hgvaRange "position"
#define hgvaRegionType "hgva_regionType"
#define hgvaRegionTypeEncode "encode"
#define hgvaRegionTypeGenome "genome"
#define hgvaRegionTypeRange "range"
#define hgvaPositionContainer "positionContainer"

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
jsBeginCollapsibleSectionFontSize(cart, "hgva", sectionSuffix, title, FALSE, "1.1em");
}

#define endCollapsibleSection jsEndCollapsibleSection


static struct dyString *onChangeStart()
/* Start up a javascript onChange command */
{
struct dyString *dy = jsOnChangeStart();
jsTextCarryOver(dy, hgvaRegionType);
jsTextCarryOver(dy, hgvaRange);
return dy;
}

static char *onChangeClade()
/* Return javascript executed when they change clade. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "clade");
dyStringAppend(dy, " document.hiddenForm.org.value=0;");
dyStringAppend(dy, " document.hiddenForm.db.value=0;");
dyStringAppend(dy, " document.hiddenForm." hgvaRange ".value='';");
return jsOnChangeEnd(&dy);
}

static char *onChangeOrg()
/* Return javascript executed when they change organism. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "clade");
jsDropDownCarryOver(dy, "org");
dyStringAppend(dy, " document.hiddenForm.db.value=0;");
dyStringAppend(dy, " document.hiddenForm." hgvaRange ".value='';");
return jsOnChangeEnd(&dy);
}

static char *onChangeDb()
/* Return javascript executed when they change database. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "clade");
jsDropDownCarryOver(dy, "db");
dyStringAppend(dy, " document.hiddenForm." hgvaRange ".value='';");
return jsOnChangeEnd(&dy);
}

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
printf("<SELECT ID='"hgvaRegionType"' NAME='"hgvaRegionType"' "
       "onchange=\"hgva.changeRegion();\">\n");
struct sqlConnection *conn = hAllocConn(db);
hFreeConn(&conn);
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
				     hgvaRange, 26,
				     addCommasToPos(NULL, cartString(cart, hgvaRange)));
return dyStringCannibalize(&dy);
}

void printCtAndHubButtons()
/* Print a div with buttons for hgCustom and hgHubConnect */
{
puts("<div style='padding-top: 5px; padding-bottom: 5px'>");
hOnClickButton("document.customTrackForm.submit(); return false;",
	       hasCustomTracks(cart) ? CT_MANAGE_BUTTON_LABEL : CT_ADD_BUTTON_LABEL);
printf(" ");
if (hubConnectTableExists())
    hOnClickButton("document.trackHubForm.submit(); return false;", "track hubs");
nbSpaces(3);
printf("To reset <B>all</B> user cart settings (including custom tracks), \n"
	"<A HREF=\"/cgi-bin/cartReset?destination=%s\">click here</A>.\n",
	cgiScriptName());
puts("</div>");
}

void hgGatewayCladeGenomeDb()
/* Make a row of labels and row of buttons like hgGateway, but not using tables. */
{
boolean gotClade = hGotClade();
if (gotClade)
    {
    topLabelSpansStart("clade");
    printCladeListHtml(genome, onChangeClade());
    topLabelSpansEnd();
    }
topLabelSpansStart("genome");
if (gotClade)
    printGenomeListForCladeHtml(database, onChangeOrg());
else
    printGenomeListHtml(database, onChangeOrg());
topLabelSpansEnd();
topLabelSpansStart("assembly");
printAssemblyListHtml(database, onChangeDb());
topLabelSpansEnd();
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
printf("<FORM ACTION='%s' NAME='customTrackForm'>", hgCustomName());
cartSaveSession(cart);
printf("</FORM>\n");

/* Hidden form for jumping to track hub manager CGI. */
printf("<FORM ACTION='%s' NAME='trackHubForm'>", hgHubConnectName());
//#*** well, almost verbatim... lib version should use cgiScriptName.
cgiMakeHiddenVar(hgHubConnectCgiDestUrl, cgiScriptName());
cartSaveSession(cart);
printf("</FORM>\n");

printf("<FORM ACTION=\"%s\" NAME=\"mainForm\" ID=\"mainForm\" METHOD=%s>\n",
	cgiScriptName(), cartUsualString(cart, "formMethod", "GET"));
cartSaveSession(cart);

//#*** ------------------ end verbatim ---------------

printf("<div class='sectionLiteHeader noReorderRemove'>"
       "Select Genome Assembly and Region</div>\n");

/* Print clade, genome and assembly line. */
hgGatewayCladeGenomeDb();

//#*** No longer ending form here...
}

void askUserForVariantCustomTrack()
/* Tell the user that we need a custom track of variants to work on. */
{
puts("<BR>");
printf("<div class='sectionLiteHeader'>Upload Variants as Custom Track</div>\n");
printf("Please create a custom track containing your variants of interest, using either "
       "<A HREF='../FAQ/FAQformat.html#format10' TARGET=_BLANK>pgSnp</A> or "
       "<A HREF='../goldenPath/help/vcf.html' TARGET=_BLANK>VCF</A> format:<BR>\n");
printCtAndHubButtons();
puts("<BR>");
}

typedef boolean TdbFilterFunction(struct trackDb *tdb, void *filterData);
/* Return TRUE if tdb passes filter criteria. */

void rFilterTrackList(struct trackDb *trackList, struct slRef **pPassingRefs,
		      TdbFilterFunction *filterFunc, void *filterData)
/* Recursively apply filterFunc and filterData to all tracks in fullTrackList and
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
/* This is a TdbFilterFunction to get custom or hub tracks with type pgSnp or vcfTabix. */
{
return ((sameString(tdb->grp, "user") || isHubTrack(tdb->track)) &&
	(sameString(tdb->type, "pgSnp") || sameString(tdb->type, "vcfTabix")));
}

void selectVariants(struct slRef *varGroupList, struct slRef *varTrackList)
/* Offer selection of user's variant custom tracks. */
{
printf("<div class='sectionLiteHeader'>Select Variants</div>\n");
printf("If you have more than one custom track in "
       "<A HREF='../FAQ/FAQformat.html#format10' TARGET=_BLANK>pgSnp</A> or "
       "<A HREF='../goldenPath/help/vcf.html' TARGET=_BLANK>VCF</A> format, "
       "please select the one you wish to annotate.<BR>\n");
printf("<B>variants: </B>");
printf("<SELECT ID='hgva_variantTrack' NAME='hgva_variantTrack'>\n");
char *selected = cartUsualString(cart, "hgva_variantTrack", "");
struct slRef *ref;
for (ref = varTrackList;  ref != NULL;  ref = ref->next)
    {
    struct trackDb *tdb = ref->val;
    printOption(tdb->track, selected, tdb->longLabel);
    }
printf("</SELECT><BR>\n");
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
return (startsWith("genePred", tdb->type));
}

void selectGenes()
/* Let user select a gene predictions track */
{
struct slRef *trackRefList = NULL;
tdbFilterGroupTrack(fullTrackList, fullGroupList, isGeneTrack, NULL, NULL, &trackRefList);
if (trackRefList == NULL)
    return;
printf("<div class='sectionLiteHeader'>Select Genes</div>\n");
printf("The gene predictions selected here will be used to determine the effect of "
       "each variant on genes, for example intronic, missense, splice site, intergenic etc."
       "<BR>\n");
char *selected = cartUsualString(cart, "hgva_geneTrack", ""); //#*** per-db cart vars??
//#*** should show more info about each track... button to pop up track desc?

printf("<SELECT ID='hgva_geneTrack' NAME='hgva_geneTrack'>\n");
struct slRef *ref;
for (ref = trackRefList;  ref != NULL;  ref = ref->next)
    {
    struct trackDb *tdb = ref->val;
    if (tdb->subtracks == NULL)
	printOption(tdb->track, selected, tdb->longLabel);
    }
puts("</SELECT><BR>");
}

char *dbNsfpDescFromTableName(char *tableName)
/* Return a description for table name... if these become tracks, use tdb of course. */
{
if (sameString(tableName, "dbNsfpSift"))
    return "<A HREF='http://sift.bii.a-star.edu.sg/' TARGET=_BLANK>"
	"SIFT</A> scores";
else if (sameString(tableName, "dbNsfpPolyPhen2"))
    return "<A HREF='http://genetics.bwh.harvard.edu/pph2/' TARGET=_BLANK>"
	"PolyPhen-2</A> scores and predictions (HDIV, HVAR, UniProt...)";
else if (sameString(tableName, "dbNsfpMutationTaster"))
    return "<A HREF='http://www.mutationtaster.org/' TARGET=_BLANK>"
	"MutationTaster</A> scores and predictions";
else if (sameString(tableName, "dbNsfpMutationAssessor"))
    return "<A HREF='http://mutationassessor.org/' TARGET=_BLANK>"
	"MutationAssessor</A> scores and predictions";
//else if (sameString(tableName, "dbNsfpGerpNr"))
//    return "<A HREF='http://mendel.stanford.edu/SidowLab/downloads/gerp/index.html' "
//	"TARGET=_BLANK>GERP++</A> neutral rate (NR) score";
//else if (sameString(tableName, "dbNsfpGerpRs"))
//    return "<A HREF='http://mendel.stanford.edu/SidowLab/downloads/gerp/index.html' "
//	"TARGET=_BLANK>GERP++</A> Rejected Substitutions (RS) score";
else if (sameString(tableName, "dbNsfpLrt"))
    return "<A HREF='http://www.mutationtaster.org' TARGET=_BLANK>"
	"Likelihood ratio test (LRT)</A> scores";
else if (sameString(tableName, "dbNsfpInterpro"))
    return "Protein domains from <A HREF='http://www.ebi.ac.uk/interpro/' TARGET=_BLANK"
	"Interpro</A>";
return NULL;
}

struct slName *findDbNsfpTables()
/* See if this database contains dbNSFP tables. */
{
struct sqlConnection *conn = hAllocConn(database);
struct slName *dbNsfpTables = sqlQuickList(conn, "show tables like 'dbNsfp%'");
hFreeConn(&conn);
return dbNsfpTables;
}

void selectDbNsfp(struct slName *dbNsfpTables)
/* Let user select scores/predicitions from various tools collected by dbNSFP. */
{
if (dbNsfpTables == NULL)
    return;
startCollapsibleSection("dbNsfp", "Non-synonymous functional predictions", FALSE);
printf("The "
       "<A HREF='https://sites.google.com/site/jpopgen/dbNSFP' TARGET=_BLANK>"
       "Database of Non-Synonymous Functional Predictions (dbNSFP)</A> release 2.0 "
       "provides pre-computed scores and predictions of functional significance "
       "from a variety of tools.  Every possible coding change to transcripts in "
 //#*** hardcoded version info... sigh, we need trackDb... or metaDb??
       "Gencode release 9 (Ensembl 64, Dec. 2011) gene predictions "
       "has been evaluated.  "
       "<em>Note: This may not encompass all transcripts in your "
       "selected gene set.</em><BR>\n");
//#*** Another cheap hack: reverse alph order happens to be what we want,
//#*** but priorities would be cleaner:
slReverse(&dbNsfpTables);
struct slName *table;
for (table = dbNsfpTables;  table != NULL;  table = table->next)
    {
    char *description = dbNsfpDescFromTableName(table->name);
    if (description != NULL)
	{
	char cartVar[512];
	safef(cartVar, sizeof(cartVar), "hgva_track_%s_%s", database, table->name);
	boolean defaultChecked = (sameString("dbNsfpSift", table->name) ||
				  sameString("dbNsfpPolyPhen2", table->name));
	cartMakeCheckBox(cart, cartVar, defaultChecked);
	printf("%s<BR>\n", description);
	}
    }
puts("<BR>");
endCollapsibleSection();
}

boolean findSnpBed4(char *suffix, char **retFileName, struct trackDb **retTdb)
/* If we can find the latest snpNNNsuffix track using 'show tables rlike "regex"',
 * or better yet a bigBed file for it (faster),
 * set the appropriate ret* and return TRUE, otherwise return FALSE. */
{
if (suffix == NULL)
    suffix = "";
char query[64];
safef(query, sizeof(query), "show tables like 'snp1__%s'", suffix);
struct sqlConnection *conn = hAllocConn(database);
struct slName *snpNNNTables = sqlQuickList(conn, query);
hFreeConn(&conn);
if (snpNNNTables == NULL)
    return FALSE;
// Skip to last in list -- highest number (show tables can't use rlike or 'order by'):
struct slName *table = snpNNNTables;
while (table->next != NULL && isdigit(table->next->name[4]) && isdigit(table->next->name[5]))
    table = table->next;
// Do we happen to have a bigBed version?  Better yet, bed4 only for current uses:
char fileName[HDB_MAX_PATH_STRING];
safef(fileName, sizeof(fileName), "/gbdb/%s/vai/%s.bed4.bb", database, table->name);
if (fileExists(fileName))
    {
    if (retFileName != NULL)
	*retFileName = cloneString(fileName);
    return TRUE;
    }
// Not bed4; try just .bb:
safef(fileName, sizeof(fileName), "/gbdb/%s/vai/%s.bb", database, table->name);
if (fileExists(fileName))
    {
    if (retFileName != NULL)
	*retFileName = cloneString(fileName);
    return TRUE;
    }
struct trackDb *tdb = tdbForTrack(database, table->name, &fullTrackList);
if (tdb != NULL)
    {
    if (retTdb != NULL)
	*retTdb = tdb;
    return TRUE;
    }
return FALSE;
}

void selectDbSnp(boolean gotSnp)
/* Offer to include rsID (and other fields, or leave that for advanced output??) if available */
{
if (!gotSnp)
    return;
startCollapsibleSection("dbSnp", "Known variation", FALSE);
cartMakeCheckBox(cart, "hgva_rsId", TRUE);
printf("Include dbSNP rs# ID if one exists<BR>\n");
puts("<BR>");
endCollapsibleSection();
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

void trackRefListToCheckboxes(struct slRef *trackRefList)
{
struct slRef *ref;
for (ref = trackRefList;  ref != NULL;  ref = ref->next)
    {
    struct trackDb *tdb = ref->val;
    char cartVar[512];
    safef(cartVar, sizeof(cartVar), "hgva_track_%s_%s", database, tdb->track);
    cartMakeCheckBox(cart, cartVar, FALSE);
    printf("%s<BR>\n", tdb->longLabel);
    }
}

void selectCons(struct slRef *elTrackRefList, struct slRef *scoreTrackRefList)
/* Offer checkboxes for optional Conservation scores. */
{
if (elTrackRefList == NULL && scoreTrackRefList == NULL)
    return;
if (elTrackRefList != NULL)
    {
    startCollapsibleSection("ConsEl", "Conserved elements", FALSE);
    trackRefListToCheckboxes(elTrackRefList);
    puts("<BR>");
    endCollapsibleSection();
    }
if (scoreTrackRefList != NULL)
    {
    startCollapsibleSection("ConsScore", "Conservation scores",
			    FALSE);
    trackRefListToCheckboxes(scoreTrackRefList);
    endCollapsibleSection();
    }
}

void selectAnnotations()
/* Beyond predictions of protein-coding effect, what other basic data can we integrate? */
{
struct slName *dbNsfpTables = findDbNsfpTables();
boolean gotSnp = findSnpBed4("", NULL, NULL);
struct slRef *elTrackRefList = NULL, *scoreTrackRefList = NULL;
findCons(&elTrackRefList, &scoreTrackRefList);
if (dbNsfpTables == NULL && !gotSnp && elTrackRefList == NULL && scoreTrackRefList == NULL)
    return;
puts("<BR>");
printf("<div class='sectionLiteHeader'>Select More Annotations (optional)</div>\n");
// Make wrapper table for collapsible sections:
puts("<TABLE border=0 cellspacing=5 cellpadding=0 style='padding-left: 10px;'>");
selectDbNsfp(dbNsfpTables);
selectDbSnp(gotSnp);
selectCons(elTrackRefList, scoreTrackRefList);
puts("</TABLE>");
}

void selectFiltersFunc()
/* Options to restrict variants based on gene region/soTerm from gpFx */
{
startCollapsibleSection("filtersFunc", "Functional role", FALSE);
printf("Include variants annotated as<BR>\n");
cartMakeCheckBox(cart, "hgva_include_intergenic", FALSE);
printf("intergenic<BR>\n");
cartMakeCheckBox(cart, "hgva_include_upDownstream", TRUE);
printf("upstream/downstream of gene<BR>\n");
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
    cartMakeCheckBox(cart, "hgva_exclude_snpMult", FALSE);
    printf("Exclude variants mapped to multiple genomic locations by dbSNP<BR>\n");
    }
if (gotCommon)
    {
    cartMakeCheckBox(cart, "hgva_exclude_snpCommon", FALSE);
    printf("Exclude \"common\" variants "
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
printf("</TD><TD>Include only the variants that overlap");
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
	cgiMakeOnClickRadioButton("hgva_require_consEl_track", tdb->track,
				  sameString(tdb->track, selected),
	  "onclick=\"setCheckboxList('hgva_require_consEl', true);\"");
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
char *selected = cartUsualString(cart, "hgva_outFormat", "");
printf("<SELECT ID='hgva_outFormat' NAME='hgva_outFormat'>\n");
printOption("vepTab", selected, "Variant Effect Predictor (tab-separated text)");
printf("</SELECT><BR>\n");
char *compressType = cartUsualString(cart, "hgva_compressType", textOutCompressGzip);
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
puts("<BR>");
puts("<div class='warn-note' style='border: 2px solid #9e5900; padding: 5px 20px; background-color: #ffe9cc;'>");
puts("<p><span style='font-weight: bold; color: #c70000;'>NOTE:</span><br>");
if (hIsPrivateHost())
    puts("<span style='font-weight: bold; color: #c70000;'>THIS IS UNTESTED SOFTWARE. "
	 "We are still working out the bugs. Feel free to play with this and tell us "
	 "about the bugs, but treat results with extra skepticism.</span><br>");
puts("This tool is for research use only. While this tool is open to the "
     "public, users seeking information about a personal medical or genetic "
     "condition are urged to consult with a qualified physician for "
     "diagnosis and for answers to personal questions.");
puts("</p></div><BR>");
printf("&nbsp;<img id='loadingImg' src='../images/loading.gif' />\n");
printf("<span id='loadingMsg'></span>\n");
cgiMakeButtonWithOnClick("startQuery", "Go!",
			 "get the results of your query",
			 "loadingImage.run(); $('#mainForm').submit();");
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
printAssemblySection();

/* Check for variant custom tracks.  If there are none, tell user they need to
 * upload at least one. */
struct slRef *varTrackList = NULL, *varGroupList = NULL;
tdbFilterGroupTrack(fullTrackList, fullGroupList, isVariantCustomTrack, NULL,
		    &varGroupList, &varTrackList);

if (varTrackList == NULL)
    {
    askUserForVariantCustomTrack();
    }
else
    {
    puts("<BR>");
    // Make wrapper table for collapsible sections:
    selectVariants(varGroupList, varTrackList);
    selectGenes();
    selectAnnotations();
    selectFilters();
    selectOutput();
    submitAndDisclaimer();
    }
printf("</FORM>");

webNewSection("Using the Variant Annotation Integrator");
webIncludeHelpFile("hgVaiHelpText", FALSE);
}

void doUi()
/* Set up globals and make web page */
{
cartWebStart(cart, database, "Variant Annotation Integrator");
jsInit();
jsIncludeFile("jquery-ui.js", NULL);
webIncludeResourceFile("jquery-ui.css");
jsIncludeFile("hgVarAnnogrator.js", NULL);
addSomeCss();
doMainPage();
cartWebEnd();
/* Save variables. */
cartCheckout(&cart);
}

void checkVariantTrack(struct trackDb *tdb)
/* variantTrack should be either pgSnp or VCF. */
{
if (! sameString(tdb->type, "pgSnp") &&
    ! sameString(tdb->type, "vcfTabix"))
    {
    errAbort("Expected variant track '%s' to be either pgSnp or vcfTabix, but it's '%s'",
	     tdb->track, tdb->type);
    }
}

char *fileNameFromTable(char *table)
/* Get fileName from a bigData table (for when we don't have a trackDb, just table). */
{
struct sqlConnection *conn = hAllocConn(database);
char query[512];
safef(query, sizeof(query), "select fileName from %s", table);
char *fileName = sqlQuickString(conn, query);
hFreeConn(&conn);
return fileName;
}

void textOpen()
/* Start serving up plain text, possibly via a pipeline to gzip. */
{
char *fileName = cartUsualString(cart, "hgva_outFile", "");
char *compressType = cartUsualString(cart, "hgva_compressType", textOutCompressGzip);
compressPipeline = textOutInit(fileName, compressType);
}

struct annoGrator *gratorForSnp(char *suffix, struct annoAssembly *assembly, char *chrom,
				enum annoGratorOverlap overlapRule)
/* Look up snpNNNsuffix; if we find it, return a grator, otherwise return NULL. */
{
char *fileName = NULL;
struct trackDb *tdb = NULL;
if (! findSnpBed4(suffix, &fileName, &tdb))
    return NULL;
struct annoGrator *grator = NULL;
if (fileName != NULL)
    {
    struct annoStreamer *streamer = annoStreamBigBedNew(fileName, assembly, BIGNUM);
    grator = annoGratorNew(streamer);
    grator->setOverlapRule(grator, overlapRule);
    }
else
    {
    grator = gratorFromTrack(assembly, tdb->table, tdb, chrom, BIGNUM, NULL, overlapRule);
    }
return grator;
}

void updateGratorLists(struct annoGrator *grator, struct annoGrator **pGratorList,
		       struct annoGrator **pFirstExtra)
/* If grator is non-NULL, add it to gratorList; if *pFirstExtra is NULL, set it to grator. */
{
if (grator != NULL)
    {
    slAddHead(pGratorList, grator);
    if (pFirstExtra != NULL && *pFirstExtra == NULL)
	*pFirstExtra = grator;
    }
}

void doQuery()
/* Translate simple form inputs into anno* components and execute query. */
{
char *chrom = NULL;
uint start = 0, end = 0;
if (sameString(regionType, hgvaRegionTypeRange))
    {
    char *position = cartString(cart, hgvaRange);
    if (! parsePosition(position, &chrom, &start, &end))
	errAbort("Expected position to be chrom:start-end but got '%s'", position);
    }
char *variantTrack = cartString(cart, "hgva_variantTrack");
char *geneTrack = cartString(cart, "hgva_geneTrack");

struct annoAssembly *assembly = getAnnoAssembly(database);
struct trackDb *varTdb = tdbForTrack(database, variantTrack, &fullTrackList);
if (varTdb == NULL)
    {
    if (isHubTrack(variantTrack))
	warn("Can't find hub track %s; try the \"check hub\" button in "
	     "<A href=\"%s?%s&%s=%s\">Track Data Hubs</A>.",
	     variantTrack,
	     hgHubConnectName(), cartSidUrlString(cart), hgHubConnectCgiDestUrl, cgiScriptName());
    else
	warn("Can't find tdb for variant track %s", variantTrack);
    doUi();
    return;
    }
checkVariantTrack(varTdb);
int maxVarRows = cartUsualInt(cart, "hgva_variantLimit", 10);
struct annoStreamer *primary = streamerFromTrack(assembly, varTdb->table, varTdb, chrom,
						 maxVarRows);

struct trackDb *geneTdb = tdbForTrack(database, geneTrack, &fullTrackList);
enum annoGratorOverlap geneOverlapRule = agoMustOverlap;
struct annoGrator *gpVarGrator = gratorFromTrack(assembly, geneTdb->table, geneTdb, chrom,
						 BIGNUM, primary->asObj, geneOverlapRule);
struct annoGratorGpVarFuncFilter aggvFuncFilter;
ZeroVar(&aggvFuncFilter);
aggvFuncFilter.intergenic = cartUsualBoolean(cart, "hgva_include_intergenic", FALSE);
aggvFuncFilter.upDownstream = cartUsualBoolean(cart, "hgva_include_upDownstream", TRUE);
aggvFuncFilter.utr = cartUsualBoolean(cart, "hgva_include_utr", TRUE);
aggvFuncFilter.cdsSyn = cartUsualBoolean(cart, "hgva_include_cdsSyn", TRUE);
aggvFuncFilter.cdsNonSyn = cartUsualBoolean(cart, "hgva_include_cdsNonSyn", TRUE);
aggvFuncFilter.intron = cartUsualBoolean(cart, "hgva_include_intron", TRUE);
aggvFuncFilter.splice = cartUsualBoolean(cart, "hgva_include_splice", TRUE);
aggvFuncFilter.nonCodingExon = cartUsualBoolean(cart, "hgva_include_nonCodingExon", TRUE);
annoGratorGpVarSetFuncFilter(gpVarGrator, &aggvFuncFilter);

struct annoGrator *snpGrator = NULL;
if (cartBoolean(cart, "hgva_rsId"))
    {
    snpGrator = gratorForSnp("", assembly, chrom, agoNoConstraint);
    }

// Now construct gratorList in the order in which annoFormatVep wants to see them,
// i.e. first the gpVar, then the snpNNN, then whatever else:
struct annoGrator *gratorList = NULL;
slAddHead(&gratorList, gpVarGrator);
if (snpGrator != NULL)
    slAddHead(&gratorList, snpGrator);
// extraGrators will point to the first grator in gratorList after snpGrator;
// i.e. first grator that is meant for the EXTRAS column and/or used by a filter
struct annoGrator *extraGrators = NULL;
if (cartUsualBoolean(cart, "hgva_exclude_snpCommon", FALSE))
    {
    struct annoGrator *grator = gratorForSnp("Common", assembly, chrom, agoMustNotOverlap);
    updateGratorLists(grator, &gratorList, &extraGrators);
    }
if (cartUsualBoolean(cart, "hgva_exclude_snpMult", FALSE))
    {
    struct annoGrator *grator = gratorForSnp("Mult", assembly, chrom, agoMustNotOverlap);
    updateGratorLists(grator, &gratorList, &extraGrators);
    }
// If we're including Conserved Elements as a filter, and it has also been selected as an
// extra output value, don't make two grators for it:
char *consElTrack = NULL;
if (cartUsualBoolean(cart, "hgva_require_consEl", FALSE))
    {
    struct annoGrator *grator = NULL;
    consElTrack = cartString(cart, "hgva_require_consEl_track");
    struct trackDb *tdb = tdbForTrack(database, consElTrack, &fullTrackList);
    if (tdb != NULL)
	grator = gratorFromTrack(assembly, tdb->table, tdb, chrom, BIGNUM, NULL, agoMustOverlap);
    updateGratorLists(grator, &gratorList, &extraGrators);
    }

boolean addedDbNsfpSeqChange = FALSE;
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
    if (sameOk(consElTrack, trackName))
	// We already have this as a grator because it's a filter (above):
	continue;
    struct trackDb *tdb = tdbForTrack(database, trackName, &fullTrackList);
    struct annoGrator *grator = NULL;
    if (tdb != NULL)
	grator = gratorFromTrack(assembly, tdb->table, tdb, chrom, BIGNUM, NULL, agoNoConstraint);
    else if (startsWith("dbNsfp", trackName))
	{
	// If the user has selected dbNsfp* data, we also need the underlying dbNsfpSeqChange
	// data, so annoFormatVep can tell whether the variant and gpFx are consistent with the
	// variant and transcript that dbNsfp used to calculate scores.
	if (!addedDbNsfpSeqChange)
	    {
	    char *fileName = fileNameFromTable("dbNsfpSeqChange");
	    if (fileName == NULL)
		errAbort("'%s' requested, but I can't find fileName for dbNsfpSeqChange",
			 trackName);
	    struct annoStreamer *streamer = annoStreamBigBedNew(fileName, assembly, BIGNUM);
	    grator = annoGratorNew(streamer);
	    updateGratorLists(grator, &gratorList, &extraGrators);
	    addedDbNsfpSeqChange = TRUE;
	    }
	char *fileName = fileNameFromTable(trackName);
	if (fileName != NULL)
	    {
	    struct annoStreamer *streamer = annoStreamBigBedNew(fileName, assembly, BIGNUM);
	    grator = annoGratorNew(streamer);
	    }
	}
    updateGratorLists(grator, &gratorList, &extraGrators);
    }
slReverse(&gratorList);

struct annoFormatter *vepOut = annoFormatVepNew("stdout", primary,
						(struct annoStreamer *)gpVarGrator,
						(struct annoStreamer *)snpGrator);
//#*** can all the extraItem config smarts be pulled inside annoFormatVep?
struct annoStreamer *extraSource = (struct annoStreamer *)extraGrators;
for (;  extraSource != NULL;  extraSource = extraSource->next)
    {
    char *tableName = extraSource->name;
    char *tag = cloneString(tableName);
    char *p = strrchr(tag, '/');
    if (p != NULL)
	{
	// file path; strip to basenaem
	tableName = tag = p+1;
	p = strchr(tag, '.');
        if (p != NULL)
	    *p = '\0';
	}
    p = strstr(tag, "dbNsfp");
    if (p != NULL)
	{
	tableName = p;
	tag = p + strlen("dbNsfp");
	}
    else
	{
	// database.table -- skip database.
	p = strchr(tag, '.');
	if (p != NULL)
	    tag = p+1;
	}
    touppers(tag);
    char *description = dbNsfpDescFromTableName(tableName);
    if (description == NULL)
	description = extraSource->name;
//#*** This is where the smarts probably can't all be pulled into annoFormatVep:
    char *column = "ignoredVepKnowsBetterOrOverlapExcludesVariant";
    if (extraSource->rowType == arWords)
	{
	if (asColumnFindIx(extraSource->asObj->columnList, "name") >= 0)
	    column = "name";
	}
    annoFormatVepAddExtraItem(vepOut, extraSource, tag, description, column);
    }

// Undo the htmlPushEarlyHandlers() because after this point they make ugly text:
popWarnHandler();
popAbortHandler();
textOpen();
webStartText();

struct annoGratorQuery *query = annoGratorQueryNew(assembly, primary, gratorList, vepOut);
if (chrom != NULL)
    annoGratorQuerySetRegion(query, chrom, start, end);
annoGratorQueryExecute(query);

textOutClose(&compressPipeline);
annoGratorQueryFree(&query);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (hIsPrivateHost())
    pushCarefulMemHandler(LIMIT_2or6GB);
htmlPushEarlyHandlers(); /* Make errors legible during initialization. */

cgiSpoof(&argc, argv);
oldVars = hashNew(10);
setUdcCacheDir();
boolean startQuery = (cgiUsualString("startQuery", NULL) != NULL);
if (startQuery)
    cart = cartAndCookieNoContent(hUserCookie(), excludeVars, oldVars);
else
    cart = cartAndCookie(hUserCookie(), excludeVars, oldVars);

/* Set up global variables. */
getDbAndGenome(cart, &database, &genome, oldVars);
regionType = cartUsualString(cart, hgvaRegionType, hgvaRegionTypeGenome);
if (isEmpty(cartOptionalString(cart, hgvaRange)))
    cartSetString(cart, hgvaRange, hDefaultPos(database));

int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
#if ((defined USE_BAM || defined USE_TABIX) && defined KNETFILE_HOOKS)
knetUdcInstall();
#endif//def (USE_BAM || USE_TABIX) && KNETFILE_HOOKS

if (lookupPosition(cart, hgvaRange))
    {
    initGroupsTracksTables(cart, &fullTrackList, &fullGroupList);
    if (startQuery)
	doQuery();
    else
	doUi();
    }
else if (webGotWarnings())
    {
    // We land here when lookupPosition pops up a warning box.
    // Reset the problematic position and show the main page.
    char *position = cartUsualString(cart, "lastPosition", hDefaultPos(database));
    cartSetString(cart, hgvaRange, position);
    doMainPage();
    }
// If lookupPosition returned FALSE and didn't report warnings,
// then it wrote HTML showing multiple position matches & links.

cartCheckout(&cart);
return 0;
}
