package SNIPS;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);
use vars qw($opt_a $opt_d $opt_f $opt_o $opt_x);
use Getopt::Std;

require Exporter;
require DynaLoader;
#require AutoLoader;	# requires one package statement per file

@ISA = qw(Exporter DynaLoader);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw(
	     &snips_main  &snips_startup  &snips_reload
	     &read_event &write_event &new_event
	     &pack_event &unpack_event &print_event
	     $s_configfile $s_datafile $s_sender
	     $E_CRITICAL $E_ERROR $E_WARNING $E_INFO
	     $n_UP $n_DOWN $n_UNKNOWN $n_TEST $n_NODISPLAY
	    );

@EXPORT_OK = qw(
	       );

use vars qw (
	     $autoreload $debug $do_reload $dorrd 
	     $prognm $pollinterval $libdebug
	     $s_configfile $s_datafile $s_sender $extension
	     $HOSTMON_SERVICE $HOSTMON_PORT $bindir $ping $rpcping
	     $E_CRITICAL $E_ERROR $E_WARNING $E_INFO
	     $n_UP $n_DOWN $n_UNKNOWN $n_TEST $n_NODISPLAY
	     $dummy
	    );

$VERSION = '0.01';

bootstrap SNIPS $VERSION;

# Preloaded methods go here.
tie $debug, 'SNIPS::globals', 'debug';
tie $dorrd, 'SNIPS::globals', 'dorrd';
tie $do_reload, 'SNIPS::globals', 'do_reload';
tie $autoreload, 'SNIPS::globals', 'autoreload';
tie $s_configfile, 'SNIPS::globals', 'configfile';
tie $s_datafile, 'SNIPS::globals', 'datafile';

## Some cruft from the old snipslib.pl file, FIX FIX FIX

$HOSTMON_SERVICE = "hostmon" unless $HOSTMON_SERVICE; # for 'hostmon'
$HOSTMON_PORT = 5355 unless $HOSTMON_PORT; # used if not in /etc/services

$ping = "ping";				# SET_THIS to ping location
$rpcping = "$bindir/rpcping" unless $rpcping; # SET_THIS, used by 'hostmon'

$pollinterval = 300 ;	# set to a default value in seconds
$libdebug = 0;

$E_CRITICAL = 1;
$E_ERROR    = 2;
$E_WARNING  = 3;
$E_INFO     = 4;

$n_UP          = 0x01;
$n_DOWN        = 0x02;
$n_UNKNOWN     = 0x04;
$n_TEST        = 0x08;
$n_NODISPLAY   = 0x10;

##  ### ### ### ##

## following are aliases for functions (safer to @EXPORT)
sub snips_main {  return ( SNIPS::main(@_) ); }

sub snips_startup {  return SNIPS::startup(@_); }

sub snips_reload { return SNIPS::reload(@_); }

sub open_datafile { return SNIPS::fopen_datafile(@_); }

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

  $prognm = $0;
  $test_func = \&do_test  if ( ! defined($test_func) );	# FIX FIX

  if ( ! defined($poll_func) && !defined($test_func) ) {
    print STDERR ("FATAL: need either poll_function or test_function ",
		  "set when calling ", (caller(0))[3], "()\n" );
    exit 1;
  }

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

    if ( $do_reload ) {
      reload($readconf_func);
      next;	# dont sleep
    }

    my $polltime = time - $starttm;
    print STDERR "$prognm: polltime = $polltime secs\n"  if ($debug);

    if ($polltime < $pollinterval) {
      my $sleeptime = $pollinterval - $polltime;
      print STDERR "$prognm: Sleeping for $sleeptime secs\n"  if $debug;
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
  if ($opt_f) { $s_configfile = $opt_f; }
  if ($opt_o) { $s_datafile = $opt_o ; }
  if ($opt_x) { $extension = $opt_x ; }

}

## Sets up signal handlers, kills other running process, sets 
#  default  config and data filenames.
sub startup {

  $prognm = $0;
  $s_sender = $prognm;
  $s_sender =~ s|^.*/||;	# without the directory prefix

  my $rc = SNIPS::_startup($prognm, $extension);

  return $rc;
}

## Perl version of the C function. Close all open filehandles BEFORE
#  calling this routine. Needs reference to read_conf function.
sub reload {
  my ($readconfig_func) = @_;

  my $ndatafile = $s_datafile . ".hup";	# new datafile

  print STDERR "Reloading...";
  $do_reload = 0;		# reset at start

  if (! defined($readconfig_func)) {
    print STDERR "Cannot reload, no readconfig function set in program\n";
    return -1;
  }
  my $odatafile = $s_datafile;
  $s_datafile = $ndatafile;	# temporarily set to new filename

  &$readconfig_func();		# writes to new datafile

  $s_datafile = $odatafile;	# restore
  
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

  $do_reload = 0;		# reset
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

  # $debug = 3;
  print STDERR "inside generic poll_sites\n" if ($debug > 2);

  my $fd = open_datafile($s_datafile, "r+");	# open for read + write

  while ( ($event = read_event($fd) ) ) 
  {
    ++$siteno;

    # call test function
    # only $value is really needed, rest can be undef.
    # If $thres is undef, update_event() will keep old threshold
    ($status, $value, $thres, $maxsev) = &$test_func(\$event, $siteno);

    $maxsev = $E_CRITICAL  if (! defined($maxsev));
    if ( ! defined ($status) )
    {
      my %event = unpack_event($event);
      ($status, $thres, $maxsev) = 
	calc_status($value, $event{threshold}, $event{threshold},
		    $event{threshold});
    }
    
    update_event($event, $status, $value, $thres, $maxsev);
    rewrite_event($fd, $event);
    last if ($do_reload > 0);

  }	# while (readevent)

  print STDERR "poll_sites(): Processed $siteno sites\n" if ($debug > 1);
  close_datafile($fd);
  return 1;
}	# sub poll_sites()

