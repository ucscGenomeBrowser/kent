/* hui - human genome browser user interface controls that are shared
 * between more than one CGI. */
#ifndef HUI_H
#define HUI_H

#include "cart.h"
#include "trackDb.h"
struct lineFile;

char *hUserCookie();
/* Return our cookie name. */

char *wrapWhiteFont(char *s);
/* Write white font around s */

#define HELP_DIR "/goldenPath/help"
/*	will be appended to DOCUMENT_ROOT	*/

#define DOCUMENT_ROOT	"/usr/local/apache/htdocs"
/*	default DocumentRoot as configured in Apache httpd.conf	*/
/*	DocumentRoot can be set in hg.conf as browser.documentRoot */
#define DEFAULT_BACKGROUND	"../images/floret.jpg"
/*	full path is determined by Apache's DocumentRoot since this is a
 *	relative URL reference
 */

char *hDocumentRoot();
/* get the path to the DocumentRoot, or the default */

char *hHelpFile(char *fileRoot);
/* Given a help file root name (e.g. "hgPcrResult" or "cutters"),
 * prepend the complete help directory path and add .html suffix. 
 * Do not free the statically allocated result. */

char *hFileContentsOrWarning(char *file);
/* Return the contents of the html file, or a warning message. 
 * The file path may begin with hDocumentRoot(); if it doesn't, it is
 * assumed to be relative and hDocumentRoot() will be prepended. */

char *hCgiRoot();
/* get the path to the CGI directory.
 * Returns NULL when not running as a CGI (unless specified by browser.cgiRoot) */

char *hBackgroundImage();
/* get the path to the configured background image to use, or the default */

/* Definitions for ruler pseudo-track.  It's not yet a full-fledged
 * track, so it can't appear in trackDb. */
#define RULER_TRACK_NAME        "ruler"
#define RULER_TRACK_LABEL       "Base Position"
#define RULER_TRACK_LONGLABEL   "Genome Base Position"
#define SCALE_BAR_LABEL         "Scale"
#define WIN_POS_LABEL           "Window Position"
#define WIN_TITLE_LABEL         ""  /* "Title" */

/* Definitions for restriction enzyme track. */
#define cutterVar "hgt.cutters"
#define cutterDefault ""
#define CUTTERS_POPULAR "ClaI,BamHI,BglII,EcoRI,EcoRV,HindIII,PstI,SalI,SmaI,XbaI,KpnI,SacI,SphI"
#define CUTTERS_TRACK_NAME "cutters"
#define CUTTERS_TRACK_LABEL "Restr Enzymes"
#define CUTTERS_TRACK_LONGLABEL "Restriction Enzymes from REBASE"

/* Definition for oligo match track. */
#define oligoMatchVar "hgt.oligoMatch" 
#define oligoMatchDefault "aaaaa"
#define OLIGO_MATCH_TRACK_NAME "oligoMatch"
#define OLIGO_MATCH_TRACK_LABEL "Short Match"
#define OLIGO_MATCH_TRACK_LONGLABEL "Perfect Match to Short Sequence"

/* Definitions for User Psl track (hgBlat results). */
#define USER_PSL_TRACK_NAME "hgUserPsl"
#define USER_PSL_TRACK_LABEL "Blat Sequence"
#define USER_PSL_TRACK_LONGLABEL "Your Sequence from BLAT Search"

/* Display of bases on the ruler, and multiple alignments.
 * If present, indicates reverse strand */
#define COMPLEMENT_BASES_VAR    "complement"
/*	For trackUi and hgTracks, motif highlight options	*/
#define BASE_MOTIFS	"hgt.motifs"
#define MOTIF_COMPLEMENT	"hgt.motifComplement"
#define BASE_SCALE_BAR  "hgt.baseShowScaleBar"
#define BASE_SHOWRULER  "hgt.baseShowRuler"
#define BASE_SHOWPOS	"hgt.baseShowPos"
#define BASE_SHOWASM	"hgt.baseShowAsm"
#define BASE_TITLE	"hgt.baseTitle"
#define REV_CMPL_DISP   "hgt.revCmplDisp"

/* Configuration variable to cause ruler zoom to zoom to base level */
#define RULER_BASE_ZOOM_VAR      "rulerBaseZoom"

