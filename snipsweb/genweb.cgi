#!/usr/bin/perl -w
my $vcid = '$Id$ ';#

# my $vcid = '$Id$ ';
# ------------
# This CGI creates an HTML status page for collected snips events.
#
#  Various parameters can be used to customize.
#  view:   selects the view seen possible values are 
#   User   a critical-only non-updatable page for public use.
#          We suggest you link index.html to this file so that
#          it gets displayed by default.
#   Critical     Critical view - allows updating of status
#   Error        Error view    _   "       "      "    "
#   Warning      Warning view  _   "       "      "    "
#   Info         Information view _"       "      "    "
#
#  view:      see names above
#  refresh:   refresh interval in seconds, your browers should 
#             be able to handle this in the HTTP header, most modern browers
#             (IE, Netscape, Mozilla) do.
#  sound:     define 0 or '' if sound is not desired.
#  sort:      comma separted list of fields. Major sort the first field,
#             within that the 2nd field, etc.
#             sort fields are name, varname, siteaddr, severe, monitor
#             and varvalue
#  maxrows:   integer specifies the maximum number of entries to put 
#             output in tabular format. Beyond this number the items are 
#             displayed as verbatim output: HTML <pre> tag.
#  namepat:   narrow display to just those device names matching a regexp.
#  varpat:    narrow display to just those variable names matching a regexp.
#  monpat:    narrow display to just those monitors matching a regexp.
#  altprint:  A different table printing format mentioned on the mail archives
#
# Here is an example: 
# http://snips.snips.net/cgi-bin/genweb2.cgi?view=Info&sort=name,varname&refresh=120&sound=0&maxrows=70&monpat=^n
#
# which means: 
#
#  I want the Info display sorted by name and within that by variable
#  name.  The refresh rate is 120 seconds (2 minutes), don't play sound
#  on an alert and I only want to see monitors starting with the letter
#  'N' (ntpmon and nsmon)
#
# If I wanted all information about jefe I'd put:
#
# http://snips.snips.net:8890/cgi-bin/genweb2.cgi?view=Info&namepat=jefe
#
# This script also reads in an 'updates' file and hide's the event or adds
# a update message to an event. Once the site comes back up, this script
# will remove the status line from the 'updates' file (to prevent any
# confusion when the site goes down next).

# Any global 'messages' that are in the snips messages directory are
# also displayed.
#
# The snipsweb.cgi CGI program allows displaying help files, traceroutes,
# old-logs, and also updating the 'updates' file.
#
# INSTALLATION
# ------------
#
# 1. customize variables in snipsweb-cfg.pl
# 2. Create a '/snips/etc/updates' file with the owner being the httpd 
#    daemon's owner. (this will be edited by the cgi script)
# 3. Install the snipsweb.cgi CGI script in your CGI directory and set the
#    URL in this file. Install the entire 'gifs' dir under the
#    $baseurl directory.
# 4. If you want to create a separate 'User' view for outsiders, then you
#    can copy the 'Users.html' file to a public web site. Else create
#    a link from Critical.html to 'index.html' so that this is the
#    default page. You can either password protect this URL tree using
#    httpd's access mechanism (.htaccess) or else rely on the $etcdir/$authfile
#    file (see snipsweb.cgi). Either way, you have to create this authfile
#    to list the users who can edit the various snips files, etc.
# 5. Create help files, etc. under $snipsroot/device-help/
#    Note that these help files are HTML files and can contain HTML tags
#    for formatting and clickable links (see details in the CGI script).
#
# AUTHOR
#	- Richard Beebe (richard.beebe@yale.edu) 1998
#	- Updates: Vikas Aggarwal (vikas@navya_.com) 1998
#       - Modernized and turned into CGI with sorting and
#         selection: Rocky Bernstein (rocky@panix.com) 2000
#
#   This script was distributed as part of the SNIPS package.
#
#########################################################################

my $VERSIONSTR = "1.0";			# version
# Global variables. 
use vars qw (
	     $baseurl $webdir $snipsweb_cgi $sound $snipsroot $imageurl $etcdir
	     $bindir $datadir $msgsdir $updatesfile @views $view
             %view2severity $newevents $neweventAge
	     @level_color @level_imgs $emptyimg $tfontsize $max_table_rows
	     %updates $q $refresh $large_refresh $my_url $prognm @z1
             $sec $min $hour $mday $mon $year $weekday $yrday $daylite
	     $debug @row_data
	    );

