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

char *snp125OldColorVarToNew(char *oldVar, char *attribute);
/* Abbreviate an old cart var name -- new name is based on track plus this. Don't free result. */

#define SNP125_DEFAULT_MAX_WEIGHT 1
#define SNP125_DEFAULT_MIN_AVHET 0.0
#define SNP132_DEFAULT_MIN_SUBMITTERS 0
#define SNP132_DEFAULT_MIN_MINOR_AL_FREQ 0.0
#define SNP132_DEFAULT_MAX_MINOR_AL_FREQ 0.5
#define SNP132_DEFAULT_MIN_AL_FREQ_2N 0

extern boolean snp125ExtendedNames;

enum snp125Color {
    snp125ColorRed,
    snp125ColorGreen,
    snp125ColorBlue,
    snp125ColorGray,
    snp125ColorBlack,
};

extern char *snp125ColorLabel[];
extern int snp125ColorArraySize;

/****** Color source related controls *******/
/* Molecule Type, Class, Validation, Function */

enum snp125ColorSource {
    snp125ColorSourceLocType,
    snp125ColorSourceClass,
    snp125ColorSourceValid,
    snp125ColorSourceFunc,
    snp125ColorSourceMolType,
    snp125ColorSourceExceptions,
    snp125ColorSourceBitfields,
    snp125ColorSourceAlleleFreq,
};

#define SNP125_DEFAULT_COLOR_SOURCE snp125ColorSourceFunc

enum snp125ColorSource snp125ColorSourceFromCart(struct cart *cart, struct trackDb *tdb);
/* Look up color source in cart, keeping backwards compatibility with old cart var names. */

char *snp125ColorSourceToLabel(struct trackDb *tdb, enum snp125ColorSource cs);
/* Due to availability of different color sources in several different versions,
 * this is not just an array lookup, hence the encapsulation. Don't modify return value. */

extern char *snp125ColorSourceLabels[];
extern char *snp125ColorSourceOldVar;
extern int snp125ColorSourceArraySize;

extern char *snp128ColorSourceLabels[];
extern int snp128ColorSourceArraySize;

extern char *snp132ColorSourceLabels[];
extern int snp132ColorSourceArraySize;

/****** MolType related controls *******/
/* unknown, genomic, cDNA */

extern char *snp125MolTypeLabels[];
extern char *snp125MolTypeOldColorVars[];
extern char *snp125MolTypeDataName[];
extern char *snp125MolTypeDefault[];
extern int snp125MolTypeArraySize;

/****** Class related controls *******/

extern char *snp125ClassLabels[];
extern char *snp125ClassOldColorVars[];
extern char *snp125ClassDataName[];
extern char *snp125ClassDefault[];
extern int snp125ClassArraySize;

/****** Valid related controls *******/

extern char *snp125ValidLabels[];
extern char *snp125ValidOldColorVars[];
extern char *snp125ValidDataName[];
extern char *snp125ValidDefault[];
extern int snp125ValidArraySize;

/****** Func related controls *******/

extern char *snp125FuncLabels[];
extern char *snp125FuncOldColorVars[];
extern char *snp125FuncDataName[];
extern char *snp125FuncDefault[];
extern char **snp125FuncDataSynonyms[];
extern int snp125FuncArraySize;

/****** LocType related controls *******/
/* unknown, range, exact, between,
   rangeInsertion, rangeSubstitution, rangeDeletion */

extern char *snp125LocTypeLabels[];
extern char *snp125LocTypeOldColorVars[];
extern char *snp125LocTypeDataName[];
extern char *snp125LocTypeDefault[];
extern int snp125LocTypeArraySize;

/****** Exception related controls *******/

extern char *snp132ExceptionLabels[];
extern char *snp132ExceptionVarName[];
extern char *snp132ExceptionDefault[];
extern int snp132ExceptionArraySize;

/****** Miscellaneous attributes (bitfields) related controls *******/

extern char *snp132BitfieldLabels[];
extern char *snp132BitfieldVarName[];
extern char *snp132BitfieldDataName[];
extern char *snp132BitfieldDefault[];
extern int snp132BitfieldArraySize;

#endif /* SNP125UI_H */

