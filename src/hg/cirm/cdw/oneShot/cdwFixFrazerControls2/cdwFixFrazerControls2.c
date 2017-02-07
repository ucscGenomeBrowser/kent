/* cdwFixFrazerControls2 - Fix controls in a Frazer lab tagStorm.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwFixFrazerControls2 - Fix controls in a Frazer lab tagStorm.\n"
  "usage:\n"
  "   cdwFixFrazerControls2 in.tags out.tags\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

boolean doFixControl(struct tagStorm *ts, struct tagStanza *stanza)
/* Do fix on control_association and return TRUE if did it. */
{
char *sample_label = tagFindLocalVal(stanza, "sample_label");
if (sample_label != NULL)
    {
    char *assay = tagFindVal(stanza, "assay");
    if (assay != NULL && sameWord(assay, "ChIPH3K27AC"))
	{
	if (sameWord(sample_label, "iPSC-CM"))
	    {
	    tagStanzaAppend(ts, stanza, "control_association", "iPSC_CM_ChIP_input");
	    return 1;
	    }
	else if (sameWord(sample_label, "iPSC"))
	    {
	    tagStanzaAppend(ts, stanza, "control_association", "iPSC_ChIP_input");
	    return 1;
	    }
	}
    }
return FALSE;
}

boolean doFixAssay(struct tagStorm *ts, struct tagStanza *stanza)
/* Do fix on control_association and return TRUE if did it. */
{
char *assay = tagFindLocalVal(stanza, "assay");
if (assay != NULL && sameWord(assay, "ChIPH3K27AC"))
    {
    tagStormUpdateTag(ts, stanza, "assay", "broad-ChIP-seq");
    tagStanzaAppend(ts, stanza, "target_epitope", "H3K27Ac");
    return TRUE;
    }
return FALSE;
}

static void rFixControl(struct tagStorm *ts, struct tagStanza *list, int *pCount)
/* Return number of tags in storm */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (doFixControl(ts, stanza))
       ++(*pCount);
    rFixControl(ts, stanza->children, pCount);
    }
}

static void rFixAssay(struct tagStorm *ts, struct tagStanza *list, int *pCount)
/* Return number of tags in storm */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (doFixAssay(ts, stanza))
       ++(*pCount);
    rFixAssay(ts, stanza->children, pCount);
    }
}


void cdwFixFrazerControls2(char *inTags, char *outTags)
/* cdwFixFrazerControls2 - Fix controls in a Frazer lab tagStorm.. */
{
struct tagStorm *ts = tagStormFromFile(inTags);
int controlCount = 0, assayCount = 0;
rFixControl(ts, ts->forest, &controlCount);
rFixAssay(ts, ts->forest, &assayCount);
verbose(1, "Fixed %d controls and %d assays\n", controlCount, assayCount);
tagStormWrite(ts, outTags, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
cdwFixFrazerControls2(argv[1], argv[2]);
return 0;
}
