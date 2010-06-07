/* snp125Ui.c - enums & char arrays for snp UI features and shared util code */
#include "common.h"

#ifndef SNP125UI_H
#define SNP125UI_H

#include "trackDb.h"

char *snp125OrthoTable(struct trackDb *tdb, int *retSpeciesCount);
/* Look for a setting that specifies a table with orthologous alleles.
 * If retSpeciesCount is not null, set it to the number of other species
 * whose alleles are in the table. Do not free the returned string. */

extern boolean snp125ExtendedNames;

enum snp125ColorEnum {
    snp125ColorRed,
    snp125ColorGreen,
    snp125ColorBlue,
    snp125ColorGray,
    snp125ColorBlack,
    snp125ColorExclude
};

extern char *snp125ColorLabel[];

extern int snp125ColorLabelSize;

/****** Color source related controls *******/
/* Molecule Type, Class, Validation, Function */

enum snp125ColorSourceEnum {
    snp125ColorSourceLocType,
    snp125ColorSourceClass,
    snp125ColorSourceValid,
    snp125ColorSourceFunc,
    snp125ColorSourceMolType,
};

extern char *snp125ColorSourceLabels[];
extern char *snp125ColorSourceDataName[];
extern char *snp125ColorSourceDefault[];
extern char *snp125ColorSourceCart[];

extern int snp125ColorSourceLabelsSize;
extern int snp125ColorSourceDataNameSize;
extern int snp125ColorSourceDefaultSize;
extern int snp125ColorSourceCartSize;

extern char *snp128ColorSourceLabels[];
extern int snp128ColorSourceLabelsSize;

/****** MolType related controls *******/
/* unknown, genomic, cDNA */

extern char *snp125MolTypeLabels[];
extern char *snp125MolTypeStrings[];
extern char *snp125MolTypeDataName[];
extern char *snp125MolTypeDefault[];
extern char *snp125MolTypeCart[];
extern char *snp125MolTypeIncludeStrings[];
extern boolean snp125MolTypeIncludeDefault[];
extern boolean snp125MolTypeIncludeCart[];

extern int snp125MolTypeLabelsSize;
extern int snp125MolTypeStringsSize;
extern int snp125MolTypeDataNameSize;
extern int snp125MolTypeDefaultSize;
extern int snp125MolTypeCartSize;
extern int snp125MolTypeIncludeStringsSize;

/****** Class related controls *******/

extern char *snp125ClassLabels[];
extern char *snp125ClassStrings[];
extern char *snp125ClassDataName[];
extern char *snp125ClassDefault[];
extern char *snp125ClassCart[];
extern char *snp125ClassIncludeStrings[];
extern boolean snp125ClassIncludeDefault[];
extern boolean snp125ClassIncludeCart[];

extern int snp125ClassLabelsSize;
extern int snp125ClassStringsSize;
extern int snp125ClassDataNameSize;
extern int snp125ClassDefaultSize;
extern int snp125ClassCartSize;
extern int snp125ClassIncludeStringsSize;

/****** Valid related controls *******/

extern char *snp125ValidLabels[];
extern char *snp125ValidStrings[];
extern char *snp125ValidDataName[];
extern char *snp125ValidDefault[];
extern char *snp125ValidCart[];
extern char *snp125ValidIncludeStrings[];
extern boolean snp125ValidIncludeDefault[];
extern boolean snp125ValidIncludeCart[];

extern int snp125ValidLabelsSize;
extern int snp125ValidStringsSize;
extern int snp125ValidDataNameSize;
extern int snp125ValidDefaultSize;
extern int snp125ValidCartSize;
extern int snp125ValidIncludeStringsSize;

/****** Func related controls *******/

extern char *snp125FuncLabels[];
extern char *snp125FuncStrings[];
extern char *snp125FuncDataName[];
extern char *snp125FuncDefault[];
extern char *snp125FuncCart[];
extern char **snp125FuncDataSynonyms[];
extern char *snp125FuncIncludeStrings[];
extern boolean snp125FuncIncludeDefault[];
extern boolean snp125FuncIncludeCart[];

extern int snp125FuncLabelsSize;
extern int snp125FuncStringsSize;
extern int snp125FuncDataNameSize;
extern int snp125FuncDefaultSize;
extern int snp125FuncCartSize;
extern int snp125FuncIncludeStringsSize;

/****** LocType related controls *******/
/* unknown, range, exact, between,
   rangeInsertion, rangeSubstitution, rangeDeletion */

extern char *snp125LocTypeLabels[];
extern char *snp125LocTypeStrings[];
extern char *snp125LocTypeDataName[];
extern char *snp125LocTypeDefault[];
extern char *snp125LocTypeCart[];
extern char *snp125LocTypeIncludeStrings[];
extern boolean snp125LocTypeIncludeDefault[];
extern boolean snp125LocTypeIncludeCart[];

extern int snp125LocTypeLabelsSize;
extern int snp125LocTypeStringsSize;
extern int snp125LocTypeDataNameSize;
extern int snp125LocTypeDefaultSize;
extern int snp125LocTypeCartSize;
extern int snp125LocTypeIncludeStringsSize;

/* minimum Average Heterozygosity cutoff  */

extern float snp125AvHetCutoff;
extern int snp125WeightCutoff;

#endif /* SNP125UI_H */

