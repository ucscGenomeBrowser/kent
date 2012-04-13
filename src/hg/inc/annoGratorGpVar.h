#ifndef ANNOGRATORGPVAR_H
#define ANNOGRATORGPVAR_H

#include "annoGrator.h"


struct annoGrator *annoGratorGpVarNew(struct annoStreamer *mySource);
/* Make a new integrator of columns from mySource with (positions of) rows passed to integrate().
 * mySource becomes property of the new annoGrator. */

#endif /* ANNOGRATORGPVAR_H*/
