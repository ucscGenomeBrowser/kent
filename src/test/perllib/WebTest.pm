#
# WebTest: fetch pages, submit forms, check results etc.
#

package WebTest;

use strict;
use vars qw($VERSION @ISA @EXPORT_OK);
use Exporter;

use LWP::UserAgent;
# use LWP::Debug qw(+);
use HTTP::Cookies;
use HTML::Form;
use Carp;

@ISA = qw(Exporter);
@EXPORT_OK = qw( new configure checkPage getPage );
$VERSION = '0.01';

#
# new
#
sub new {
    my $class   = shift;
    my $ua      = shift;
    confess "Too few arguments" if (! defined $ua);
    my $verbose = shift;
    my $debug   = shift;
    my $config  = shift;
    confess "Too many arguments" if (defined shift);
    my $this = {
	'ua'     => $ua,
	'verbose'=> $verbose,
	'debug'  => $debug,
	'config' => $config,
    };
    bless $this, $class;
} # end new


#
# configure
#
sub configure {
    my $this   = shift;
    my $config = shift;

    if ((defined $config) && (ref($config) ne "HASH")) {
	logErr("WebTest::configure: \$config should be a ref to a hash, not $config.\n");
    } else {
	$this->{'config'} = $config;
    }
} # end configure

#
# checkPage
#
sub checkPage {
    my $this   = shift;
    my $page   = shift;
    my $query  = shift;
    my $config = shift;
    confess "too few args" if (! defined $page);
    confess "too many args" if (defined shift);
    $config = $this->{'config'} if (! defined $config);
    my $v = $this->{'verbose'};
    my $d = $this->{'debug'};

    my $req;
    if ((defined $query) && ($query ne "")) {
	$req  = HTTP::Request->new(POST=>$page);
	$req->content_type('application/x-www-form-urlencoded');
	$req->content("$query");
    } else {
	$req  = HTTP::Request->new(GET=>$page);
    }

    return($this->checkRequest($req));
} # end checkPage

#
# checkRequest
#
sub checkRequest {
    my $this   = shift;
    my $req  = shift;
    my $config = shift;
    confess "too few args" if (! defined $req);
    confess "too many args" if (defined shift);
    $config = $this->{'config'} if (! defined $config);
    my $v = $this->{'verbose'};
    my $d = $this->{'debug'};

    my $desc = &cleanReqStr($req->as_string());
    print "$desc\n" if ($v);

    my $res = $this->{'ua'}->request($req);

    my $ok;
    my $resStr = $res->as_string();
    if ($res->is_success()) {
	print "|$desc| fetch successful\n" if ($d);
	$ok = 1;
	#
	# If $config specifies patterns that must match, check those:
	#
	if (exists $config->{'mustMatch'}) {
	    foreach my $pat (@{$config->{'mustMatch'}}) {
		if ($resStr !~ m/$pat/i) {
		    $ok = 0;
		    logErr("WebTest::checkPage: $desc: mismatch on mustMatch pattern /$pat/\n");
		}
	    }
	}
	#
	# If $config specifies patterns that must *not* match, check those:
	#
	if (exists $config->{'mustNotMatch'}) {
	    foreach my $pat (@{$config->{'mustNotMatch'}}) {
		if ($resStr =~ m/$pat/i) {
		    $ok = 0;
		    logErr("WebTest::checkPage: $desc: match on mustNotMatch pattern /$pat/\n");
		}
	    }
	}
	#
	# If $config specifies a min size, check it:
	#
	if (exists $config->{'minSize'}) {
	    if (length($resStr) < $config->{'minSize'}) {
		$ok = 0;
		logErr(sprintf("WebTest::checkPage: $desc: result length %d < min %d\n", 
			       length($resStr), $config->{'minSize'}));
	    }
	}
	#
	# If $config specifies a max size, check it:
	#
	if (exists $config->{'maxSize'}) {
	    if (length($resStr) > $config->{'maxSize'}) {
		$ok = 0;
		logErr(sprintf("WebTest::checkPage: $desc: result length %d > max %d\n", 
			       length($resStr), $config->{'maxSize'}));
	    }
	}
    } else {
	&logErr("FETCH FAILED: |$desc|\n");
	$ok = 0;
    }

    print $resStr if ($d);
    return ($ok);
} # end checkPage


#
# getPage
#
sub getPage {
    my $this   = shift;
    my $page   = shift;
    confess "too few args" if (! defined $page);
    my $query  = shift;
    confess "too many args" if (defined shift);
    my $v = $this->{'verbose'};
    my $d = $this->{'debug'};

    my $req;
    my $desc;
    if ((defined $query) && ($query ne "")) {
	$req  = HTTP::Request->new(POST=>$page);
	$req->content_type('application/x-www-form-urlencoded');
	$req->content("$query");
	$desc = "$page?$query";
    } else {
	$req  = HTTP::Request->new(GET=>$page);
	$desc = "$page";
    }
    print "Fetching $desc\n" if ($v);

    my $res = $this->{'ua'}->request($req);
    my $resStr = $res->as_string();
    if ($res->is_success()) {
	return($resStr);
    } else {
	&logErr("FETCH FAILED: |$desc|\n");
	return undef;
    }
} # end getPage


sub cleanReqStr {
    my $str = shift;
    $str =~ s/POST //;
    $str =~ s/GET //;
    $str =~ s/\nContent-Type.*\n\n/?/;
    $str =~ s/\n$//;
    return $str;
} # end cleanReqStr


sub logErr {
    # could do better...
    print "--------------------------------------------------------------------------\n";
    print @_;
} # end logErr

# perl packages need to end by returning a positive value:
1;
