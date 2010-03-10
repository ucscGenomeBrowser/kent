#!/bin/tcsh
source `which qaConfig.csh`

###############################################
# 
#  03-09-2010
#  Robert Kuhn
#
#  Gets tableName from trackDb_public on beta and compares to RR
# 
###############################################

set db=""
set field2=""
set rm="rm"

if ( $#argv < 1  || $#argv > 2 ) then
  # no command line args
  echo
  echo "  gets tableName from trackDb_public on beta and compares to RR."
  echo
  echo "    usage:  $0 database [field2] [leaveFiles]"
  echo "      where leaveFiles lets you see the diffs"
  echo
  exit
else
  set db=$argv[1]
endif

if ( $#argv > 1 ) then
  if ( $argv[2] == "leaveFiles" ) then
    set rm=''
  else
    set field2=", $argv[2]"
  endif
endif


#  get files of tables and compare: 
# beta public:
hgsql -N -h $sqlbeta -e "SELECT tableName $field2 FROM trackDb_public" $db | sort  > $db.public

# build url for RR TB query:
set url1="http://genome.ucsc.edu/cgi-bin/hgTables?db=$db"
set url2="&hgta_database=$db"
set url3="&hgta_fieldSelectTable=$db.trackDb"
set url4="&hgta_fs.check.$db.trackDb.tableName=1"
set url5="&hgta_group=allTables&hgta_outputType=selectedFields&hgta_table=trackDb&hgta_track=$db"
set url6="&hgta_doPrintSelectedFields=get&#32;output"

set url="$url1$url2$url3$url4$url5$url6"

wget -q -O /dev/stdout "$url" | grep -v "#tableName" > $db.rr
echo
commTrio.csh $db.public $db.rr $rm

if ( $rm == 'rm' ) then
  rm -f $db.public
  rm -f $db.rr
endif

