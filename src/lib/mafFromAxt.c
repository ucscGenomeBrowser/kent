/* mafFromAxt - convert a axt into maf. */
#include "common.h"
#include "axt.h"
#include "maf.h"

void mafFromAxtTemp(struct axt *axt, int tSize, int qSize,
	struct mafAli *temp)
/* Make a maf out of axt,  parasiting on the memory in axt.
 * Do *not* mafFree this temp.  The memory it has in pointers
 * is still owned by the axt.  Furthermore the next call to
 * this function will invalidate the previous temp value.
 * It's sort of a kludge, but quick to run and easy to implement. */
{
static struct mafComp qComp, tComp;
ZeroVar(temp);
ZeroVar(&qComp);
ZeroVar(&tComp);
temp->score = axt->score;
temp->textSize = axt->symCount;
qComp.src = axt->qName;
qComp.srcSize = qSize;
qComp.strand = axt->qStrand;
qComp.start = axt->qStart;
qComp.size = axt->qEnd - axt->qStart;
qComp.text = axt->qSym;
slAddHead(&temp->components, &qComp);
tComp.src = axt->tName;
tComp.srcSize = tSize;
tComp.strand = axt->tStrand;
tComp.start = axt->tStart;
tComp.size = axt->tEnd - axt->tStart;
tComp.text = axt->tSym;
slAddHead(&temp->components, &tComp);
}

