This directory includes 3 scripts created by Matt Speir for parsing Apache access/error logs
and for generating stats on how often different search terms, dbs, and tracks are used.

The scripts are:
* generateSearchUse.py - used to generate information on search term usage. Certain 

* generateUsageStats.py - used to genertate information on db and track usage information. By
                          default it outputs summary/combined counts over the period covered
			  by input log files. Includes options to output to output these counts
			  on a per month basis as well. Information is also output on a per hgsid
			  basis. Those hgsid with counts of 1 are discarded from counts.

* makeLogSymLinks.sh - helper script for creating the flat directory structure needed by the
                       above two scripts. It's not smart about wrapping around year ends, so
		       you'll have to run it twice, one for the end of one year and another for
		       the beginning of the next. For example, if you're attempting to create a
		       directory for 10-2017 - 4-2018, you'll have to run it once for
		       10-17 - 12-17 and then another time for 1-18 - 4-18.

All scripts havea a usage statement with more information about command-line options. 
