/* hui - human genome browser user interface controls that are shared
 * between more than one CGI. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef HUI_H
#define HUI_H

#include "common.h"
#include "cart.h"
#include "trackDb.h"
#include "customTrack.h"
#include "wiggle.h"
#include "asParse.h"

struct lineFile;

void setUdcTimeout(struct cart *cart);
/* set the udc cache timeout */

void setUdcCacheDir();
/* set the path to the udc cache dir */

char *hDownloadsServer();
/* get the downloads server from hg.conf or the default */

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

#define ENCODE_DATA_RELEASE_POLICY "/ENCODE/terms.html"
char *encodeRestrictionDate(char *db,struct trackDb *trackDb,boolean excludePast);
/* Create a string for ENCODE restriction date of this track
   if return is not null, then free it after use */
#define encodeRestrictionDateDisplay(db,tdb) encodeRestrictionDate(db,tdb,TRUE)

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

char *hTrackUiForTrack(char *trackName);
/* Relative URL to extended track UI, delegated to a track-specific UI if available. */

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
#define CUTTERS_TRACK_PRIORITY 100

/* Definition for oligo match track. */
#define oligoMatchVar "hgt.oligoMatch"
#define oligoMatchDefault ""
#define OLIGO_MATCH_TRACK_NAME "oligoMatch"
#define OLIGO_MATCH_TRACK_LABEL "Short Match"
#define OLIGO_MATCH_TRACK_LONGLABEL "Perfect Match to Short Sequence"
#define OLIGO_MATCH_TRACK_PRIORITY 100

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
#define BASE_SHOWASM_SCALEBAR	"hgt.baseShowAsmScaleBar"
#define BASE_TITLE	"hgt.baseTitle"
#define REV_CMPL_DISP   "hgt.revCmplDisp"

/* Configuration variable to cause ruler zoom to zoom to base level */
#define RULER_BASE_ZOOM_VAR      "rulerBaseZoom"

/* Maf track display variables and their values */
#define MAF_GENEPRED_VAR  "mafGenePred"
#define MAF_FRAMING_VAR   "mafFrame"
#define MAF_DOT_VAR       "mafDot"
#define MAF_CHAIN_VAR     "mafChain"
#define MAF_SHOW_SNP      "mafShowSnp"

/* display of bases for tracks that are type psl and have sequence e.g. ESTs */
#define PSL_SEQUENCE_BASES	"pslSequenceBases"
#define PSL_SEQUENCE_DEFAULT	"no"

/******  Some stuff for tables of controls ******/
#define CONTROL_TABLE_WIDTH 940
/* this number is 10 less than hgDefaultPixWidth and DEFAULT_PIX_WIDTH
 *	defined in hCommon.h */

#define EXTENDED_DNA_BUTTON "extended case/color options"

/* Net track option */
#define NET_OPT_TOP_ONLY  "netTopOnly"

/* Microarray default setting. */
#define MICROARRAY_CLICK_LIMIT 200

/* itemImagePath trackDb variable names */
#define ITEM_IMAGE_PATH "itemImagePath"
#define ITEM_BIG_IMAGE_PATH "itemBigImagePath"

/* SwitchGear TSS default filter. */
#define SWITCHDBTSS_FILTER 10

/* Subtrack color support in UI */
#define SUBTRACK_COLOR_PATCH    "showSubtrackColorOnUi"
// for use with subGroup and sortOrder settings
// ie. subGroup1 subtrackColor Color; sortOrder subtrackColor=+ cellType=+
#define SUBTRACK_COLOR_SUBGROUP "subtrackColor"

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
#define tvShow tvFull

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

void hTvDropDownClassVisOnlyAndExtra(char *varName, enum trackVisibility vis,
                                     boolean canPack, char *class, char *visOnly, struct slPair *events);
/* Make track visibility drop down for varName with style class,
	and potentially limited to visOnly */
#define hTvDropDownClassVisOnly(varName,vis,canPack,class,visOnly) \
        hTvDropDownClassVisOnlyAndExtra(varName,vis,canPack,class,visOnly,NULL)

void hTvDropDownClassWithJavascript(char *varName, char *id, enum trackVisibility vis, boolean canPack,
                                    char *class, struct slPair *events);
/* Make track visibility drop down for varName with style class and javascript */
#define hTvDropDownClass(varName,vis,canPack,class) \
        hTvDropDownClassWithJavascript((varName),NULL,(vis),(canPack),(class),NULL)
#define hTvDropDownWithJavascript(varName,vis,canPack,javascript) \
        hTvDropDownClassWithJavascript((varName),NULL,(vis),(canPack),"normalText",(javascript))
#define hTvDropDown(varName,vis,canPack) \
        hTvDropDownClassWithJavascript((varName),NULL,(vis),(canPack),"normalText",NULL)

#define SUPERTRACK_DEFAULT_VIS  "hide"

