/* runErr - handle run-time errors (and try/catch). */

#ifndef RUNERR_H
#define RUNERR_H

#include <setjmp.h>
#define boolean int
#include "../../../inc/errCatch.h"

void _pf_run_err(char *format, ...);
/* Run time error.  Prints message, dumps stack, and aborts. */


typedef struct errCatch *_pf_Err_catch;
#define _pf_err_catch_new errCatchNew
#define _pf_err_catch_start errCatchStart
#define _pf_err_catch_end errCatchEnd
#define _pf_err_catch_free errCatchFree
#define _pf_err_catch_got_err(e) (e->gotError)

void _pf_err_check_level_and_unwind(_pf_Err_catch err, int level,
	struct _pf_activation *curActivation);
/* Check level against err->level.  If err is deeper then we
 * just throw to next level handler if any.  Otherwise we
 * unwind stack and return. */

#endif /* RUNERR_H */
