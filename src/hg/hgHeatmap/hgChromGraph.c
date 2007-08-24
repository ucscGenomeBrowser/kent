/* Display chromGraph data in hgHeatmap */

#include "common.h"
#include "hgChromGraph.h"

int chromGraphHeight()
{
return 30;
}

double chromGraphMax(char* tableName)
{
if (sameWord (tableName, "cnvLungBroadv2_summary"))
    return 0.4; 
if (sameWord (tableName, "cnvLungBroadv2_ave100K_summary"))
    return 0.1;
if (sameWord (tableName, "CGHBreastCancerStanford_summary"))
    return 0.4;
return 0.5;
}

double chromGraphMin(char* tableName)
{
if (sameWord (tableName, "cnvLungBroadv2_summary"))
    return -0.5; 
if (sameWord (tableName, "cnvLungBroadv2_ave100K_summary"))
    return -0.1;
return -0.5;
}

int chromGraphMaxGapToFill(char* tableName)
{
return 1000000;
}

Color chromGraphColor(char* tableName)
{
if (sameWord (tableName, "CGHBreastCancerUCSF_summary"))
    return MG_BLUE;
if (sameWord (tableName, "cnvLungBroadv2_ave100K_summary"))
    return MG_BLACK;
return MG_BLACK;
}