/* Maf track display variables and their values */
#define MAF_GENEPRED_VAR  "mafGenePred"
#define MAF_FRAMING_VAR   "mafFrame"
#define MAF_DOT_VAR       "mafDot"
#define MAF_CHAIN_VAR     "mafChain"

/* display of bases for tracks that are type psl and have sequence e.g. ESTs */
#define PSL_SEQUENCE_BASES	"pslSequenceBases"
#define PSL_SEQUENCE_DEFAULT	"no"

/******  Some stuff for tables of controls ******/
#define CONTROL_TABLE_WIDTH 610

#define EXTENDED_DNA_BUTTON "extended case/color options"

/* Net track option */
#define NET_OPT_TOP_ONLY  "netTopOnly"

/* Microarray default setting. */
#define MICROARRAY_CLICK_LIMIT 200

/* SwitchGear TSS default filter. */
#define SWITCHDBTSS_FILTER 10

void netUi(struct trackDb *tdb);

struct controlGrid
/* Keep track of a control grid (table) */
    {
    int columns;	/* How many columns in grid. */
    int columnIx;	/* Index (0 based) of current column. */
    char *align;	/* Which way to align. */
    boolean rowOpen;	/* True if have opened a row. */
    };

struct controlGrid *startControlGrid(int columns, char *align);
/* Start up a control grid. */

void controlGridStartCell(struct controlGrid *cg);
/* Start a new cell in control grid. */

void controlGridEndCell(struct controlGrid *cg);
/* End cell in control grid. */

void endControlGrid(struct controlGrid **pCg);
/* Finish up a control grid. */

void controlGridEndRow(struct controlGrid *cg);
/* Force end of row. */

/******  Some stuff for hide/dense/full controls ******/
enum trackVisibility 
/* How to look at a track. */
    {
    tvHide=0, 		/* Hide it. */
    tvDense=1,          /* Squish it together. */
    tvFull=2,           /* Expand it out. */
    tvPack=3,           /* Zig zag it up and down. */
    tvSquish=4,         /* Pack with thin boxes and no labels. */
    };  

enum trackVisibility hTvFromString(char *s);
/* Given a string representation of track visibility, return as
 * equivalent enum. */

enum trackVisibility hTvFromStringNoAbort(char *s);
/* Given a string representation of track visibility, return as
 * equivalent enum. */

char *hStringFromTv(enum trackVisibility vis);
/* Given enum representation convert to string. */

/* Standard width for visibility dropdowns */
#define TV_DROPDOWN_STYLE "width: 70px"

void hTvDropDownClassVisOnly(char *varName, enum trackVisibility vis,
	boolean canPack, char *class, char *visOnly);
/* Make track visibility drop down for varName with style class,
	and potentially limited to visOnly */

void hTvDropDownClassWithJavascript(char *varName, enum trackVisibility vis, boolean canPack, char *class,char *javascript);
/* Make track visibility drop down for varName with style class and javascript */
#define hTvDropDownClass(varName,vis,canPack,class) \
        hTvDropDownClassWithJavascript((varName),(vis),(canPack),(class),"")
#define hTvDropDownWithJavascript(varName,vis,canPack,javascript) \
        hTvDropDownClassWithJavascript((varName),(vis),(canPack),"normalText",(javascript))
#define hTvDropDown(varName,vis,canPack) \
        hTvDropDownClassWithJavascript((varName),(vis),(canPack),"normalText","")

#define SUPERTRACK_DEFAULT_VIS  "hide"

void hideShowDropDown(char *varName, boolean show, char *class);
/* Make hide/show dropdown for varName */

/****** Some stuff for stsMap related controls *******/
enum stsMapOptEnum {
   smoeGenetic = 0,
   smoeGenethon = 1,
   smoeMarshfield = 2,
   smoeDecode = 3,
   smoeGm99 = 4,
   smoeWiYac = 5,
   smoeWiRh = 6,
   smoeTng = 7,
};

enum stsMapOptEnum smoeStringToEnum(char *string);
/* Convert from string to enum representation. */

char *smoeEnumToString(enum stsMapOptEnum x);
/* Convert from enum to string representation. */

void smoeDropDown(char *var, char *curVal);
/* Make drop down of options. */

