#
# Package for handling ftp downloads.  Use global state (maybe should be
# a perl object, but I don't want to bless such a mess).
#
package gbFtp;
use strict;
use warnings;
use Carp;
use gbCommon;

# hideous magic required by perl modules
BEGIN {
    use Exporter();
    @gbFtp::ISA = qw(Exporter);
    @gbFtp::EXPORT = qw(ftpInit ftpOpen ftpClose ftpGetOrCheck
                        ftpGetDirList ftpSafeGet ftpGetModTime);

    # Constants
    $gbFtp::NUM_RETRIES = 3;
    $gbFtp::RETRY_TIME = 60;  # seconds to sleep (times retry count)

    # global state
    $gbFtp::ftpHost = undef;
    $gbFtp::ftpUser = undef;
    $gbFtp::ftpPassword = undef;
    $gbFtp::ftpVerbose = 0;
    $gbFtp::handle = undef;

    my $hostname = `hostname`;
    chomp($hostname);
    $gbFtp::tmpSuffix = $hostname . ".$$.tmp";
}

# Guts of retry, doesn't do open
sub ftpHandleRetryGuts($$) {
    my($tryNum, $msg) = @_;
    print STDERR "$msg\n";
    if ($tryNum+1 >= $gbFtp::NUM_RETRIES) {
        print STDERR "bummer, maximum number of ftp retries ($gbFtp::NUM_RETRIES) exceeded, giving up\n";
        endTask();
        exit(2);
    }
    ftpClose();
    sleep(($tryNum+1)*$gbFtp::RETRY_TIME);
}

# Setup retry for an ftp error. If we can't connect, exit non-zero but don't
# leave any sempahores so we can try again later
sub ftpHandleRetry($$) {
    my($tryNum, $msg) = @_;
    ftpHandleRetryGuts($tryNum, $msg);
    ftpOpen();
}

# Initialize host, user, and password (default to anonymous).
sub ftpInit($;$$) {
    ($gbFtp::host, $gbFtp::user, $gbFtp::password) = @_;
    if (!defined($gbFtp::user)) {
        $gbFtp::user = "anonymous";
    }
    if (!defined($gbFtp::password)) {
        $gbFtp::password = '-anonymous@';
    }
}

# Open (or reopen) an ftp connection to NCBI
sub ftpOpen() {
    ftpClose();
    my $tryNum = 1;
    while (!defined($gbFtp::handle)) {
        $gbFtp::handle =  Net::FTP->new($gbFtp::host, Debug => $gbFtp::verbose);
        if (!defined($gbFtp::handle)) {
            ftpHandleRetryGuts($tryNum, "connect failed to ftp://$gbFtp::host");
        }
        $tryNum++;
    }
    $tryNum = 1;
    my $stat;
    while (!defined($stat)) {
        $stat = $gbFtp::handle->login($gbFtp::user, $gbFtp::password);
        if (!defined($stat)) {
            ftpHandleRetryGuts($tryNum, "login to ftp://$gbFtp::host failed");
        }
        $tryNum++;
    }

    # WARNING: seems to be random if this is set
    $gbFtp::handle->binary();
}

# close the ftp connection (if open)
sub ftpClose() {
    if (defined($gbFtp::handle)) {
        $gbFtp::handle->quit();
    }
    $gbFtp::handle = undef;
}

# Get the size of a remote file
sub ftpGetSize($) {
    my($remFile) = @_;
    my $remSize;
    my $tryNum = 1;
    while (!defined($remSize)) {
        $remSize = $gbFtp::handle->size($remFile);
        if (!defined($remSize)) {
            ftpHandleRetry($tryNum, "can't get file size for ftp://$gbFtp::host/$remFile");
        }
        $tryNum++;
    }
    return $remSize;
}

# Get the modification time of a remote file
sub ftpGetModTime($) {
    my($remFile) = @_;
    my $modTime;
    my $tryNum = 1;
    while (!defined($modTime)) {
        $modTime = $gbFtp::handle->mdtm($remFile);
        if (!defined($modTime)) {
            ftpHandleRetry($tryNum, "can't get modification time for ftp://$gbFtp::host/$remFile");
        }
        $tryNum++;
    }
    return $modTime;
}

# Get the list of a remote files in a directory
sub ftpGetDirList($) {
    my($remDir) = @_;
    my(@remFiles);
    my $tryNum = 1;
    while ($#remFiles < 0) {
        @remFiles = $gbFtp::handle->ls($remDir);
        if ($#remFiles < 0) {
            ftpHandleRetry($tryNum, "can't get file list for ftp://$gbFtp::host/$remDir");
        }
        $tryNum++;
    }
    return @remFiles;
}

# Set ftp restart point
sub ftpRestart($) {
    my($off) = @_;
    $gbFtp::handle->restart($off);
}

# Do ftp get
sub ftpGet($$) {
    my($remFile, $localTmp) = @_;
    my $tryNum = 1;
    my $stat;
    while (!defined($stat)) {
        $stat = $gbFtp::handle->get($remFile, $localTmp);
        if (!defined($stat)) {
            ftpHandleRetry($tryNum, "download of ftp://$gbFtp::host/$remFile to $localTmp failed for unknown reason");
        }
        $tryNum++;
    }
}
# Safely ftp a remote file to a local file.  
#  - Never overwrites an existing file
#  - downloads to a tmp name and renames to prevent partial downloads
#  - verify before and after remote file sizes
#  - verify size of downloaded file
#  - make read-only after download
#  - if the file is compressed, run through zcat to verify
sub ftpSafeGet($$) {
    my($remFile, $localFile) = @_;
    my $localTmp = "$localFile.$gbFtp::tmpSuffix";
    makeFileDir($localFile);
    unlink($localTmp);

    if (-e $localFile) {
        die("file exists, will not overwrite: $localFile");
    }

    my $startRemSize = ftpGetSize($remFile);
    if ($main::verbose || 1) {  # FIXME:
        print "getting ftp://$gbFtp::host/$remFile\n";
    }
    ftpGet($remFile, $localTmp);

    # verify size
    my $localSize = getFileSize($localTmp);
    my $remSize = ftpGetSize($remFile);
    if ($remSize != $startRemSize) {
        die("size at end of download ($remSize) does not match size at start ($startRemSize) for ftp://$gbFtp::host/$remFile");
    }
    if ($localSize != $remSize) {
        die("size of just downloaded file $localTmp ($localSize) does not match ftp://$gbFtp::host/$remFile ($remSize)");
        return 0;
    }
    
    # set modification time from server and write protect
    my $modTime = ftpGetModTime($remFile);
    utime($modTime, $modTime, $localTmp)
        || die("can't change mod time of $localTmp");

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
# size matches the remote size.  Verify that the local size matches the
# remote size after download.  True return if gotten, false if exists.
# If justCheck is true, just do the size check or error if not found.
# If warnOnCheckError is true, don't about if there if check fails
sub ftpGetOrCheck($$$$) {
    my($justCheck, $warnOnCheckError, $remFile, $localFile) = @_;

    my $remSize = ftpGetSize($remFile);
    if (!defined($remSize)) {
        die("can't get file size for ftp://$gbFtp::host/$remFile");
    }
    if (-e $localFile) {
        my $localSize = getFileSize($localFile);
        my $remSize = ftpGetSize($remFile);
        if ($localSize != $remSize) {
            my $msg = "size of existing file $localFile ($localSize) does not match ftp://$gbFtp::host/$remFile ($remSize)";
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
        ftpSafeGet($remFile, $localFile);
        return 1;
    }
}

# perl requires a true value at the end
1;
