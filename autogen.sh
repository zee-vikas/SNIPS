#!/bin/sh

# Create the config/ directory beforehand, automake will not do so.
mkdir -p config

# Use aclocal to generate the aclocal.m4 file.
aclocal --install -I m4

# Use automake to copy the config.guess, config.sub files.  Ignore all
# other output but the Installing bits since we don't use automake in
# the first place.
automake --force --add-missing 2>&1 | fgrep 'installing'

# Create a fake install-sh file to make autoconf happy. But found up
# to 2.63 release and the current (2009-02-19) GIT version of
# autoconf.
touch config/install-sh cmu-snmp/snmp/install-sh

# Let autoheader do its magic
autoheader --force

# Then finally use autoconf to generate the final file!
autoconf --force

# We need to manually do the same for cmu-snmp though.
pushd cmu-snmp/snmp

# Here autoreconf will mostly do it.
autoreconf -fi

# We symlink the .sub/.guess files though
ln -sf ../../config/config.{guess,sub} .

popd
