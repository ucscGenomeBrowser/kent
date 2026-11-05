
This process creates log files in:

/hive/data/inside/GenArk/pushRR/logs/yyyy/mm/

# push out the /gbdb/hubs/GC[AF]/ hierarchy to:
#  hgwbeta hgw0 hgw1 hgw2 euroNode
03 01 * * * /hive/data/inside/GenArk/pushRR/pushRR.sh

The asiaNode used to use 'pullHgwdev.sh' running on the Asia
node itself to pull over files from hgwdev.  That procedure is obsolete
and the management of Asia node resources are taken care of by
cluster admin