/****** Some stuff for stsMapMouseNew related controls *******/
enum stsMapMouseOptEnum {
   smmoeGenetic = 0,
   smmoeWig = 1,
   smmoeMgi = 2,
   smmoeRh = 3,
};

enum stsMapMouseOptEnum smmoeStringToEnum(char *string);
/* Convert from string to enum representation. */

char *smmoeEnumToString(enum stsMapMouseOptEnum x);
/* Convert from enum to string representation. */

void smmoeDropDown(char *var, char *curVal);
/* Make drop down of options. */

/****** Some stuff for stsMapRat related controls *******/
enum stsMapRatOptEnum {
   smroeGenetic = 0,
   smroeFhh = 1,
   smroeShrsp = 2,
   smroeRh = 3,
};

enum stsMapRatOptEnum smroeStringToEnum(char *string);
/* Convert from string to enum representation. */

char *smroeEnumToString(enum stsMapRatOptEnum x);
/* Convert from enum to string representation. */

void smroeDropDown(char *var, char *curVal);
/* Make drop down of options. */

/****** Some stuff for fishClones related controls *******/
enum fishClonesOptEnum {
   fcoeFHCRC = 0,
   fcoeNCI = 1,
   fcoeSC = 2,
   fcoeRPCI = 3,
   fcoeCSMC = 4,
   fcoeLANL = 5,
   fcoeUCSF = 6,
};

enum fishClonesOptEnum fcoeStringToEnum(char *string);
/* Convert from string to enum representation. */

char *fcoeEnumToString(enum fishClonesOptEnum x);
/* Convert from enum to string representation. */

void fcoeDropDown(char *var, char *curVal);
/* Make drop down of options. */

/****** Some stuff for recombRate related controls *******/
enum recombRateOptEnum {
   rroeDecodeAvg = 0,
   rroeDecodeFemale = 1,
   rroeDecodeMale = 2,
   rroeMarshfieldAvg = 3,
   rroeMarshfieldFemale = 4,
   rroeMarshfieldMale = 5,
   rroeGenethonAvg = 6,
   rroeGenethonFemale = 7,
   rroeGenethonMale = 8,
};

enum recombRateOptEnum rroeStringToEnum(char *string);
/* Convert from string to enum representation. */

char *rroeEnumToString(enum recombRateOptEnum x);
/* Convert from enum to string representation. */

void rroeDropDown(char *var, char *curVal);
/* Make drop down of options. */

/****** Some stuff for recombRateRat related controls *******/
enum recombRateRatOptEnum {
   rrroeShrspAvg = 0,
   rrroeFhhAvg = 1,
};

enum recombRateRatOptEnum rrroeStringToEnum(char *string);
/* Convert from string to enum representation. */

char *rrroeEnumToString(enum recombRateRatOptEnum x);
/* Convert from enum to string representation. */

void rrroeDropDown(char *var, char *curVal);
/* Make drop down of options. */

/****** Some stuff for recombRateMouse related controls *******/
enum recombRateMouseOptEnum {
   rrmoeWiAvg = 0,
   rrmoeMgdAvg = 1,
};

enum recombRateMouseOptEnum rrmoeStringToEnum(char *string);
/* Convert from string to enum representation. */

char *rrmoeEnumToString(enum recombRateMouseOptEnum x);
/* Convert from enum to string representation. */

void rrmoeDropDown(char *var, char *curVal);
/* Make drop down of options. */

/****** Some stuff for cghNci60 related controls *******/
enum cghNci60OptEnum {
   cghoeTissue = 0,
   cghoeBreast = 1,
   cghoeCns = 2,
   cghoeColon = 3,
   cghoeLeukemia = 4,
   cghoeLung = 5,
   cghoeMelanoma = 6,
   cghoeOvary = 7,
   cghoeProstate = 8,
   cghoeRenal = 9,
   cghoeAll = 10,
};

enum cghNci60OptEnum cghoeStringToEnum(char *string);
/* Convert from string to enum representation. */

char *cghoeEnumToString(enum cghNci60OptEnum x);
/* Convert from enum to string representation. */

void cghoeDropDown(char *var, char *curVal);
/* Make drop down of options. */

/****** Some stuff for Expression Data tracks in general *******/

