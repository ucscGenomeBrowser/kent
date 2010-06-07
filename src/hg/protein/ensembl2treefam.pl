#!/usr/bin/perl -w
#
#Author: Ruan Jue <ruanjue@gamil.com>
#

=head1
INPUT: STDIN
#ucsc_acc       ens_gene_id
uc004fqe.1      ENST00000250784
...
OUTPUT: STDOUT
URL=http://www.treefam.org/cgi-bin/TFinfo.pl?ac=TF300612&highlight=RPS4Y1_HUMAN
#ucsc_acc       treefam_ac        highlight_symbol
uc004fqe.1      TF300612          RPS4Y1_HUMAN
=cut

use warnings;
use strict;
use DBI;


my $db     = 'treefam_6';
my $host   = 'db.treefam.org';
my $port   = 3308;
my $user   = 'anonymous';
my $passwd = '';

my $dbh = DBI->connect("dbi:mysql:$db:$host:$port", $user, $passwd) or die($!);

my $sth1 = $dbh->prepare(qq{select ID from genes where GID = (select GID from genes where ID like ?)});

my $sth2 = $dbh->prepare(qq{select f.ac, a.disp_id from fam_genes f, aa_full_align a where f.ID = ? and a.ID = f.ID and a.AC = f.AC});

my $last_ensid = '';
my $last_ac = '';
my $last_disp_id = '';

while(<>){
	next if(/^#/);
	my @tabs = split;
	if($last_ensid eq $tabs[1]){
		print_rs($tabs[0], $tabs[1], $last_ac, $last_disp_id);
		next;
	}
	($last_ensid, $last_ac, $last_disp_id) = trans_id($tabs[0], $tabs[1]);
}

$dbh->disconnect;

1;

sub trans_id {
	my ($ucid, $ensid) = @_;
	my ($ac, $id, $disp_id);
	$sth1->execute("$ensid%");
	while(($id) = $sth1->fetchrow_array){
		$sth2->execute($id);
		($ac, $disp_id) = $sth2->fetchrow_array;
		$sth2->finish;
		last if($ac);
	}
	$sth1->finish;
	print_rs($ucid, $ensid, $ac, $disp_id);
	return ($ensid, $ac, $disp_id);
}

sub print_rs {
	my ($ucid, $ensid, $ac, $disp_id) = @_;
	if($ac){
		print "$ucid\t$ac\t$disp_id\n";
	} else {
		print "#$ucid\t$ensid\n";
	}
}
