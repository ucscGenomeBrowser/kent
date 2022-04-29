#
# Package for handling rsync downloads.  Use global state (maybe should be
# a perl object, but I don't want to bless such a mess).
#
package gbRsync;
use strict;
use warnings;
use Carp;
use gbCommon;
use File::Basename;

# hideous magic required by perl modules
BEGIN {
    use Exporter();
    @gbRsync::ISA = qw(Exporter);
    @gbRsync::EXPORT = qw(rsyncInit rsyncGetOrCheck rsyncGet
                        rsyncGetDirList rsyncSafeGet rsyncGetModTime);

    # Constants
    $gbRsync::NUM_RETRIES = 3;
    $gbRsync::RETRY_TIME = 60;  # seconds to sleep (times retry count)

    # global state
    $gbRsync::rsyncHost = undef;
    $gbRsync::rsyncVerbose = 0;

    my $hostname = `hostname`;
    chomp($hostname);
    $gbRsync::tmpSuffix = $hostname . ".$$.tmp";
}

# Setup retry for an rsync error. If we can't connect, exit non-zero but don't
# leave any sempahores so we can try again later
sub rsyncHandleRetry($$) {
    my($tryNum, $msg) = @_;
    print STDERR "$msg\n";
    if ($tryNum+1 >= $gbRsync::NUM_RETRIES) {
        print STDERR "bummer, maximum number of rsync retries ($gbRsync::NUM_RETRIES) exceeded, giving up\n";
        endTask();
        exit(2);
    }
    sleep(($tryNum+1)*$gbRsync::RETRY_TIME);
}

# Get the list of remote files in a directory
sub rsyncGetDirList($$) {
    my ($remDir, $fileRE) = @_;
    # this command gets the list of files available
    my $rsyncListCmd = "rsync --no-h --list-only $fileRE --exclude=\"*\" $gbRsync::rsyncHost/$remDir/ | awk 'x==1 {print \$5} \$5 == \".\" {x = 1}' > rsyncList.out";
    my (@files);
    runPipe($rsyncListCmd);
    open(FILE, "rsyncList.out") || die("can't open rsyncList.out");
    chomp (@files = <FILE>);
    close(FILE) || die("can't close opened file 'rsyncList.out'");
    unlink("rsyncList.out");
    return @files;
}

sub rsyncGetSize($) {
    my ($fileName) = @_;
    my $tmpBase = basename($fileName);
    my $tmpFile = "$tmpBase.$gbRsync::tmpSuffix.size";
    unlink($tmpFile);
    my $rsyncSizeCmd = "rsync --no-h --list-only $gbRsync::rsyncHost/$fileName | tail -1 | awk '{print \$2}' > $tmpFile";
    runPipe($rsyncSizeCmd);
    my $size = readFile($tmpFile);
    unlink($tmpFile);
    return $size;
}

sub rsyncDownloadFile($$) {
    my ($remFile, $tmpFile) = @_;
    unlink($tmpFile);
    my $rsyncCmd = "rsync --no-h --times --quiet $gbRsync::rsyncHost/$remFile $tmpFile";
    my $stat = runProgNoAbort($rsyncCmd);
    return $stat;
}

# Set the host, and initialize the file list to transfer
sub rsyncInit($) {
    my ($host) = @_;
    $gbRsync::rsyncHost = $host;
}

# Do rsync get
sub rsyncGet($$) {
    my($remFile, $localTmp) = @_;
    my $tryNum = 1;
    my $stat;
    while (!defined($stat)) {
        $stat = rsyncDownloadFile($remFile, $localTmp);
        if (!defined($stat)) {
            rsyncHandleRetry($tryNum, "download of $gbRsync::host/$remFile to $localTmp failed for unknown reason");
        $tryNum++;
        }
    }
}

# Safely rsync a remote file to a local file.
#  - Never overwrites an existing file
#  - downloads to a tmp name and renames to prevent partial downloads
#  - verify before and after remote file sizes
#  - verify size of downloaded file
#  - make read-only after download
#  - if the file is compressed, run through zcat to verify
sub rsyncSafeGet($$) {
    my($remFile, $localFile) = @_;
    my $localTmp = "$localFile.$gbRsync::tmpSuffix";
    makeFileDir($localFile);
    unlink($localTmp);

    if (-e $localFile) {
        die("file exists, will not overwrite: $localFile");
    }

    if ($main::verbose || 1) {  # FIXME:
        print "getting $gbRsync::rsyncHost/$remFile\n";
    }
    # rsync natively takes care of verifying sizes
    rsyncGet($remFile, $localTmp);

    # set write protect
    chmod(0444, $localTmp)
        || die("can't change mode of $localTmp");

    # verify compressed files
    if (($localFile =~ /\.gz$/) || ($localFile =~ /\.Z$/)) {
        runProg("zcat < $localTmp >/dev/null");
    }

    # finally, install under the real name
    renameFile($localTmp, $localFile);
}

# Get a file if it doesn't exist locally, otherwise check that the local
# size matches the remote size. Verify that the local size matches the
# remote size after download. True return if gotten, false if exists.
# If justCheck is true, just do the size check or error if not found.
# If warnOnCheckError is true, don't about if there if check fails
sub rsyncGetOrCheck($$$$) {
    my($justCheck, $warnOnCheckError, $remFile, $localFile) = @_;

    my $remSize = rsyncGetSize($remFile);
    if (!defined($remSize)) {
        die("can't get file size for $gbRsync::rsyncHost/$remFile");
    }
    if (-e $localFile) {
        my $localSize = getFileSize($localFile);
        if ($localSize != $remSize) {
            my $msg = "size of existing file $localFile ($localSize) does not match ftp://$gbFtp::rsyncHost/$remFile ($remSize)";
            if ($warnOnCheckError) {
                print STDERR "Warning: $msg\n";
            } else {
                die($msg);
            }
        } elsif ($main::verbose) {
            print STDERR "file exists with same size: $remSize == $localFile ($localSize)\n";
        }
        return 0;
    } elsif ($justCheck) {
        die("download file should exist: $localFile");
    } else {
        rsyncSafeGet($remFile, $localFile);
        return 1;
    }
}

# perl requires a true value at the end
1;
