/* longTabix -- long range pairwise interaction format, from Wash U and Ensembl
 *    Documented here:  http://wiki.wubrowse.org/Long-range
 *           and here:  http://www.ensembl.org/info/website/upload/pairwise.html
 */

/* Copyright (C) 2017 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "asParse.h"
#include "annoStreamer.h"
#include "bedTabix.h"


static char *longTabixAutoSqlString =
"table longTabix\n"
"\"Long Range Tabix file. Each interaction is represented by two items/regions in the file. \"\n"
"   (\n"
"   string chrom;      \"Reference sequence chromosome or scaffold\"\n"
"   uint   chromStart; \"Start position in chromosome\"\n"
"   uint   chromEnd;   \"End position in chromosome\"\n"
"   string interactingRegion;       \"(e.g. chrX:123-456,3.14, where chrX:123-456 is the coordinate of the mate, and 3.14 is the score of the interaction or (or comma-separated RGB value)\"\n"
"   uint   id;      \"Unique Id\"\n"
"   char[1] strand;    \"+ or -\"\n"
"   )\n"
;

struct asObject *longTabixAsObj()
// Return asObject describing fields of longTabix file
{
return asParseText(longTabixAutoSqlString);
}

struct annoStreamLongTabix
    {
    struct annoStreamer streamer;	// Parent class members & methods
    // Private members
    char *asWords[6];	          // Current row of longTabix with genotypes squashed for autoSql
    struct bedTabixFile *btf;		// longTabix parsed header and file object
    int numFileCols;			// Number of columns in longTabix file.
    int maxRecords;			// Maximum number of annoRows to return.
    int recordCount;			// Number of annoRows we have returned so far.
    boolean eof;			// True when we have hit end of file or maxRecords
    };