void hideShowDropDownWithClassAndExtra(char *varName, char *id, boolean show, char *class, struct slPair *events);
#define hideShowDropDown(varName,show,class) \
        hideShowDropDownWithClassAndExtra(varName,NULL,show,class,NULL)
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

/*	net track level display options	*/
enum netLevelEnum {
   netLevel0 = 0,
   netLevel1 = 1,
   netLevel2 = 2,
   netLevel3 = 3,
   netLevel4 = 4,
   netLevel5 = 5,
   netLevel6 = 6,
};

enum netLevelEnum netLevelStringToEnum(char *string);
/* Convert from string to enum representation. */

char *netLevelEnumToString(enum netLevelEnum x);
/* Convert from enum to string representation. */

void netLevelDropDown(char *var, char *curVal);
/* Make drop down of options. */

/*	net track color options	*/
enum netColorEnum {
   netColorChromColors = 0,
   netColorGrayScale = 1,
};

enum netColorEnum netColorStringToEnum(char *string);
/* Convert from string to enum representation. */

char *netColorEnumToString(enum netColorEnum x);
/* Convert from enum to string representation. */

void netColorDropDown(char *var, char *curVal);
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
   wiggleWindowingWhiskers = 0,
   wiggleWindowingMax = 1,
   wiggleWindowingMean = 2,
   wiggleWindowingMin = 3,
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
   wiggleSmoothingMax = MAX_SMOOTHING,	/* Not an option, but lets us keep track of memory to use */
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

/*	Wiggle track transform func*/
enum wiggleTransformFuncEnum {
   wiggleTransformFuncNone = 0,
   wiggleTransformFuncLog = 1,
};

enum wiggleTransformFuncEnum wiggleTransformFuncToEnum(char *string);
/* Convert from string to enum representation. */

/*	Wiggle track always include zero */
enum wiggleAlwaysZeroEnum {
   wiggleAlwaysZeroOff = 0,
   wiggleAlwaysZeroOn = 1,
};

enum wiggleAlwaysZeroEnum wiggleAlwaysZeroToEnum(char *string);
/* Convert from string to enum representation. */

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

#define WIG_VIEWFUNC_SHOW_ALL "showAll"
#define WIG_VIEWFUNC_ADD_ALL "addAll"
#define WIG_VIEWFUNC_SUBTRACT_ALL "subtractAll"

/*	Wiggle track aggregate option	*/
enum wiggleViewFuncEnum {
   wiggleViewFuncShowAll = 0,
   wiggleViewFuncAddAll = 1,
   wiggleViewFuncSubtractAll = 2,
};

char *wiggleViewFuncEnumToString(enum wiggleViewFuncEnum x);
/* Convert from enum to string representation. */

enum wiggleViewFuncEnum wiggleViewFuncStringToEnum(char *string);

#define WIG_AGGREGATE_NONE "none"
#define WIG_AGGREGATE_TRANSPARENT "transparentOverlay"
#define WIG_AGGREGATE_SOLID "solidOverlay"
#define WIG_AGGREGATE_STACKED "stacked"
#define WIG_AGGREGATE_ADD "add"
#define WIG_AGGREGATE_SUBTRACT "subtract"

/*	Wiggle track aggregate option	*/
enum wiggleAggregateFunctionEnum {
   wiggleAggregateNone = 0,
   wiggleAggregateTransparent = 1,
   wiggleAggregateSolid = 2,
   wiggleAggregateStacked = 3,
   wiggleAggregateAdd = 4,
   wiggleAggregateSubtract = 5,
};

char *wiggleAggregateFunctionEnumToString(enum wiggleAggregateFunctionEnum x);
/* Convert from enum to string representation. */

enum wiggleAggregateFunctionEnum wiggleAggregateFunctionStringToEnum(char *string);

/*** BAM alignment track coloring code: ***/
// tdbSettings / cart var suffixes and default values:
#define BAM_PAIR_ENDS_BY_NAME "pairEndsByName"
#define BAM_SHOW_NAMES "showNames"
#define BAM_MIN_ALI_QUAL "minAliQual"
#define BAM_MIN_ALI_QUAL_DEFAULT "0"

#define BAM_COLOR_MODE "bamColorMode"
#define BAM_COLOR_MODE_GRAY "gray"
#define BAM_COLOR_MODE_STRAND "strand"
#define BAM_COLOR_MODE_TAG "tag"
#define BAM_COLOR_MODE_OFF "off"
#define BAM_COLOR_MODE_DEFAULT BAM_COLOR_MODE_STRAND

#define BAM_GRAY_MODE "bamGrayMode"
#define BAM_GRAY_MODE_ALI_QUAL "aliQual"
#define BAM_GRAY_MODE_BASE_QUAL "baseQual"
#define BAM_GRAY_MODE_UNPAIRED "unpaired"
#define BAM_GRAY_MODE_DEFAULT BAM_GRAY_MODE_ALI_QUAL

