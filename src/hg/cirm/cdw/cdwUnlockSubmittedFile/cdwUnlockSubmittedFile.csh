#!/bin/tcsh
if ("$1" == "") then
    echo "Specify symlinked file to unlock."
    exit 1
endif
set sym = $1
echo $sym
if (! -l $sym) then
    echo "File specified $sym is not a symlink."
    exit 1
endif
if (! -e $sym) then
    echo "File specified by symlink $sym is a dead link."
    exit 1
endif
cp $sym $sym.tmp
rm $sym
mv $sym.tmp $sym
chmod 664 $sym
