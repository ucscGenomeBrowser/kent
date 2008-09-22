/* gvUi.c - char arrays for Genome Variation UI features */
#include "gvUi.h"
#include "common.h"
#include "cart.h"
#include "cheapcgi.h"

static char const rcsid[] = "$Id: gvUi.c,v 1.17 2008/09/22 18:53:00 giardine Exp $";

/***************Filters**************/

/* some stuff for mutation type choices */
/* labels for checkboxes */
char *gvTypeLabel[] = {
    "substitution",
    "insertion",
    "duplication",
    "deletion",
    "complex",
};

/* names for checkboxes */
char *gvTypeString[] = {
    "gvPos.filter.sub",
    "gvPos.filter.ins",
    "gvPos.filter.dup",
    "gvPos.filter.del",
    "gvPos.filter.complex",
};

/* values in the db */
char *gvTypeDbValue[] = {
    "substitution",
    "insertion",
    "duplication",
    "deletion",
    "complex",
};

int gvTypeSize = ArraySize(gvTypeString);

/* some stuff for the mutation location choices */
/* labels for checkboxes */
char *gvLocationLabel[] = {
    "exon",
    "intron",
    "5' UTR",
    "3' UTR",
    "not within known transcription unit",
};

/* names for checkboxes */
char *gvLocationString[] = {
    "gvPos.filter.xon",
    "gvPos.filter.intron",
    "gvPos.filter.utr5",
    "gvPos.filter.utr3",
    "gvPos.filter.intergenic",
};

char *gvLocationDbValue[] = {
    "exon",
    "intron",
    "5' UTR",
    "3' UTR",
    "not within known transcription unit",
};

int gvLocationSize = ArraySize(gvLocationLabel);

char *gvSrcString[] = {
    "gvPos.filter.src.SP",
    "gvPos.filter.src.LSDB",
};

char *gvSrcDbValue[] = {
    "UniProt (Swiss-Prot/TrEMBL)",
    "LSDB",
};

int gvSrcSize = ArraySize(gvSrcString);

char *gvAccuracyLabel[] = {
    "estimated coordinates",
};

char *gvAccuracyString[] = {
    "gvPos.filter.estimate",
};

unsigned gvAccuracyDbValue[] = {
    0,
};

int gvAccuracySize = ArraySize(gvAccuracyLabel);

char *gvFilterDALabel[] = {
    "phenotype-associated",
    "not phenotype-associated",
    "phenotype association unknown",
};

char *gvFilterDAString[] = {
    "gvPos.filter.da.known",
    "gvPos.filter.da.not",
    "gvPos.filter.da.null",
};

char *gvFilterDADbValue[] = {
    "phenotype-associated",
    "not phenotype-associated",
    "NULL",
};

int gvFilterDASize = ArraySize(gvFilterDAString);

/***************Attribute display**************/

/* list in display order of attribute type, key that is used in table */
char *gvAttrTypeKey[] = {
    "longName",
    "commonName",
    "alias",
    "srcLink",
    "links",
    "mutType",
    "rnaNtChange",
    "protEffect",
    "enzymeActivityFC",
    "enzymeActivityQual",
    "funChange",
    "disease",
    "phenoCommon",
    "phenoOfficial",
    "geneVarsDis",
    "hap",
    "ethnic",
    "comment",
    "userAnno",
};

/* list in display order of attribute type display names */
char *gvAttrTypeDisplay[] = {
    "Full name",
    "Common name",
    "Alias",
    "External links",
    "External links",
    "Type of mutation",
    "RNA nucleotide change",
    "Effect on Protein",
    "Enzyme activity, fold change relative to wild type",
    "Qualitative effect on activity",
    "Functional Change",
    "Phenotype association",
    "Phenotype common name",
    "Phenotype official nomenclature",
    "Variation and Disease information related to gene locus",
    "Haplotype",
    "Ethnicity/Nationality",
    "Comments",
    "User annotations",
};

/* category for each type above, match up by array index */
char *gvAttrCategory[] = {
    "Alias",
    "Alias",
    "Alias",
    "Links",
    "Links",
    "RNA effect",
    "RNA effect",
    "Protein effect",
    "Protein effect",
    "Protein effect",
    "Function",
    "Patient/Subject phenotype",
    "Patient/Subject phenotype",
    "Patient/Subject phenotype",
    "Data related to gene locus",
    "Other Ancillary data",
    "Other Ancillary data",
    "Other Ancillary data",
    "Other Ancillary data",
};

int gvAttrSize = ArraySize(gvAttrTypeKey);

/***************Color options**************/

char *gvColorLabels[] = {
    "red",
    "purple",
    "green",
    "orange",
    "blue",
    "brown",
    "black",
    "gray",
};

int gvColorLabelSize = ArraySize(gvColorLabels);

char *gvColorTypeLabels[] = {
    "substitution",
    "insertion",
    "duplication",
    "deletion",
    "complex",
    "unknown",
};

char *gvColorTypeStrings[] = {
    "gvColorTypeSub",
    "gvColorTypeIns",
    "gvColorTypeDup",
    "gvColorTypeDel",
    "gvColorTypeComplex",
    "gvColorTypeUnk",
};

char *gvColorTypeDefault[] = {
    "purple",
    "green",
    "orange",
    "blue",
    "brown",
    "black",
};

char *gvColorTypeBaseChangeType[] = {
    "substitution",
    "insertion",
    "duplication",
    "deletion",
    "complex",
    "unknown",
};

