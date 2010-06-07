#
# HgStepManager: object for specifying and executing distinct sequential steps
#                of a process, with support for -continue and -stop options.
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/HgStepManager.pm instead.

# $Id: HgStepManager.pm,v 1.3 2007/03/10 16:47:21 kate Exp $
package HgStepManager;

use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use Carp;
use vars qw(@ISA @EXPORT_OK);
use Exporter;

@ISA = qw(Exporter);
@EXPORT_OK = qw( new getOptionHelp processOptions
		 stepOrder stepPrecedes getStartStep getStopStep
		 execute
		 @optionVars @optionSpec );

use vars qw( @optionVars @optionSpec );

@optionVars = qw(
    $opt_continue
    $opt_stop
    );

@optionSpec = ( "continue=s",
		"stop=s",
	      );

sub new {
  # Create and return an HgStepManager object.
  # Note the particular data structure that is expected for $spec:
  # - $spec is a reference to a list of hashes.
  # - Each hash specifies a step.  Hashes must appear in order of execution.
  # - Each hash is expected to have these keys: name, func.
  # - The value associated with name should be a single word that uniquely
  #   identifies a step.  This word can be used as a -continue or -stop option.
  # - The value associated with func should be a function reference.
  # Here is an example spec for a two-step process:
  # [ { name => 'setup', func => \&doSetup },
  #   { name => 'run',   func => \&doRun  },
  # ]
  my $class = shift;
  my ($spec) = @_;
  confess "Too few arguments" if (! defined $spec);
  confess "Too many arguments" if (scalar(@_) > 1);
  my $this = {};
  $this->{'spec'} = $spec;
  # Add num values to the step specs so we can do order comparisons,
  # build a hash of step names to step specs, and build a list of just 
  # the step names.
  my $stepNum = 0;
  my @stepNames = ();
  my %stepHash;
  foreach my $stepSpec ( @{$this->{'spec'}} ) {
    $stepSpec->{num} = $stepNum++;
    push @stepNames, $stepSpec->{name};
    $stepHash{$stepSpec->{name}} = $stepSpec;
  }
  $this->{'stepNames'} = \@stepNames;
  $this->{'stepHash'} = \%stepHash;
  $this->{'startStep'} = $stepNames[0];
  $this->{'stopStep'} = $stepNames[$#stepNames];
  bless $this, $class;
}

sub getOptionHelp {
  # Return description of command line options (for usage message).
  my $this = shift;
  confess "Too many arguments" if (scalar(@_) > 0);
  my $stepVals = join(", ", @{$this->{'stepNames'}});
  my $help =<<_EOF_
    -continue step        Pick up at the step where a previous run left off
                          (some debugging and cleanup may be necessary first).
                          step must be one of the following:
                          $stepVals
    -stop step            Stop after completing the specified step.
                          (same possible values as for -continue above)
_EOF_
  ;
  return $help;
}

sub processOptions {
  # Handle command line options.  Return nonzero if there is a problem that
  # indicates that the usage message should be displayed.
  my $this = shift;
  confess "Too many arguments" if (scalar(@_) > 0);
  if ($main::opt_continue) {
    my $step = $main::opt_continue;
    if (! exists $this->{'stepHash'}->{$step}) {
      warn "\nUnsupported -continue value \"$step\".\n";
      return 1;
    }
    $this->{'startStep'} = $step;
  }
  if ($main::opt_stop) {
    my $step = $main::opt_stop;
    if (! exists $this->{'stepHash'}->{$step}) {
      warn "\nUnsupported -stop value \"$step\".\n";
      return 1;
    }
    $this->{'stopStep'} = $step;
  }
  if ($this->stepOrder($this->{'startStep'}) >
      $this->stepOrder($this->{'stopStep'})) {
    warn "\n-stop step ($this->{stopStep}) must not precede -continue step " .
      "($this->{startStep}).\n";
    return 1;
  }
  return 0;
}

sub getStartStep {
  # Return the name of startStep.
  my $this = shift;
  confess "Too many arguments" if (scalar(@_) > 0);
  return $this->{'startStep'};
}

sub getStopStep {
  # Return the name of stopStep.
  my $this = shift;
  confess "Too many arguments" if (scalar(@_) > 0);
  return $this->{'stopStep'};
}

sub stepOrder {
  # Return the numeric order value of the given step, for order comparison.
  my $this = shift;
  my ($step) = @_;
  confess "Too few arguments" if (! defined $step);
  confess "Too many arguments" if (scalar(@_) > 1);
  if (! exists $this->{'stepHash'}->{$step}) {
    confess("unrecognized step name $step");
  }
  return $this->{'stepHash'}->{$step}->{'num'};
}

sub stepPrecedes {
  # Return true if first step strictly precedes second step.
  my $this = shift;
  my ($stepA, $stepB) = @_;
  confess "Too few arguments" if (! defined $stepB);
  confess "Too many arguments" if (scalar(@_) > 2);
  return ($this->stepOrder($stepA) < $this->stepOrder($stepB));
}

sub execute {
  # Execute the steps in order.
  my $this = shift;
  confess "Too many arguments" if (scalar(@_) > 0);
  my $startStep = $this->{'startStep'};
  my $stopStep = $this->{'stopStep'};
  &HgAutomate::verbose(1, "HgStepManager: executing from step '$startStep' " .
		       "through step '$stopStep'.\n");
  foreach my $stepName ( @{$this->{'stepNames'}} ) {
    if ($this->stepOrder($startStep) <= $this->stepOrder($stepName) &&
	$this->stepOrder($stepName)  <= $this->stepOrder($stopStep)) {
      my $now = localtime();
      &HgAutomate::verbose(1, "HgStepManager: executing step '$stepName' $now.\n");
      &{$this->{'stepHash'}->{$stepName}->{'func'}}();
    }
  }
}

# perl packages need to end by returning a positive value:
1;
