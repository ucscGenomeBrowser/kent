/* landmarkUi.c - char arrays for landmark UI features */
#include "landmarkUi.h"
#include "common.h"

/* some stuff for mutation type choices */
/* labels for checkboxes */
char *landmarkTypeLabel[] = {
    "hypersensitive site",
    "promoter",
    "enhancer",
    "regulatory",
    "other",
    "unknown",
};

/* names for checkboxes */
char *landmarkTypeString[] = {
    "landmark.filter.hss",
    "landmark.filter.prom",
    "landmark.filter.enh",
    "landmark.filter.reg",
    "landmark.filter.oth",
    "landmark.filter.unk",
};

/* values in the db */
char *landmarkTypeDbValue[] = {
    "HSS",
    "promoter",
    "enhancer",
    "regulatory",
    "other",
    "unknown",
};

int landmarkTypeSize = ArraySize(landmarkTypeString);

