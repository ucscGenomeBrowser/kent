/* snpUi.h - enums and char arrays for snp UI features */

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef SNPUI_H
#define SNPUI_H

/****** Some stuff for snp colors *******/

enum snpColorEnum {
    snpColorRed,
    snpColorGreen,
    snpColorBlue,
    snpColorBlack,
    snpColorExclude
};

extern char *snpColorLabel[];

extern int snpColorLabelSize;

/****** Some stuff for snpMapSource related controls *******/
/* dbSnp, Affy10K, Affy10Kv2, Affy50K_HindIII, Affy50K_XbaI */

extern char *snpMapSourceLabels[];
extern char *snpMapSourceStrings[];
extern char *snpMapSourceDataName[];
extern char *snpMapSourceDefault[];
extern char *snpMapSourceCart[];

extern int snpMapSourceLabelsSize;
extern int snpMapSourceStringsSize;
extern int snpMapSourceDataNameSize;
extern int snpMapSourceDefaultSize;
extern int snpMapSourceCartSize;

/****** Some stuff for snpMapType related controls *******/
/* SingleNP, indel, segnemtal */

enum snpMapTypeEnum {
    snpMapTypeInclude,
    snpMapTypeExclude
};

extern char *snpMapTypeLabel[];
extern char *snpMapTypeLabels[];
extern char *snpMapTypeStrings[];
extern char *snpMapTypeDataName[];
extern char *snpMapTypeDefault[];
extern char *snpMapTypeCart[];

extern int snpMapTypeLabelSize;
extern int snpMapTypeLabelsSize;
extern int snpMapTypeStringsSize;
extern int snpMapTypeDataNameSize;
extern int snpMapTypeDefaultSize;
extern int snpMapTypeCartSize;

/****** Some stuff for snpColorSource related controls *******/
/* Source, Molecule Type, Class, Validation, Function */

enum snpColorSourceEnum {
    snpColorSourceSource,
    snpColorSourceMolType,
    snpColorSourceClass,
    snpColorSourceValid,
    snpColorSourceFunc,
    snpColorSourceLocType,
    snpColorSourceBlack
};

extern char *snpColorSourceLabels[];
extern char *snpColorSourceStrings[];
extern char *snpColorSourceDataName[];
extern char *snpColorSourceDefault[];
extern char *snpColorSourceCart[];

extern int snpColorSourceLabelsSize;
extern int snpColorSourceStringsSize;
extern int snpColorSourceDataNameSize;
extern int snpColorSourceDefaultSize;
extern int snpColorSourceCartSize;

/****** Some stuff for snpSource related controls *******/
/* dbSnp, Affy10K, Affy10Kv2, Affy50K_HindIII, Affy50K_XbaI */

extern char *snpSourceLabels[];
extern char *snpSourceStrings[];
extern char *snpSourceDataName[];
extern char *snpSourceDefault[];
extern char *snpSourceCart[];

extern int snpSourceLabelsSize;
extern int snpSourceStringsSize;
extern int snpSourceDataNameSize;
extern int snpSourceDefaultSize;
extern int snpSourceCartSize;

/****** Some stuff for snpMolType related controls *******/
/* unknown, genomic, cDNA, mito, chloro */

extern char *snpMolTypeLabels[];
extern char *snpMolTypeStrings[];
extern char *snpMolTypeDataName[];
extern char *snpMolTypeDefault[];
extern char *snpMolTypeCart[];

extern int snpMolTypeLabelsSize;
extern int snpMolTypeStringsSize;
extern int snpMolTypeDataNameSize;
extern int snpMolTypeDefaultSize;
extern int snpMolTypeCartSize;

/****** Some stuff for snpClass related controls *******/
/* unknown, snp, in-del, het, microsat, named, no-variation, mixed,
 * mnp */

extern char *snpClassLabels[];
extern char *snpClassStrings[];
extern char *snpClassDataName[];
extern char *snpClassDefault[];
extern char *snpClassCart[];

extern int snpClassLabelsSize;
extern int snpClassStringsSize;
extern int snpClassDataNameSize;
extern int snpClassDefaultSize;
extern int snpClassCartSize;

/****** Some stuff for snpValid related controls *******/
/* unknown, other-pop, by-frequency, by-cluster, by-2hit-2allele,
 * by-hapmap, genotype */

extern char *snpValidLabels[];
extern char *snpValidStrings[];
extern char *snpValidDataName[];
extern char *snpValidDefault[];
extern char *snpValidCart[];

extern int snpValidLabelsSize;
extern int snpValidStringsSize;
extern int snpValidDataNameSize;
extern int snpValidDefaultSize;
extern int snpValidCartSize;

/****** Some stuff for snpFunc related controls *******/
/* unknown, locus-region, coding, coding-synon, coding-nonsynon,
 * mrna-utr, intron, splice-site, reference, exception */

extern char *snpFuncLabels[];
extern char *snpFuncStrings[];
extern char *snpFuncDataName[];
extern char *snpFuncDefault[];
extern char *snpFuncCart[];

extern int snpFuncLabelsSize;
extern int snpFuncStringsSize;
extern int snpFuncDataNameSize;
extern int snpFuncDefaultSize;
extern int snpFuncCartSize;

/****** Some stuff for snpLocType related controls *******/
/* unknown, range, exact, between */

extern char *snpLocTypeLabels[];
extern char *snpLocTypeStrings[];
extern char *snpLocTypeDataName[];
extern char *snpLocTypeDefault[];
extern char *snpLocTypeCart[];

extern int snpLocTypeLabelsSize;
extern int snpLocTypeStringsSize;
extern int snpLocTypeDataNameSize;
extern int snpLocTypeDefaultSize;
extern int snpLocTypeCartSize;

/****** Some stuff for snpLocType related controls *******/
/* minimum Average Heterozygosity cutoff  */

extern float snpAvHetCutoff;

#endif /* SNPUI_H */

