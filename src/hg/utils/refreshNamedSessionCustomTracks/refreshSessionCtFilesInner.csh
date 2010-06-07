#!/bin/csh -ef

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/refreshNamedSessionCustomTracks/refreshSessionCtFilesInner.csh instead.

# $Id: refreshSessionCtFilesInner.csh,v 1.5 2010/05/05 08:18:42 galt Exp $

setenv HGDB_CONF /cluster/home/qateam/.hg.conf.hgcentral

set today = `date +%y-%m-%d`
set hour = `date +%H`
set thisHour = $today.$hour
set logDir = /cluster/home/qateam/refrLog/hgcentral
set tmpLog = $logDir/tmp.$hour.log
set tmpOut = /trash/ctDoNotRmNext.$hour.txt
set finalOut = /trash/ctDoNotRm.txt

mkdir -p $logDir/$today

if (-e $tmpLog || -e $tmpOut) then
  echo "ERROR: $tmpLog and/or $tmpOut already exists!  Is another instance running?"
  echo "       Or does the file simply need to be cleaned up since yesterday?  Aborting."
  uptime
  exit 1
endif

set strayLogs = `find $logDir -maxdepth 1 -name tmp\*.log`
if ("$strayLogs" != "") then
  echo "WARNING: stray log files (is a previous instance still running?)"
  echo $strayLogs
  uptime
endif

set strayOut = `find $tmpOut:h -maxdepth 1 -name ctDoNoRmNext\*`
if ("$strayOut" != "") then
  echo "WARNING: stray temporary output files (is a previous instance still running?)"
  echo $strayOut
  uptime
endif

ps -eafl > $logDir/before.log
/cluster/bin/x86_64/refreshNamedSessionCustomTracks hgcentral -atime=123 -verbose=4 \
>& $tmpLog
ps -eafl > $logDir/after.log
/cluster/bin/scripts/makeExclusionList.pl $tmpOut $tmpLog \
| fgrep -w expired \
  > $logDir/$today/expired.$thisHour

sleep 1

if (-s $tmpOut) then
  mv $tmpOut $finalOut
else
  echo $tmpOut does not exist or is empty -- not replacing $finalOut .
  exit 1
endif

sort -u $finalOut | sed -e 's@^/export@@' \
| xargs -n 512 ls -lu > $logDir/$today/$thisHour

# Don't clean up $tmpLog here -- caller may use it.