struct expdFilter
/* Info on one type of expression data filter. */
{
    struct expdFilter *next;  /* Next in list. */
    char *filterInclude;         /* Identifier associated with items to include, NULL indicates include all. */
    char *filterExclude;         /* Identifier associated with items to exclude, NULL indicates exclude none. */
    boolean redGreen;         /* True if red/green color scheme, Otherwise blue/red color scheme. */
};

/*** Some Stuff for the NCI60 track ***/

enum nci60OptEnum {
   nci60Tissue = 0,
   nci60All = 1,
   nci60Breast = 2,
   nci60Cns = 3,
   nci60Colon = 4,
   nci60Leukemia = 5,
   nci60Melanoma = 6,
   nci60Ovary = 7,
   nci60Prostate = 8,
   nci60Renal = 9,
   nci60Nsclc = 10,
   nci60Duplicates = 11,
   nci60Unknown = 12
};

enum nci60OptEnum nci60StringToEnum(char *string);
/* Convert from string to enum representation. */

char *nci60EnumToString(enum nci60OptEnum x);
/* Convert from enum to string representation. */

void nci60DropDown(char *var, char *curVal);
/* Make drop down of options. */

/*	chain track color options	*/
enum chainColorEnum {
   chainColorChromColors = 0,
   chainColorScoreColors = 1,
   chainColorNoColors = 2,
};

enum chainColorEnum chainColorStringToEnum(char *string);
/* Convert from string to enum representation. */

char *chainColorEnumToString(enum chainColorEnum x);
/* Convert from enum to string representation. */

void chainColorDropDown(char *var, char *curVal);
/* Make drop down of options. */

/*	Wiggle track Windowing combining function option	*/
enum wiggleWindowingEnum {
   wiggleWindowingMax = 0,
   wiggleWindowingMean = 1,
   wiggleWindowingMin = 2,
};

enum wiggleWindowingEnum wiggleWindowingStringToEnum(char *string);
/* Convert from string to enum representation. */

char *wiggleWindowingEnumToString(enum wiggleWindowingEnum x);
/* Convert from enum to string representation. */

void wiggleWindowingDropDown(char *var, char *curVal);
/* Make drop down of options. */

/*	Wiggle track use Smoothing option, 0 and 1 is the same as Off	*/
enum wiggleSmoothingEnum {
   wiggleSmoothingOff = 0,
   wiggleSmoothing2 = 1,
   wiggleSmoothing3 = 2,
   wiggleSmoothing4 = 3,
   wiggleSmoothing5 = 4,
   wiggleSmoothing6 = 5,
   wiggleSmoothing7 = 6,
   wiggleSmoothing8 = 7,
   wiggleSmoothing9 = 8,
   wiggleSmoothing10 = 9,
   wiggleSmoothing11 = 10,
   wiggleSmoothing12 = 11,
   wiggleSmoothing13 = 12,
   wiggleSmoothing14 = 13,
   wiggleSmoothing15 = 14,
   wiggleSmoothing16 = 15,
};

enum wiggleSmoothingEnum wiggleSmoothingStringToEnum(char *string);
/* Convert from string to enum representation. */

char *wiggleSmoothingEnumToString(enum wiggleSmoothingEnum x);
/* Convert from enum to string representation. */

void wiggleSmoothingDropDown(char *var, char *curVal);
/* Make drop down of options. */

/*	Wiggle track y Line Mark on/off option	*/
enum wiggleYLineMarkEnum {
   wiggleYLineMarkOff = 0,
   wiggleYLineMarkOn = 1,
};

enum wiggleYLineMarkEnum wiggleYLineMarkStringToEnum(char *string);
/* Convert from string to enum representation. */

char *wiggleYLineMarkEnumToString(enum wiggleYLineMarkEnum x);
/* Convert from enum to string representation. */

void wiggleYLineMarkDropDown(char *var, char *curVal);
/* Make drop down of options. */

/*	Wiggle track use AutoScale option	*/
enum wiggleScaleOptEnum {
   wiggleScaleManual = 0,
   wiggleScaleAuto = 1,
};

enum wiggleScaleOptEnum wiggleScaleStringToEnum(char *string);
/* Convert from string to enum representation. */

char *wiggleScaleEnumToString(enum wiggleScaleOptEnum x);
/* Convert from enum to string representation. */

