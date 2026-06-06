#!/bin/bash

# otto cron job
# 1,8,15,22,29,36,43,50,57 * * * * /hive/data/outside/otto/liftRequest/devAndRR.sh

# script source from source tree:
#    src/hg/utils/otto/userRequests/devAndRR.sh

cd /hive/data/outside/otto/liftRequest

#### do NOT need to check the one on hgwdev, this would duplicate things
# without arguments, will use /usr/local/apache/cgi-bin/hg.conf
#   to use hgcentraltest ottoRequest table for hgwdev
### /hive/data/outside/otto/liftRequest/ottoRequest.py

# with this .conf file it will use hgcentral ottoRequest table
export HGDB_CONF=/hive/data/outside/otto/liftRequest/.hg.central.conf
/hive/data/outside/otto/liftRequest/ottoRequest.py \
  --conf=/hive/data/outside/otto/liftRequest/.hg.central.conf
