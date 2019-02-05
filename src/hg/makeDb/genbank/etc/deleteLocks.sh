
#!/bin/sh -e

srcDb=$1
tmpDb=$2
alphaCfg=$3

HGDB_CONF=$alphaCfg hgsql $tmpDb -e  "drop table genbankLock"
HGDB_CONF=$alphaCfg hgsql $srcDb -e  "drop table genbankLock"
hgsql $srcDb -e  "drop table genbankLock"
hgsql $tmpDb -e  "drop table genbankLock"
