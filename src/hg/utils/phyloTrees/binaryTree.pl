#!/usr/bin/env perl

# convert NCBI taxonomy 'phylip' file.phy into a binary newick .nh tree
# These NCBI outputs are from the tool:
#    https://www.ncbi.nlm.nih.gov/Taxonomy/CommonTree/wwwcmt.cgi
# this type of output is from the function: 'Save as' 'phylip tree'

# Input file format:  It is actually newick format output, however it is
#    conveniently organized for parsing line by line instead of
#    having to scan input character by character.  Each line is one element of
#    the graph:
# 1. lines that are only left paren: ( start a new branch to the tree
# 2. lines that start: ')name:length' are internal node names
#    and close a branch,
# 3. each leaf of the tree is a line without ( or )
#    they have a name in 'single quotes' and a :length 'distance'
#    the spaces in the name will be replaced by _ and the
#    'single' quote marks removed.  Use :length if given or add :0.1
#    NCBI appears to output all lengths as '4'
# 4. For our purpose here, the commas in the input can be ignored.
#    They do appear correctly after node names or leaf element names to
#    indicate a sister element follows this element.  There is no comma
#    when it is the final element in a branch and or leaf with no sister
#
# The NCBI tree is often polytomy, for our purpose it needs to be converted
# to a binary tree.  The conversion to binary is done simply making binary
# nodes out of the polytomy elements, thereby pushing the sister elements
# into branches below the polytomy elements.

# parsing and node construction:
# each node in the tree is one of two possible types:
#    1. a 'branch' with right and left pointers to other elements.
#       can have an internal node name: name:distance
#    2. a 'leaf' element with a name:distance
#
# Start with 'root' node as an empty current node
#	There is a 'nextLeft' boolean that means the next element of this node
#	 goes on the left, otherwise new items are always placed on the right.
#       This boolean helps rectify polytomys into binary branches.
# 1. encounter open paren ( operation:
#    This starts a new 'branch' node.  It is inserted as the right
#	pointer of the current node when nextLeft FALSE
#	then nextLeft set to TRUE, else it goes into the left pointer
#	and nextLeft stays at TRUE.  This new 'branch' node
#	becomes the current node.  Polytomy is detected when
#	the left pointer is already occupied.  This left pointer moves to
#	the new node right pointer, and this new node becomes the 'current'
#	node.  nextLeft is set TRUE for this new node since right is taken.
# 2. encounter: 'string':length
#	This becomes a new 'leaf' node, 'name' and 'distance' are set.
#	Inserted on right pointer of current node when nextLeft FALSE
#	and nextLeft set to TRUE, else inserted on left pointer,
#	nextLeft stays at TRUE.
#	If nextLeft is TRUE and the left pointer is
#	already in use, this is a polytomy which creates a new 'branch'
#	node inserted into the left pointer, the left elements move down
#	as the right pointer of the new branch node, and this leaf
#	becomes the left pointer element.  This new branch node becomes
#	the current.  nextLeft stays at TRUE.
# 3. encounter )'string':length
#	This 'string' becomes the internal node 'name' of this branch, and
#	the distance is recorded.  The 'parent' of this branch becomes
#	the 'current' node.
# 4 effectively ignoring the commas.  They are supposed to mean the next
#	element goes into the left, but we are keeping track of this
#	with the nextLeft boolean.  The polytomys are a series of elements
#	more than two, with commas at the ends of the element names
#
# Tree print: recursive function:
# 1. if right pointer exists output (  and print right tree then
#        if left pointer, output )nodeName:length, and print left tree
# 2. no pointers, this is a leaf, print name:length when parent right
#	pointer got here, else print ,name:length when left pointer got here

use strict;
use warnings;
use Getopt::Long;

##############################################################################
sub usage() {
  printf STDERR "usage: binaryTree.pl [options] file.phy
options:
  -noInternal - do not output internal node names
  -defaultDistance=0.1 - use this distance when not given in input
  -allDistances=0.1 - use this distance for everything, default use input
  -lineOutput - output one line per leaf output, indented per depth
  -nameTranslate=<file> - two column file, translate names from input file,
	first column is name in input file, second column is output name
	tab separation columns
  -verbose=N - specify verbose debug printout, 0 nothing, 1 a bit, 2 more, etc
reads 'phylip' file format from NCBI taxonomy and outputs
binary newick tree format, resolving the polytomys common
to NCBI output format.  Output is to 'stdout'.\n";
  exit 255;
}

##############################################################################
# globals and options

