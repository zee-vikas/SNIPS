## $Header$

require 5.000;

use ExtUtils::MakeMaker;

# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
# e.g.
#	perl Makefile.PL CC="$(CC)" OPTIMIZE="$(CFLAGS)" LIBS="$(LIBS)" \
#		INSTALLSITELIB=$(ROOTDIR)/lib/perl \
#		INSTALLSITEARCH=$(ROOTDIR)/lib/perl \
#		INSTALLARCHLIB=$(ROOTDIR)/lib/perl
#
##
##  -lc_nonshared  needed on Linux if compiled with -g
#  Need to link with RRD library also if needed.

WriteMakefile(
    'NAME'	=> 'SNIPS',
    'VERSION_FROM' => 'SNIPS.pm', # finds $VERSION
    'LIBS'	=> ["-L../lib -lsnips"],   # e.g., '-lm' 
    'DEFINE'	=> "-O",     # e.g., '-DHAVE_SOMETHING' 
    'INC'	=> '-I../include',     # e.g., '-I/usr/include/other'
    #
    'depend'	=> {'SNIPS.c' => '../lib/libsnips.a'},
    'clean'	=> {FILES => "pod2html-*cache" },
);


sub MY::postamble {
'
init: 
	@echo ""
';
}

