#include "common.h"
#include "ggPrivate.h"
#include "jksql.h"


static struct ggAliInfo *makeTestDa(int *data, int dataSize)
/* Make up ggAliInfo from test data. */
{
int vertexCount;
struct ggAliInfo *da;
struct ggVertex *v;
int i;

vertexCount = dataSize;
AllocVar(da);
da->vertexCount = vertexCount;
da->vertices = AllocArray(v, vertexCount);
for (i=0; i<dataSize; i += 1 )
    {
    v->position = *data++;
    if (i == 0)
	v->type = ggSoftStart;
    else if (i == dataSize-1)
	v->type = ggSoftEnd;
    else if (i&1)
	v->type = ggHardEnd;
    else
	v->type = ggHardStart;
    ++v;
    }
return da;
}

#define RAY_SET

#ifdef JIM_SET
int rna1[] = {50, 100, 1000, 1150};
int rna2[] = {25, 100, 1000, 1150};
int rna3[] = {50, 100, 1000, 1200};
int rna4[] = {50,  75, 1000, 1150};
int rna5[] = {50, 100, 1050, 1150};
int rna6[] = {50, 100, 500, 600, 1000, 1150};
int rna7[] = {1100, 1250, 1300, 1400};
int rna8[] = {1075, 1175, 1300, 1400};
int rna9[] = {1025, 1175, 1300, 1400};
int rna10[] = {50, 100, 500, 602,};
int rna11[] = {498, 600, 1000, 1150};
#endif

#ifdef RAY_SET
int rna1[] = {0, 100, 150, 500};
int rna2[] = {50, 100, 150, 200, 250, 550};
int rna3[] = {300, 700, 750, 800};
int rna4[] = {350, 600, 650, 700, 750, 850};
#endif

#ifdef SIMPLE_SET
//int rna1[] = {0, 100,};
//int rna2[] = {50, 150,};
int rna1[] = {110,889, 1045,1118, }; 
int rna2[] = {111,508}; 
#endif

#ifdef BIG_SET
int rna1[] = {109,889, 1045,1118, 1210,1275, 1449,1618, 1703,1736, 4705,4776, 4868,4943, 5111,5283, 5425,5538, 5734,5859, 6073,6157, 6850,6999, 7085,7225, 7526,7612, 7708,7786}; 
int rna2[] = {110,889, 1045,1118, 1210,1275, 1449,1618, 1703,1736, 4705,4776, 4868,4943, 5111,5283, 5425,5538, 5734,5859, 6073,6157, 6850,6999, 7085,7225, 7526,7612, 7708,7809, 8228,8351, 10897,11011, 11410,11485, 12838,12946, 13030,13068, 13213,13340, 13502,13598, 13960,14007, 19852,20013}; 
int rna3[] = {111,508}; 
int rna4[] = {115,397}; 
int rna5[] = {1096,1118, 1210,1275, 1449,1618, 1703,1736, 4705,4749}; 
int rna6[] = {1459,1618, 1703,1736, 4705,4776, 4868,4914}; 
int rna7[] = {1499,1618, 1703,1737}; 
int rna8[] = {1502,1618, 1703,1736, 4705,4776, 4868,4937}; 
int rna9[] = {1539,1618, 1703,1736, 4705,4776, 4868,4914}; 
int rna10[] = {4866,4943, 5111,5283, 5425,5485}; 
int rna11[] = {5146,5283, 5425,5528}; 
int rna12[] = {5207,5283, 5425,5538, 5734,5781}; 
int rna13[] = {5257,5283, 5425,5538, 5734,5859, 6073,6119}; 
int rna14[] = {5465,5538, 5734,5859, 6073,6117}; 
int rna15[] = {5772,5859, 6073,6157, 6850,6999, 7085,7225, 7526,7567}; 
int rna16[] = {6092,6157, 6850,6999, 7085,7176}; 
int rna17[] = {6946,6999, 7085,7225, 7526,7612, 7708,7809, 8228,8351, 8909,8972}; 
int rna18[] = {7111,7225, 7526,7612, 7708,7786}; 
int rna19[] = {7199,7225, 7526,7612, 7722,7807}; 
int rna20[] = {7537,7612, 7708,7809, 8228,8321}; 
int rna21[] = {11410,11485, 12838,12946, 13030,13068, 13213,13338}; 
int rna22[] = {12837,12946, 13030,13068, 13213,13343, 13502,13598, 13960,14007, 19852,19940}; 
int rna23[] = {12840,12946, 13030,13068, 13213,13346}; 
int rna24[] = {12884,12946, 13030,13068, 13213,13343, 13502,13598, 13960,14007, 19852,19954}; 
int rna25[] = {12887,12946, 13030,13068, 13213,13343, 13502,13598, 13960,14007, 19852,19905}; 
int rna26[] = {12913,12946, 13030,13068, 13213,13343, 13502,13598, 13960,14007, 19852,19972}; 
#endif /* BIG_SET */