my $noInternal = 0;	# option -noInternal - do not output internal node names
my $defaultDistance = "0.1";	# to set distances when not given in input
my $verbose = 0;	# verbose debug level, integer
my $allDistances = "";	# to set all distances to this value, default use input
my $lineOutput = 0;	# one line per leaf output format
my $nameTranslate = "";	# two column tab separated: inputName<tab>outputName
my %translateName;	# key is input name, value is output name

# establish empty root branch parent
my %root;
my $root = \%root;	# pointer handle to root node
$root->{'parent'} = undef;
$root->{'right'} = undef;
$root->{'left'} = undef;
$root->{'name'} = 'root';
$root->{'distance'} = $defaultDistance;
$root->{'nextLeft'} = 0;		# starts out false
# the following two are only on this root node for global bookeeping
$root->{'branchCount'} = 0;
$root->{'leafCount'} = 0;

##########################################################################
# forward declaration for recursive function
sub printTree($);

my $printDepth = 0;
my $printNewLine = 0;

# Tree print:
# 1. if this node is not a leaf:
#    a. print ( and print right tree
#    b. print , and print left tree
#    c. print )
# 2. then print the name:distance of this node
#
# the if defines were added to the prints "(" "," and ")" since it
#    was printing too many of those things.  It seems to work OK.

# this algorithm was found in the paper:
# http://people.sc.fsu.edu/~pbeerli/BSC-5936/08-29-05/Lecture_Notes_1.pdf
# algorithm 3 page 17

sub printTree($) {
  my ($node) = @_;
  if ($lineOutput && $printNewLine) {
     for (my $i =0; $i < ($printDepth-2); ++$i) {
       printf " ";
     }
     $printNewLine = 0;
  }
  ++$printDepth;
  if (! $node->{'isLeaf'}) {	# not a leaf, follow pointers right,left
     printf "(" if defined($node->{'left'});
     if (defined($node->{'right'})) {
	printTree($node->{'right'});
        printf "," if (defined($node->{'left'}));
        if ($lineOutput && defined($node->{'left'})) {
          printf "\n";
          $printNewLine = 1;
        }
     }
     printTree($node->{'left'}) if (defined($node->{'left'}));
     printf ")" if defined($node->{'left'});
  }
  my $distOut = sprintf("%.9f", $node->{'distance'});
  $distOut =~ s/0+$//g;
  $distOut = "0.000001" if ($distOut eq "0.");
  if ( $node->{'isLeaf'} ) {
    printf "%s:%s", $node->{'name'}, $distOut;
  } elsif ( ! $noInternal) {
    printf "%s:%s", $node->{'name'}, $distOut;
  } else {
    printf ":%s", $distOut;
  }
  $printDepth -= 1;
}	# sub printTree($)

##############################################################################
# start a new node element
sub newNode($$$$$$) {
  my ($parent, $right, $left, $name, $distance, $nextLeft) = @_;
  my %node;
  $node{'parent'} = $parent;
  $node{'right'} = $right;
  $node{'left'} = $left;
  $node{'name'} = $name;
  $node{'isLeaf'} = 0;
  $node{'distance'} = $distance;
  $node{'nextLeft'} = $nextLeft;
  return \%node;
}	# sub newNode($$$$$$)

