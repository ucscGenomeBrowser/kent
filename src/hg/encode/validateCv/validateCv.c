/* validateCv - validate controlled vocabulary file and metadata. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "validateCv - validate controlled vocabulary file and metadata\n"
  "usage:\n"
  "   validateCv cv.ra\n"
  "options:\n"
  "   -metaTbl=name       specify a metadata table to validate against cv.ra\n"
  "   -setMeta=file.lst   specify a file.lst that has name=val pairs to select\n"
  "                       a subset of metadata to validate\n"
  "   -checkMeta=file.lst specify a file.list that list metavariables to validate\n"
  );
}

char *setMetaName = NULL;
char *checkMetaName = NULL;

static struct optionSpec options[] = {
   {"setMeta", OPTION_STRING},
   {"checkMeta", OPTION_STRING},
   {NULL, 0},
};

void validateCv(char *cvName)
/* validateCv - validate controlled vocabulary file and metadata. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
setMetaName = optionVal("setMeta", setMetaName);
checkMetaName = optionVal("checkMeta", checkMetaName);
validateCv(argv[1]);
return 0;
}
