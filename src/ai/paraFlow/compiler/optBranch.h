/* optBranch - optimize branch instructions.  This works on the
 * intermediate isx code.  It replaces jumps to jumps with
 * direct single jumps, and turns conditional jumps followed directly
 * by unconditional jumps into a single conditional jump. */

#ifndef OPTBRANCH_H
#define OPTBRANCH_H


void optBranch(struct dlList *iList);
/* Optimize branch instructions. */

#endif /* OPTBRANCH_H */

