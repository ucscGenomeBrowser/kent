#!/usr/bin/perl
use Net::SFTP::Foreign;
my $host="sftpsrv.sanger.ac.uk";
open(INFO, "sftp.info") or die("Could not open  file.");
my $username = <INFO>;
my $password = <INFO>;
chomp $username;
chomp$password;
print $username, $password;
my $sftp = Net::SFTP::Foreign->new(host=> $host, password => $password, user => $username);
$sftp->die_on_error("Unable to establish SFTP connection");
$sftp->setcwd("decipher-agreements/pub") or die "unable to change cwd: " . $sftp->error;
my ($file) = $sftp->glob("decipher-cnvs*txt.gpg", names_only => 1);
$sftp->get($file, $file) or die "get failed: " . $sftp->error;
($file) = $sftp->glob("decipher-snvs*txt.gpg", names_only => 1);
$sftp->get($file, $file) or die "get failed: " . $sftp->error;
($file) = $sftp->glob("decipher-variants*bed.gpg", names_only => 1);
$sftp->get($file, $file) or die "get failed: " . $sftp->error;
