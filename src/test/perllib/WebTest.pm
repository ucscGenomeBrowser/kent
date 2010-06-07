#
# WebTest: fetch pages, submit forms, check results etc.
#
# WebTest uses a configuration hash ref with a very specific structure:
#
# $config = { 'mustMatch'    => [ $goodPat,... ],
#             'mustNotMatch' => [ $badPat,...  ],
#             'minSize'      => $n,
#             'maxSize'      => $m,
#           };
# 
# $config is a ref to a hash that may contain any of the keys 
# 'mustMatch', 'mustNotMatch', 'minSize', and/or 'maxSize' .
# The keys 'mustMatch' and 'mustNotMatch' map to references to lists 
# of patterns; a pattern is a string that will be interpreted as a 
# regexp on the HTML returned from a query.  
#
# $config can be passed to a WebTest object when it is created, 
# when it is checking a page, or with the method configure().  
# When WebTest is checking a page, for each of the above keys that 
# are defined in its $config, it will perform a test against the 
# HTML for the page.  If any test fails, WebTest will describe the 
# failure(s) and return 0 for failure.  
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
# new: create a WebTest object.
# Mandatory argument: $ua = LWP::UserAgent for making HTTP requests.
# Optional arguments: $sleep (# seconds)
#                     $verbose (flag)
#                     $debug (flag)
#                     $config (config hash - see comments above)
#
sub new {
    my $class   = shift;
    my $ua      = shift;
    confess "Too few arguments" if (! defined $ua);
    my $sleep   = shift;
    my $verbose = shift;
    my $debug   = shift;
    my $config  = shift;
    confess "Too many arguments" if (defined shift);
    my $this = {
	'ua'     => $ua,
	'sleep'  => $sleep,
	'verbose'=> $verbose,
	'debug'  => $debug,
	'config' => $config,
    };
    bless $this, $class;
} # end new


#
# configure: set this object's $config.
# Mandatory argument: $config (config hash - see comments above)
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
# checkPage: given URL info, fetch the page and check it.
# Mandatory argument: $page (URL, up to but not includig the '?' for CGI's)
# Optional arguments: $query (for CGI's, whatever goes after the '?')
#                     $config (config hash - see comments above)
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
# checkRequest: given an HTTP::Request object, fetch and check it.
# Mandatory argument: $req (HTTP::Request object)
# Optional arguments: $config (config hash - see comments above)
#
sub checkRequest {
    my $this   = shift;
    my $req  = shift;
    my $config = shift;
    confess "too few args" if (! defined $req);
    confess "too many args" if (defined shift);
    $config = $this->{'config'} if (! defined $config);
    my $sleep = $this->{'sleep'};
    my $v = $this->{'verbose'};
    my $d = $this->{'debug'};

    my $desc = &cleanReqStr($req->as_string());
    print "$desc\n" if ($v);

    sleep($sleep) if ($sleep);
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
# getPage: given URL info, fetch & return HTML
# Mandatory argument: $page (URL, up to but not includig the '?' for CGI's)
# Optional arguments: $query (for CGI's, whatever goes after the '?')
#
sub getPage {
    my $this   = shift;
    my $page   = shift;
    confess "too few args" if (! defined $page);
    my $query  = shift;
    confess "too many args" if (defined shift);

    my $req;
    if ((defined $query) && ($query ne "")) {
	$req  = HTTP::Request->new(POST=>$page);
	$req->content_type('application/x-www-form-urlencoded');
	$req->content("$query");
    } else {
	$req  = HTTP::Request->new(GET=>$page);
    }
    return($this->getRequest($req));
} # end getPage

#
# getRequest: given an HTTP::Request object, fetch & return HTML
# Mandatory argument: $req (HTTP::Request object)
#
sub getRequest {
    my $this   = shift;
    my $req   = shift;
    confess "too few args" if (! defined $req);
    confess "too many args" if (defined shift);
    my $sleep = $this->{'sleep'};
    my $v = $this->{'verbose'};
    my $d = $this->{'debug'};

    my $desc = &cleanReqStr($req->as_string());
    print "Fetching $desc\n" if ($v);

    sleep($sleep) if ($sleep);
    my $res = $this->{'ua'}->request($req);
    my $resStr = $res->as_string();
    if ($res->is_success()) {
	return($resStr);
    } else {
	&logErr("FETCH FAILED: |$desc|\n");
	return undef;
    }
} # end getRequest


# internal use only, for making more succinct request descriptions
sub cleanReqStr {
    my $str = shift;
    $str =~ s/POST //;
    $str =~ s/GET //;
    $str =~ s/\nContent-Type.*\n\n/?/;
    $str =~ s/\n$//;
    return $str;
} # end cleanReqStr


# internal use only, for reporting errors
sub logErr {
    # could do better...
    print "--------------------------------------------------------------------------\n";
    print @_;
} # end logErr

# perl packages need to end by returning a positive value:
1;
