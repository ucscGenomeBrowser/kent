The GA4H beacon system http://ga4gh.org/#/beacon is a tiny little webservice
that accepts a chromosome position and replies with "yes", "no" or "maybe"
if it finds information in a data track. This is to allow queries from outside
users on protected variants. If they find variants in someone's beacon, they
then can contact the institution to get more information about it.

It's implemented as a CGI script (query) which uses bigBedToBed and a set of
.bb files.  No mysql is required and the data files live in the same directory
as the script and the bigBedtoBed binary. The makefile copies everything
together into cgi-bin/beacon/.

The data for the beacon, the .bb files, are copied into the cgi-bin directory
from /hive/data/genomes/hg19/bed/beacon

We don't have personal data at UCSC so the beacon in this directory serves
primarily LOVD and HGMD, which we cannot distribute.  These tracks are also
not in the table browser, but the beacon makes it possible to at least query
the positions in some way. 

To get usage info, run the CGI with no option, e.g.
http://hgwdev-max.soe.ucsc.edu/cgi-bin/beacon/query

The CGI keeps a list of counts of IP+date, to limit the maximum number of
queries to 1000 a day. This is to make it harder to query the whole genome and
identify a person from the responses. This limit can most likely be increased.

The CGI also accepts INSDC accessions instead of "chr1" or just "1". The
mapping is stored in insdcToUcsc.tab. 

The CGI currently only supports hg19.

