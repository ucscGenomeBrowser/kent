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
# Check for a lock or stop file file.  If the lock file exists and is less
# than a day old, silently exit 0.  If the lock file is more than a day old,
# generate an error.  After the first warning, a second timestamp file is
# created so that warns occur only twice a day.  An absolute path to lockFile
# will result in a better error message.  This scheme supports light-weight
# polling.
#
gbCheckLock() {
    local lockFile=$1
    if [ -e $lockFile ] ; then
        # compare modification time, first word in file contains modtime
        gawk -v "lockFile=$lockFile" '{
           modTime=$1;
           if (systime() > modTime+(60*60*24)) {
                print "Error: lock or stop file more than 1 day old: " lockFile  > "/dev/stderr"
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


# Compare time files.  First file must exist. Returns 0 if second file is
# out-of-date.  Option delayDays only returns 0 if file is out-of-date by that
# many days.
gbCmpTimeFiles() {
    local timeFile1=$1 timeFile2=$2 delayDays=$3
    if [ "x$delayDays" = "x" ] ; then
        delayDays=0
    fi
    if [ ! -e "$timeFile1" ] ; then
        echo "gbCmpTimeFiles: $timeFile1 doesn't exist" >&2
        exit 1
    fi
    if [ ! -e "$timeFile2" ] ; then
        return 0  # time2 out-of-date
    fi
    local time1=`gawk '{print $1}' $timeFile1`
    local time2=`gawk -v delayDays=$delayDays '{print $1+(60*60*24*delayDays)}' $timeFile2`
    if [ $time1 -gt $time2 ] ; then
        return 0  # time2 out-of-date
    else
        return 1  # time2 current
    fi
}

# gbGetDatabases dbs1 ...
#
# Get list of database to process.  This takes one or more files
# containing the databases, removes comment and empty lines.
gbGetDatabases() {
    # gnu and FreeBSD have different sed options for extended regular expressions
    local reOpt="-r"
    if [ "`uname`" = "FreeBSD" ] ; then
        reOpt="-E"
    fi
    sed $reOpt -e 's/#.*$//' -e '/^[[:space:]]*$/d' $* 
}
