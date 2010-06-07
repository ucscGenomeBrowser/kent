/* pentStruct - output assembly language versions of structures.
 * This would be easier if gnu assembly were a little better.... */

#include "common.h"
#include "pentStruct.h"

void pentStructPrint(FILE *f)
/* Output pentium structures to file */
{
fprintf(f, "%s",
"# Data Sizes\n"
"byteSize = 1\n"
"shortSize = 2\n"
"intSize = 4\n"
"longSize = 8\n"
"objSize = 4\n"
"floatSize = 4\n"
"doubleSize = 8\n"
"\n"
"# Offsets into structures\n"
"# Generic object\n"
"pfRefCount=0\n"
"pfType=4\n"
"# String-specific fields\n"
"pfS=8\n"
"pfSize=12\n"
"pfAllocated=16\n"
"pfStringIsConst=20\n"
"# Array specific fields not shared with string\n"
"pfArrayElements=8\n"
"pfArrayElType=20\n"
"pfArrayElSize=24\n"
"# Dir-specific fields\n"
"pfDirElType=8\n"
"pfDirHash=12\n"
"\n"
);
}
