#!/bin/bash

# delete files that are two weeks or older
directory="/hive/groups/browser/cartDumps"
find "$directory" -type f -mtime +14 -delete

cd /hive/users/qateam/cronCartDumps

# first copy over the files
scp -pr qateam@hgw0:/usr/local/apache/trash/cartDumps/'*/*' . > /dev/null 2>&1

# then build an index html to view them from
# https://genecats.gi.ucsc.edu/qa/cronCartDumps/index.html
(echo '<!DOCTYPE html>'; echo "<table>"
for i in `ls -r 0*`; 
do 
echo "<tr>"
date=`ls -l  --time-style="+%Y-%m-%d-%H:%M" $i | awk '{print $6}'`
echo "<td><A href=\"http://genome.ucsc.edu/cgi-bin/hgTracks?hgS_doLoadUrl=submit&hgS_loadUrlName=https://hgwdev.soe.ucsc.edu/~qateam/cartDumps/$i&ignoreCookie=1\"> $i </A></td><td>$date</td>" ;
echo "</tr>"
done) > /usr/local/apache/htdocs-genecats/qa/cronCartDumps/index.html
