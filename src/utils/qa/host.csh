#!/bin/tcsh
source `which qaConfig.csh`

/bin/rm -f /tmp/hostlist
hgsql -h $sqlrr -e "select remote_host from access_log" apachelog  > /tmp/hostlist

/bin/sort /tmp/hostlist > /tmp/hostlist.sort
/bin/rm -f /tmp/hostlist

/usr/bin/uniq -c /tmp/hostlist.sort > /tmp/hostlist.uniq
/bin/rm -f /tmp/hostlist.sort

/bin/sort -nr /tmp/hostlist.uniq 
/bin/rm -f /tmp/hostlist.uniq
