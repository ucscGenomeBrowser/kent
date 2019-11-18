#!/usr/bin/env perl

# a quick and dirty implementation of a look-alike parasol job management
# system
# given a jobList of commands, and N number specification,
# run N jobs at a time from the jobList, waiting while N jobs are running,
# when one finishes, start the next one.  After all jobs have been started,
# then wait for them all to finish.  Finally, show all exit codes for all jobs.

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 2) {
  printf STDERR "usage: perlPara.pl N cmd.list\n";
  printf STDERR "will run commands from the cmd.list file up to N at a time\n";
  exit 255;
}

my $nProcs = shift;
my $cmdList = shift;

if ($nProcs < 1) {
  printf STDERR "# ERROR: given N '%d' must be > 0\n", $nProcs;
  exit 255;
}

my @commands;	# the job commands to run, read in from the cmd.list
my @pidList;	# corresponding pid for each command
my %cmdPid;	# key is pid, value is command string
my %exitCodes;	# key is pid, value is exit code from child

############################################################################
### read in the jobList, save in @commands array
############################################################################
open (FH, "<$cmdList") or die "can not read $cmdList\n";
while (my $cmd = <FH>) {
  next if ($cmd =~ m/^#/);
  chomp $cmd;
  push @commands, $cmd;
}
close (FH);

my $cmdCount = scalar(@commands);
printf STDERR "# counted $cmdCount commands in $cmdList\n";

############################################################################
### while loop to start N jobs, wait for one to finish, start the next one
### keeping N jobs running until all the jobs have started
############################################################################
my %childPids;	# key is child PID, value is 1 running or 0 for done
my $processCount = 0;
my $nextCmd = 0;
while ($nextCmd < $cmdCount) {
  ### keep starting jobs until N number processes are running
  if ($processCount < $nProcs) {
#    printf STDERR "# starting process $nextCmd '%s'\n", $commands[$nextCmd];
    my $pid = -1;
    if(!defined($pid = fork())) {
       # fork returned undef, so unsuccessful
       die "Cannot fork process $nextCmd '%s' $!", $commands[$nextCmd];;
    } elsif ($pid == 0) {
      printf STDERR "# Child $nextCmd '%s' pid: $$\n", $commands[$nextCmd];
      my $ret = system($commands[$nextCmd]);
      $ret >>= 8;
      exit $ret;
#    exec("date") || die "can't exec date: $!";
    } else {
      # fork returned 0 nor undef
      # so this branch is parent
      printf STDERR "process $nextCmd '%s' pid: $pid\n", $commands[$nextCmd];
      $childPids{$pid} = 1;	# running
      push @pidList, $pid;
      $cmdPid{$pid} = $commands[$nextCmd];
      ++$nextCmd;
      ++$processCount;
    }
  } else {
    printf STDERR "# waiting for processes\n";
    ######### N jobs are running, wait for a job to finish to start a new one
    my $childPid = wait;
    my $ret = $?;
    printf STDERR "# odd childPid '$childPid' ret '$ret'" if ($childPid < 0);
    $exitCodes{$childPid} = $ret;
    printf STDERR "Completed %s pid $childPid ret '$ret'\n", $cmdPid{$childPid};
    $childPids{$childPid} = 0;	# done
    --$processCount;
  }
}

#############################################################################
### all jobs have been submitted, show the currently running jobs
#############################################################################
printf STDERR "# done running $cmdCount processes\n";
printf STDERR "# processCount is at: %d\n", $processCount;
my $stillRunning = 0;
foreach my $childPid (sort keys %childPids) {
  if ($childPids{$childPid} > 0) {
    printf STDERR "# child pid $childPid running %s\n", $cmdPid{$childPid};
    ++$stillRunning;
  }
}

#############################################################################
### all jobs have been submitted, now wait for the last ones to finish
#############################################################################
for (my $i = 0; $i < $stillRunning; ++$i) {
    printf STDERR "# waiting for processes\n";
    my $childPid = wait;
    my $ret = $?;
    printf STDERR "# odd childPid '$childPid' ret '$ret'" if ($childPid < 0);
    $exitCodes{$childPid} = $ret;
    printf STDERR "Completed %s pid $childPid ret '$ret'\n", $cmdPid{$childPid};
    $childPids{$childPid} = 0;	# done
    --$processCount;
}
printf STDERR "# processCount is at: %d\n", $processCount;

#############################################################################
### show all exit codes for all jobs
#############################################################################
printf STDERR "# process exit codes:\n";
for (my $i = 0; $i < $cmdCount; ++$i) {
  my $childPid = $pidList[$i];
  my $exitCode = $exitCodes{$childPid};
  printf STDERR "# cmd $i pid $childPid exit code '$exitCode' %d %s\n", $exitCode >> 8, $cmdPid{$childPid};
}