##############################################################################
# Note functional description above of how this insert functions to resolve
#    polytomys
# to the current node, add a new node,
#	returns new node as current node
sub addNewBranch($) {
    my ($curNode) = @_;

    $root->{'branchCount'} += 1;
    my $nodeName = sprintf("branch.%d", $root->{'branchCount'});
  my $newNode = newNode($curNode, undef, undef, $nodeName, $defaultDistance, 0);

    if (0 == $curNode->{'nextLeft'}) {	# when false, insert on right
       if (defined($curNode->{'right'})) {
printf STDERR "# ERROR: addNewBranch: nextLeft is FALSE but right pointer is already used on '%s'\n", $curNode->{'name'};
printf STDERR "# not expecting this ?\n";
        exit 255;
       } else {	# good to go, newNode becomes right of current
	$newNode->{'parent'} = $curNode;
	$curNode->{'right'} = $newNode;
	$curNode->{'nextLeft'} = 1;
 	printf STDERR "# add new node %s as right child of parent %s\n",
	  $newNode->{'name'}, $curNode->{'name'} if ($verbose > 2);
       }
    } else {	# nextLeft is TRUE, see if node is fully occupied == POLYTOMY !
	if (defined($curNode->{'left'})) {	# move entire curNode down
          my $parent = $curNode->{'parent'};
          if (!defined($parent)) {	# only root has no parent
            $root->{'branchCount'} += 1;
            my $tmpName = sprintf("branch.%d", $root->{'branchCount'});
            printf STDERR "# create new node '%s' to take pointers from %s\n",
		$tmpName, $curNode->{'name'} if ($verbose > 2);
	    # need to make up a new node to hold the two pointers from curNode,
	    # and this new node becomes the right branch of this curNode,
	    # thereby freeing up the left branch of curNode
	    my $extraNode = newNode($curNode, $curNode->{'right'},
		$curNode->{'left'}, $tmpName, $defaultDistance, 1);
            $curNode->{'right'} = $extraNode;
	    $newNode->{'parent'} = $curNode;
	    $curNode->{'left'} = $newNode;
 	    printf STDERR "# add new node %s as left child of parent %s\n",
	       $newNode->{'name'}, $curNode->{'name'} if ($verbose > 2);
            return $newNode;	# all done here, do nothing else
          } else {	# left pointer already occupied == POLYTOMY !
 	    printf STDERR "# polytomy parent '%s' of current node '%s'\n",
		$parent->{'name'}, $curNode->{'name'} if ($verbose > 2);
          if ($parent->{'right'} == $curNode) {
 		printf STDERR "# gets new right child '%s'\n",
			$newNode->{'name'} if ($verbose > 2);
		$parent->{'right'} = $newNode;
          } elsif ($parent->{'left'} == $curNode) {
                printf STDERR "# gets new left child '%s'\n",
			$newNode->{'name'} if ($verbose > 2);
		$parent->{'left'} = $newNode;
          } else {
 printf STDERR "# what ? curNode '%s' has no pointer to it from parent '%s'\n", $curNode->{'name'}, $parent->{'name'};
 exit 255;
          }
       printf STDERR "# previous child %s becomes right child of new node %s\n",
	    $curNode->{'name'}, $newNode->{'name'} if ($verbose > 2);

          $newNode->{'right'} = $curNode;
          $newNode->{'parent'} = $curNode->{'parent'};
	  $curNode->{'parent'} = $newNode;
	  $newNode->{'nextLeft'} = 1;
          }
        } else {	# if (defined($curNode->{'left'})) == left is free
	  $newNode->{'parent'} = $curNode;
	  $curNode->{'left'} = $newNode;
	  printf STDERR "# add new node %s as left child of parent %s\n",
		$newNode->{'name'}, $curNode->{'name'} if ($verbose > 2);
        }
    }	# been managing leftNext TRUE
    return $newNode;
}	# sub addNewBranch($$)

##############################################################################
# clean up a leaf name, or use a translated name if given
##############################################################################
sub cleanLeafName($) {
  my ($cleanName) = @_;
  my ($leafName, $leafDistance) = split(':', $cleanName);
  $leafName =~ s/'//g;		# eliminate 'single quotes'
  $leafName =~ s/,//g;		# eliminate commas
  $leafName = "noName" if (length($leafName) < 1);
  $leafDistance = $defaultDistance if (!defined($leafDistance));
  $leafDistance = $allDistances if (length($allDistances) > 0);
  $leafDistance =~ s/,//;	# eliminate trailing comma
  if (defined($translateName{$leafName})) {
    $leafName = $translateName{$leafName};
  } else {
    $leafName=~ s/\s+/_/g;		# blanks to _
  }
  return ($leafName, $leafDistance);
}

##############################################################################
# clean up a node name, or use a translated name if given
##############################################################################
sub cleanNodeName($) {
  my ($cleanName) = @_;
  my ($nodeName, $nodeDistance) = split(':', $cleanName);
  $nodeName =~ s/\)//g;	# eliminate right ) paren
  $nodeName =~ s/;//g;		# eliminate ending semi-colon ;
  $nodeName =~ s/,//g;		# eliminate commas
  $nodeName =~ s/'//g;		# eliminate 'single quotes'
  $nodeName = "noName" if (length($nodeName) < 1);
  $nodeDistance = $defaultDistance if (!defined($nodeDistance));
  $nodeDistance = $allDistances if (length($allDistances) > 0);
  $nodeDistance =~ s/,//;	# eliminate trailing comma
  if (defined($translateName{$nodeName})) {
    $nodeName = $translateName{$nodeName};
  } else {
    $nodeName =~ s/\s+/_/g;		# blanks to _
  }
  return ($nodeName, $nodeDistance);
}

