/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "ra.h"
#include "basicBed.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen val desiredVal\n");
}


char *fields[] = {
//id
//updateTime
//series
//accession
"organism",
"lab",
"dataType",
"cellType",
#ifdef SOON
"ab",
"age",
"attic",
"category",
"control",
"fragSize",
"grantee",
"insertLength",
"localization",
"mapAlgorithm",
"objStatus",
"phase",
"platform",
"promoter",
"protocol",
"readType",
"region",
"restrictionEnzyme",
"rnaExtract",
"seqPlatform",
"sex",
"strain",
"tissueSourceType",
"treatment",
#endif /* SOON */
};

void freen(char *input)
/* Test some hair-brained thing. */
{
/* Make huge sql query */
printf("select e.id,updateTime,series,accession,version");
int i;
for (i=0; i<ArraySize(fields); ++i)
    printf(",cvDb_%s.tag %s", fields[i], fields[i]);
printf("\n");
printf("from cvDb_experiment e");
for (i=0; i<ArraySize(fields); ++i)
    printf(",cvDb_%s", fields[i]);
printf("\n");
printf("where ");
for (i=0; i<ArraySize(fields); ++i)
    {
    if (i != 0)
        printf(" and ");
    printf("cvDb_%s.id = e.%s\n", fields[i], fields[i]);
    }
printf("\n");
printf("limit 10\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
