/* encodePatchTdb - Lay a trackDb.ra file from the pipeline gently on top of the trackDb system. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "options.h"
#include "obscure.h"
#include "errabort.h"
#include "dystring.h"
#include "portable.h"
#include "ra.h"
#include "../inc/encodePatchTdb.h"

char *clMode = "add";
extern char *clTest;
extern boolean clNoComment;
extern boolean glReplace;	// If TRUE then do a replacement operation.

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encodePatchTdb - Lay a trackDb.ra file from the pipeline gently on top of the trackDb system\n"
  "usage:\n"
  "   encodePatchTdb patchFile.ra fileToChange.ra\n"
  "Example:\n"
  "   encodePatchTdb 849/out/trackDb.ra ~/kent/src/makeDb/trackDb/human/hg18/trackDb.wgEncode.ra\n"
  "options:\n"
  "   -mode=mode (default %s).  Operate in one of the following modes\n"
  "         replace - replace existing records rather than doing field by field update.\n"
  "                   Leaves existing record commented out.\n"
  "         add - add new records at end of parent's subtrack list. Complain if record isn't new\n"
  "               warn if it's a new track rather than just new subtracks\n"
  "   -noComment - If set will not leave old record commented out\n"
  "   -test=patchFile - rather than doing patches in place, write patched output to this file\n"
  , clMode
  );
}

static struct optionSpec options[] = {
   {"mode", OPTION_STRING},
   {"test", OPTION_STRING},
   {"noComment", OPTION_BOOLEAN},
   {"root", OPTION_STRING},
   {NULL, 0},
};


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clMode = optionVal("mode", clMode);
clTest = optionVal("test", clTest);
clNoComment = optionExists("noComment");
if (sameString(clMode, "add"))
    glReplace = FALSE;
else if (sameString(clMode, "replace"))
    glReplace = TRUE;
else
    errAbort("unrecognized mode %s", clMode);
encodePatchTdb(argv[1], argv[2]);
return 0;
}
