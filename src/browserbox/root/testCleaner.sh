#!/bin/bash
#
# exit on any error at any time
set -beEu -o pipefail

# rm -f /usr/local/apache/userdata/cleaner.pid
# rm -f /var/tmp/trashSizeMonitor.pid

echo "running two commands to test the cleaner."
echo "There should be no output to your terminal from the two commands."
echo "There should be some files created in /root/browserLogs/trashCleaner/"
echo "and /root/browserLogs/trashLog/"

echo "=========================================================="
echo "/root/browserLogs/scripts/measureTrash.sh /root/browserLogs/scripts/virtualBox.txt"
/root/browserLogs/scripts/measureTrash.sh /root/browserLogs/scripts/virtualBox.txt

echo "/root/browserLogs/scripts/trashCleanMonitor.sh /root/browserLogs/scripts/virtualBox.txt searchAndDestroy"
/root/browserLogs/scripts/trashCleanMonitor.sh /root/browserLogs/scripts/virtualBox.txt searchAndDestroy
echo "=========================================================="

echo "find /root/browserLogs/trashCleaner -type f"
find /root/browserLogs/trashCleaner -type f
echo "find /root/browserLogs/trashLog -type f"
find /root/browserLogs/trashLog -type f
