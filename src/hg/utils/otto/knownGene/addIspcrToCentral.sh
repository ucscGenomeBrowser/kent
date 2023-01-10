serverName=$1
port=$2

hgsql hgcentraltest -e \
        "INSERT into blatServers values ('${db}KgSeq${GENCODE_VERSION}', '$serverName', $port, 0, 1,'');"
hgsql hgcentraltest -e \
                       "INSERT into targetDb values('${db}KgSeq${GENCODE_VERSION}', 'GENCODE Genes', \
                                            '$db', 'kgTargetAli', '', '', \
                                                                          '/gbdb/${db}/targetDb/${db}KgSeq${GENCODE_VERSION}.2bit', 1, now(), '');"
