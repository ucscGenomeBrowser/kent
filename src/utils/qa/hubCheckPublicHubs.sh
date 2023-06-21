#!/usr/bin/env bash

# Program Header
# Name:   Gerardo Perez
# Description: A program that runs hubCheck for all the public hubs on the RR and outputs it into a file
#
# hubCheckPublicHubs.sh
#

echo '#############################################' >> /hive/users/qateam/hubCheckCronArchive/`date +'%Y'`/hubCheck_output
for output in $(hgsql -h genome-centdb -Ne "select hubUrl from hubPublic" hgcentral | tail -n +2)
do
    echo $output >> /hive/users/qateam/hubCheckCronArchive/`date +'%Y'`/hubCheck_output
    hubCheck $output >> /hive/users/qateam/hubCheckCronArchive/`date +'%Y'`/hubCheck_output
    echo '#############################################' >> /hive/users/qateam/hubCheckCronArchive/`date +'%Y'`/hubCheck_output
done

