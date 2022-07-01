#!/bin/bash
set -beEu -o pipefail
today=`date +%F`

WORKDIR=$1
mkdir -p ${WORKDIR}/${today}
cd ${WORKDIR}/${today}

wget --quiet ftp://ftp.ebi.ac.uk/pub/databases/lrgex/LRG_public_xml_files.zip
unzip -oqq LRG_public_xml_files.zip
../buildLrg.sh ${WORKDIR}
