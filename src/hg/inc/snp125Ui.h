/* snp125Ui.h - enums and char arrays for snp125 UI features */
#ifndef SNP125UI_H
#define SNP125UI_H

enum snp125ColorEnum {
    snp125ColorRed,
    snp125ColorGreen,
    snp125ColorBlue,
    snp125ColorBlack,
    snp125ColorExclude
};

extern char *snp125ColorLabel[];

extern int snp125ColorLabelSize;

/****** Color source related controls *******/
/* Molecule Type, Class, Validation, Function */

enum snp125ColorSourceEnum {
    snp125ColorSourceMolType,
    snp125ColorSourceClass,
    snp125ColorSourceValid,
    snp125ColorSourceFunc,
};

extern char *snp125ColorSourceLabels[];
extern char *snp125ColorSourceStrings[];
extern char *snp125ColorSourceDataName[];
extern char *snp125ColorSourceDefault[];
extern char *snp125ColorSourceCart[];

extern int snp125ColorSourceLabelsSize;
extern int snp125ColorSourceStringsSize;
extern int snp125ColorSourceDataNameSize;
extern int snp125ColorSourceDefaultSize;
extern int snp125ColorSourceCartSize;

/****** MolType related controls *******/
/* unknown, genomic, cDNA */

extern char *snp125MolTypeLabels[];
extern char *snp125MolTypeStrings[];
extern char *snp125MolTypeDataName[];
extern char *snp125MolTypeDefault[];
extern char *snp125MolTypeCart[];

extern int snp125MolTypeLabelsSize;
extern int snp125MolTypeStringsSize;
extern int snp125MolTypeDataNameSize;
extern int snp125MolTypeDefaultSize;
extern int snp125MolTypeCartSize;

/****** Class related controls *******/

extern char *snp125ClassLabels[];
extern char *snp125ClassStrings[];
extern char *snp125ClassDataName[];
extern char *snp125ClassDefault[];
extern char *snp125ClassCart[];

extern int snp125ClassLabelsSize;
extern int snp125ClassStringsSize;
extern int snp125ClassDataNameSize;
extern int snp125ClassDefaultSize;
extern int snp125ClassCartSize;

/****** Valid related controls *******/

extern char *snp125ValidLabels[];
extern char *snp125ValidStrings[];
extern char *snp125ValidDataName[];
extern char *snp125ValidDefault[];
extern char *snp125ValidCart[];

extern int snp125ValidLabelsSize;
extern int snp125ValidStringsSize;
extern int snp125ValidDataNameSize;
extern int snp125ValidDefaultSize;
extern int snp125ValidCartSize;

/****** Func related controls *******/

extern char *snp125FuncLabels[];
extern char *snp125FuncStrings[];
extern char *snp125FuncDataName[];
extern char *snp125FuncDefault[];
extern char *snp125FuncCart[];

extern int snp125FuncLabelsSize;
extern int snp125FuncStringsSize;
extern int snp125FuncDataNameSize;
extern int snp125FuncDefaultSize;
extern int snp125FuncCartSize;

/* minimum Average Heterozygosity cutoff  */

extern float snp125AvHetCutoff;

#endif /* SNP125UI_H */

