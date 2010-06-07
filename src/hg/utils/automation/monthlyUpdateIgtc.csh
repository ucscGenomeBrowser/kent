#!/bin/csh -ef

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/monthlyUpdateIgtc instead.

# $Id: monthlyUpdateIgtc.csh,v 1.2 2007/09/19 19:31:07 angie Exp $

set updateOne = $HOME/kent/src/hg/utils/automation/updateIgtc.pl
set dbs = (mm7 mm8 mm9)

foreach db ($dbs)
  # Do a -debug run to make a run directory:
  set runDir = `$updateOne -debug $db \
                |& grep 'Steps were performed' \
                | sed -re 's/ \*\*\* Steps were performed in //'`
  if ($runDir == "") then
    echo "ERROR: failure of $updateOne -debug $db"
    exit 1
  endif
  # Do the update for real and log it:
  $updateOne $db >& $runDir/do.log
  set success = `grep '^ \*\*\* All done' $runDir/do.log | wc -l`
  if ($success != 1) then
    echo "ERROR: failure of $updateOne $db -- check $runDir/do.log"
    exit 1
  endif
end

echo "Updated igtc in ($dbs) -- add a pushQ entry." \
| mail -s "Updated igtc in ($dbs)" $USER >& /dev/null

