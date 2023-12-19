/* bigBedCmdSupport - functions to support writing bigBed related commands. */

/* Copyright (C) 2022 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef bigBedCmdSupport_h
#define bigBedCmdSupport_h
#include "bigBed.h"

void bigBedCmdOutputHeader(struct bbiFile *bbi, FILE *f);
/* output a autoSql-style header from the autoSql in the file */

void bigBedCmdOutputTsvHeader(struct bbiFile *bbi, FILE *f);
/* output a TSV-style header from the autoSql in the file */

#endif
