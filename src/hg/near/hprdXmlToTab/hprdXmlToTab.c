/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hprd.h"

void usage(char *self)
/* show usage */
{
errAbort("Syntax error. Correct usage:\n%s input.xml output.P2P.tab complex.tab\n",self);
}

int main(int argc, char *argv[])
{
if (argc != 4)
    usage(argv[0]);

struct hprdEntrySet *entrySet = hprdEntrySetLoad(argv[1]);
struct hprdInteraction *i=NULL, *iL = entrySet->hprdEntry->hprdInteractionList->hprdInteraction;
int iCount = 0;
FILE *fp = mustOpen(argv[2], "w");
FILE *fc = mustOpen(argv[3], "w");

//uglyf("interactionList count = %d\n", slCount(iList));

for(i=iL;i;i=i->next)
    {
    ++iCount;
    struct hprdParticipant *q=NULL, *p=NULL, *pL = i->hprdParticipantList->hprdParticipant;
    int pCount = slCount(pL);
    if (pCount < 2)
	{
	warn("participant count=%d which is < 2 for participant id = %s\n", pCount, pL->id);
	}
    else
	{

	double distance = pCount==2 ? 1.0 : 1.5;
	for(p=pL;p;p=p->next)
	    for(q=p->next;q;q=q->next)
		fprintf(fp, "%s\t%s\t%f\n", 
		    p->hprdInteractorRef->text, 
		    q->hprdInteractorRef->text,
		    distance);
	if (pCount > 2)  /* complex */
	    for(p=pL;p;p=p->next)
		{
		fprintf(fc,"%d\t%s\n", iCount, p->hprdInteractorRef->text );
		}
	    
	}
    }


uglyf("interaction count = %d\n", iCount);

carefulClose(&fp);
carefulClose(&fc);

hprdEntrySetFree(&entrySet);



return 0;
}

