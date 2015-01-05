The GA4H beacon system http://ga4gh.org/#/beacon is a tiny little webservice
that accepts a chromosome position and replies with "yes", "no" or "maybe"
if it finds information in a data track. This is to allow queries from outside
users on protected variants. If they find variants in someone's beacon, they
then can contact the institution to get more information about it.

Please read the GA4H page before concluding that this CGI is inefficient. It's
inefficient, but the purpose of the system is not maximum data transfer, but
just limited information, so as to not identify patients or at least make it
quite hard to identify them.

It's implemented as a CGI script (hgBeacon) which calls bigBedToBed and a set of
.bb files.  No mysql is required and the data files live in the same directory
as the script and the bigBedtoBed binary. The makefile copies everything
together into cgi-bin/beacon/.  The overhead of calling a binary is very
small. We do this quite often in hgGene.

We don't have personal data at UCSC so the beacon in this directory serves
primarily LOVD and HGMD, which we cannot distribute.  These tracks are also
not in the table browser, but the beacon makes it possible to at least query
the positions in some way. 

To get usage info, run the CGI with no option, e.g.
http://genome-test.soe.ucsc.edu/cgi-bin/hgBeacon

The CGI also accepts INSDC accessions instead of "chr1" or just "1". The
mapping is stored in insdcToUcsc.tab. 

The CGI currently only supports hg19.


