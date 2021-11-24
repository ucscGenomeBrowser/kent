/* Methods for SwitchGear Genomics tracks. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef SWITCHGEAR_H
#define SWITCHGEAR_H

#ifndef SWITCHDBTSS_H
#include "switchDbTss.h"
#endif

void switchDbTssMethods(struct track *tg);
/* Methods for switchDbTss track uses mostly linkedFeatures stuff. */

#endif