#define BAM_COLOR_TAG "bamColorTag"
#define BAM_COLOR_TAG_DEFAULT "YC"

#define BAMWIG_MODE "bamWigMode"

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
#define BASE_COLOR_DRAW_ITEM_BASES_CDS_LABEL "mRNA bases"
#define BASE_COLOR_DRAW_DIFF_BASES_CDS_LABEL "different mRNA bases"
#define BASE_COLOR_DRAW_ITEM_BASES_NC_LABEL "item bases"
#define BASE_COLOR_DRAW_DIFF_BASES_NC_LABEL "different item bases"

/* Drawing mode select box values: */
#define BASE_COLOR_DRAW_OFF "none"
#define BASE_COLOR_DRAW_GENOMIC_CODONS "genomicCodons"
#define BASE_COLOR_DRAW_ITEM_CODONS "itemCodons"
#define BASE_COLOR_DRAW_DIFF_CODONS "diffCodons"
#define BASE_COLOR_DRAW_ITEM_BASES "itemBases"
#define BASE_COLOR_DRAW_DIFF_BASES "diffBases"

/* Drawing mode per-track cart variable suffix: */
#define BASE_COLOR_VAR_SUFFIX "baseColorDrawOpt"
#define CODON_NUMBERING_SUFFIX "codonNumbering"

/* trackDb settings: */
#define BASE_COLOR_USE_CDS "baseColorUseCds"
#define BASE_COLOR_USE_SEQUENCE "baseColorUseSequence"
#define BASE_COLOR_DEFAULT "baseColorDefault"
#define SHOW_DIFF_BASES_ALL_SCALES "showDiffBasesAllScales"

/* Coloring help pages: */
#define CDS_HELP_PAGE "../goldenPath/help/hgCodonColoring.html"
#define CDS_MRNA_HELP_PAGE "../goldenPath/help/hgCodonColoringMrna.html"
#define CDS_BASE_HELP_PAGE "../goldenPath/help/hgBaseLabel.html"

/* Settings for coloring and filtering genePred tables from an item class table. */
#define GENEPRED_CLASS_VAR "geneClasses"
#define GENEPRED_CLASS_PREFIX "gClass_"
#define GENEPRED_CLASS_TBL "itemClassTbl"
#define GENEPRED_CLASS_NAME_COLUMN "itemClassNameColumn"
#define GENEPRED_CLASS_NAME_COLUMN_DEFAULT "name"
#define GENEPRED_CLASS_CLASS_COLUMN "itemClassClassColumn"
#define GENEPRED_CLASS_CLASS_COLUMN_DEFAULT "class"

/* genbank track cart variable suffixes: */
#define SHOW_PATENT_SEQUENCES_SUFFIX "showPatentSequences"

void baseColorDrawOptDropDown(struct cart *cart, struct trackDb *tdb);
/* Make appropriately labeled drop down of options if any are applicable.*/

enum baseColorDrawOpt baseColorDrawOptEnabled(struct cart *cart,
					      struct trackDb *tdb);
/* Query cart & trackDb to determine what drawing mode (if any) is enabled. */

/*** Other Gene Prediction type options: ***/
#define HIDE_NONCODING_SUFFIX "hideNoncoding"
#define HIDE_NONCODING_DEFAULT FALSE

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
#define ZOOM_100X       "100x"
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
   char *suffix;  /* Suffix of cgi variable holding search pattern. */
   char *table;	  /* Associated table to search. */
   char *pattern; /* Pattern to find. */
   int mrnaTableIx;	/* Index of field in mrna table. */
   struct hash *hash;  /* Hash of id's in table that match pattern */
   };