$snipsroot = "/usr/local/snips"  unless $snipsroot;	# SET_THIS

init();
read_status_file($updatesfile);
my @dfiles = get_datafile_names($datadir);
process_parameters();

my $ADMINMODE = ($view eq "User") ? 0 : 1; # No href links for userPage
my $entries = 1;

get_row_data();
print_html_header();
print_html_prologue($view, $refresh);
print_column_headers();
print_row_data();

if (@row_data > $max_table_rows) {
  print "\n</PRE>\n";
}
else {
  # Print a helpful message if there's nothing wrong
  if ($entries == 1) {
    print "<TR><TD colspan=$numcols align=center bgcolor=\"\#CCCC99\">\n";
    print "<br><H3>The Network is <U>healthy</U>!</H3>";
    print "</TD></TR>\n";
  }
  
  print "<TR><TD  height=5></TD></TR>\n"; # vertical space
  print "</TABLE>\n";
}

print_msgs();		# stuff from msgs directory

if ($newevents > 0) {
  if ($sound) {
    print "
<EMBED src=\"$sound\" 
       TYPE=\"audio/x-wav\"
       autostart=true hidden=true loop=FALSE>\n";
    #print "<NO EMBED><bgsound=\"$sound\" loop=1></NO EMBED>\n"; # for IE
  }
  print "<P>$newevents new events (less than $neweventAge minutes old)</P>\n";
}

show_config($q) if $debug;

print_footer();	# closing stuff

exit 0;



#------------------------------------------------------------
sub init {
  # use CGI qw/:standard -debug/;
  use CGI;
  use URI;
  use URI::Escape;

  $etcdir  = "$snipsroot/etc"  unless $etcdir;	# location of config file
  $bindir  = "$snipsroot/bin"  unless $bindir;
  $datadir = "$snipsroot/data" unless $datadir;
  $msgsdir = "$snipsroot/msgs" unless $msgsdir;
  
  push(@INC, $bindir); push(@INC, $etcdir); # add to search paths for 'require'
  
  require  "snipsweb-cfg.pl";		# all configurable options
  require  "snipslib.pl";		# snips perl library

  # we have to append a trailing '/' in baseurl if it is specified.
  $baseurl .= "/" 
    if !defined($baseurl) || $baseurl ne "" && ! ($baseurl =~ /\/$/);
  
  # url for accessing the various snips images. Don't make it 'images' since
  # it usually gets mapped by the httpd daemon.
  $imageurl = "${baseurl}gifs/";  # check. Note baseurl has trailing /
  $imageurl .= "/" if ($imageurl ne "" && ! ($imageurl =~ /\/$/));
  
  # 'updatesfile' is the full path to the file containing update messages
  # Since this file is updated via the web (snipsweb.cgi), this file should
  # be owned by 'www' or 'httpd' to allow editing. this must match the
  # file location in the snipsweb.cgi script also.
  $updatesfile = "$etcdir/updates";		# check this
  
  @views = qw( User Critical Error Warning Info );
  %view2severity = ( User => 1, Critical => 1, Error => 2, Warning => 3,
		     Info => 4 );
  @level_color = ("", "Red", "Blue", "Brown", "Black");
  @level_imgs  = ("", "${imageurl}redsq.gif", "${imageurl}bluesq.gif",
		     "${imageurl}yellowsq.gif", "${imageurl}greensq.gif");
  
  ## These gif images are provided with snips. Put them in your httpd directory
  #  tree under $baseurl/images/
  $emptyimg = "${imageurl}empty.gif";
  $newevents = 0;
  
  @z1 = ('00' .. '59');	# To convert single digit minutes -> 08

  $q = new CGI;
  $my_url = $q->self_url;
  $my_url =~ s/;/&/g;

  $debug = 0 unless $debug;

}	# init()

