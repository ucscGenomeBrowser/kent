/* oregannoUi.c - char arrays for oreganno UI features */
#include "oregannoUi.h"
#include "common.h"

static char const rcsid[] = "$Id: oregannoUi.c,v 1.2 2008/09/22 18:53:00 giardine Exp $";

/* some stuff for mutation type choices */
/* labels for checkboxes */
char *oregannoTypeLabel[] = {
    "regulatory polymorphism",
    "transcription factor binding site",
    "regulatory region",
};

/* names for checkboxes */
char *oregannoTypeString[] = {
    "oreganno.filter.poly",
    "oreganno.filter.tfbs",
    "oreganno.filter.reg",
};

/* values in the db */
char *oregannoTypeDbValue[] = {
    "REGULATORY POLYMORPHISM",
    "TRANSCRIPTION FACTOR BINDING SITE",
    "REGULATORY REGION",
};

int oregannoTypeSize = ArraySize(oregannoTypeString);

/* list of attribute fields for oreganno, in order to be displayed */
char *oregannoAttributes[] = {
    "SrcLink",
    "Extlink",
    "type",
    "Gene",
    "TFbs",
    "Dataset",
    "Evidence",
    "EvidenceSubtype",
};

char *oregannoAttrLabel[] = {
    "Links",
    "Links",
    "Region type",
    "Gene",
    "Transcription factor",
    "Dataset",
    "Evidence type",
    "Evidence subtype(s)",
};

int oregannoAttrSize = ArraySize(oregannoAttributes);
