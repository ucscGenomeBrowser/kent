/* asmString - interface between generated assembly code and
 * strings */

#include "common.h"
#include "../compiler/pfPreamble.h"

struct _pf_string *_pfaNewString(char *s)
{
uglyf("Theoretically getting new string for %p %s\n", s, s);
return _pf_string_from_const(s);
}

