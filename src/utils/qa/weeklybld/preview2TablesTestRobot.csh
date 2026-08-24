#!/bin/tcsh
cd $WEEKLYBLD

cd $BUILDDIR/v${BRANCHNN}_branch/kent/src/hg/hgTablesTest
make 
cd $WEEKLYBLD

@ NEXTNN = ( $BRANCHNN + 1 )

setenv HGDB_CONF /cluster/home/build/.hg.conf
set log = v${NEXTNN}.preview2.hgTables.log
# Combined stdout/stderr of the two hgTablesTest runs.  Kept SEPARATE from $log:
# hgTablesTest writes its structured report to $log itself and ALSO echoes the
# summary to stdout, so merging the two streams into one file would double every
# "Total:" line and defeat the completed-run count below.
set runout = v${NEXTNN}.preview2.hgTables.runout

# Who hears about the result.  Defaults to the build meister plus QA; a
# diagnostic re-run can narrow it with e.g.
#   setenv ROBOT_MAILTO build@soe.ucsc.edu
# so re-testing does not notify the QA list.
set mailto = "${BUILDMEISTEREMAIL} browser-qa@soe.ucsc.edu"
if ( $?ROBOT_MAILTO ) then
    set mailto = "$ROBOT_MAILTO"
endif

rm -f ./logs/$runout
echo "/cluster/bin/$MACHTYPE/hgTablesTest -db=hg38 https://hgwdev.gi.ucsc.edu/cgi-bin/hgTables ./logs/$log" > ./logs/$log
/cluster/bin/$MACHTYPE/hgTablesTest -appendLog -db=hg38 https://hgwdev.gi.ucsc.edu/cgi-bin/hgTables ./logs/$log >>& ./logs/$runout
set st1 = $status
echo "" >> ./logs/$log
echo "/cluster/bin/$MACHTYPE/hgTablesTest -appendLog -org=Mouse -orgs=1  https://hgwdev.gi.ucsc.edu/cgi-bin/hgTables ./logs/$log" >> ./logs/$log
/cluster/bin/$MACHTYPE/hgTablesTest -appendLog -org=Mouse -orgs=1  https://hgwdev.gi.ucsc.edu/cgi-bin/hgTables ./logs/$log >>& ./logs/$runout
set st2 = $status

# creates hgTables.log - look for unusual errors

#-- to check for errors:
# hgTablesTest prints its "Total: N tests, N soft errors, N hard errors" line
# from reportSummary() only after a run finishes.  When a run dies partway --
# errAbort ("Couldn't get org var", "Couldn't select track ..."), the carefulAlloc
# 500MB ceiling, a segfault -- that line never gets written.  The old check looked
# ONLY for Total lines that carried errors, so a crash produced an empty match and
# was reported as success; every log back to v490 has zero Total lines, meaning
# this gate had never once fired.  So now a missing Total summary, or a nonzero
# exit from either run, is itself a hard failure.
set expectedRuns = 2
set nTotal = `egrep -c "^[ 	]*Total:" ./logs/$log`
set res  = `egrep "^[ 	]*Total:" ./logs/$log | egrep -v "0 soft errors,  0 hard errors"`
set res2 = `awk '/^[ \t]*Total/ {print} /TablesTest/ {print}' ./logs/$log`
set crash = `cat ./logs/$log ./logs/$runout | egrep "carefulAlloc|Couldn.t |needLargeMem|needMem is null|Segmentation|Out of memory|Assertion|no form produced"`

set problems = ""
if ( "$st1" != "0" ) then
    set problems = "$problems hg38-run-exited-$st1"
endif
if ( "$st2" != "0" ) then
    set problems = "$problems mouse-run-exited-$st2"
endif
if ( "$nTotal" != "$expectedRuns" ) then
    set problems = "$problems incomplete-only-$nTotal-of-$expectedRuns-runs-reached-their-Total-summary"
endif
if ( "$res" != "" ) then
    set problems = "$problems errors-reported-in-Total-summary"
endif
if ( "$crash" != "" ) then
    set problems = "$problems crash-signature-in-log"
endif

if ( "$problems" != "" ) then
    echo "errs found:$problems"
    echo "$res2"
    ( echo "hgTablesTest robot FAILED for v${NEXTNN} preview2 on $HOST" ; \
      echo "" ; \
      echo "problems:$problems" ; \
      echo "" ; \
      echo "hg38 run exit status : $st1" ; \
      echo "Mouse run exit status: $st2" ; \
      echo "completed runs       : $nTotal of $expectedRuns reached their Total summary" ; \
      echo "" ; \
      echo "crash signatures:" ; \
      echo "$crash" ; \
      echo "" ; \
      echo "summary lines:" ; \
      echo "$res2" ; \
      echo "" ; \
      echo "report log: $WEEKLYBLD/logs/$log" ; \
      echo "run output: $WEEKLYBLD/logs/$runout" ) \
      | mail -s "Errors in v${NEXTNN}.preview2 hgTablesTestRobot on $HOST" $mailto
    exit 1
endif
#
echo Done.
echo "$res2" | mail -s "v${NEXTNN}.preview2 hgTablesTest robot done successfully on $HOST." $mailto
exit 0
