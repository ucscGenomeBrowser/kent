#
# gbCommon.sh: common setup functions used by genbank bash scripts.
# These scripts must set gbRoot to the absolute path to the local
# root directory and then source this file.  This will set the PATH
# Error handling assumes running with set -e.

# setup
PATH="$gbRoot/bin:$gbRoot/bin/i386:$PATH"

#
# gbCheckLock lockFile
#
# Check for a lock file.  If the lock file exists and is less than
# a day old, silently exit 0.  If the lock file is more than a day old,
# generate an error.  A absolute path to lockFile will result in a better
# error message.  This scheme supports light-weight polling.
#
gbCheckLock() {
    local lockFile=$1
    if [ -e $lockFile ] ; then
        # compare modification time, first work in file contains modtime
        awk -v "lockFile=$lockFile" '{
           modTime=$1;
           if (systime() > modTime+(60*60*24)) {
                print "Error: lock file more than 1 day old: " lockFile  > "/dev/stderr"
                exit(1);
           }
        }' $lockFile
        exit 0
    fi
}

#
# gbMkTimeFile tsfile
#
# Create a time file, which contains the epoch time in seconds
#
gbMkTimeFile() {
    local tsFile=$1
    local tsTmp=$tsFile.tmp
    mkdir -p `dirname $tsFile`
    date +%s >$tsTmp
    mv -f $tsTmp $tsFile
}


# Compare time files.  First must exist. Returns 0 if second is 
# out-of-date
gbCmpTimeFiles() {
    local timeFile1=$1 timeFile2=$2
    if [ ! -e $timeFile2 ] ; then
        return 0  # time2 out-of-date
    fi
    local time1=`awk '{print $1}' $timeFile1`
    local time2=`awk '{print $1}' $timeFile2`
    if [ $time1 -gt $time2 ] ; then
        return 0  # time2 out-of-date
    else
        return 1  # time2 current
    fi
}
