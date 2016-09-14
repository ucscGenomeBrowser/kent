This directory contains the pieces of the pipeline that builds the CRISPR
track. The makefile in this directory will copy them to
/hive/data/outside/crisprTrack/scripts so cluster jobs are not accessing
the home directory file system (even though that may be fine these days)
and are not accessing the /cluster/bin file system (even though this may 
most likely be fine, as the "automation" stuff is run from there)

The driver script for this pipeline is called doCrispr.sh and located
in kent/src/utils/doCrispr.sh. In this way it gets installed automatically
into /cluster/bin or a user's bin directory.

The scripts require an installation of the CRISPOR software from 
https://github.com/maximilianh/crisporWebsite
By default, the crispor git clone is in 
/hive/data/outside/crisprTrack/crispor/
The crispor installation really has to be on hive as it includes indexed 
genomes and is accessed a lot from the cluster nodes.
Additional indexed genomes can be downloaded from
http://crispor.tefor.net/genomes/ and should be copied into
/hive/data/outside/crisprTrack/crispor/genomes