##############################################################################
#   main starts here
##############################################################################
my $argc = scalar(@ARGV);
if ($argc < 1) {
  usage;
}

GetOptions ("noInternal" => \$noInternal,
    "defaultDistance=f"  => \$defaultDistance,
    "verbose=i"  => \$verbose,
    "nameTranslate=s"  => \$nameTranslate,
    "lineOutput"  => \$lineOutput,
    "allDistances=f"  => \$allDistances)
    or die "Error in command line arguments\n";

$defaultDistance = $allDistances if (length($allDistances));

printf STDERR "# noInternal: %s\n", $noInternal ? "TRUE" : "FALSE";
printf STDERR "# defaultDistance: %f\n", $defaultDistance;
printf STDERR "# allDistances: %f\n", $allDistances if (length($allDistances));
printf STDERR "# nameTranslate from: %s\n", $nameTranslate if (length($nameTranslate));
printf STDERR "# lineOutput '%s'\n", $lineOutput ? "TRUE" : "FALSE";
printf STDERR "# verbose: %d\n", $verbose;

if (length($nameTranslate)) {
  open (FH, "<$nameTranslate") or die "can not read nameTranslate file '$nameTranslate'";
  while (my $line = <FH>) {
    chomp $line;
    my ($inName, $outName) = split('\t', $line);
    $translateName{$inName} = $outName;
  }
  close (FH);
}

my $phyFile = shift;
my $currentNode = $root;
printf STDERR "# reading %s\n", $phyFile if ($verbose > 0);

#############################################################################
# processing the input file
#############################################################################
open (FH, "<$phyFile") or die "can not read $phyFile";
while (my $line = <FH>) {
  chomp $line;
  $line =~ s/^\s+//;	# eliminate leading space, just in case garbage
  $line =~ s/\s+$//;	# eliminate trailing space, just in case garbage
  if ($line =~ m/^\(/) {
    $currentNode = addNewBranch($currentNode);
  } elsif ($line =~ m/^\)/) {	# name:length usually follows ) to name this
    my ($nodeName, $nodeDistance) = cleanNodeName($line);  # clean up the name
    printf STDERR "# node: '%s' becomes '%s:%s'\n", $currentNode->{'name'},
	$nodeName, $nodeDistance if ($verbose > 2);
    $currentNode->{'name'} = $nodeName;
    $currentNode->{'distance'} = $nodeDistance;
 $currentNode = $currentNode->{'parent'} if (defined($currentNode->{'parent'}));
    printf STDERR "# new currentNode: '%s'\n",
	$currentNode->{'name'} if ($verbose > 2);

  } else { #  encounter 'string:length,'  See procedure description above
    my ($leafName, $leafDistance) = cleanLeafName($line);
    my $newNode = newNode($currentNode, undef, undef, $leafName, $leafDistance, 0);
    $newNode->{'isLeaf'} = 1;
    if ($currentNode->{'nextLeft'}) {
	if (defined($currentNode->{'left'})) {	# POLYTOMY add new branch
	    $currentNode = addNewBranch($currentNode);
	}
 	$currentNode->{'left'} = $newNode;
 	$newNode->{'parent'} = $currentNode;
	printf STDERR "# new leaf '%s' added to left parent '%s'\n",
		$newNode->{'name'}, $currentNode->{'name'} if ($verbose > 2);
    } else {	# new leaf goes on right
	if (defined($currentNode->{'right'})) {
printf STDERR "# ERROR: adding new leaf nextLeft is FALSE but right pointer is used on '%s:%s'\n", $currentNode->{'name'}, $currentNode->{'distance'};
printf STDERR "# not expecting this ?\n";
           exit 255;
	} else {
	    $currentNode->{'right'} = $newNode;
	    $newNode->{'parent'} = $currentNode;
	    $currentNode->{'nextLeft'} = 1;
	    printf STDERR "# new leaf '%s' added to right parent '%s'\n",
		$newNode->{'name'}, $currentNode->{'name'} if ($verbose > 2);
	}
    }
    $root->{'leafCount'} += 1;
  }	# been managing 'string':length leaf element
}	# while (my $line = <FH>)
close (FH);

########################  output  ##########################################

printf STDERR "# tree print summary:\n" if ($verbose > 0);
printf STDERR "# branch count: %d\n", $root->{'branchCount'} if ($verbose > 0);
printf STDERR "# leaf count: %d\n", $root->{'leafCount'} if ($verbose > 0);
printTree($root);
printf ";\n";	# end of 'root' branch
