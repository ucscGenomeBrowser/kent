#
# SafePipe class: a safe replacement for system; checks every command in a pipe for failure
# (which is NOT possible with `` or system).
#
# We also capture stderr from all commands in a separate buffer.
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/SafePipe.pm instead.

# $Id: SafePipe.pm,v 1.1 2008/07/26 02:10:00 larrym Exp $

package SafePipe;

use warnings;
use strict;

sub new
{
# $args{CMDS} (required): list of commands
# $args{STDOUT} (optional): file to use for stdout; if not provided, we copy text of stdout into
# the STDOUT field. Use STDOUT => '/dev/null' if you want to discard stdout.
    my ($class, %args) = (@_);
    die "$class: missing CMDS list" unless(defined($args{CMDS}) && ref($args{CMDS}) eq 'ARRAY');
    my $ref = {};
    $ref->{$_} = $args{$_} for (keys %args);
    bless $ref, $class;
    return $ref;
}

sub stderr
{
# returns text of STDERR from executed commands
    my ($obj) = @_;
    return $obj->{STDERR};
}

sub stdout
{
# returns text of STDOUT from executed commands; use only
# if you did NOT provide a STDOUT file to the new method.
    my ($obj) = @_;
    return $obj->{STDOUT};
}

sub cmd
{
# returns text of executed command pipe (useful for debugging)
    my ($obj) = @_;
    return $obj->{CMD};
}

sub statuses
{
# returns list of statuses from executed command (useful if you want to
# print very specific errors).
    my ($obj) = @_;
    return $obj->{STATUSES};
}

sub exec
{
# Exececute the pipe; returns sum of statuses, to facilitate quick test by caller
# for success. Individual statuses are available via statuses method.
    my ($obj) = @_;
    my $errFile = "/tmp/SafePipe$$.err";
    `rm $errFile` if(-e $errFile);
    my $stdoutCreated = 0;
    my $stdoutFile = $obj->{STDOUT};
    if(!(defined($stdoutFile) && -e $stdoutFile)) {
        $stdoutFile = "/tmp/SafePipe$$.out";
        $stdoutCreated = 1;
    }
    my $cmd = join(" 2>> $errFile | ", @{$obj->{CMDS}});
    $cmd .= " > $stdoutFile 2>> $errFile; echo \${PIPESTATUS[@]}";
    $obj->{CMD} = $cmd;

    my $output = `$cmd`;
    open(ERR, $errFile) || die "ERROR: Can't open error file \'$errFile\': $!\n";
    $obj->{STDERR} = join("", <ERR>);
    close(ERR);
    `rm $errFile`;
    if($stdoutCreated) {
        open(OUT, $stdoutFile) || die "ERROR: Can't open stdout file \'$stdoutFile\': $!\n";
        $obj->{STDOUT} = join("", <OUT>);
        close(OUT);
        `rm $stdoutFile`;
    }
    my @list = split(/\s+/, $output);
    print STDERR "cmd: $cmd; output: $output\n" if($obj->{DEBUG});
    $obj->{STATUSES} = \@list;
    $obj->{SUM} = 0;
    $obj->{SUM} += $_ for (@list);
    return $obj->{SUM};
}

1;