void wiggleScaleDropDown(char *var, char *curVal);
/* Make drop down of options. */

/*	Wiggle track type of graph option	*/
enum wiggleGraphOptEnum {
   wiggleGraphPoints = 0,
   wiggleGraphBar = 1,
};

enum wiggleGraphOptEnum wiggleGraphStringToEnum(char *string);
/* Convert from string to enum representation. */

char *wiggleGraphEnumToString(enum wiggleGraphOptEnum x);
/* Convert from enum to string representation. */

void wiggleGraphDropDown(char *var, char *curVal);
/* Make drop down of options. */

/*	Wiggle track Grid lines on/off option	*/
enum wiggleGridOptEnum {
   wiggleHorizontalGridOn = 0,
   wiggleHorizontalGridOff = 1,
};

enum wiggleGridOptEnum wiggleGridStringToEnum(char *string);
/* Convert from string to enum representation. */

char *wiggleGridEnumToString(enum wiggleGridOptEnum x);
/* Convert from enum to string representation. */

void wiggleGridDropDown(char *var, char *curVal);
/* Make drop down of options. */


/*** Control of base/codon coloring code: ***/

/* Drawing modes: values <= baseColorDrawOff don't render at base or codon
 * level */
enum baseColorDrawOpt
    {
    baseColorDrawCds = -1,        /* not a selection option, a fall back when
                                   * zoomed out. */
    baseColorDrawOff = 0,
    baseColorDrawGenomicCodons = 1,
    baseColorDrawItemCodons = 2,
    baseColorDrawDiffCodons = 3,
    baseColorDrawItemBases = 4,
    baseColorDrawDiffBases = 5,
    };

/* Drawing mode select box labels: */
#define BASE_COLOR_DRAW_OFF_LABEL "OFF"
#define BASE_COLOR_DRAW_GENOMIC_CODONS_LABEL "genomic codons"
#define BASE_COLOR_DRAW_ITEM_CODONS_LABEL "mRNA codons"
#define BASE_COLOR_DRAW_DIFF_CODONS_LABEL "nonsynonymous mRNA codons"
#define BASE_COLOR_DRAW_ITEM_BASES_LABEL "mRNA bases"
#define BASE_COLOR_DRAW_DIFF_BASES_LABEL "different mRNA bases"

/* Drawing mode select box values: */
#define BASE_COLOR_DRAW_OFF "none"
#define BASE_COLOR_DRAW_GENOMIC_CODONS "genomicCodons"
#define BASE_COLOR_DRAW_ITEM_CODONS "itemCodons"
#define BASE_COLOR_DRAW_DIFF_CODONS "diffCodons"
#define BASE_COLOR_DRAW_ITEM_BASES "itemBases"
#define BASE_COLOR_DRAW_DIFF_BASES "diffBases"

/* Drawing mode per-track cart variable suffix: */
#define BASE_COLOR_VAR_SUFFIX "baseColorDrawOpt"

/* trackDb settings: */
#define BASE_COLOR_USE_CDS "baseColorUseCds"
#define BASE_COLOR_USE_SEQUENCE "baseColorUseSequence"
#define BASE_COLOR_DEFAULT "baseColorDefault"
#define SHOW_DIFF_BASES_ALL_SCALES "showDiffBasesAllScales"

/* Coloring help pages: */
#define CDS_HELP_PAGE "../goldenPath/help/hgCodonColoring.html"
#define CDS_MRNA_HELP_PAGE "../goldenPath/help/hgCodonColoringMrna.html"


void baseColorDrawOptDropDown(struct cart *cart, struct trackDb *tdb);
/* Make appropriately labeled drop down of options if any are applicable.*/

enum baseColorDrawOpt baseColorDrawOptEnabled(struct cart *cart,
					      struct trackDb *tdb);
/* Query cart & trackDb to determine what drawing mode (if any) is enabled. */


/*** Control of fancy indel display code: ***/

/* trackDb settings: */
#define INDEL_DOUBLE_INSERT "indelDoubleInsert"
#define INDEL_QUERY_INSERT "indelQueryInsert"
#define INDEL_POLY_A "indelPolyA"

#define INDEL_HELP_PAGE "../goldenPath/help/hgIndelDisplay.html"

