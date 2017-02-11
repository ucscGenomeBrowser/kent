/* cdwFlowCharts - Library functions to look through SQL tables and generate dagger d3 flowcharts.*/
#include "cart.h"
#ifndef CDWFLOWCHARTS_H
#define CDWFLOWCHARTS_H

struct dyString *makeCdwFlowchart(int fileId, struct cart *cart);
/* Runs through SQL tables and print out a history flow chart for the file. Note that you must
 * provide a fileId from the cdwValidFile table (or an id from cdwFile). An id from cdwValidFile 
 * can produce a corrupted result!. */

#endif /* CDWFLOWCHARTS_H */ 
