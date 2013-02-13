#ifndef ANNOGRATORGPVAR_H
#define ANNOGRATORGPVAR_H

#include "annoGrator.h"

struct annoGrator *annoGratorGpVarNew(struct annoStreamer *mySource, boolean cdsOnly);
/* Make a subclass of annoGrator that combines genePreds from mySource with
 * pgSnp rows from primary source to predict functional effects of variants
 * on genes.  If cdsOnly is true, return only rows with effects on coding seq.
 * mySource becomes property of the new annoGrator. */

#endif /* ANNOGRATORGPVAR_H*/
