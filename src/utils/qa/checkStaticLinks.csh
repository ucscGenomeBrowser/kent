#!/bin/tcsh

###############################################
#  05-10-04
# 
#  checks the links in all the files in a directory
#  Robert Kuhn
# 
###############################################

set filePath=""
set yymmmdd=""
set file=""
set currDir=$cwd

if ($#argv < 1) then
  # no command line args
  echo
  echo "  checks the links in all the files in a directory (uses list on hgwbeta)."
  echo
  echo "    usage:  path in /htdocs (zero for root), "
  echo '      [yymmmdd (or other dateString)] defaults to "today"'
  echo
  exit
else
  # set filePath=$argv[1]
  set filePath=`echo $argv[1] | sed -e 's/\/$//'`
  if ($#argv == 2) then
    set yymmmdd=$argv[2]
  else
    set yymmmdd="today"
  endif
endif

if ($argv[1] == 0) then
  set filePath=""
  set terminalDir="htdocs"
  # get list of active files from beta and trim pathname:
  ssh hgwbeta 'ls /usr/local/apache/htdocs/*html' | sed -e "s/.*\///g" \
      > ${currDir}/filelist
else
  set terminalDir=`echo $filePath | sed -e "s/.*\///g"`

  # get list of active files from beta:
  ssh hgwbeta 'ls /usr/local/apache/htdocs/'${filePath}'/*html' \
      | sed -e "s/.*\///g" > ${currDir}/filelist
endif

echo "files in htdocs/${filePath}"
cat filelist

echo

foreach file (`cat ${currDir}/filelist`)
  echo $file
  LinkCheck $argv[1] $file $yymmmdd
end

rm -f linkCheck.all.$yymmmdd

echo  >>  linkCheck.all.$yymmmdd
echo "========  reporting only on files with errors   ========" >>  linkCheck.all.$yymmmdd
echo "========================================================" >>  linkCheck.all.$yymmmdd
echo  >>  linkCheck.all.$yymmmdd
foreach file (`ls -1 *.$yymmmdd.errors`)
  set fileflag=`cat $file | grep -e "Response Code" | wc -l`
  if ($fileflag > 0) then
    set filename=`echo $file | sed -e "s/\./\//g" | sed -e "s/\/$yymmmdd\/errors/\.html/"`
    echo >>  linkCheck.all.$yymmmdd
    echo "for htdocs/$filename" >>  linkCheck.all.$yymmmdd
    echo >>  linkCheck.all.$yymmmdd
    cat $file >>  linkCheck.all.$yymmmdd
    echo >>  linkCheck.all.$yymmmdd
    echo "========================================================" >>  linkCheck.all.$yymmmdd
    echo >>  linkCheck.all.$yymmmdd
  endif
end