#### To access the C variables  #####
#
# Cannot use Autoloader if putting another package in the
# same .pm file. These functions call XSUB routines.
package SNIPS::globals;
use Carp;

sub TIESCALAR {
  my $class = shift;
  my $var = shift;	# debug or dorrd or...
  # print "TIESCALAR, blessing $var in class $class\n";
  return bless \$var, $class;
}

sub FETCH {
  my $self = shift;
  confess "wrong type" unless ref $self;
  croak "usage error" if @_;
  my $value;
  local ($!) = 0;
  $value = SNIPS::globals::_FETCH($self);
  if ($!) { croak "get_function failed: $!"; }
  return $value;
}

sub STORE {
  my $self = shift;
  confess "wrong type" unless ref $self;
  my $newval = shift;
  croak "usage error" if @_;

  # print "Trying to STORE ", $$self, " with value $newval\n";
  unless ( defined SNIPS::globals::_STORE($self, $newval) ) {
    confess "Could not set ", $$self, ": $!";
  }
  return $newval;
}

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__
# Below is the stub of documentation for your module. You better edit it!

=head1 NAME

snips - Perl extension for SNIPS (System & Network Integrated Polling Software)

=head1 SYNOPSIS

  use SNIPS;

=head1 DESCRIPTION

SNIPS.pm is a perl interface to SNIPS
(http://www.netplex-tech.com/software/snips) for writing Perl monitors.

=head2 API FUNCTIONS

The following functions can be called by specifying the SNIPS package name:

    SNIPS::function()

Alternatively, some functions are exported into the calling file's namespace
as a convenience. These are aliases for the main(), reload() etc. functions
below, but can be called without specifying the complete package name:

	     snips_main()
	     snips_startup()
	     snips_reload()
	     snips_datafile()

=over

=item main(\&readconfig, \&dopoll, \&dotest)

This is a generic main, and it will call the user defined functions dopoll()
or if undefined, it will invoke a generic dopoll() with dotest() for each
site.

Note the '\' before the '&' to pass function references to main().

The readconfig() function should ideally write out an event for each device
to be monitored to the output file.
If dopoll() is defined, then this function must do one complete pass over
monitoring all the sites. main() will call dopoll() in an endless loop and
check for any HUP signal between polls to see if the readconfig() function
should be called to reload the new config file.

If dopoll() is NOT defined, but dotest() is defined, then this function is
invoked with the EVENT and the event-number (starting with 1) as input
parameters. The function should return a list with the following fields:

       ($status, $value, $thres, $max_severity) = &dotest(EVENT *ev, count)

Instead of using main(), a monitor can use the following instead (in
pseudocode, no parameters are listed for the functions):

	startup()
	sub readconfig()
	{
		open_datafile()
		new_event()
		foreach device
		{
			alter_event()
			write_event()
		}
		close_datafile()
	}
	while (1)
	{
	  sub dopoll()
	  {
		open_datafile()
		while (read_event())
		{
			sub dotest()
			{
				%event = unpack_event()
				### test function here ###
				calc_status()
				update_event() # or set %event fields directly
				# pack_event() if changed %event fields
				write_event()
			}
			close_datafile()
			if ($do_reload)
			   reload()
			sleep()
		} # end while
	  } # end dopoll

	}  # end while

=item startup()

=item reload(\&readconfig)

On getting a HUP signal, the 'doreload' flag is set to a non-zero value. On
detecting this flag, you should call reload() with the reference to the
readconfig() function as a parameter.

This function will then call readconfig() (which should write out the initial
datafile by re-reading the config file), and then copy over current monitored 
state information from the current datafile to the new datafile. It resets
the reload flag before returning.

=item EVENT * new_event()

=item init_event(EVENT * event)

=item alter_event(EVENT *ev, sender, sitename, siteaddr, varname, varunits)

This is a utility routine which allows you to set the fields in the EVENT
structure without having to unpack() and then pack(). ALL the fields are
character strings, and any of the fields can be 'undef' to leave it
unchanged. To delete a field, set it to the empty string "".

=item update_event(EVENT *ev, status, value, thres, severity)

This updates the event 'ev' with the specified values. 'status' is 0 or 1,
'value' is the measured/monitored value, 'thres' is the new threshold that
has been exceeded, 'severity'  is the new severity of the event. 'thres' can
be 'undef' if it is unchanged (i.e. not working with 3 thresholds).

=item open_datafile()

=item close_datafile()

=item calc_status(int value, int warn_thres, error_thres, crit_thres)

If the monitor has 3 separate thresholds for the variable being monitored,
calc_status() is a utility routine which calculates which threshold the value
exceeds. It returns:

	 ($status, $threshold, $maxseverity) = calc_status(...)

This is typically called to get the parameters to call update_event()


=item read_event()

=item write_event()

=item pack_event()

=item unpack_event()

=back

=head2 VARIABLES

=over

=item $s_configfile  $s_datafile

These are the configfile and datafile names, and automatically created by the 
startup() function.

=item $E_INFO $E_WARN $E_ERROR $E_CRITICAL

These are constants for the various severity levels.

=item $debug $dorrd $autoreload $doreload

More global variables.

=back

=head1 AUTHOR

Vikas Aggarwal (vikas@navya_.com)

=head1 SEE ALSO

snips(8) perl(1).

=cut