void indelShowOptions(struct cart *cart, struct trackDb *tdb);
/* Make HTML inputs for indel display options if any are applicable. */

void indelEnabled(struct cart *cart, struct trackDb *tdb, float basesPerPixel,
		  boolean *retDoubleInsert, boolean *retQueryInsert,
		  boolean *retPolyA);
/* Query cart & trackDb to determine what indel display (if any) is enabled. Set
 * basesPerPixel to -1.0 to disable check for zoom level.  */

/*** Some Stuff for the base position (ruler) controls ***/

#define ZOOM_1PT5X      "1.5x"
#define ZOOM_3X         "3x"
#define ZOOM_10X        "10x"
#define ZOOM_BASE       "base"

void zoomRadioButtons(char *var, char *curVal);
/* Make a list of radio buttons for all zoom options */

/*** Some Stuff for the wiggle track ***/

enum wiggleOptEnum {
   wiggleNoInterpolation = 0,
   wiggleLinearInterpolation = 1,
};

enum wiggleOptEnum wiggleStringToEnum(char *string);
/* Convert from string to enum representation. */

char *wiggleEnumToString(enum wiggleOptEnum x);
/* Convert from enum to string representation. */

void wiggleDropDown(char *var, char *curVal);
/* Make drop down of options. */



/*** Some Stuff for the GCwiggle track ***/

enum GCwiggleOptEnum {
   GCwiggleNoInterpolation = 0,
   GCwiggleLinearInterpolation = 1,
};

enum GCwiggleOptEnum GCwiggleStringToEnum(char *string);
/* Convert from string to enum representation. */

char *GCwiggleEnumToString(enum GCwiggleOptEnum x);
/* Convert from enum to string representation. */

void GCwiggleDropDown(char *var, char *curVal);
/* Make drop down of options. */



/*** Some Stuff for the chimp track ***/

enum chimpOptEnum {
   chimpNoInterpolation = 0,
   chimpLinearInterpolation = 1,
};

enum chimpOptEnum chimpStringToEnum(char *string);
/* Convert from string to enum representation. */

char *chimpEnumToString(enum chimpOptEnum x);
/* Convert from enum to string representation. */

void wiggleDropDown(char *var, char *curVal);
/* Make drop down of options. */





/*** Some Stuff for the AFFY track ***/

enum affyOptEnum {
    affyChipType = 0,
    affyId = 1,
    affyTissue = 2,
    affyAllData = 3,
};

enum affyOptEnum affyStringToEnum(char *string);
/* Convert from string to enum representation. */

char *affyEnumToString(enum affyOptEnum x);
/* Convert from enum to string representation. */

void affyDropDown(char *var, char *curVal);
/* Make drop down of options. */

/*** Some Stuff for the affy all exon track ***/

enum affyAllExonOptEnum {
    affyAllExonChip = 0,
    affyAllExonTissue = 1,
    affyAllExonAllData = 2,
};

enum affyAllExonOptEnum affyAllExonStringToEnum(char *string);
/* Convert from string to enum representation. */

char *affyAllExonEnumToString(enum affyAllExonOptEnum x);
/* Convert from enum to string representation. */

void affyAllExonDropDown(char *var, char *curVal);
/* Make drop down of options. */

/****** Some stuff for Rosetta related controls *******/

enum rosettaOptEnum {
    rosettaAll =0,
    rosettaPoolOther=1,
    rosettaPool=2,
    rosettaOther=3
};

enum rosettaExonOptEnum {
    rosettaConfEx,
    rosettaPredEx,
    rosettaAllEx
};

enum rosettaOptEnum rosettaStringToEnum(char *string);
/* Convert from string to enum representation. */

char *rosettaEnumToString(enum rosettaOptEnum x);
/* Convert from enum to string representation. */

void rosettaDropDown(char *var, char *curVal);
/* Make drop down of options. */

enum rosettaExonOptEnum rosettaStringToExonEnum(char *string);
/* Convert from string to enum representation of exon types. */

char *rosettaExonEnumToString(enum rosettaExonOptEnum x);
/* Convert from enum to string representation of exon types. */

void rosettaExonDropDown(char *var, char *curVal);
/* Make drop down of exon type options. */


/****** Some stuff for mRNA and EST related controls *******/