struct mrnaUiData
/* Data for mrna-specific user interface. */
   {
    char *filterTypeSuffix; /* cgi variable suffix that holds type of filter. */
    char *logicTypeSuffix;  /* cgi variable suffix that indicates logic. */
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

void rAddTrackListToHash(struct hash *trackHash, struct trackDb *tdbList, char *chrom,
	boolean leafOnly);
/* Recursively add trackList to trackHash */

struct hash *trackHashMakeWithComposites(char *db,char *chrom,struct trackDb **tdbList,
                                         bool withComposites);
// Make hash of trackDb items for this chromosome, including composites, not just the subtracks.
// May pass in prepopulated trackDb list, or may receive the trackDb list as an inout.
#define makeTrackHashWithComposites(db,chrom,withComposites) \
        trackHashMakeWithComposites(db,chrom,NULL,withComposites)
#define makeTrackHash(db,chrom) trackHashMakeWithComposites(db,chrom,NULL,FALSE)

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

char *compositeGroupLabel(struct trackDb *childTdb, char *group, char *id);
/* Given ID from group, return corresponding label,  looking through parent's subGroupN's */

char *compositeGroupId(struct trackDb *tdb, char *group, char *id);
/* Given label, return id,  looking through parent's subGroupN's */

char *compositeLabelWithVocabLink(char *db,struct trackDb *parentTdb, struct trackDb *childTdb,
	char *vocabType, char *label);
/* If the parentTdb has an ENCODE controlledVocabulary setting and the vocabType is found,
   then label will be wrapped with the link to display it.  Return string is cloned. */

char *wgEncodeVocabLink(char *file,char *term,char *value,char *title, char *label,char *suffix);
// returns allocated string of HTML link to ENCODE controlled vocabulary term

char *metadataAsHtmlTable(char *db,struct trackDb *tdb,boolean
        showLongLabel,boolean showShortLabel);
/* If metadata from metaDb exists, return string of html with table definition */

boolean compositeMetadataToggle(char *db,struct trackDb *tdb,char *title,
        boolean embeddedInText,boolean showLongLabel);
/* If metadata from metaTbl exists, create a link that will allow toggling it's display */

boolean superTrackDropDownWithExtra(struct cart *cart, struct trackDb *tdb,
                                    int visibleChild, struct slPair *events);
/* Displays hide/show dropdown for supertrack.
 * Set visibleChild to indicate whether 'show' should be grayed
 * out to indicate that no supertrack members are visible:
 *    0 to gray out (no visible children)
 *    1 don't gray out (there are visible children)
 *   -1 don't know (this function should determine)
 * If -1,i the subtracks field must be populated with the child trackDbs.
 * Returns false if not a supertrack */
#define superTrackDropDown(cart,tdb,visibleChild) \
        superTrackDropDownWithExtra(cart,tdb,visibleChild,NULL)

typedef struct _dimensions
    {
    int count;
    char**names;
    char**subgroups;
    char* setting;
    } dimensions_t;

boolean dimensionsExist(struct trackDb *parentTdb);
/* Does this parent track contain dimensions? */

typedef struct _membership
    {
    int count;
    char **subgroups;  // Ary of Tags in parentTdb->subGroupN and in childTdb->subGroups (ie view)
    char **membership; // Ary of Tags of subGroups that child belongs to (ie PK)
    char **titles;     // Ary of Titles of subGroups a child belongs to (ie Peak)
    char * setting;
    } membership_t;

typedef struct _membersForAll
    {
    int abcCount;
    int dimMax;               // Arrays of "members" structs will be ordered as
			      //    [view][dimX][dimY][dimA]... with first 3 in fixed spots
			      //    and rest as found (and non-empty)
    boolean filters;          // ABCs use filterComp boxes (as upposed to check boxes
    dimensions_t *dimensions; // One struct describing "deimensions" setting"
			      //    (e.g. dimX:cell dimY:antibody dimA:treatment)
    members_t* members[27];   // One struct for each dimension describing groups in dimension
			      //    (e.g. cell: GM12878,K562)
    char* checkedTags[27];  // FIXME: Should move checkedTags into
			    // membersForAll->members[ix]->selected;
    char letters[27];
    } membersForAll_t;

membersForAll_t* membersForAllSubGroupsGet(struct trackDb *parentTdb, struct cart *cart);
// Returns all the parents subGroups and members

membership_t *subgroupMembershipGet(struct trackDb *childTdb);
/* gets all the subgroup membership for a child track */

int subgroupCount(struct trackDb *parentTdb);
/* How many subGroup setting does this parent have? */

char * subgroupSettingByTagOrName(struct trackDb *parentTdb, char *groupNameOrTag);
/* look for a subGroup by name (ie subGroup1) or tag (ie view) and return an unallocated char* */

boolean subgroupingExists(struct trackDb *parentTdb, char *groupNameOrTag);
/* Does this parent track contain a particular subgrouping? */

boolean subgroupFind(struct trackDb *childTrack, char *name,char **value);
/* looks for a single tag in a childTrack's subGroups setting */

void subgroupFree(char **value);
/* frees subgroup memory */

boolean subgroupRequired(char *value);
/* Returns whether subgroup much be specified for each track.
 * Generally true.  Exceptions are specially defined subgroups */

int multViewCount(struct trackDb *parentTdb);
/* returns the number of multiView views declared */

int tvConvertToNumericOrder(enum trackVisibility v);
/* Convert the enum to numeric order of display power full=4,hide=0 */

int tvCompare(enum trackVisibility a, enum trackVisibility b);
/* enum trackVis isn't in numeric order by visibility, so compare
 * symbolically: */

enum trackVisibility tvMin(enum trackVisibility a, enum trackVisibility b);
/* Return the less visible of a and b. */

enum trackVisibility tdbLocalVisibility(struct cart *cart, struct trackDb *tdb,
                                        boolean *subtrackOverride);
// returns visibility NOT limited by ancestry.
// Fills optional boolean if subtrack specific vis is found
// If not NULL cart will be examined without ClosestToHome.
// Folders/supertracks resolve to hide/full

enum trackVisibility tdbVisLimitedByAncestors(struct cart *cart, struct trackDb *tdb,
                                              boolean checkBoxToo, boolean foldersToo);
// returns visibility limited by ancestry.
// This includes subtrack vis override and parents limit maximum.
// cart may be null, in which case, only trackDb settings (default state) are examined
// checkBoxToo means ensure subtrack checkbox state is visible
// foldersToo means limit by folders (aka superTracks) as well.
#define tdbVisLimitedByAncestry(cart,tdb,noFolders) \
        tdbVisLimitedByAncestors(cart, tdb, TRUE, !(noFolders))

char *compositeViewControlNameFromTdb(struct trackDb *tdb);
/* Returns a string with the composite view control name if one exists */
void compositeViewControlNameFree(char **name);
/* frees a string allocated by compositeViewControlNameFromTdb */

void wigOption(struct cart *cart, char *name, char *title, struct trackDb *tdb);
/* let the user choose to see the track in wiggle mode */

void wigCfgUi(struct cart *cart, struct trackDb *tdb,char *name,char *title,boolean boxed);
/* UI for the wiggle track */

void labelCfgUi(char *db, struct cart *cart, struct trackDb *tdb, char *prefix);
/* Put up a choice for labels. */

#define NO_SCORE_FILTER  "noScoreFilter"
#define  SCORE_FILTER      "scoreFilter"
#define  SCORE_LABEL      "scoreLabel"
#define SIGNAL_FILTER      "signalFilter"
#define PVALUE_FILTER      "pValueFilter"
#define QVALUE_FILTER      "qValueFilter"
#define _NO                "No"
#define _LIMITS            "Limits"
#define _MIN               "Min"
#define _MAX               "Max"
#define _BY_RANGE          "ByRange"
#define  SCORE_MIN         "scoreMin"
#define  GRAY_LEVEL_SCORE_MIN SCORE_MIN
#define  MIN_GRAY_LEVEL  "minGrayLevel"

boolean colonPairToDoubles(char * colonPair,double *first,double *second);
// Non-destructive. Only sets values if found. No colon: value goes to *first

boolean colonPairToInts(char * colonPair,int *first,int *second);
// Non-destructive. Only sets values if found. No colon: value goes to *first

void getScoreIntRangeFromCart(struct cart *cart, struct trackDb *tdb, boolean parentLevel,
                                 char *scoreName, int *limitMin, int *limitMax,int *min,int *max);
// gets an integer score range from the cart, but the limits from trackDb
// for any of the pointers provided, will return a value found, if found, else it's contents
// are undisturbed (use NO_VALUE to recognize unavaliable values)
//
void getScoreFloatRangeFromCart(struct cart *cart, struct trackDb *tdb, boolean parentLevel,
                         char *scoreName, double *limitMin,double *limitMax,double*min,double*max);
// gets an double score range from the cart, but the limits from trackDb
// for any of the pointers provided, will return a value found, if found, else it's contents
// are undisturbed (use NO_VALUE to recognize unavaliable values)

void filterButtons(char *filterTypeVar, char *filterTypeVal, boolean none);
/* Put up some filter buttons. */

void radioButton(char *var, char *val, char *ourVal);
/* Print one radio button */

void oneMrnaFilterUi(struct controlGrid *cg, struct trackDb *tdb, char *text, char *var,
                     char *suffix, struct cart *cart);
/* Print out user interface for one type of mrna filter. */

void bedFiltCfgUi(struct cart *cart, struct trackDb *tdb, char *prefix, char *title, boolean boxed);
/* Put up UI for an "bedFilter" tracks. */

void mrnaCfgUi(struct cart *cart, struct trackDb *tdb, char *prefix, char *title, boolean boxed);
/* Put up UI for an mRNA (or EST) track. */

boolean bedScoreHasCfgUi(struct trackDb *tdb);
// Confirms that this track has a bedScore Cfg UI

void scoreCfgUi(char *db, struct cart *cart, struct trackDb *parentTdb, char *name,char *title,int maxScore,boolean boxed);
/* Put up UI for filtering bed track based on a score */

void crossSpeciesCfgUi(struct cart *cart, struct trackDb *tdb);
// Put up UI for selecting rainbow chromosome color or intensity score.

void pslCfgUi(char *db, struct cart *cart, struct trackDb *parentTdb, char *prefix ,char *title, boolean boxed);
/* Put up UI for psl tracks */

void netAlignCfgUi(char *db, struct cart *cart, struct trackDb *parentTdb, char *prefix ,char *title, boolean boxed);
/* Put up UI for net tracks */

void chainCfgUi(char *db, struct cart *cart, struct trackDb *parentTdb, char *prefix ,char *title, boolean boxed, char *chromosome);
/* Put up UI for chain tracks */

void scoreGrayLevelCfgUi(struct cart *cart, struct trackDb *tdb, char *prefix, int scoreMax);
/* If scoreMin has been set, let user select the shade of gray for that score, in case
 * the default is too light to see or darker than necessary. */

struct dyString *dyAddFilterAsInt(struct cart *cart, struct trackDb *tdb,
                                  struct dyString *extraWhere,char *filter,
                                  char *defaultVal, char*field, boolean *and);
// creates the where clause condition to support numeric int filter range.
// Filters are expected to follow
//      {fiterName}: trackDb min or min:max - default value(s);
//      {filterName}Min or {filterName}: min (user supplied) cart variable;
//      {filterName}Max: max (user supplied) cart variable;
//      {filterName}Limits: trackDb allowed range "0:1000" Optional
//         uses:{filterName}Min: old trackDb value if {filterName}Limits not found
//              {filterName}Max: old trackDb value if {filterName}Limits not found
//              defaultLimits: function param if no tdb limits settings found)
// The 'and' param and dyString in/out allows stringing multiple where clauses together

struct dyString *dyAddFilterAsDouble(struct cart *cart, struct trackDb *tdb,
                                     struct dyString *extraWhere,char *filter,
                                     char *defaultLimits, char*field, boolean *and);
// creates the where clause condition to support numeric double filters.
// Filters are expected to follow
//      {fiterName}: trackDb min or min:max - default value(s);
//      {filterName}Min or {filterName}: min (user supplied) cart variable;
//      {filterName}Max: max (user supplied) cart variable;
//      {filterName}Limits: trackDb allowed range "0.0:10.0" Optional
//          uses:  defaultLimits: function param if no tdb limits settings found)
// The 'and' param allows stringing multiple where clauses together

struct dyString *dyAddAllScoreFilters(struct cart *cart, struct trackDb *tdb,
                                      struct dyString *extraWhere,boolean *and);
// creates the where clause condition to gather together all random double filters
// Filters are expected to follow
//      {fiterName}: trackDb min or min:max - default value(s);
//      {filterName}Min or {filterName}: min (user supplied) cart variable;
//      {filterName}Max: max (user supplied) cart variable;
//      {filterName}Limits: trackDb allowed range "0.0:10.0" Optional
//          uses:  defaultLimits: function param if no tdb limits settings found)
// The 'and' param and dyString in/out allows stringing multiple where clauses together

boolean encodePeakHasCfgUi(struct trackDb *tdb);
// Confirms that this track has encode Peak cfgUI

void encodePeakCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title,
                     boolean boxed);
