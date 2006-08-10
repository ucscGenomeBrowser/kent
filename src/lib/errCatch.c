/* errCatch - help catch errors so that errAborts aren't
 * fatal, and warn's don't necessarily get printed immediately. 
 * Note that error conditions caught this way will tend to
 * leak resources unless there are additional wrappers. 
 *
 * Typical usage is
 * errCatch = errCatchNew();
 * if (errCatchStart(errCatch))
 *     doFlakyStuff();
 * errCatchEnd(errCatch);
 * if (errCatch->gotError)
 *     warn(errCatch->message->string);
 * errCatchFree(&errCatch); 
 * cleanupFlakyStuff();
 */

#include "common.h"
#include "errabort.h"
#include "dystring.h"
#include "errCatch.h"

static char const rcsid[] = "$Id: errCatch.c,v 1.2 2006/08/10 01:02:47 kent Exp $";


struct errCatch *errCatchNew()
/* Return new error catching structure. */
{
struct errCatch *errCatch;
AllocVar(errCatch);
errCatch->message = dyStringNew(0);
return errCatch;
}

void errCatchFree(struct errCatch **pErrCatch)
/* Free up resources associated with errCatch */
{
struct errCatch *errCatch = *pErrCatch;
if (errCatch != NULL)
    {
    dyStringFree(&errCatch->message);
    freez(pErrCatch);
    }
}

static struct errCatch *errCatchStack = NULL;

static void errCatchAbortHandler()
/* semiAbort */
{
errCatchStack->gotError = TRUE;
longjmp(errCatchStack->jmpBuf, -1);
}

static void errCatchWarnHandler(char *format, va_list args)
/* Write an error to top of errCatchStack. */
{
dyStringVaPrintf(errCatchStack->message, format, args);
dyStringAppendC(errCatchStack->message, '\n');
}

boolean errCatchPushHandlers(struct errCatch *errCatch)
/* Push error handlers.  Not usually called directly. */
{
pushAbortHandler(errCatchAbortHandler);
pushWarnHandler(errCatchWarnHandler);
slAddHead(&errCatchStack, errCatch);
return TRUE;
}

void errCatchEnd(struct errCatch *errCatch)
/* Restore error handlers and pop self off of catching stack. */
{
popWarnHandler();
popAbortHandler();
if (errCatch != errCatchStack)
   errAbort("Mismatch betweene errCatch and errCatchStack");
errCatchStack = errCatch->next;
}

boolean errCatchFinish(struct errCatch **pErrCatch)
/* Finish up error catching.  Report error if there is a
 * problem and return FALSE.  If no problem return TRUE.
 * This handles errCatchEnd and errCatchFree. */
{
struct errCatch *errCatch = *pErrCatch;
boolean ok = TRUE;
if (errCatch != NULL)
    {
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	{
	ok = FALSE;
	warn(errCatch->message->string);
	}
    errCatchFree(pErrCatch);
    }
return ok;
}

