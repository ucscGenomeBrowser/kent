
#!/bin/sh -e

PATH=/cluster/bin/x86_64:$PATH
EMAIL="lrnassar@ucsc.edu"
WORKDIR="/hive/data/outside/otto/omim"

cd $WORKDIR
./checkOmimUpload.sh $WORKDIR 2>&1
