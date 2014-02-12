/* eapFreen - Little program to help test things and figure them out.  It has no permanent one fixed purpose.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "../../encodeDataWarehouse/inc/encodeDataWarehouse.h"
#include "../../encodeDataWarehouse/inc/edwLib.h"
#include "eapDb.h"
#include "eapLib.h"
#include "eapGraph.h"
#include "longList.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "eapFreen - Little program to help test things and figure them out.  It has no permanent one fixed purpose.\n"
  "usage:\n"
  "   eapFreen output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void deprecateRun(struct eapGraph *eg, struct eapRun *run, FILE *f, 
    struct edwValidFile *v1, struct edwValidFile *v2)
/* Write code to f to deprecate run. */
{
struct slRef *ref, *refList = eapGraphRunOutputs(eg, run->id);
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct eapOutput *out = ref->val;
    fprintf(f, "# Fail on rep '%s' vs '%s'\n", v1->replicate, v2->replicate);
    fprintf(f, "update edwFile set deprecated='Analysis run on wrong inputs.' where id = %u;\n", 
	out->fileId);
    }
}

void embraceRun(struct eapGraph *eg, struct eapRun *run, FILE *f,
    struct edwValidFile *v1, struct edwValidFile *v2)
/* Write comment saying how good run was. */
{
struct slRef *ref, *refList = eapGraphRunOutputs(eg, run->id);
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct eapOutput *out = ref->val;
    struct edwValidFile *vf = eapGraphValidFromId(eg, out->fileId);
    fprintf(f, "# %u %s seems to originate from two replicates '%s' and '%s'\n", 
	vf->fileId, vf->licensePlate, v1->replicate, v2->replicate);
    }
}

void eapFreen(char *output)
/* eapFreen - Little program to help test things and figure them out.  It has no permanent one fixed purpose.. */
{
/* Load up our nice random access graph structure */
struct sqlConnection *conn = eapConnect();
struct eapGraph *eg = eapGraphNew(conn);
FILE *f = mustOpen(output, "w");

/* Get list of all likely runs that might have output we want */
char query[512];
sqlSafef(query, sizeof(query), 
    "select * from eapRun where analysisStep='replicated_hotspot' "
    " and assemblyId in (4,5) "
    " and createStatus > 0 ");
struct eapRun *run, *runList = eapRunLoadByQuery(conn, query);

/* Try and figure out if input to run is appropriate - from two different replicates of same
 * experiment. */
uglyf("Total of %d runs\n", slCount(runList));
for (run = runList; run != NULL; run = run->next)
    {
    struct slRef *ref2, *ref1 = eapGraphRunInputs(eg, run->id);
    int refCount = slCount(ref1);
    assert(refCount == 2);

    ref2 = ref1->next;
    struct eapInput *in1 = ref1->val, *in2 = ref2->val;
    struct edwValidFile *v1 = eapGraphValidFromId(eg, in1->fileId);
    struct edwValidFile *v2 = eapGraphValidFromId(eg, in2->fileId);
    if (sameString(v1->replicate, v2->replicate))
        {
	// Replicates same, no point, we should deprecate
	deprecateRun(eg, run, f, v1, v2);
	}
    else
        {
	if (isEmpty(v1->replicate) || isEmpty(v2->replicate))
	    {
	    // Both sides need to be replicates
	    deprecateRun(eg, run, f, v1, v2);
	    }
	else
	    {
	    // Got good one, two different real replicates
	    embraceRun(eg, run, f, v1, v2);
	    }
	}
    }


eapGraphFree(&eg);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
eapFreen(argv[1]);
return 0;
}
