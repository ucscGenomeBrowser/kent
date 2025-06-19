
This process creates log files in:

/hive/data/inside/GenArk/pushRR/logs/yyyy/mm/

# push out the /gbdb/hubs/GC[AF]/ hierarchy to:
#  hgwbeta hgw0 hgw1 hgw2 euroNode
#   There is an equivalent 'pull' script running on the asiaNode
03 01 * * * /hive/data/inside/GenArk/pushRR/pushRR.sh

