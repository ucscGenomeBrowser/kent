/* ldUi.h - ld UI features */
#ifndef LDUI_H
#define LDUI_H

#include "common.h"

#define ldValueDefault   "lod"
#define ldPosDefault     "red"
#define ldNegDefault     "blue"
#define ldOutDefault     "black"
#define ldTrimDefault    TRUE
#define ldInvertDefault  FALSE

extern char    *ldValue;
extern char    *ldPos;
extern char    *ldNeg;
extern char    *ldOut;
extern boolean  ldTrim;

extern boolean hapmapLdCeu_inv;
extern boolean hapmapLdChb_inv;
extern boolean hapmapLdChbJpt_inv;
extern boolean hapmapLdJpt_inv;
extern boolean hapmapLdYri_inv;
extern boolean rertyHumanDiversityLd_inv;

#endif /* LDUI_H */

