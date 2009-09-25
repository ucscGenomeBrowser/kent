#!/bin/csh -f

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/refreshNamedSessionCustomTracks/refreshSessionCtFiles.csh instead.

# $Id: refreshSessionCtFiles.csh,v 1.1 2009/09/25 00:34:31 angie Exp $

set logDir = /cluster/home/qateam/refrLog/hgcentral
set hour = `date +%H`
set tmpLog = $logDir/tmp.$hour.log
set errLog = $logDir/err.$hour.log

/cluster/bin/scripts/refreshSessionCtFilesInner.csh

if ($status != 0) then
  echo refreshSessionCtFilesInner.csh failed\!
  tail $tmpLog
  mv $tmpLog $errLog
  exit 1
endif

rm $tmpLog
