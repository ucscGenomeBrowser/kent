/* hgData_common - common routines for hgData. */
#include "common.h"
#include "hgData.h"
#include "hdb.h"
#include "chromInfo.h"

static char const rcsid[] = "$Id: hgData_common.c,v 1.1.2.1 2009/01/08 05:22:39 mikep Exp $";


char *quote(char *field)
// format field surrounded by quotes "field" in buf and return buf
// return 'null' without quotes if field is NULL
{
quoteBuf[0]='\0';
if (field)
    safef(quoteBuf, sizeof(quoteBuf), "\"%s\"", field);
else
    safef(quoteBuf, sizeof(quoteBuf), "null");
return quoteBuf;
}
