/* snp125Ui.c - enums & char arrays for snp UI features and shared util code */
#include "common.h"

#ifndef SNP125UI_H
#define SNP125UI_H

#include "cart.h"
#include "trackDb.h"

char *snp125OrthoTable(struct trackDb *tdb, int *retSpeciesCount);
/* Look for a setting that specifies a table with orthologous alleles.
 * If retSpeciesCount is not null, set it to the number of other species
 * whose alleles are in the table. Do not free the returned string. */

struct slName *snp125FilterFromCart(struct cart *cart, char *track, char *attribute,
				    boolean *retFoundInCart);
/* Look up snp125 filter settings in the cart, keeping backwards compatibility with old
 * cart variable names. */

#define SNP125_DEFAULT_MAX_WEIGHT 1
#define SNP125_DEFAULT_MIN_AVHET 0.0

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
extern int snp125ColorArraySize;

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
extern char *snp125ColorSourceVarName;
extern char *snp125ColorSourceDefault;
extern int snp125ColorSourceArraySize;

extern char *snp128ColorSourceLabels[];
extern int snp128ColorSourceArraySize;

/****** MolType related controls *******/
/* unknown, genomic, cDNA */

extern char *snp125MolTypeLabels[];
extern char *snp125MolTypeStrings[];
extern char *snp125MolTypeDataName[];
extern char *snp125MolTypeDefault[];
extern int snp125MolTypeArraySize;

/****** Class related controls *******/

extern char *snp125ClassLabels[];
extern char *snp125ClassStrings[];
extern char *snp125ClassDataName[];
extern char *snp125ClassDefault[];
extern int snp125ClassArraySize;

/****** Valid related controls *******/

extern char *snp125ValidLabels[];
extern char *snp125ValidStrings[];
extern char *snp125ValidDataName[];
extern char *snp125ValidDefault[];
extern int snp125ValidArraySize;

/****** Func related controls *******/

extern char *snp125FuncLabels[];
extern char *snp125FuncStrings[];
extern char *snp125FuncDataName[];
extern char *snp125FuncDefault[];
extern char **snp125FuncDataSynonyms[];
extern int snp125FuncArraySize;

/****** LocType related controls *******/
/* unknown, range, exact, between,
   rangeInsertion, rangeSubstitution, rangeDeletion */

extern char *snp125LocTypeLabels[];
extern char *snp125LocTypeStrings[];
extern char *snp125LocTypeDataName[];
extern char *snp125LocTypeDefault[];
extern int snp125LocTypeArraySize;

#endif /* SNP125UI_H */

