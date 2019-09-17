This directory includes 3 scripts created by Matt Speir for parsing Apache access/error logs
and for generating stats on how often different search terms, dbs, and tracks are used.

The scripts are:
* generateSearchUse.py - used to generate information on general search term and HGVS search term
                         usage. Certain HGVS search terms are ignored as they are being hit by a
                         test script run by Brian Lee.

			 Ex. usage:

			 ./generateSearchUse.py -f /hive/data/inside/wwwstats/RR/2018/hgw1/access_log.20180107.gz

* generateUsageStats.py - used to genertate information on db and track usage information. By
                          default it outputs summary/combined counts over the period covered
			  by input log files. Includes options to output to output these counts
			  on a per month basis as well. Information is also output on a per hgsid
			  basis. Those hgsid with counts of 1 are discarded from counts.

			  Ex. usage:

			  ./generateUsageStats.py -pmjtf /hive/data/inside/wwwstats/RR/2018/hgw1/error_log.20180107.gz

			  This will take the specified log file and generate summary usage stats,
			  per month usage stats, json files of the internal dictionaries
			  containing the usage stats, the months/years covered in that log
			  file, and the usage stats for the default tracks for the top 15 most
			  used assemblies.


* makeLogSymLinks.sh - helper script for creating the flat directory structure needed by the
                       above two scripts. It's not smart about wrapping around year ends, so
		       you'll have to run it twice, one for the end of one year and another for
		       the beginning of the next. For example, if you're attempting to create a
		       directory for 10-2017 - 4-2018, you'll have to run it once for
		       10-17 - 12-17 and then another time for 1-18 - 4-18.

		       Ex. usage:

		       # Make a directory for your log files and go into it
		       mkdir rrLogFiles20180107; cd rrLogFiles20180107
		       # Run makeLogSymLinks.sh
		       makeLogSymLinks.sh 20180107 4 error

		       Now your directory will be populated with error_log files from
		       January 7, 2018 up to the end of April.

All scripts have a usage statement with more information about command-line options.
