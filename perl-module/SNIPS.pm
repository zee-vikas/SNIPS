package SNIPS;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);
use vars qw($opt_a $opt_d $opt_f $opt_o $opt_x);
use Getopt::Std;

require Exporter;
require DynaLoader;
require AutoLoader;

@ISA = qw(Exporter DynaLoader);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw(
	     &snips_main  &snips_startup  &snips_reload
	     &read_event &write_event &new_event
	     &pack_event &unpack_event &print_event
	     $configfile $datafile
	    );

@EXPORT_OK = qw(
	       );

use vars qw (
	     $autoreload $debug $myname
	     $configfile $datafile $extension
	     $HOSTMON_SERVICE $HOSTMON_PORT $bindir $ping $rpcping
	     $libdebug
	    );

$VERSION = '0.01';

bootstrap SNIPS $VERSION;

# Preloaded methods go here.

## Some cruft from the old snipslib.pl file, FIX FIX FIX

$HOSTMON_SERVICE = "hostmon" unless $HOSTMON_SERVICE; # for 'hostmon'
$HOSTMON_PORT = 5355 unless $HOSTMON_PORT; # used if not in /etc/services

$ping = "ping";				# SET_THIS to ping location
$rpcping = "$bindir/rpcping" unless $rpcping; # SET_THIS, used by 'hostmon'

$libdebug = 0 unless $libdebug;

##  ### ### ### ##

## following are aliases for functions (safer to @EXPORT)
sub snips_main {  return ( SNIPS::main(@_) ); }

sub snips_startup {  return ( SNIPS::startup(@_) ); }

sub snips_reload { return ( SNIPS::reload(@_) ); }


## snips_main()
# This is a sample generic main(). You dont have to call this function.
# Requires references to the read_config and do_test functions, e.g.
#
#	SNIPS::main(\&read_conf, \&do_test)
#
#  to read configfile -	readconf()
#  to test one site  -	do_test()

sub main {
  my ($readconf_func, $poll_func, $test_func) = @_;

  $myname = $0;
  # my $prognm = $myname;
  # $prognm =~ s|^.*/||;
  if ( ! defined($poll_func) && !defined($test_func) ) {
    print STDERR ("FATAL: need either poll_function or test_function ",
		  "set when calling ", (caller(0))[3], "()\n" );
    exit 1;
  }

  my $pollinterval = 60 ;	# FIX FIX, should be in calling routine
  parse_opts();

  startup();

  open_eventlog();

  &$readconf_func();

  while (1)
  {
    my $starttm = time;
    if (defined ($poll_func)) {
      done() if ( &$poll_func() < 0 ) ;
    }
    else {
      done () if ( SNIPS::poll_sites($test_func) < 0 );
    }

    check_configfile_age()  if ($autoreload);

    if ( get_reload_flag() ) {
      reload();
      next;
    }

    my $polltime = time - $starttm;
    print STDERR "$myname: polltime = $polltime secs\n"  if ($debug);

    if ($polltime < $pollinterval) {
      my $sleeptime = $pollinterval - $polltime;
      print STDERR "$myname: Sleeping for $sleeptime secs\n"  if $debug;
      $SIG{ALRM} = 'DEFAULT';	# restore in case blocked
      sleep($sleeptime);
    }
    
  } # while(1)
}	# sub main()

## Set global variables after parsing command line options.
#
sub parse_opts {

  getopts("adf:o:x:");	# sets $opt_x

  if ($opt_a) { ++$autoreload ; } # will reload if configfile modified
  if ($opt_d) { ++$debug ; }
  if ($opt_f) { set_configfile($opt_f) ; }
  if ($opt_o) { set_datafile($opt_o) ; }
  if ($opt_x) { $extension = $opt_x ; }

  set_debug_flag($debug) if ($debug);
  set_autoreload_flag($autoreload) if ($autoreload);
}

## Sets up signal handlers, kills other running process, sets 
#  default  config and data filenames.
sub startup {

  $myname = $0;
  # my $prognm = $myname;
  # $prognm =~ s|^.*/||;

  my $rc = SNIPS::_startup($myname, $extension);

  $configfile = get_configfile() if (! $configfile);
  $datafile = get_datafile() if (! $datafile);

  return $rc;
}

## Perl version of the C function. Close all open filehandles BEFORE
#  calling this routine. Needs reference to read_conf function.
sub reload {
  my ($readconfig_func) = @_;

  my $ndatafile = $datafile . ".hup";	# new datafile

  print STDERR "Reloading...";

  my $odatafile = $SNIPS::datafile;
  $SNIPS::datafile = $ndatafile;	# temporarily set to new filename

  &$readconfig_func();			# writes to new datafile

  $SNIPS::datafile = $odatafile;	# restore
  
  copy_events_datafile($odatafile, $ndatafile);

  unlink $odatafile;
  if (rename ($ndatafile, $odatafile) != 1) {
    print STDERR "failed. FATAL rename() $!\n";
    SNIPS::done();
    exit -1;
  }
  else {
    print STDERR "done\n";
  }

  set_reload_flag(0);		# reset
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

## print event structure, useful for debugging
sub print_event {
  my ($event) = @_;

  my @fields = SNIPS::get_eventfields();
  my %event  = SNIPS::unpack_event($event);
  foreach my $f (@fields) {
    print "$f = $event{$f}\n";
  }
}

## generic poll function. Need a reference to the test function in the
#  arg.
#
sub poll_sites {
  my ($test_func) = @_;
  my $siteno = 0;
  my $event = undef;
  my ($status, $value, $thres, $maxsev);

  open (OEVENTS, "+> $datafile");	# open for read + write
  my $fd = fileno(OEVENTS);		# get file descriptor

  read_dataversion($fd);

  while ( ($event = read_event($fd) ) ) 
  {
    ++$siteno;
    ($status, $value) = &$test_func(\$event, $siteno);
    if ( ! defined ($status) )
    {
      my %event = unpack_event($event);
      ($status, $thres, $maxsev) = 
	calc_status($value, $event{threshold}, $event{threshold},
		    $event{threshold});
    }
    
    update_event($event, $status, $value, $maxsev);
    rewrite_event($fd, $event);
    last if (get_reload_flag());

  }	# while (readevent)

  close (OEVENTS);
  return 1;
}	# sub poll_sites()

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
