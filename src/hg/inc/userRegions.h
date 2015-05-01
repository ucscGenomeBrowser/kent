/* userRegions: parse user regions entered as BED3, BED4 or chr:start-end
 * optionally followed by name. */

/* Copyright (C) 2015 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

#ifndef USERREGIONS_H
#define USERREGIONS_H

char *userRegionsParse(char *db, char *regionsText, int maxRegions, int maxErrs,
                       int *retRegionCount, char **retWarnText);
/* Parse user regions entered as BED3, BED4 or chr:start-end optionally followed by name.
 * Return name of trash file containing BED for parsed regions if regionsText contains
 * valid regions; otherwise NULL.
 * If maxRegions <= 0, it is ignored; likewise for maxErrs.
 * If retRegionCount is non-NULL, it will be set to the number of valid parsed regions
 * in the trash file.
 * If retWarnText is non-NULL, it will be set to a string containing warning and error
 * messages encountered while parsing input. */

#endif /* USERREGIONS_H */
