#!/usr/bin/perl
#
# $Id$
#
# This file is part of C TAP Harness.  The current version plus supporting
# documentation is at <http://www.eyrie.org/~eagle/software/c-tap-harness/>.
#
# Test POD formatting for INN documentation.
#
# Copyright 2009, 2010, 2013 Russ Allbery <eagle@eyrie.org>

use 5.006;
use strict;

use File::Spec;
use Test::More;

# Abort if SOURCE isn't set.
if (!$ENV{SOURCE}) {
    BAIL_OUT('SOURCE environment variable not set');
}

# Load the Test::Pod module.
if (!eval { require Test::Pod }) {
    plan skip_all => 'Test::Pod required for testing POD';
}
Test::Pod->import;

# SOURCE will be the test directory.  Change to the parent.
my ($vol, $dirs) = File::Spec->splitpath($ENV{SOURCE}, 1);
my @dirs = File::Spec->splitdir($dirs);
pop(@dirs);
if ($dirs[-1] eq File::Spec->updir) {
    pop(@dirs);
    pop(@dirs);
}
my $root = File::Spec->catpath($vol, File::Spec->catdir(@dirs), q{});
chdir($root) or BAIL_OUT("cannot chdir to $root: $!");

$Test::Pod::ignore_dirs{'.libs'} = 'Libs';

# Test all the POD in the root directory.
all_pod_files_ok(all_pod_files('.'));
