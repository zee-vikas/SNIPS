package SNIPS;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);

require Exporter;
require DynaLoader;
require AutoLoader;

@ISA = qw(Exporter DynaLoader);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw(
	     debug do_reload autoreload dorrd
	    );

use vars qw ($configfile $datafile);

$VERSION = '0.01';

bootstrap SNIPS $VERSION;

# Preloaded methods go here.

## Always invoke readconf() for reading the config file.
sub startup {

  my $prognm = $0;
  my $sender = ($prognm =~ s|^.*/||);
  
  my $rc = SNIPS::_startup(\&readconf);
  $configfile = SNIPS::get_configfile();
  $datafile = SNIPS::get_datafile();
  return $rc;
}

## De-reference list pointer
sub get_eventfields {
  my $fields = SNIPS::_get_eventfields();
  return @$fields;
}

## Send XS routine a hash reference.
sub pack_event {
  my (%hevent) = @_;

  return undef if (! defined(%hevent));

  return SNIPS::_pack_event(\%hevent) ;
}

## de-reference hash pointer
sub unpack_event {
  my ($event) = @_;

  my $hevent = SNIPS::_unpack_event($event);
  return (%$hevent);
}

sub print_event {
  my ($event) = @_;

  my @fields = SNIPS::get_eventfields();
  my %event  = SNIPS::unpack_event($event);
  foreach my $f (@fields) {
    print "$f = $event{$f}\n";
  }

}

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__
# Below is the stub of documentation for your module. You better edit it!

=head1 NAME

snips - Perl extension for SNIPS (System & Network Integrated Polling Software)

=head1 SYNOPSIS

  use SNIPS;
  blah blah blah

=head1 DESCRIPTION

SNIPS.pm is a perl interface to SNIPS
(http://www.netplex-tech.com/software/snips) for writing Perl monitors.

=head1 AUTHOR

Vikas Aggarwal (vikas@navya_.com)

=head1 SEE ALSO

snips(8) perl(1).

=cut