// Put up UI for filtering wgEnocde peaks based on score, Pval and Qval

void genePredCfgUi(char *db, struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed);
// Put up genePred-specific controls

void wigMafCfgUi(struct cart *cart, struct trackDb *tdb,char *name, char *title, boolean boxed, char *db);
/* UI for maf/wiggle track */

void bamCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed);
/* BAM: short-read-oriented alignment file format. */


void lrgCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed);
/* LRG: Locus Reference Genomic sequences mapped to assembly. */

void lrgTranscriptAliCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title,
			   boolean boxed);
/* LRG Transcripts: Locus Reference Genomic transcript sequences mapped to assembly. */

boolean tdbSortPrioritiesFromCart(struct cart *cart, struct trackDb **tdbList);
// Updates the tdb->priority from cart then sorts the list anew.
// Returns TRUE if priorities obtained from cart

boolean tdbRefSortPrioritiesFromCart(struct cart *cart, struct slRef **tdbRefList);
/* Updates the tdb->priority from cart then sorts the list anew.
   Returns TRUE if priorities obtained from cart */

enum trackVisibility visCompositeViewDefault(struct trackDb *parentTdb,char *view);
// returns the default track visibility of particular view within a composite track

boolean isNameAtParentLevel(struct trackDb *tdb,char *name);
// cfgUi controls are passed a prefix name that may be at the composite, view or subtrack level
// returns TRUE if name at view or composite level