# Read in the status file. Entries in this file will get a status
# message if not in User view. Global %updates set.
sub read_status_file {
  my ($updatesfile)=@_;
  if (!open (INPUT, "<$updatesfile")) {
    ooops("Can't open $updatesfile for reading: $!");
  }
  while (<INPUT>) {
    chomp;
    next if (/^\s*\#/ || /^\s*$/);   # skip comments & blank lines
    my ($addr, $update) = split(/\t/);
    $updates{$addr} = $update;
  }
  close (INPUT);
}	# read_status_file

# Get all log files from SNIPS data directory (extract only those
# files that have '-output' as prefix).
sub get_datafile_names {
  opendir(DATADIR, $datadir) 
    || ooops("\nCannot open input directory $datadir: $!");
  
  my @dfiles = sort grep(/.+-output/i, readdir(DATADIR));
  closedir(DATADIR);
  return @dfiles;
}

sub process_parameters {
  
  $view=$q->param('view');
# $view = 'User' if !defined($view);

  # Did we specify a sound to play? If so, what sound? 
  my $s=$q->param('sound');
  $sound=$s if defined($s) && $s ne '' && $s ne 'no' ;

  my $d=$q->param('debug');
  $debug=1 if defined($d) && $d ;
  
  # Did we specify a refresh rate? If so what values?
  my $r=$q->param('refresh');
  if (defined($r) && $r =~ /\d+/) {
    $refresh = $q->param('refresh');
  }

  # Did we specify compact Info format? 
  my $m=$q->param('maxrows');
  $max_table_rows=$m if defined($m);
  
}

#------------------------------------------------------------
# Print HTTP/HTML header and start body.
sub print_html_header {

  my $my_refresh=$refresh;
  $my_refresh = $large_refresh if (@row_data > $max_table_rows);

  my $action = $views[$view2severity{$view}];
  print $q->header(-Refresh=>"$my_refresh; CONTENT=$my_refresh; URL=$my_url",
                   -expires=>'now');
  print $q->start_html(-title=>"SNIPS - $action view", 
                       -type =>'text/css',
                       -bgcolor=>"#FFFFFF",
                       -link=>"#003366",
                       -vlink=>"#003366",
                       -alink=>"#003366"
                      ), "\n";
  print $q->comment("Generated by $vcid"), "\n";
}

# Print form buttons to select the severity.
sub print_html_prologue {
  my ($view, $refresh) = @_;

  my $action = $views[$view2severity{$view}];

  # Date stuff -- replace with Perl module Parse::Date?
  ($sec, $min, $hour, $mday, $mon, $year, $weekday, $yrday, $daylite) 
    = localtime(time);
  $mon++;
  $year += 1900;	# fix y2k problem

  $today = "$z1[$mon]/$z1[$mday]/$year  $hour:$z1[$min]";  # MMDDYY, US-centric
  #$today = "$year/$z1[$mon]/$z1[$mday]  $hour:$z1[$min]";  # YYMMDD
  $cursecs = time;	# get Unix timestamp


  print <<EOT;
    <!-- title banner -->
    <TABLE cellpadding=2 cellspacing=0 border=0>
     <tr><td height="6"> &#160 </td></tr>	<!-- vertical space -->
     <tr><td bgcolor="#003366">
	  <font class="header" face="arial,helvetica" size=4 color="#FFFFFF">
          <b>&nbsp;&nbsp; SNIPS (System and Network Integrated Polling Software)&nbsp;&nbsp;</b>
	  </font></td>
     </tr>
     <tr><td height="6"> &#160 </td></tr>	<!-- vertical space -->
    </TABLE>

    <P>
    <!-- last update date -->
    <TABLE width="100%" cellpadding=0 cellspacing=0 border=0>
     <tr><td width=50% align=left> &nbsp;
          <b>Current view: <FONT color=$level_color[$view2severity{$view}]>$action</FONT></b>
	 </td>
	<td align=right>
	 <FONT size="-1"><i>Last update: $today &nbsp;</i> </FONT>
        </td> </tr>
     <tr><td></td>
    <SCRIPT language="JavaScript">
	<!-- Begin
	updateTime = $cursecs;	// get unix timestamp
	now = new Date();
	age = ((now.getTime() / 1000) - updateTime) / 60;
        if (age > 0 && age < 1) { age = 0; }
        else { age = parseInt(age); }
	if ( age > 15 ) {
	  document.write('<td bgcolor=yellow align=right>');
          document.write('This data is <b>OUTDATED</b></td>');
	}
	else if (age < 0)
        {
	  document.write('<td align=right nowrap><font size="-1">(is your clock out of sync by ' + age + 'min ?)</font></td>');
        }
	else
        {
	  document.write('<td align=right><font size="-1">(updated ');
	  document.write(age + ' min ago)</font></td>');
	}
	onError = null;
	// End -->
    </SCRIPT>
     </tr>
     <!-- <tr><td height="6"></td></tr>	<!-- gap -->
    </TABLE>
    <P></P>
EOT
  
  my $esc_url = uri_escape($my_url);
  if ($ADMINMODE) {
    my $u = URI->new($my_url);
    my $p=$u->path;
    my $query=$u->query;
    print <<EOT1;
   <!--	--- buttons for other views --- -->
   <TABLE border = 0 cellpadding=0 cellspacing=5>
      <TR>
EOT1
    my $newq;
    foreach my $v (@views) {
      ($newq=$query) =~ s/view=[A-Za-z]+//;
      $newq .= '&' if $newq;
      $newq .="view=$v";
      print "<TD valign=middle><FORM action=\"$p?$newq\" method=\"get\">
	<input type=submit name=view value=\"$v\"></FORM></TD>\n";
    }
    print "<TD valign=middle><FORM action=\"${p}-config?$newq\" method=\"get\">
	<input type=submit value=\"Configure\"></FORM></TD>\n";


    print "
      <TD width=50>&nbsp;</TD>
      <TD valign=middle><FORM action=\"$snipsweb_cgi\" method=\"post\">
        <input type=submit name=command value=\"Help\">
        <input type=hidden name=return value=\"$esc_url\"></FORM>
    ";
    foreach ( qw(view refresh sound sort maxrows namepat varpat monpat
		 filepat altprint)) { 
      my $p =$q->param($_);
      next if !defined($p) || $p eq '' || $p eq 'no';
      print $q->hidden(-name => $_, -value=>$q->param($_));
    }
    print "</TD>
      </TR>
   </TABLE>
   <font face=\"arial,helvetica\" size=\"$tfontsize\">
   <i>Select a device name to update or troubleshoot it </i>
   </font> <P>";
  }
  else {
    print <<EOT1a;
   <P align="right">
   <FORM action="$snipsweb_cgi" method="get">
       <input type=submit name=command value="UserHelp">
       <input type=hidden name=return value="$esc_url">
   </FORM> </P>
EOT1a
  }

}	# print_html_prologue()

# print out the header for the that will be collected in @row_data.
# If data rows are going to be in tabular form, we print out the
# table header too.
sub print_column_headers {
  my @fields = ('', '#', 'Status', 'Device Name', 'Address', 
		'Variable / Value', 'Down At', 'Monitor', 'File');
  
  push @fields, 'Updates' 
    if $ADMINMODE || $userViewUpdates;
  
  $numcols = ($#fields + 1) * 2;
  
  if (@row_data > $max_table_rows)  {	
    # Print status in a compact format
    print $q->comment('     --- main data table --'), "\n";
    print "     <PRE> <b>\n";
    printf "%3s %1.1s %14s %15s  %12s %9s %11s %-15s %s\n",
      '  #', 'S', 'Device Name ', '  Address',
	'Variable', 'Value', 'Down At', 'Monitor', 'File', '  Updates';
    print "   </b>\n\n";
  } else {
    # Print status in a table format
  print <<EOT2;
    <!-- --- main data table --- -->
    <TABLE cellpadding=0 cellspacing=0 border=0>
       <!-- thin blank line -->
     <TR><td colspan=$numcols bgcolor="#000000">
	 <img src="$emptyimg"></td></TR>
       <!-- table header row -->
     <TR bgcolor="#FFFFFF">
EOT2
  
    foreach $field (@fields) {
      $field =~ s@\/@<br>&nbsp;\/@g ; # split words onto two separate lines
      print <<EOT2a;
      <td nowrap align=center><font face="arial,helvetica" size="$tfontsize"> &nbsp;
       <b>$field</b>  &nbsp;     </font></td>
      <td bgcolor="#AAAAAA" width=1>
	  <img src=\'$emptyimg\'></td>  <!-- thin vertical divider -->
EOT2a
    }	# foreach $field
  print "</TR>\n";
  
  print <<EOT2b;
    </TR>
      <!-- thin black line -->
    <TR><td colspan=$numcols bgcolor="#000000">
	<img src="$emptyimg"></td></TR>
EOT2b

  } # endif $view eq Info
}

sub get_row_data {
  my $namepat   = $q->param('namepat');
  my $varpat    = $q->param('varpat');
  my $monpat    = $q->param('monpat');
  my $filepat    =$q->param('filepat');

  foreach $dfile (@dfiles) {
    # process each data file
    my $file=$dfile;
    $file =~ s/-output//;
    next if defined($filepat) && $file !~ /$filepat/;
    if (!open (INPUT, "<$datadir/$dfile")) {
      logerr("cannot open $dfile for reading: $!");
      next;
    }
    &read_dataversion(INPUT);	# skip past first record

    # process lines of data file.
    while ( readevent(INPUT, 1) ) {
      
      # A whole bunch of global variables were set a and stored with
      # index 1 by readevent().
      # Save them elsewhere.
      my %r;
      $r{file}=$file;
      foreach $field ( 
         qw ( sender sitename siteaddr 
              varname varvalue varthres varunits
              mon day hour min
              severity loglevel nocop ) ) {
	my $cmd = sprintf '$r{%s}=$%s{1}', $field, $field; 
	eval $cmd;
      }
      
      ## clean up the sitename, etc.
      # $sitename =~ tr/a-zA-Z0-9._\-\(\)//cd;
      $r{sitename} =~ tr/\000//d;
      $r{siteaddr} =~ tr/\000//d;
      $r{varname}   =~ tr/\000//d;
      
      my $update = $updates{"$r{sitename}:$r{siteaddr}:$r{varname}"};
      #if ($update eq "") {$update = $updates{"$r{sitename}:$r{siteaddr}"}; }
      #if ($update eq "") {$update = $updates{"$r{sitename"}; }
      
      # If device is no longer critical, remove its status information
      if (($r{severity} > 1) && $update) {
	&remove_updates_entry($r{sitename}, $r{siteaddr}, $r{varname});
      }

      # Are we interested in this event?
      next if (defined($namepat) && $r{sitename} !~ /$namepat/);
      next if (defined($varpat)  && $r{varname}  !~ /$varpat/);
      next if (defined($monpat)  && $r{sender}   !~ /$monpat/);
      
      if ($r{severity} <= $view2severity{$view}) {
	# Yep, we want this event.
	select_sound($r{severity}, $view);
	push @row_data, \%r;
	++$entries ;
      }
    } 
  }	# end foreach($dfile), process next log file

  # Sorting comes here...
  my $sort = $q->param('sort');
  if (defined($sort)) {
    my @ordering = split(/,/, $sort);
    while (@ordering) {
      $order = pop @ordering;
      if ($order eq 'name') {
	@row_data = sort { $a->{sitename} cmp $b->{sitename} } @row_data;
      } elsif ($order eq 'severe') {
	@row_data = sort { $a->{severity} cmp $b->{severity} } @row_data;
      } elsif ($order eq 'varname') {
	@row_data = sort { $a->{varname} cmp $b->{varname} } @row_data;
      } elsif ($order eq 'siteaddr') {
	@row_data = sort { $a->{siteaddr} cmp $b->{siteaddr} } @row_data;
      } elsif ($order eq 'varvalue') {
	@row_data = sort { $a->{varvalue} cmp $b->{varvalue} } @row_data;
      } elsif ($order eq 'monitor') {
	@row_data = sort { $a->{sender} cmp $b->{sender} } @row_data;
      }
    }
  }

}

# write in data collected from the xxxmon programs.
#
sub print_row_data {

  print $q->comment("--- Start of real data rows --"), "\n";

  my $np = $q->param('altprint');
  my $print_routine = defined($np) && $np ne 'no' ? 
      \&print_row_new : \&print_row_old;

  # Dump out processed rows...
  my $i=1;
  foreach $r (@row_data) {
    &$print_routine($r, $i++);  
  }
}

#------------------------------------------------------------
## Write out one row of data.

#  Alternates the row colors. Also, if the event is new (less than
#  $neweventAge minutes old), it sets the button background to yellow.
#  If $view is 'Info', then does not write out in table format so that
#  the size of the data file is small.
#
sub print_row_old {
  my ($r, $entry_count) = @_;
  my  $ifnewbg = "";	# new background if new event

  my @rowcolor = ("#FFFFcc", "#D8D8D8");	# alternating row colors
  my $action   = $views[$view2severity{$view}];

  # the data is already clean since the new snipslib.pl uses 'A' to unpack
  # which strips out all the nulls.
  # &clean_data($i);	# delete unwanted characters (not needed anymore)

  my $update = ($updates{"$r->{sitename}:$r->{siteaddr}:$r->{varname}"} 
		or '') ;
  #if ($update eq "") {$update = $updates{"$r->{sitename}:$r->{siteaddr}"}; }
  #if ($update eq "") {$update = $updates{"$r->{sitename}"}; }

  # hide if Critical display
  return if $update =~ /^\(H\)/ && $action eq 'Critical';

  my $esc_url = uri_escape($my_url);
  my $siteHREF = "<A HREF=\"$snipsweb_cgi?displaylevel=$action";
  $siteHREF .= "&sitename=$r->{sitename}&siteaddr=$r->{siteaddr}";
  $siteHREF .= "&variable=$r->{varname}&sender=$r->{sender}";
  $siteHREF .= "&command=Updates&return=$esc_url\">";

  if (@row_data > $max_table_rows) {
    # need to put the href in front of the sitename, but we dont want
    # the sitename to be prepended with underlined blanks. i.e. convert
    #   '<a href="xx">   site</a>'  INTO  '  <a href="xx">site</a>'
    my $site = sprintf "%-14.14s", $r->{sitename}; # printing size
    $site =~ s|(\S+)|$siteHREF$1</a>| ;
    printf "%3d %1.1s %s %-15.15s  %12.12s=%9lu %02d/%02d %02d:%02d %-15s %-15s %s\n",
      $entry_count, $views[$r->{severity}], $site,
      $r->{siteaddr}, $r->{varname}, $r->{varvalue},
      $r->{mon},$r->{day},$r->{hour},$z1[$r->{min}], $r->{sender}, 
      $r->{file}, $update;
    return;
  }

  ## see if this is a recent event (less than $neweventAge minutes old)
  if ($mon == $r->{mon} && $mday == $r->{day} &&
      (($hour * 60) + $min) - (($r->{hour} * 60) + $r->{min}) < $neweventAge)
  {
    ++$newevents;			# total displayed in Messages
    $ifnewbg = "bgcolor=yellow";	# background of the little button
  }

  my $tdstart = "<td nowrap align=\"left\"> <font face=\"arial,helvetica\" size=\"$tfontsize\"> \&nbsp\;\n";
  my $tdend = "</font> </td>\n   <td bgcolor=\"\#AAAAAA\" width=1>\n";
  $tdend .= "<img src=\"${emptyimg}\" alt=\"&nbsp;\"></td>\n";

  ## begin the row of data
  # 	ser-no  severity  sitename  address  variable+value
  print "<TR bgcolor=\"$rowcolor[$entry_count % 2]\">\n";
  print "<td $ifnewbg><font>";
  if ($ADMINMODE) { print "$siteHREF"; }
  print "<img src=\"$level_imgs[$r->{severity}]\" alt=\"\" border=\"0\">";
  if ($ADMINMODE) { print "</a>"; }
  print $tdend;

  my $sitename = $r->{sitename};
  if ($ADMINMODE) {
    $siteHREF =~ s/command\=Updates/command\=SiteHelp/;  # change the command
    $sitename = "$siteHREF" . "$r->{sitename}" . "</a>";
  }

  print <<EoRow ;
    $tdstart $entry_count $tdend
    $tdstart $views[$r->{severity}]  $tdend
    $tdstart $sitename $tdend
    $tdstart $r->{siteaddr} $tdend

    <td nowrap align=right><font face="arial,helvetica" size="$tfontsize">
    &nbsp; $r->{varname}=$r->{varvalue} &nbsp;
    $tdend

    <td nowrap align=right $ifnewbg><font face="arial,helvetica" size="$tfontsize">
    $r->{mon}/$r->{day} $r->{hour}:$z1[$r->{min}]  $tdend
EoRow

  print "
     $tdstart $r->{sender} $tdend
     $tdstart $r->{file}   $tdend
";
  if ($ADMINMODE || $userViewUpdates) {
    print "$tdstart $update $tdend"; 
  }
  print "</tr>";	# end of row
}	# print_row_old()

#------------------------------------------------------------
## Write out one row of data.
#  Alternates the row colors. Also, if the event is new (less than 5 minutes
#  old), it sets the button background to yellow.
#  If $view is 'Info', then does not write out in table format so that the
#  size of the data file is small.
#
sub print_row_new
{
  my  ($r, $entry_count) = @_;
  my  $ifnewbg = '';  # new background if new event
  
  my  $RED    =   '#FF0000';
  my  $ORANGE =   '#FF9900';
  my  $YELLOW =   '#FFFF00';
  my  $BLACK  =   '#000000';
  my  $WHITE  =   '#FFFFFF';
  my  $GREY   =   '#AAAAAA';
  my  @rowcolor = (
		     $RED,       #   1       critcal
		     $ORANGE,        #       2       error
		     $YELLOW,        #       3       warning
		     $WHITE,         #       4       info
		    );      # row colours for different severities
  my  @textcolour = (
		       "$WHITE",       #       1       critcal
		       "$BLACK",       #       2       error
		       "$BLACK",       #       3       warning
		       "$BLACK",       #       4       info
		      );      # row colours for different severities
  
  my  $action     = $views[$view2severity{$view}];
  
  # the data is already clean since the new snipslib.pl uses 'A' to unpack
  # which strips out all the nulls.
  # &clean_data($i);	# delete unwanted characters (not needed anymore)
  
  my $update = ($updates{"$r->{sitename}:$r->{siteaddr}:$r->{varname}"} 
		or '') ;
  #if ($update eq "") {$update = $updates{"$r->sitename:$r->siteaddr"}; }
  #if ($update eq "") {$update = $updates{"$r->sitename"}; }
  
  # hide if Critical display
  return if $update =~ /^\(H\)/ && $action eq 'Critical';

  my $esc_url = uri_escape($my_url);
  my $siteHREF = "<A HREF=\"$snipsweb_cgi?displaylevel=$action";
  $siteHREF .= "&sitename=$r->{sitename}&siteaddr=$r->{siteaddr}";
  $siteHREF .= "&variable=$r->{varname}&sender=$r->{sender}";
  $siteHREF .= "&command=Updates&return=$esc_url\">";

  if (@row_data > $max_table_rows) {
    # need to put the href in front of the sitename, but we dont want
    # the sitename to be prepended with underlined blanks. i.e. convert
    #   '<a href="xx">   site</a>'  INTO  '  <a href="xx">site</a>'
    my $site = sprintf "%-14.14s", $r->{sitename}; # printing size
    $site =~ s|(\S+)|$siteHREF$1</a>| ;
    printf "%3d %1.1s %s %-15.15s  %12.12s=%9lu %02d/%02d %02d:%02d %-15s %-15s %s\n",
      $entry_count, $views[$r->{severity}], $site,
      $r->{siteaddr}, $r->{varname}, $r->{varvalue},
      $r->{mon},$r->{day},$r->{hour},$z1[$r->{min}], $r->{sender}, 
      $r->{file}, $update;
    return;
  }

  ## see if this is a recent event (less than $neweventAge minutes old)
  if ($mon == $r->{mon} && $mday == $r->{day} &&
      (($hour * 60) + $min) - (($r->{hour} * 60) + $r->{min}) < $neweventAge)
  {
    ++$newevents;                   # total displayed in Messages
    $ifnewbg = "bgcolor=yellow";    # background of the little button
  }
  
  # construct a <font face=... size=... color=...> tag, with colour to contrast with row colour for this severity level
  my  $font_tag   = sprintf qq (<font face="arial,helvetica" size="%s" color="%s">), $tfontsize, $textcolour[$r->{severity}-1] ;
  
  my  $tdstart    = qq(<td nowrap align="left">$font_tag &nbsp;) ,
    
    my  $tdend  = qq(</font> </td>
		     <td bgcolor="$GREY" width=1><img src="${emptyimg}" alt="&nbsp;"></td>\n);
  
  ## begin the row of data
  #   ser-no  severity  sitename  address  variable+value
  
  print qq(<TR bgcolor="$rowcolor[$r->{severity}-1]">\n);
  
  print "<td $ifnewbg><font>";
  if ($ADMINMODE) { print "$siteHREF"; }
  print qq(<img src="$level_imgs[$r->{severity}]" alt="" border="0">);
  if ($ADMINMODE) { print "</a>"; }
  print $tdend;
  
  my  $sitename   = $r->{sitename};
  if ($ADMINMODE) {
    $siteHREF =~ s/command\=Updates/command\=SiteHelp/;  # change the command
    $sitename = $siteHREF . $font_tag . $r->{sitename} . '</font></a>' ;
  }
  
  print <<EoRow ;
    $tdstart $entry_count $tdend
    $tdstart $views[$r->{severity}]  $tdend
    $tdstart $sitename $tdend
    $tdstart $r->{siteaddr} $tdend

    <td nowrap align=right>$font_tag
    &nbsp; $r->{varname}= $r->{varvalue} &nbsp;
    $tdend
    <td nowrap align=right $ifnewbg>$font_tag
    $r->{mon}/$r->{day} $r->{hour}:$z1[$r->{min}]  $tdend
EoRow

  print "
     $tdstart $r->{sender} $tdend
     $tdstart $r->{file}   $tdend
";
  if ($ADMINMODE || $userViewUpdates) {
    print "$tdstart $update $tdend"; 
  }
  print "</tr>";      # end of row
}       # print_row()

#------------------------------------------------------------
## Print out the file contents from the MSGSDIR if any.
sub print_msgs {
   ## Now write out the messages from the message dir. Perhaps should be
   #  in a separate frame.
   if (opendir(MSGSDIR, "$msgsdir")) {
     @msgfiles = grep (!/^\./, readdir(MSGSDIR) );
     if ($#msgfiles >= 0) {
       print "<center><hr noshade width=\"100\%\"><h3>Messages</h3></center>\n"; }
     foreach $mfile (@msgfiles)
     {
       if (open(F, "< $msgsdir/$mfile"))
       {
	 # print STDERR "Opened $mfile\n" if $debug;
	 print "<P>";
	 while (<F>) { print ; }
	 close (F);
       }
     }				# foreach()
     closedir(MSGSDIR);
   }				# if opendir()
}

sub show_config {
  my($query) = @_;
  my(@values,$key);

  print "<H2>Here are the current settings in this form</H2>";
  
  foreach $key ($query->param) {
    print "<STRONG>$key</STRONG> -> ";
    @values = $query->param($key);
    print join(", ",@values),"<BR>\n";
  }
}

#------------------------------------------------------------
##
sub print_footer {
   ## simple footer
  print $q->p("<hr width=\"20\%\" shade align=\"left\">"), "\n";
  print "<font size=\"-2\">
<a href=\"http://www.netplex-tech.com/software/snips\">SNIPS- v$VERSIONSTR</a>";
   print "</body></html>\n";
}

##------------------------------------------------------------
# This routine deletes an entry from the updates file if it is
# no longer critical.
#
# If you've added a status for a device that's been down and that
# device  comes  up,  we  want  to  remove that status to prevent
# confusion should that device go down at a later date.

sub remove_updates_entry {
  local ($sitename, $ipaddr, $varname) = @_;

  open (SFILE, "< $updatesfile") || return; 

  local (@list) = <SFILE>;	# slurp into memory
  close (SFILE);
  # if we're not successful in opening it this time, we'll just
  # wait until the next time through.
  open (SFILE, ">> $updatesfile") || return ;
  foreach (1..3) {	# try locking the file three times...
    if (flock(SFILE, 2)) {
      seek(SFILE, 0, 0);
      truncate(SFILE, 0);
      last;
    }
    if ($_ == 3) {
      print STDERR "Could not flock $updatesfile, cannot remove entry $sitename\n";
      return;
    }
    sleep 1;
  }
  
  foreach (@list)
  {
    # skip comments and blank lines
    if (/^\s*\#/ || /^\s*$/) { print SFILE; next; }
    print "$sitename:$ipaddr:$varname<br>\n" if $verbose;
    next if /^$sitename\:$ipaddr\:$varname/; # dont write out, delete
    print "not removed<P>\n" if $verbose;
    
    print SFILE;

  }		# foreach

  close (SFILE);

}	# remove_entry()


# Decide which, if any, sound file should be played.
# My set global $sound.
sub select_sound {
  my ($severity, $view) = @_;
  return if !defined ($sound) || !$sound ;

  if ($severity == $view2severity{"Critical"}) { 
    $sound = "critical.wav";
  } elsif (($severity == $view2severity{"Error"}) 
      && ($sound ne "critical.wav")) {
    $sound = "error.wav";
  } elsif (($severity == $view2severity{"Warning"}) &&
	   (($sound ne "critical.wav") && ($sound ne "error.wav"))) {
    $sound = "warning.wav";
  }
}

# log error to syslog and print to HTML.
sub logerr {
  my($msg) = shift;

  logger($msg);
  print $q->comment("error: $msg"), "\n";
}

sub ooops {
  my ($msg) = @_;
  print_html_header();
  print $q->h2($msg);
  print_footer();
  exit 1;
}
