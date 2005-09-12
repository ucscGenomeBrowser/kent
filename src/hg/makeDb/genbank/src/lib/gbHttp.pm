#
# Package for handling http downloads.  Kind of like gbFtp.  
#
package gbHttp;
use strict;
use warnings;
use Carp;
use gbCommon;
use HTTP::Response;
use HTTP::Request;
use HTTP::Headers;
use LWP::UserAgent;

# perl module crap
BEGIN {
    use Exporter();
    @gbHttp::ISA = qw(Exporter);
    @gbHttp::EXPORT = qw(httpInit httpGetSize httpGetModTime httpGet httpSafeGet httpGetOrCheck);

    # Constants
    $gbHttp::NUM_RETRIES = 3;
    $gbHttp::RETRY_TIME = 30;  # seconds to sleep (times retry count)
    $gbHttp::AGENT_NAME = "UCSCgbHttp";

    # global state
    $gbHttp::Url = undef;
    $gbHttp::Verbose = 0;
    $gbHttp::handle = undef;

    my $hostname = `hostname`;
    chomp($hostname);
    $gbHttp::tmpSuffix = $hostname . ".$$.tmp";
}

# Yeah I might not need this later.
sub httpInit($) {
    ($gbHttp::Url) = @_;
    $gbHttp::handle = LWP::UserAgent->new;
    $gbHttp::handle->agent($gbHttp::AGENT_NAME);
}

# Retrying
sub httpHandleRetry($$) {
    my($tryNum, $msg) = @_;
    print STDERR "$msg\n";
    if ($tryNum >= $gbHttp::NUM_RETRIES) {
        print STDERR "bummer, maximum number of HTTP retries ($gbHttp::NUM_RETRIES) exceeded, giving up\n";
        endTask();
        exit(2);
    }
    sleep(($tryNum+1)*$gbHttp::RETRY_TIME);
}

# Get the header of the URL.  Return an HTTP::Response object.
sub httpGetHeader() {
    my $tryNum = 1;
    my $resp;
    while (!defined($resp)) {
	$resp = $gbHttp::handle->head($gbHttp::Url);
	if ($resp->is_error) {
	    httpHandleRetry($tryNum, "Error " .  $resp->code . " on attempt " . $tryNum);
	    $resp = undef;
	    $tryNum++;
	}
    }
    return $resp;
}

# Get the size of the URL.
sub httpGetSize() {
    my $resp = httpGetHeader();
    return $resp->content_length;
}

# Get the modification time (in internet time) on the URL.
sub httpGetModTime() {
    my $resp = httpGetHeader();
    return $resp->last_modified;
}

# Do the HTTP get
sub httpGet($) {
    my($localTmp) = @_;
    my $tryNum = 1;
    my $stat;
    while (!defined($stat)) {
	print "$localTmp\n$gbHttp::Url\n";
	$stat = $gbHttp::handle->get($gbHttp::Url);
        if ($stat->is_error) {	    
            httpHandleRetry($tryNum, "download of $gbHttp::Url to $localTmp failed for unknown reason");
	    $stat = undef;
	    $tryNum++;
        }
	# Save file.
	else {
	    open(TMP, ">$localTmp") || die "Could not open $localTmp for writing.\n";
	    print TMP $stat->content;
	    close(TMP);
	}
    }
}

# Safely save a remote file to a local file.  
#  - Never overwrites an existing file
#  - downloads to a tmp name and renames to prevent partial downloads
#  - verify before and after remote file sizes
#  - verify size of downloaded file
#  - make read-only after download
#  - if the file is compressed, run through zcat to verify
sub httpSafeGet($) {
    my($localFile) = @_;
    my $localTmp = "$localFile.$gbHttp::tmpSuffix";
    makeFileDir($localFile);
    unlink($localTmp);

    if (-e $localFile) {
        die("file exists, will not overwrite: $localFile");
    }

    my $startRemSize = httpGetSize();
    if ($main::verbose || 1) {  # FIXME:
        print "getting $gbHttp::Url\n";
    }
    httpGet($localTmp);

    # verify size
    my $localSize = getFileSize($localTmp);
    my $remSize = httpGetSize();
    if ($remSize != $startRemSize) {
        die("size at end of download ($remSize) does not match size at start ($startRemSize) for $gbHttp::Url");
    }
    if ($localSize != $remSize) {
        dir("size of just downloaded file $localTmp ($localSize) does not match $gbHttp::Url ($remSize)");
        return 0;
    }
    
    # set modification time from server and write protect
    my $modTime = httpGetModTime();
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
sub httpGetOrCheck($$) {
    my($justCheck, $localFile) = @_;

    my $remSize = httpGetSize();
    if (!defined($remSize)) {
        die("can't get file size for $gbHttp::Url");
    }
    if (-e $localFile) {
        my $localSize = getFileSize($localFile);
        my $remSize = httpGetSize();
        if ($localSize != $remSize) {
            die("size of existing file $localFile ($localSize) does not match $gbHttp::Url ($remSize)");
        }
        if ($main::verbose) {
            print STDERR "file exists with same size: $remSize == $localFile ($localSize)\n";
        }
        return 0;
    } elsif ($justCheck) {
        die("download file should exist: $localFile");
    } else {
        httpSafeGet($localFile);
        return 1;
    }
}

# perl requires a true value at the end
1;