boolean hSameTrackDbType(char *type1, char *type2);
/* Compare type strings: require same string unless both are wig tracks. */

typedef struct _sortOrder
// Sort order is used for sorting trackDb entries (hgTrackDb)
// and setting up javascript sorting (hui.c)
    {
    int count;
    char*sortOrder;      // from cart (eg: CEL=+ FAC=- view=-)
    char*htmlId;         // {tableName}.sortOrder
    char**column;        // Always order in trackDb.ra (eg: FAC,CEL,view) TAG
    char**title;         // Always order in trackDb.ra (eg: Factor,Cell Line,View)
    boolean* forward;    // Always order in trackDb.ra but value of cart! (eg: -,+,-)
    int*  order;  // 1 based
    char *setting;
    } sortOrder_t;

sortOrder_t *sortOrderGet(struct cart *cart,struct trackDb *parentTdb);
// Parses any list sort order instructions for parent of subtracks (from cart or trackDb)
// Some trickiness here.  sortOrder->sortOrder is from cart (changed by user action),
// as is sortOrder->order, but columns are in original tdb order (unchanging)!
// However, if cart is null, all is from trackDb.ra */

void sortOrderFree(sortOrder_t **sortOrder);
// frees any previously obtained sortOrder settings

typedef struct _sortColumn
// link list of columns to sort contained in sortableItem
    {
    struct _sortColumn *next;
    char *value;                // value to sort on
    boolean fwd;                // direction
    } sortColumn;

