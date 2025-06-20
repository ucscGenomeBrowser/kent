
This process creates log files in:

/hive/data/inside/GenArk/pushRR/logs/yyyy/mm/

# push out the /gbdb/hubs/GC[AF]/ hierarchy to:
#  hgwbeta hgw0 hgw1 hgw2 euroNode
#   There is an equivalent 'pull' script running on the asiaNode
03 01 * * * /hive/data/inside/GenArk/pushRR/pushRR.sh

A copy of the asiaNode 'pull' script is in the file here:

  pullHgwdev.sh

With the crontab entry there in the 'qateam' account:

#  pull down the /gbdb/hubs/GC[AF]/ files from hgwdev daily
#    near midnight Pacific time.  Logs maintained in ~/rsyncLog/YYYY/MM/
02 16 * * * ~/cronScripts/pullHgwdev.sh
