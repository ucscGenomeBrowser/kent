/* gtexEqtlTracks.c - display GTEx eQTL SNPs */

/* Copyright (C) 2017 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "hgTracks.h"
#include "bigBed.h"
#include "gtexEqtlBed.h"

static char *gtexEqtlItemName(struct track *tg, void *item)
/* Return target name (tissue or gene) */
{
struct gtexEqtlBed *eqtl = (struct gtexEqtlBed *)item;
if (eqtl == NULL)
    return "error";
if (isNotEmpty(eqtl->target))
    return eqtl->target;
return eqtl->name;
}

void gtexEqtlMethods(struct track *tg)
{
tg->canPack = TRUE;
tg->isBigBed = TRUE;
tg->itemName = gtexEqtlItemName;
}
