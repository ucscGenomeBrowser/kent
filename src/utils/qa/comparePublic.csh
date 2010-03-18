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
set fields=""
set field2=""
set field2rr=""
set rm="rm"

if ( $#argv < 1  || $#argv > 3 ) then
  # no command line args
  echo
  echo "  compares tableName, otherField from trackDb_public on beta to beta, RR trackDbs."
  echo
  echo "    usage:  database [field2] [leaveFiles]"
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
    set field2="$argv[2]"
    set field2rr="$argv[2]"
    # get list of acceptable trackDb fields
    set fields=`hgsql -e "SELECT * FROM trackDb LIMIT 1" $db | head -1`
    echo $fields | grep $field2 > /dev/null
    if ( $status ) then
      echo
      echo "sorry, $field2 is not a trackDb field"
      echo "   try one of these:"
      echo $fields | sed "s/ /\n/g"
      echo
      exit 1
    endif
    if ( "$field2" == "html" ) then
      echo
      echo " sorry, field-checking disabled for html"
      echo
      exit 1
    endif
    if ( "$field2" == "settings" ) then
      echo
      echo " sorry, field-checking disabled for settings for now"
      echo
      exit 1
    endif
    set field2=", $field2"
  endif
endif

if ( $#argv == 3 ) then
  if ( $argv[3] == "leaveFiles" ) then
    set rm=''
  else
    echo 'sorry, third argument must be "leaveFiles"'
   exit 1
  endif
endif

#  get files of tables and compare: 
# beta public:
hgsql -N -h $sqlbeta -e "SELECT tableName $field2 FROM trackDb_public" $db | sort  > $db.public1
cp $db.public1 $db.public2
hgsql -N -h $sqlbeta -e "SELECT tableName $field2 FROM trackDb" $db | sort  > $db.beta

# build url for RR TB query:
set url1="http://genome.ucsc.edu/cgi-bin/hgTables?db=$db"
set url2="&hgta_database=$db"
set url3="&hgta_fieldSelectTable=$db.trackDb"
set url4="&hgta_fs.check.$db.trackDb.tableName=1"
set url4b="&hgta_fs.check.$db.trackDb.$field2rr=1"
set url5="&hgta_group=allTables&hgta_outputType=selectedFields&hgta_table=trackDb&hgta_track=$db"
set url6="&hgta_doPrintSelectedFields=get&#32;output"

set url="$url1$url2$url3$url4$url4b$url5$url6"

wget -q -O /dev/stdout "$url" | grep -v "#tableName" > $db.rr
echo
commTrio.csh $db.public1 $db.beta $rm | grep -v removed
commTrio.csh $db.public2 $db.rr $rm

echo "$db.public2 $db.rr $rm"

if ( $rm == 'rm' ) then
  rm -f $db.public1 
  rm -f $db.public2 
  rm -f $db.rr
  rm -f $db.beta
endif



