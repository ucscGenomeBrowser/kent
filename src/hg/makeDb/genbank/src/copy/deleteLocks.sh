
#!/bin/sh -e

srcDb=$1
tmpDb=$2
betaCfg=$3

HGDB_CONF=$betaCfg hgsql $tmpDb -e  "drop table genbankLock"
HGDB_CONF=$betaCfg hgsql $srcDb -e  "drop table genbankLock"
hgsql $srcDb -e  "drop table genbankLock"
hgsql $tmpDb -e  "drop table genbankLock"