struct ggMrnaCluster *makeTestInput()
/* Make up nice test set for graph thing. */
{
struct ggMrnaCluster *mc;
AllocVar(mc);
mc->orientation = 1;

#ifdef BIG_SET
//slAddTail(&mc->mrnaList, makeTestDa(rna1, ArraySize(rna1)));
slAddTail(&mc->mrnaList, makeTestDa(rna2, ArraySize(rna2)));
slAddTail(&mc->mrnaList, makeTestDa(rna3, ArraySize(rna3)));
#ifdef NEVER
slAddTail(&mc->mrnaList, makeTestDa(rna4, ArraySize(rna4)));
slAddTail(&mc->mrnaList, makeTestDa(rna5, ArraySize(rna5)));
slAddTail(&mc->mrnaList, makeTestDa(rna6, ArraySize(rna6)));
slAddTail(&mc->mrnaList, makeTestDa(rna7, ArraySize(rna7)));
slAddTail(&mc->mrnaList, makeTestDa(rna8, ArraySize(rna8)));
slAddTail(&mc->mrnaList, makeTestDa(rna9, ArraySize(rna9)));
slAddTail(&mc->mrnaList, makeTestDa(rna10, ArraySize(rna10)));
slAddTail(&mc->mrnaList, makeTestDa(rna11, ArraySize(rna11)));
slAddTail(&mc->mrnaList, makeTestDa(rna12, ArraySize(rna12)));
slAddTail(&mc->mrnaList, makeTestDa(rna13, ArraySize(rna13)));
slAddTail(&mc->mrnaList, makeTestDa(rna14, ArraySize(rna14)));
slAddTail(&mc->mrnaList, makeTestDa(rna15, ArraySize(rna15)));
slAddTail(&mc->mrnaList, makeTestDa(rna16, ArraySize(rna16)));
slAddTail(&mc->mrnaList, makeTestDa(rna17, ArraySize(rna17)));
slAddTail(&mc->mrnaList, makeTestDa(rna18, ArraySize(rna18)));
slAddTail(&mc->mrnaList, makeTestDa(rna19, ArraySize(rna19)));
slAddTail(&mc->mrnaList, makeTestDa(rna20, ArraySize(rna20)));
slAddTail(&mc->mrnaList, makeTestDa(rna21, ArraySize(rna21)));
slAddTail(&mc->mrnaList, makeTestDa(rna22, ArraySize(rna22)));
slAddTail(&mc->mrnaList, makeTestDa(rna23, ArraySize(rna23)));
slAddTail(&mc->mrnaList, makeTestDa(rna24, ArraySize(rna24)));
slAddTail(&mc->mrnaList, makeTestDa(rna25, ArraySize(rna25)));
slAddTail(&mc->mrnaList, makeTestDa(rna26, ArraySize(rna26)));
#endif /* NEVER */
mc->tStart = 0;
mc->tEnd = 20000;
#endif /* BIG_SET */

#ifdef JIM_SET
slAddTail(&mc->mrnaList, makeTestDa(rna1, ArraySize(rna1)));
slAddTail(&mc->mrnaList, makeTestDa(rna2, ArraySize(rna2)));
slAddTail(&mc->mrnaList, makeTestDa(rna3, ArraySize(rna3)));
slAddTail(&mc->mrnaList, makeTestDa(rna4, ArraySize(rna4)));
slAddTail(&mc->mrnaList, makeTestDa(rna5, ArraySize(rna5)));
slAddTail(&mc->mrnaList, makeTestDa(rna6, ArraySize(rna6)));
slAddTail(&mc->mrnaList, makeTestDa(rna7, ArraySize(rna7)));
slAddTail(&mc->mrnaList, makeTestDa(rna8, ArraySize(rna8)));
slAddTail(&mc->mrnaList, makeTestDa(rna9, ArraySize(rna9)));
slAddTail(&mc->mrnaList, makeTestDa(rna10, ArraySize(rna10)));
slAddTail(&mc->mrnaList, makeTestDa(rna11, ArraySize(rna11)));
mc->tStart = 0;
mc->tEnd = 1400;
#endif

#ifdef SIMPLE_SET
slAddTail(&mc->mrnaList, makeTestDa(rna1, ArraySize(rna1)));
slAddTail(&mc->mrnaList, makeTestDa(rna2, ArraySize(rna2)));
mc->tStart = 0;
mc->tEnd = 200;
#endif

#ifdef RAY_SET
slAddTail(&mc->mrnaList, makeTestDa(rna1, ArraySize(rna1)));
slAddTail(&mc->mrnaList, makeTestDa(rna2, ArraySize(rna2)));
slAddTail(&mc->mrnaList, makeTestDa(rna3, ArraySize(rna3)));
slAddTail(&mc->mrnaList, makeTestDa(rna4, ArraySize(rna4)));
mc->tStart = 0;
mc->tEnd = 1000;
#endif
}


void smallTest()
/* Test out stuff. */
{
struct ggMrnaCluster *mc = makeTestInput();
struct geneGraph *gg;
static struct ggMrnaInput dummyCi;

uglyf("Initial input\n");
dumpMc(mc);
uglyf("\n");
gg = ggGraphCluster(mc, &dummyCi);
traverseGeneGraph(gg, mc->tStart, mc->tEnd, dumpGgAliInfo);
errAbort("Done testing");
}


void constraintsForBac(char *bacAcc, char *outDir)
/* Generate a set of constraint files for bac. */
{
struct ggMrnaInput *ci = ggGetMrnaForBac(bacAcc);
struct ggMrnaCluster *mc, *mcList = ggClusterMrna(ci);
struct geneGraph *gg;

uglyf("Got %d clusters\n", slCount(mcList));
for (mc = mcList; mc != NULL; mc = mc->next)
    {
    uglyf("Initial input\n");
    dumpMc(mc);
    uglyf("\n");
    gg = ggGraphCluster(mc, ci);
    uglyf("Constraints\n");
    traverseGeneGraph(gg, mc->tStart, mc->tEnd, dumpGgAliInfo);
    uglyf("\n\n");
    }
}

void usage()
/* Describe how to use program and exit. */
{
errAbort(
  "test - test alt-splice clustering and \n"
  "Usage:\n"
  "   test bacAccession\n");
}

int main(int argc, char *argv[])
{
char *bacAcc = "AC000001";
char *outDir = "gff";


if (argc < 2)
    usage();
bacAcc = argv[1];

constraintsForBac(bacAcc, outDir);
return 0;
}