/* all type arrays are same size */
int gvColorTypeSize = ArraySize(gvColorTypeStrings);

/* color by disease association */
char *gvColorDALabels[] = {
    "phenotype-associated",
    "not phenotype-associated",
    "phenotype association unknown",
};

char *gvColorDAStrings[] = {
    "gvColorDAKnown",
    "gvColorDANot",
    "gvColorDARest",
};

char *gvColorDADefault[] = {
    "red",
    "green",
    "gray",
};

char *gvColorDAAttrVal[] = {
    "phenotype-associated",
    "not phenotype-associated",
    "NULL",
};

int gvColorDASize = ArraySize(gvColorDAStrings);

void gvDisclaimer ()
/* displays page with disclaimer forwarding query string that got us here */
{
struct cgiVar *cv, *cvList = cgiVarList();

cartHtmlStart("PhenCode Disclaimer");
printf("<TABLE WIDTH=\"%s\" BGCOLOR=\"#888888\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>", "100%");
printf("<TABLE BGCOLOR=\"fffee8\" WIDTH=\"%s\"  BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\"><TR><TD>", "100%");
printf("<TABLE BGCOLOR=\"D9E4F8\" BACKGROUND=\"../../images/hr.gif\" WIDTH=%s><TR><TD>", "100%");
printf("<FONT SIZE=\"4\"><b>&nbsp;PhenCode Disclaimer </B></FONT> ");
printf("</TD></TR></TABLE>");
printf("<TABLE BGCOLOR=\"fffee8\" WIDTH=\"%s\" CELLPADDING=0><TR><TH HEIGHT=10></TH></TR>", "100%");
printf("<TR><TD WIDTH=10>&nbsp;</TD><TD>");
printf("<b>Usage</b>\n");
printf("<p>\n");
printf("PhenCode is intended for research purposes only.  Although the data\n");
printf("are freely available to all, users should treat the reported mutations\n");
printf("with extreme caution in clinical settings or for any diagnostic or\n");
printf("population screening purpose.  This information requires expertise\n");
printf("to interpret properly; clinical diagnosis and/or treatment\n");
printf("recommendations should be made only by medical professionals.\n");
printf("<p>\n");
printf("Patients and doctors should not make treatment decisions based on the\n");
printf("information in PhenCode.  In particular, <b>PhenCode should not be used\n");
printf("to assess disease risk</b>.  Some of these variants are not associated\n");
printf("with disease at all, and for many others the association is slight,\n");
printf("depends on other factors, and/or may not be causative.  The mere fact\n");
printf("that a variant is listed here does NOT mean that a particular patient\n");
printf("will become ill.\n");
printf("<p>");
printf("<b>Errors</b>\n");
printf("<p>\n");
printf("The PhenCode track (including, but not limited to, \"Locus Variants\"\n");
printf(") is a compilation of freely-available data\n");
printf("obtained from other sources.  While reasonable effort is made to\n");
printf("promote accuracy, nevertheless there may be errors in the original\n");
printf("data and/or the compilation process.  <b>By using these data, you\n");
printf("agree and acknowledge that the information is not guaranteed to be\n");
printf("accurate.</b>  If you do find any errors, please report them to the\n");
printf("addresses listed on the <a href=\"http://phencode.bx.psu.edu/phencode/contact.html\">Contact us</a> page.\n");
printf("<p>");
printf("<b>Disclaimer</b>\n");
printf("<p>\n");
printf("This resource and data are provided \"as is\", \"where is\" and without\n");
printf("any express or implied warranties, including, but not limited to, any\n");
printf("implied warranties of merchantability and/or fitness for a particular\n");
printf("purpose.  In no event shall\n");
printf("<a href=\"http://www.psu.edu/\">The Pennsylvania State University</a>,\n");
printf("<a href=\"http://www.ucsc.edu/\">The University of California Santa Cruz</a>,\n");
printf("or any data contributors,\n");
printf("nor their respective agents, employees or representatives be liable\n");
printf("for any direct, indirect, incidental, special, exemplary, or\n");
printf("consequential damages (including, but not limited to, procurement of\n");
printf("substitute goods or services; loss of use, data, or profits; business\n");
printf("interruption; medical or legal expenses; or pain and suffering),\n");
printf("however caused and on any theory of liability, whether in contract,\n");
printf("strict liability, or tort (including negligence or otherwise) arising\n");
printf("in any way or form out of the use of this resource or data, even if\n");
printf("advised of the possibility of such damage.\n");
printf("<p>\n");
printf("Users assume all risk and responsibility for the accuracy,\n");
printf("completeness, and usefulness, or lack thereof, of any information,\n");
printf("apparatus, product, or process disclosed, and also all risk and\n");
printf("responsibility that the use hereof would or would not infringe the\n");
printf("rights of any other party.\n");
printf("<p>\n");
printf("<br>\n");
printf("<FORM ACTION=\"%s\" METHOD=POST>\n", cgiScriptName());
for (cv = cvList; cv != NULL; cv = cv->next)
    {
    cgiContinueHiddenVar(cv->name);
    }
cgiMakeButtonWithMsg("gvDisclaimer", "Agree", NULL);
printf("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
cgiMakeButtonWithMsg("gvDisclaimer", "Disagree", NULL);
printf("</FORM>\n");
printf("</td></tr></table></td></tr></table></td></tr></table>");
cartHtmlEnd();
exit(0);
}


