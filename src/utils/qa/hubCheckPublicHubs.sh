#!/usr/bin/env bash

# Program Header
# Name:   Gerardo Perez
# Description: A program that runs hubCheck for all the public hubs on the RR and outputs it into a file
#
# hubCheckPublicHubs.sh
#

mkdir -p /hive/users/qateam/hubCheckCronArchive/`date +'%Y-%m'`/
echo '#############################################' >> /hive/users/qateam/hubCheckCronArchive/`date +'%Y-%m'`/hubCheck_output
for output in $(/cluster/bin/x86_64/hgsql -h genome-centdb -Ne "select hubUrl from hubPublic" hgcentral | tail -n +2)
do
    echo $output >> /hive/users/qateam/hubCheckCronArchive/`date +'%Y-%m'`/hubCheck_output
    /cluster/bin/x86_64/hubCheck $output 2> /dev/null >> /hive/users/qateam/hubCheckCronArchive/`date +'%Y-%m'`/hubCheck_output
    echo '#############################################' >> /hive/users/qateam/hubCheckCronArchive/`date +'%Y-%m'`/hubCheck_output
done

