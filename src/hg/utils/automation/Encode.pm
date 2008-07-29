#
# Encode: support routines for ENCODE scripts
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/Encode.pm instead.
#
# $Id: Encode.pm,v 1.1 2008/07/29 19:12:55 larrym Exp $

package Encode;

use warnings;
use strict;

use RAFile;

our $fieldConfigFile = "fields.ra";
our $vocabConfigFile = "cv.ra";
our $labsConfigFile = "labs.ra";

sub getLabsConf
{
    my ($configPath) = @_;
    my %labs;
    if(-e "$configPath/$labsConfigFile") {
        # tolerate missing labs.ra in dev trees.
        %labs = RAFile::readRaFile("$configPath/$labsConfigFile", "lab");
    }
    return %labs;
}

1;
