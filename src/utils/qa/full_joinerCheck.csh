#!/bin/tcsh
# Script to run joinerCheck for full database with -key options
# 08-17-04 M. Chalup
# ---------

# Check number of arguments
if ($#argv != 1) then
        echo "Usage: full_joinerCheck.csh <current_db_release>"
        exit 1
endif

set current_dir = $cwd
set db=$argv[1]

# Update all.joiner and make symbolic link into local directory

echo "cd into ~/kent/src/hg/makeDb/schema"
cd ~/kent/src/hg/makeDb/schema
echo ""
echo "Update files from cvs root"
cvs update -dP
echo ""
echo "Return to previous directory"
cd $current_dir
echo ""
echo "Establish a symbolic link to all.joiner"
ln -s ~/kent/src/hg/makeDb/schema/all.joiner all.joiner
echo ""
echo "Running joinerCheck - this may take quite a while"
nice joinerCheck -database=$db -keys all.joiner >& $db.complete_release_joinerCheck.txt

# Filter the output

echo ""
echo "Remove all lines for unique entries and those where hits = hits"
echo ""
 
grep -v unique $db.complete_release_joinerCheck.txt | \
gawk '{ if (( $3 ~ /^hits/) && ( $4 != $6)) { print $0 }; \
if ($3 !~ /hits/ ) { print $0 };}'

