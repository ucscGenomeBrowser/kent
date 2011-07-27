#!/bin/sh

# cleanLostTables.sh - run this as a cron job on your custom trash
#	database.  It needs the perl script: lostTables.pl
# Lost tables come about in the custom trash database when they have
# no corresponding entry in the metaInfo table.  When they are in the
# metaInfo table, they will expire because of the age information
# in that table.  Without an entry there, they escape that expiration
# procedure.  The lostTables.pl can find tables that have no metaInfo
# entry at the specified age.  They are tables to be deleted since
# nothing references them.

export HGDB_CONF="/data/home/qateam/.ct.hg.conf"
export YYYY=`/bin/date "+%Y"`
export MM=`/bin/date "+%m"`
export DS=`/bin/date "+%Y-%m-%dT%H:%M:%S"`
# echo $YYYY $MM $DS

export LOGDIR="/data/home/qateam/customTrash/log/${YYYY}/${MM}"
if [ ! -d "${LOGDIR}" ]; then
    /bin/mkdir -p "${LOGDIR}"
fi
export LOGFILE="${LOGDIR}/${DS}.txt"
export FILELIST="${LOGDIR}/${DS}.list"

/usr/bin/time -p /data/home/qateam/customTrash/lostTables.pl -age=72 > "${LOGFILE}" 2>&1
/bin/grep "^t.*_LOST_$" "${LOGFILE}" | cut -f1 > "${FILELIST}"
/bin/gzip "${LOGFILE}" "${FILELIST}"
export WC=`/bin/zcat "${FILELIST}" | wc -l`
if [ "${WC}" -eq 0 ]; then
    echo "starting drop table operation on ${WC} tables"
    /bin/date "+%Y-%m-%dT%H:%M:%S"
else
/bin/zcat "${FILELIST}" | /usr/bin/xargs --no-run-if-empty -L 10 echo \
	| /bin/sed -e "s/ /,/g" | while read T
do
#    echo '/data/home/qateam/bin/x86_64/hgsql -N -e "drop table '"${T}"';" customTrash'
    /data/home/qateam/bin/x86_64/hgsql -N -e "drop table ${T};" customTrash
done
fi
if [ "${WC}" -eq 0 ]; then
    echo "done drop table operation on ${WC} tables"
    /bin/date "+%Y-%m-%dT%H:%M:%S"
fi
