#!/bin/bash
#
# trackDbCacheCleaner.sh - remove unused entries from a trackDb cache directory
#
# This file can be viewed at the following URL:
# http://github.com/ucscGenomeBrowser/kent/raw/master/src/product/scripts/trackDbCacheCleaner.sh
#
#	usage: trackDbCacheCleaner.sh [-n] <cacheDir> [expireDays]
#
#		-n          list what would be removed, remove nothing
#		cacheDir    the cacheTrackDbDir setting from hg.conf
#		expireDays  expire entries unread this many days, default 30
#
# The trackDb cache holds one directory per database or track hub, named
# for the database or for a SHA1 of the hub URL.  Each directory holds one
# or more cache files named <mmapAddress>.<trackDbVersion>, plus name.txt
# and sometimes incFiles.txt.
#
# The browser expires a cache file only when a request visits its directory
# and finds the file older than the trackDb table or the hub.  Nothing ever
# removes a directory.  A hub URL that is requested once and never again
# keeps its cache files forever.  A tmpfs cache directory hides this until
# the next reboot.  A disk backed cache directory never hides it.
#
# This script removes what the browser cannot: a whole cache directory that
# no request has read for expireDays days.  A directory still in use is
# left alone, because the browser opens every file in a directory it visits
# and that keeps the access times current.
#
# Last use is the newer of the access time and the modify time.  On a file
# system mounted noatime the access time never advances, and taking the
# newer of the two falls back to the write time, which only ever keeps
# entries longer.
#
# Removing a cache file while a CGI is using it is safe.  An existing mmap
# survives the unlink.  A CGI that loses the race finds the open fails and
# builds trackDb from the database instead, so the worst case is one slow
# request.
#
# For a weekly cron, for example:
#   0 5 * * 0 /usr/local/apache/product/scripts/trackDbCacheCleaner.sh /data/trackDbCache

# exit on any error at any time
set -beEu -o pipefail

export dryRun=0
if [ "${1-}" = "-n" ]; then
  dryRun=1
  shift
fi

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
  echo "usage: trackDbCacheCleaner.sh [-n] <cacheDir> [expireDays]" 1>&2
  echo "  expire trackDb cache directories unread for expireDays days" 1>&2
  echo "  cacheDir is the cacheTrackDbDir setting from hg.conf" 1>&2
  echo "  expireDays defaults to 30" 1>&2
  exit 255
fi

export cacheDir="${1}"
export expireDays="${2-30}"

##########################################################################
# refuse anything that does not look like a trackDb cache, since this
# script removes directory trees

