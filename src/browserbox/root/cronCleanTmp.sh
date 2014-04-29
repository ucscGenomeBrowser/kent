#!/bin/sh

# clean /tmp dir only if there is no rsync job running at the moment
# run from crontab

RUNNING=`ps --no-headers -Crsync | wc -l`
if [ ${RUNNING} -gt 2 ]; then
     echo rsync is running, not cleaning tmp
     exit 0
fi

rm -f /tmp/*
