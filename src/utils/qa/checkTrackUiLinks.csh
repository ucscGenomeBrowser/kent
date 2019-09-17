#!/bin/tcsh
source `which qaConfig.csh`

################################
#  
#  01-24-07
#  Robert Kuhn
#
#  checks all links on trackUi pages for a track
#
################################

onintr cleanup

set tableinput=""
set tables=""
set machine="hgwbeta"
set host=""
set rr="false"
set baseUrl=""
set target=""
set cgi=""
set hgsid=""
set db=""
set pgsWErrors=0
set pgsChkd=0

if ( $#argv < 2 || $#argv > 3 ) then
  echo
  echo "  checks all links on trackUi pages for a track."
  echo
  echo "    usage:  database tablelist [machine]"
  echo '           tablelist may also be single table or "all"'
  echo "           machine defaults to hgwbeta"
  echo
  echo '    note: includes assembly description.html page if "all"'
  echo
  exit
else
  set db=$argv[1]
  set tableinput=$argv[2]
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

# check for valid db
set url1="http://"
set url2=".soe.ucsc.edu/cgi-bin/hgTables?db=$db&hgta_doMetaData=1"
set url3="&hgta_metaDatabases=1"
set url="$url1$machine$url2$url3"
wget -q -O /dev/stdout "$url" | grep $db > /dev/null
if ( $status ) then
  echo
  echo "  ${db}: no such database on $machine"
  echo
  exit 1
endif

# set machine name and check validity
if ( $#argv == 3 ) then
  set machine="$argv[3]"
  checkMachineName.csh $machine
  if ( $status ) then
    exit 1
  endif
endif

# check if it is a file or a tablename and set list
file $tableinput | egrep "ASCII text" > /dev/null
if (! $status) then
  set tables=`cat $tableinput`
else
  set tables=$tableinput
endif

# set hgsid so don't fill up sessionDb table
set baseUrl="http://$machine.soe.ucsc.edu"
set hgsid=`htmlCheck  getVars $baseUrl/cgi-bin/hgBlat | grep hgsid \
  | head -1 | awk '{print $4}'`

echo "hgw1 hgw2 hgw3 hgw4 hgw5 hgw6" | grep $machine
if ( $status ) then
  set rr="true"
endif

# process descriptions page for "all" choice
if ( "all" == $tableinput ) then
  if ( "hgwdev" != "$machine" ) then
    set host="-h $sqlbeta"
  endif
  set tables=`hgsql $host -Ne "SELECT tableName FROM trackDb WHERE html != ''" $db`
  set tables="description.html $tables"
endif

# check links on all table.html pages
foreach table ( $tables )
  rm -f errorsOnPg$$
  if ( $table == "description.html" ) then
    set cgi="hgGateway?hgsid=$hgsid&db=$db"
  else
    set cgi="hgTrackUi?hgsid=$hgsid&db=$db&g=$table"
  endif
  set target="$baseUrl/cgi-bin/$cgi"
  htmlCheck getLinks "$target" | egrep "ftp:|http:" | grep -v ucsc > $db.$table.outsideLinks$$

  if ( ! -z  $db.$table.outsideLinks$$ ) then
    foreach link ( `cat $db.$table.outsideLinks$$` )
      echo "$link" >>& errorsOnPg$$
      htmlCheck ok "$link" >>& errorsOnPg$$
    end
    egrep -B1 -v "ftp:|http:" errorsOnPg$$ > /dev/null
    if ( ! $status ) then
      echo $table
      echo "=============="
      grep -B1 -v http errorsOnPg$$
      echo
      @ pgsWErrors= $pgsWErrors + 1
    endif
  endif
  @ pgsChkd= $pgsChkd + 1
  rm -f errorsOnPg$$
  rm -f $db.$table.outsideLinks$$
end

echo "Summary"
echo "======= ======="
if ( $pgsChkd == 1 ) then
  echo $pgsChkd "page checked"
else
  echo $pgsChkd "pages checked"
endif
if ( $pgsWErrors > 0 ) then
  if ( $pgsWErrors == 1 ) then
    echo $pgsWErrors "page with error(s) found"
  else
    echo $pgsWErrors "pages with errors found"
  endif
else
  echo "No errors found!"
endif
echo

cleanup:
rm -f error$$
rm -f $db.$table.outsideLinks$$
