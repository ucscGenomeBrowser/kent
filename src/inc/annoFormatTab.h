/* annoFormatTab -- collect fields from all inputs and print them out, tab-separated. */

#ifndef ANNOFORMATTAB_H
#define ANNOFORMATTAB_H

#include "annoFormatter.h"

struct annoFormatter *annoFormatTabNew(char *fileName);
/* Return a formatter that will write its tab-separated output to fileName. */

#endif//ndef ANNOFORMATTAB_H
