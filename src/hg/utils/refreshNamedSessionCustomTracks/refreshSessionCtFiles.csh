#!/bin/csh -f

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/refreshNamedSessionCustomTracks/refreshSessionCtFiles.csh instead.

# $Id: refreshSessionCtFiles.csh,v 1.2 2009/09/25 02:22:00 angie Exp $

set logDir = /cluster/home/qateam/refrLog/hgcentral
set hour = `date +%H`
set tmpLog = $logDir/tmp.$hour.log
set errLog = $logDir/err.$hour.log
set tmpOut = /trash/ctDoNotRmNext.$hour.txt
set errOut = $logDir/ctDoNotRmNext.$hour.err

/cluster/bin/scripts/refreshSessionCtFilesInner.csh

if ($status != 0) then
  echo refreshSessionCtFilesInner.csh failed\!
  tail $tmpLog
  mv $tmpLog $errLog
  echo Logfile: $errLog
  if (-e $tmpOut) then
    mv $tmpOut $errOut
    echo Intermediate out: $errOut
  endif
  exit 1
endif

mv $tmpLog $logDir/lastLog