case "${cacheDir}" in
  /*) ;;
  *)  echo "ERROR: cacheDir must be an absolute path: '${cacheDir}'" 1>&2
      exit 255 ;;
esac

# /data and /dev/shm are the parents of a cache directory, never the cache
export pathDepth=`echo "${cacheDir}" | awk -F/ '{n=0;for(i=1;i<=NF;i++)if(length($i))n++;print n}'`
if [ "${pathDepth}" -lt 2 ]; then
  echo "ERROR: refusing to clean the top level directory '${cacheDir}'" 1>&2
  exit 255
fi

if [ ! -d "${cacheDir}" ]; then
  echo "ERROR: no such directory '${cacheDir}'" 1>&2
  exit 255
fi

if ! echo "${expireDays}" | grep -q -E '^[1-9][0-9]*$'; then
  echo "ERROR: expireDays must be a positive integer, not '${expireDays}'" 1>&2
  exit 255
fi

# every trackDb cache directory holds a name.txt.  If there are
# subdirectories but not one name.txt among them, this is some other
# directory and we should not be removing anything in it.
export subDirCount=`find "${cacheDir}" -mindepth 1 -maxdepth 1 -type d | wc -l`
export nameFile=`find "${cacheDir}" -mindepth 2 -maxdepth 2 -type f -name name.txt -print -quit | wc -l`
if [ "${subDirCount}" -gt 0 ] && [ "${nameFile}" -eq 0 ]; then
  echo "ERROR: '${cacheDir}' has ${subDirCount} subdirectories and no name.txt" 1>&2
  echo "ERROR: that is not a trackDb cache, refusing to clean it" 1>&2
  exit 255
fi

##########################################################################

export now=`date +%s`
export cutoff=`echo "${now} ${expireDays}" | awk '{print $1 - ($2 * 86400)}'`
export timeStamp=`date +%FT%T`
export expireList=`mktemp /var/tmp/trackDbCacheCleaner.XXXXXX`
export dryRunNote=""
if [ "${dryRun}" -eq 1 ]; then
  dryRunNote=" (dry run)"
fi

printf "# %s trackDbCacheCleaner.sh %s expire %s days%s\n" "${timeStamp}" \
  "${cacheDir}" "${expireDays}" "${dryRunNote}" 1>&2
LC_NUMERIC=en_US printf "# %'d directories before cleaning\n" "${subDirCount}" 1>&2

# A CGI can remove a stale cache file at any moment, so find can report an
# error for a file that vanished during the scan.  That is normal here and
# must not stop the cleaning, hence the scan to a file and the || below.
export scanList=`mktemp /var/tmp/trackDbCacheCleaner.scan.XXXXXX`

# %A@ access time, %T@ modify time, %s size, %p path.  Cache directory
# names are database names and SHA1 strings, so no path here has a space.
find "${cacheDir}" -mindepth 2 -maxdepth 2 -type f -printf '%A@ %T@ %s %p\n' \
  > "${scanList}" 2>/dev/null || printf "# note: some files vanished during the scan\n" 1>&2

# group the files by directory and keep the newest use of any of them
awk -v cutoff="${cutoff}" '
{
used = ($1 > $2) ? $1 : $2
dir = $4
sub(/\/[^\/]*$/, "", dir)
if (used > newest[dir]) newest[dir] = used
bytes[dir] += $3
files[dir] += 1
}
END {
for (dir in newest)
    if (newest[dir] < cutoff)
        printf "%d %d %s\n", files[dir], bytes[dir], dir
}' "${scanList}" > "${expireList}"
rm -f "${scanList}"

export expireDirCount=`cat "${expireList}" | wc -l`
export expireFileCount=`awk '{n += $1} END {printf "%d", n}' "${expireList}"`
export expireByteCount=`awk '{n += $2} END {printf "%d", n}' "${expireList}"`

# a cache directory can be owned by any user who ran a command line
# utility, so a removal can fail on permissions.  Count those and carry on,
# rather than abandoning the rest of the cleaning.
export failedDirCount=0
export warnLimit=10

if [ "${dryRun}" -eq 1 ]; then
  awk '{printf "would remove %s (%d files, %d bytes)\n", $3, $1, $2}' "${expireList}"
else
  while read fileCount byteCount dirName
  do
    if ! rm -fr "${dirName}" 2>/dev/null; then
      failedDirCount=`echo "${failedDirCount}" | awk '{print $1 + 1}'`
      if [ "${failedDirCount}" -le "${warnLimit}" ]; then
        echo "# warning: could not remove ${dirName}" 1>&2
      fi
      if [ "${failedDirCount}" -eq "${warnLimit}" ]; then
        echo "# warning: further removal failures not listed" 1>&2
      fi
    fi
  done < "${expireList}"
fi

# a directory left holding nothing at all has no files to judge it by,
# so use the directory's own last use instead
export emptyDirCount=0
for dirName in `find "${cacheDir}" -mindepth 1 -maxdepth 1 -type d -empty 2>/dev/null || true`
do
  dirUse=`stat -c '%X %Y' "${dirName}" 2>/dev/null | awk '{print ($1 > $2) ? $1 : $2}'`
  # empty and gone already, or filled in since the scan
  if [ -z "${dirUse}" ]; then
    continue
  fi
  if [ "${dirUse}" -lt "${cutoff}" ]; then
    emptyDirCount=`echo "${emptyDirCount}" | awk '{print $1 + 1}'`
    if [ "${dryRun}" -eq 1 ]; then
      echo "would remove ${dirName} (empty)"
    else
      rmdir "${dirName}" 2>/dev/null || true
    fi
  fi
done

rm -f "${expireList}"

export remainDirCount=`find "${cacheDir}" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | wc -l`

LC_NUMERIC=en_US printf "# expired %'d directories, %'d files, %'d bytes\n" \
  "${expireDirCount}" "${expireFileCount}" "${expireByteCount}" 1>&2
LC_NUMERIC=en_US printf "# expired %'d empty directories\n" "${emptyDirCount}" 1>&2
LC_NUMERIC=en_US printf "# %'d directories remain in %s\n" "${remainDirCount}" "${cacheDir}" 1>&2

if [ "${failedDirCount}" -gt 0 ]; then
  LC_NUMERIC=en_US printf "# ERROR: %'d directories could not be removed\n" "${failedDirCount}" 1>&2
  printf "# %s trackDbCacheCleaner.sh FAILED\n" "`date +%FT%T`" 1>&2
  exit 1
fi

printf "# %s trackDbCacheCleaner.sh SUCCESS\n" "`date +%FT%T`" 1>&2
