#!/bin/tcsh

rm -f /tmp/hostlist
hgsql -h genome-centdb -e "select remote_host from access_log" apachelog  > /tmp/hostlist

sort /tmp/hostlist > /tmp/hostlist.sort
rm -f /tmp/hostlist

uniq -c /tmp/hostlist.sort > /tmp/hostlist.uniq
rm -f /tmp/hostlist.sort

sort -nr /tmp/hostlist.uniq 
rm -f /tmp/hostlist.uniq
