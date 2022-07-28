#!/usr/bin/perl
#
# create the autosave release header file
#
$release = $ARGV[0];
$now = localtime;
print "#define AUTOSAVE_RELEASE \"Autosave release $release, compiled $now\"\n";
