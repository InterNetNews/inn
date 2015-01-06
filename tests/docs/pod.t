#!/usr/bin/perl
#
# $Id$
#
# Test POD formatting.
#
# This file is part of C TAP Harness.  The current version plus supporting
# documentation is at <http://www.eyrie.org/~eagle/software/c-tap-harness/>.
#
# Copyright 2009, 2010, 2013, 2014 Russ Allbery <eagle@eyrie.org>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

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

# Check spelling of everything in root if present, and any *.pod files in the
# current directory.
my @pod_files = glob('*.pod');
if (-d '.') {
    push(@pod_files, all_pod_files('.'));
}
all_pod_files_ok(@pod_files);
