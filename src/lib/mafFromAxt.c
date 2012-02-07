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

struct mafAli *mafFromAxt(struct axt *axt, int tSize, 
	char *tPrefix, int qSize, char *qPrefix)
/* Make up a maf file from axt.  Slower than mafFromAxtTemp,
 * but the axt and maf are independent afterwards. */
{
struct mafAli *maf;
struct mafComp *mc;
char name[256];
AllocVar(maf);
maf->score = axt->score;
maf->textSize = axt->symCount;
AllocVar(mc);
if (qPrefix == NULL)
    mc->src = cloneString(axt->qName);
else
    {
    safef(name, sizeof(name), "%s.%s", qPrefix, axt->qName);
    mc->src = cloneString(name);
    }
mc->srcSize = qSize;
mc->strand = axt->qStrand;
mc->start = axt->qStart;
mc->size = axt->qEnd - axt->qStart;
mc->text = cloneStringZ(axt->qSym, axt->symCount);
slAddHead(&maf->components, mc);
AllocVar(mc);
if (tPrefix == NULL)
    mc->src = cloneString(axt->tName);
else
    {
    safef(name, sizeof(name), "%s.%s", tPrefix, axt->tName);
    mc->src = cloneString(name);
    }
mc->srcSize = tSize;
mc->strand = axt->tStrand;
mc->start = axt->tStart;
mc->size = axt->tEnd - axt->tStart;
mc->text = cloneStringZ(axt->tSym, axt->symCount);
slAddHead(&maf->components, mc);
return maf;
}
