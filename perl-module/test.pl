# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

# $Header$

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..1\n"; }
END {print "not ok 1\n" unless $loaded;}
use SNIPS;
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):

# SNIPS::main(\&readconf, undef, \&do_test);
SNIPS::startup();
$datafile = $SNIPS::datafile;
$configfile = $SNIPS::configfile;
print "\t Datafile = $datafile\n\t Configfile= $configfile\n";

print "Testing event functions:\n";
#open(DATA, "> $datafile"); $datafd = fileno(DATA);
#SNIPS::write_dataversion($datafd);

use Fcntl;	# need O_WRONLY and other definitions
$datafd = SNIPS::open_datafile($datafile, O_WRONLY | O_CREAT | O_TRUNC);

$event = SNIPS::new_event();
print "\tgot new_event\n";

SNIPS::init_event($event);
print "\tdone init_event\n";

SNIPS::update_event($event, 1, 54321, 3);
print "\tdone update_event\n";

$bytes = SNIPS::write_event($datafd, $event);
print "\tdone write_event ($bytes bytes)\n";

SNIPS::update_event($event, 1, 12345, 3);
$bytes = SNIPS::write_event($datafd, $event);
print "\tdone write_event ($bytes bytes)\n";

SNIPS::close_datafile($datafd);
print "DONE\n\n";

@fields = SNIPS::get_eventfields();
print "Event fields are:\n   ", join (" ", @fields), "\n";

($status, $threshold, $maxseverity) = SNIPS::calc_status(2, 1, 3, 5);
print "\nTested calc_status()\n";

print "\nNow reading back events written to $datafile\n";

$datafd = SNIPS::open_datafile($datafile, O_RDONLY);
while ( ($event = SNIPS::read_event($datafd) ) )
{
  %event = SNIPS::unpack_event($event);
  print "--------\n";

  foreach $key (keys %event) {
    print "$key = $event{$key} ; ";
  }
  print "\n";
  print "REPACKING EVENT\n";
  $tevent = SNIPS::pack_event(%event);
  # SNIPS::print_event($tevent);

  print "Comparing event before and after packing  ";
  %tevent = SNIPS::unpack_event($tevent);
  foreach $key (keys %event) {
    if ($event{$key} != $tevent{$key}) {
      print "\nFAILED key= $key, $event{$key} != $tevent{$key}\n";
    }
    else { 
      print ".";
    }
  }
  print "DONE\n";

}

SNIPS::close_datafile($datafd);

if (SNIPS::get_reload_flag() > 0) {
  SNIPS::reload(\&readconf);
}

#SNIPS::done();

sub readconf {
  print "This is subroutine readconf()\n";
}

sub do_test {
  print "This is function dotest\n";
}
