This directory contains Python libraries for genome browser CGIs
written in Python (e.g. hgGeneGraph). They were ported to Python 3.

The directory contains pymysql, so browser installations do not have install this
module using pip. pymysql is in the public domain, see pymysql/LICENSE. The version
bundled here is 1.0.2.

hgLib.py includes various pieces of code ported from kent source hg/lib, like
the bottleneck client, cart parsing, mysql functions, etc.

The CGIs are not currently using a more efficient way of running Python scripts
through Apache (WSGI), because this would be a considerable amount of administration
across our various servers. It seems that running CGIs the "old" way is costing us
only 100-200msecs so it is still bearable for the non-essential CGIs we may 
have in Python.