typedef struct _sortableTdbItem
// link list of tdb items to sort
    {
    struct _sortableTdbItem *next;
    struct trackDb *tdb;        // a contained item is actually a tdb entry
    sortColumn *columns;        // a link list of values to sort on
    } sortableTdbItem;

sortableTdbItem *sortableTdbItemCreate(struct trackDb *tdbChild,sortOrder_t *sortOrder);
// creates a sortable tdb item struct, given a child tdb and its parents sort table

void sortTdbItemsAndUpdatePriorities(sortableTdbItem **items);
// sort tdb items in list and then update priorities of item tdbs

void sortableTdbItemsFree(sortableTdbItem **items);
// Frees all memory associated with a list of sortable tdb items

#define FILTER_BY "filterBy"
#define HIGHLIGHT_BY "highlightBy"
typedef struct _filterBy
// A single filterBy set (from trackDb.ra filterBy column:Title=value,value
//                             [column:Title=value|label,value|label,value|label])
    {
    struct _filterBy *next;   // SL list
    char*column;              // field that will be filtered on
    char*title;               // Title that User sees
    char*htmlName;            // Name used in HTML/CGI
    boolean useIndex;         // The returned values should be indexes
    boolean valueAndLabel;    // If values list is value|label, then label is shown to the user
    boolean styleFollows;     // style settings can follow like:
                              //    value|label{background-color:#660000}
    struct slName *slValues;  // Values that can be filtered on (All is always implied)
    struct slName *slChoices; // Values that have been chosen
    } filterBy_t;

filterBy_t *filterBySetGet(struct trackDb *tdb, struct cart *cart, char *name);
// Gets one or more "filterBy" settings (ClosestToHome).  returns NULL if not found

filterBy_t *highlightBySetGet(struct trackDb *tdb, struct cart *cart, char *name);
/* Gets one or more "highlightBy" settings (ClosestToHome).  returns NULL if not found */

void filterBySetFree(filterBy_t **filterBySet);
// Free a set of filterBy structs

char *filterBySetClause(filterBy_t *filterBySet);
// returns the "column1 in (...) and column2 in (...)" clause for a set of filterBy structs

INLINE boolean filterByAllChosen(filterBy_t *filterBy)
/* Is "All" chosen in the filter list? */
{
return ((filterBy->slChoices == NULL) || (slNameInList(filterBy->slChoices,"All")));
}

void filterBySetCfgUi(struct cart *cart, struct trackDb *tdb,
                      filterBy_t *filterBySet, boolean onOneLine, char *prefix);
// Does the UI for a list of filterBy structure

char *filterByClause(filterBy_t *filterBy);
// returns the SQL where clause for a single filterBy struct: "column in (...)"

struct dyString *dyAddFilterByClause(struct cart *cart, struct trackDb *tdb,
                                     struct dyString *extraWhere,char *column, boolean *and);
// creates the where clause condition to support a filterBy setting.
// Format: filterBy column:Title=value,value [column:Title=value|label,value|label,value|label])
// filterBy filters are multiselect's so could have multiple values selected.
// thus returns the "column1 in (...) and column2 in (...)" clause.
// if 'column' is provided, and there are multiple filterBy columns,
// only the named column's clause is returned.
// The 'and' param and dyString in/out allows stringing multiple where clauses together

boolean makeDownloadsLink(char *database, struct trackDb *tdb);
// Make a downloads link (if appropriate and then returns TRUE)

boolean makeSchemaLink(char *db,struct trackDb *tdb,char *label);
// Make a table schema link (if appropriate and then returns TRUE)

void makeTopLink(struct trackDb *tdb);
// Link to top of UI page

void extraUiLinks(char *db,struct trackDb *tdb);
// Show downloads, schema and metadata links where appropriate

boolean chainDbNormScoreAvailable(struct trackDb *tdb);
/*	check if normScore column is specified in trackDb as available */

/* UI for factorSource track type */

/* trackDb settings: */
#define SOURCE_TABLE "sourceTable"

void hPrintAbbreviationTable(struct sqlConnection *conn, char *sourceTable, char *label);
/* Print out table of abbreviations. */

void hPrintFactorSourceAbbrevTable(struct sqlConnection *conn, struct trackDb *tdb);
/* Print out table of abbreviations. With 'pack' setting, 
 * show cell name only (before '+') and uniqify */

