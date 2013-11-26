#!/bin/sh

# scan the customTrash database and remove any tables that are not
# recorded in the metaInfo table and have aged enough to ensure they
# are not an actual custom track table.  Requires the script:
#          $HOME/kent/src/product/scripts/lostTables.pl
#          and the binary: $HOME/bin/$MACHTYPE/hgsql 
# *AND* requires read access permissions to the MySQL customTrash database
# files to scan last access times for the tables.  Creates log files in:
#           $HOME/customTrash/log/${YYYY}/${MM}

# special .hg.conf file where db.host, db.user and db.password are the items
# to access the customTrash database.  This is unlike the normal
# cgi-bin/hg.conf.private file were these are specified
#  in customTracks.* settings.
export HGDB_CONF="$HOME/.customTrash.hg.conf"
export YYYY=`/bin/date "+%Y"`
export MM=`/bin/date "+%m"`
export DS=`/bin/date "+%Y-%m-%dT%H:%M:%S"`
export lockFile="/var/tmp/lostTableRunning.pid"

if [ -f "${lockFile}" ]; then
  echo "ERROR: cleanLostTables.sh overrunning itself"
  /bin/date "+%Y-%m-%dT%H:%M:%S"
  exit 255
else
  echo "running $$ - "`date +%Y-%m-%dT%H:%M:%S` > "${lockFile}"
  /bin/chmod 666 "${lockFile}"
fi

export LOGDIR="$HOME/customTrash/log/${YYYY}/${MM}"
if [ ! -d "${LOGDIR}" ]; then
    /bin/mkdir -p "${LOGDIR}"
fi
export LOGFILE="${LOGDIR}/${DS}.txt"
export FILELIST="${LOGDIR}/${DS}.list"

/usr/bin/time -p $HOME/kent/src/product/scripts/lostTables.pl -age=72 \
   > "${LOGFILE}" 2>&1
/bin/grep "^t.*_LOST_$" "${LOGFILE}" | cut -f1 > "${FILELIST}"
/bin/gzip "${LOGFILE}" "${FILELIST}"
export WC=`/bin/zcat "${FILELIST}" | wc -l`

if [ "${WC}" -eq 0 ]; then
    echo "INFO: cleanLostTables.sh not finding any tables to clean."
    /bin/date "+%Y-%m-%dT%H:%M:%S"
else
/bin/zcat "${FILELIST}" | /usr/bin/xargs --no-run-if-empty -L 10 echo \
        | /bin/sed -e "s/ /,/g" | while read T
do
    $HOME/bin/$MACHTYPE/hgsql -N -e "drop table ${T};" customTrash
done
fi
/bin/rm -f "${lockFile}"
