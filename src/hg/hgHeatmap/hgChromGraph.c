/* Display chromGraph data in hgHeatmap */

#include "common.h"
#include "hgChromGraph.h"

int chromGraphHeight()
{
return 30;
}

double chromGraphMax()
{
return 0.4; 
}

double chromGraphMin()
{
return -0.5; 
}

int chromGraphMaxGapToFill()
{
return 1000000;
}

Color chromGraphColor()
{
return MG_BLUE;
}
