/* ldUi.h - enums and char arrays for ld UI features */
#ifndef LDUI_H
#define LDUI_H

#include "common.h"

#define ldCeuDefault   TRUE
#define ldHcbDefault   FALSE
#define ldJptDefault   FALSE
#define ldYriDefault   FALSE
#define ldValueDefault "rsquared"
#define ldPosDefault   "red"
#define ldNegDefault   "blue"
#define ldOutDefault   "none"
#define ldTrimDefault  TRUE

extern boolean  ldCeu;
extern boolean  ldHcb;
extern boolean  ldJpt;
extern boolean  ldYri;
extern char    *ldValue;
extern char    *ldPos;
extern char    *ldNeg;
extern char    *ldOut;
extern boolean  ldTrim;

#endif /* LDUI_H */

