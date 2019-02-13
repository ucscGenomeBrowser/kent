/* dataApi.h - functions for API access to UCSC data resources */

#ifndef DATAAPH_H

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "udc.h"
#include "knetUdc.h"
#include "genbank.h"
#include "trackHub.h"
#include "hgConfig.h"
#include "hCommon.h"
#include "hPrint.h"
#include "bigWig.h"
#include "hubConnect.h"
#include "obscure.h"
#include "errCatch.h"
#include "vcf.h"
#include "bedTabix.h"
#include "bamFile.h"
#include "jsonParse.h"
#include "jsonWrite.h"
#include "chromInfo.h"

#ifdef USE_HAL
#include "halBlockViz.h"
#endif

void jsonErrAbort(char *format, ...);
/* Issue an error message in json format, and exit(0) */

struct jsonWrite *jsonStartOutput();
/* begin json output with standard header information for all requests */

extern void getTrackData();
/* return data from a track */

extern void getSequenceData();
/* return DNA sequence, given at least a db=name and chrom=chr,
   optionally start and end  */

#endif	/*	 DATAAPH_H	*/
