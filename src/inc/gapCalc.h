/* gapCalc - Stuff to calculate complex (but linear) gap costs quickly,
 * and read specifications from a file. */

#ifndef GAPCALC_H
#define GAPCALC_H

    
struct gapCalc;	  /* Predeclare structure.  It's complex and private though, so
                   * real declaration is in gapCalc.c. */

struct gapCalc *gapCalcDefault();
/* Return default gapCalc. */

struct gapCalc *gapCalcRnaDna();
/* Return gaps suitable for RNA queries vs. DNA targets */

struct gapCalc *gapCalcCheap();
/* Return cheap gap costs. */

struct gapCalc *gapCalcOriginal();
/* Return gap costs from original paper. */

struct gapCalc *gapCalcFromFile(char *fileName);
/* Return gapCalc from file. */

struct gapCalc *gapCalcFromString(char *s);
/* Return gapCalc from description string. */

struct gapCalc *gapCalcRead(struct lineFile *lf);
/* Create gapCalc from open file. */

void gapCalcFree(struct gapCalc **pGapCalc);
/* Free up resources associated with gapCalc. */

int gapCalcCost(struct gapCalc *gapCalc, int dq, int dt);
/* Figure out gap costs. */

char *gapCalcSampleFileContents();
/* Return contents of a sample linear gap file. */

void gapCalcTest(struct gapCalc *gapCalc);
/* Print out gap cost info. */

#endif /* GAPCALC_H */