struct mrnaFilter
/* Info on one type of mrna filter. */
   {
   struct  mrnaFilter *next;	/* Next in list. */
   char *label;	  /* Filter label. */
   char *key;     /* Suffix of cgi variable holding search pattern. */
   char *table;	  /* Associated table to search. */
   char *pattern; /* Pattern to find. */
   int mrnaTableIx;	/* Index of field in mrna table. */
   struct hash *hash;  /* Hash of id's in table that match pattern */
   };

struct mrnaUiData
/* Data for mrna-specific user interface. */
   {
   char *filterTypeVar;	/* cgi variable that holds type of filter. */
   char *logicTypeVar;	/* cgi variable that indicates logic. */
   struct mrnaFilter *filterList;	/* List of filters that can be applied. */
   };

struct mrnaUiData *newBedUiData(char *track);
/* Make a new  in extra-ui data structure for a bed. */

struct mrnaUiData *newMrnaUiData(char *track, boolean isXeno);
/* Make a new  in extra-ui data structure for mRNA. */

struct trackNameAndLabel
/* Store track name and label. */
   {
   struct trackNameAndLabel *next;
   char *name;	/* Name (not allocated here) */
   char *label; /* Label (not allocated here) */
   };

int trackNameAndLabelCmp(const void *va, const void *vb);
/* Compare to sort on label. */

struct hash *makeTrackHashWithComposites(char *database, char *chrom, 
                                        bool withComposites);
/* Make hash of trackDb items for this chromosome, optionally includingc
   omposites, not just the subtracks. */

struct hash *makeTrackHash(char *database, char *chrom);
/* Make hash of trackDb items for this chromosome. */

char *genePredDropDown(struct cart *cart, struct hash *trackHash,  
                                        char *formName, char *varName);
/* Make gene-prediction drop-down().  Return track name of
 * currently selected one.  Return NULL if no gene tracks. */

/****** Stuff for acembly related options *******/

enum acemblyOptEnum {
    acemblyAll =0,
    acemblyMain=1,
    acemblyPutative=2,
};

enum acemblyOptEnum acemblyStringToEnum(char *string);
/* Convert from string to enum representation. */

char *acemblyEnumToString(enum acemblyOptEnum x);
/* Convert from enum to string representation. */

void acemblyDropDown(char *var, char *curVal);
/* Make drop down of options. */

void hCompositeUi(char *db, struct cart *cart, struct trackDb *tdb,
		  char *primarySubtrack, char *fakeSubmit, char *formName);
/* UI for composite tracks: subtrack selection.  If primarySubtrack is
 * non-NULL, don't allow it to be cleared and only offer subtracks
 * that have the same type.  If fakeSubmit is non-NULL, add a hidden
 * var with that name so it looks like it was pressed. */

boolean superTrackDropDown(struct cart *cart, struct trackDb *tdb,
                                int visibleChild);
/* Displays hide/show dropdown for supertrack.  
 * Set visibleChild to indicate whether 'show' should be grayed
 * out to indicate that no supertrack members are visible:
 *    0 to gray out (no visible children)
 *    1 don't gray out (there are visible children)
 *   -1 don't know (this function should determine)
 * If -1,i the subtracks field must be populated with the child trackDbs.
 * Returns false if not a supertrack */

boolean dimensionsExist(struct trackDb *parentTdb);
/* Does this parent track contain dimensions? */

boolean subgroupFind(struct trackDb *childTrack, char *name,char **value);
/* looks for a single tag in a childTrack's subGroups setting */

void subgroupFree(char **value);
/* frees subgroup memory */

int tvConvertToNumericOrder(enum trackVisibility v);
/* Convert the enum to numeric order of display power full=4,hide=0 */ 

int tvCompare(enum trackVisibility a, enum trackVisibility b);
/* enum trackVis isn't in numeric order by visibility, so compare
 * symbolically: */

enum trackVisibility tvMin(enum trackVisibility a, enum trackVisibility b);
/* Return the less visible of a and b. */

char *compositeViewControlNameFromTdb(struct trackDb *tdb);
/* Returns a string with the composite view control name if one exists */
void compositeViewControlNameFree(char **name);
/* frees a string allocated by compositeViewControlNameFromTdb */

#endif /* HUI_H */
