The GA4H beacon system http://ga4gh.org/#/beacon is a tiny little webservice
that accepts a chromosome position and replies with "yes", "no" or "maybe"
if it finds information in a data track. This is to allow queries from outside
users on protected variants. If they find variants in someone's beacon, they
then can contact the institution to get more information about it.

The purpose of the system is not maximum data transfer, but limiting
information, so as to prevent the identification of patients or at least make
it quite hard to identify them.

It's implemented as a CGI script (hgBeacon) which stores the data in a sqlite
database under /gbdb/hg19/beacon.
We don't have personal data at UCSC so the beacon in this directory serves
primarily LOVD and HGMD, which we cannot distribute.  These tracks are also
not in the table browser, but the beacon makes it possible to query
their positions at least. 

To get usage info, run the CGI with no option, e.g.
http://genome-test.soe.ucsc.edu/cgi-bin/hgBeacon

The CGI currently only supports hg19.

API ref at
https://docs.google.com/document/d/154GBOixuZxpoPykGKcPOyrYUcgEXVe2NvKx61P4Ybn4