// Four State checkboxes can be checked/unchecked by enable/disabled
// NOTE: fourState is not a bitmap because it is manipulated in javascript
//       and int seemed easier at the time
#define FOUR_STATE_UNCHECKED         0
#define FOUR_STATE_CHECKED           1
#define FOUR_STATE_CHECKED_DISABLED  -1
#define fourStateChecked(fourState) \
        ((fourState) == FOUR_STATE_CHECKED || (fourState) == FOUR_STATE_CHECKED_DISABLED)
#define fourStateEnabled(fourState) ((fourState) >= FOUR_STATE_UNCHECKED)
#define fourStateVisible(fourState) ((fourState) == FOUR_STATE_CHECKED)

int subtrackFourStateChecked(struct trackDb *subtrack, struct cart *cart);
// Returns the four state checked state of the subtrack

void subtrackFourStateCheckedSet(struct trackDb *subtrack, struct cart *cart,
                                 boolean checked, boolean enabled);
// Sets the fourState Checked in the cart and updates cached state

boolean hPrintPennantIcon(struct trackDb *tdb);
// Returns TRUE and prints out the "pennantIcon" when found.
// Example: ENCODE tracks in hgTracks config list.

boolean printPennantIconNote(struct trackDb *tdb);
// Returns TRUE and prints out the "pennantIcon" and note when found.
//This is used by hgTrackUi and hgc before printing out trackDb "html"

void cfgByCfgType(eCfgType cType,char *db, struct cart *cart, struct trackDb *tdb,
                  char *prefix, char *title, boolean boxed);
// Methods for putting up type specific cfgs used by composites/subtracks

boolean cfgBeginBoxAndTitle(struct trackDb *tdb, boolean boxed, char *title);
/* Handle start of box and title for individual track type settings */

void cfgEndBox(boolean boxed);
/* Handle end of box and title for individual track type settings */

void printUpdateTime(char *database, struct trackDb *tdb,
    struct customTrack *ct);
/* display table update time, or in case of bbi file, file stat time */

void printBbiUpdateTime(time_t *timep);
/* for bbi files, print out the timep value */

//#define EXTRA_FIELDS_SUPPORT
#ifdef EXTRA_FIELDS_SUPPORT
enum fieldType
    {
    ftString  =0,
    ftInteger =1,
    ftFloat   =2,
    };

struct extraField
// extraFileds are defined in trackDb and provide labls and can be used in filtering
    {
    struct extraField *next;
    char *name;                 // name of field
    char *label;                // Label (which could include HTML)
    enum fieldType type;        // string, int, float
    };

struct extraField *extraFieldsGet(char *db, struct trackDb *tdb);
// returns any extraFields defined in trackDb

struct extraField *extraFieldsFind(struct extraField *extras, char *name);
// returns the extraField matching the name (case insensitive).  Note: slNameFind does NOT work.

void extraFieldsFree(struct extraField **pExtras);
// frees all mem for extraFields list
#endif///def EXTRA_FIELDS_SUPPORT


struct asObject *asFromTableDescriptions(struct sqlConnection *conn, char *table);
// If there is a tableDescriptions table and it has an entry for table, return
// a parsed autoSql object; otherwise return NULL.

struct asObject *asForTdb(struct sqlConnection *conn, struct trackDb *tdb);
// Get autoSQL description if any associated with table, ignoring errAborts if any.

struct asObject *asForDb(struct trackDb *tdb, char* database);
/* return asObject given a database name. NULL if not found */

struct asColumn *asColumnFind(struct asObject *asObj, char *name);
// Return named column.

struct slName *asColNames(struct asObject *as);
// Get list of column names.

/********************/
/* Basic info for controlled vocabulary terms (e.g. to display for composites, or cluster tracks */

char *vocabLink(struct hash *vocabFieldHash, char *term, char *title);
/* Make an anchor with mouseover containing description and link if present */

struct hash *vocabBasicFromSetting(struct trackDb *parentTdb, struct cart *cart);
/* Get description and URL for all vocabTables. Returns a hash of hashes */

boolean vocabSettingIsEncode(char *setting);
/* Distinguish ENCODE controlled vocab settings (first arg is cv.ra filename) from non-ENCODE 
    (table-based vocabs)
*/

boolean isEncode2(char *database, char *track);
/* Return true for tracks created by UCSC DCC during ENCODE production phase */

char *replaceInUrl(char *url, char *idInUrl, struct cart *cart, char *db, char *seqName, 
                        int winStart, int winEnd, char *track, boolean encode, struct slPair *fields) ;
/* replace $$ in url with idInUrl. Supports many other wildchards, and custom fields $<field> */

struct slPair *buildFieldList(struct trackDb *tdb, char *trackDbVar, struct asObject *as);
/* Build up a hash of a list of fields in an AS file. */

char *checkDataVersion(char *database, struct trackDb *tdb);
/* see if trackDb has a dataVersion setting and check that file for version */

void printDataVersion(char *database, struct trackDb *tdb);
/* If this annotation has a dataVersion setting, print it.
 * check hgFixed.trackVersion, meta data and trackDb 'dataVersion'. */

#endif /* HUI_H */
