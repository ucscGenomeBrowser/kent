This directory contains Python libraries for genome browser CGIs
written in Python (e.g. hgGeneGraph). The ideal Python version is 2.7, but
CentOs is at the time of writing on 2.6 so hgLib.py is avoiding any special 2.7
features and uses the python2 in the hash bang line, so it will use whatever
python2 version is available (2.7 on genome-asia/genome-euro and 2.6 on the RR, 
at the time of writing).

The directory is called pyLib because in the future, other libraries can be
copied into this directory, it is likely that we will need more libraries, not
only hgLib.py.

hgLib.py includes various pieces of code ported from kent source hg/lib, like
the bottleneck client, cart parsing, mysql functions, etc.

The CGIs are not currently using a more efficient way of running Python scripts
through Apache (WSGI), because this would be a considerable amount of administration
across our various servers. It seems that running CGIs the "old" way is costing us
only 100-200msecs so it is still bearable for the non-essential CGIs we may 
have in Python.
