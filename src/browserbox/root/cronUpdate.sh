#!/bin/bash

# run from crontab to update the browser

# only update browser if the "noAutoUpdate" flagfile does not exist

if [ -f /root/noAutoUpdate ]; then
    exit;
fi

# sleep a little bit, so we don't hammer the rsync server
sleep $[RANDOM%30]

/root/updateBrowser.sh
