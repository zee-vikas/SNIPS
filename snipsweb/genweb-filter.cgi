#!/usr/bin/perl -w
#
# CGI to display GUI to invoke genweb2.cgi with appropriate parameters.
#
#
my $vcid = '$Id$ ';

use vars qw ( $debug $refresh $large_refresh $genweb2_cgi
	      $snipsroot $etcdir $max_table_rows
	    );
$snipsroot = "/usr/local/snips";	# SET_THIS
$etcdir = "$snipsroot/etc";
push (@INC, $etcdir);

init();
my $a=$query->param('Action');
my @monitors = qw ( etherload hostmon ippingmon nsmon ntpmon portmon 
                    rpcpingmon trapmon ) ;

if (defined($a) &&  $a eq 'Submit') {
  my $url=make_url($query);
  print $query->redirect($url);
} else {
  set_defaults($query);
  print $query->header;
  print $query->start_html("SNIPS Filter Configurator");
  print "<H1> SNIPS Filter Configurator</H1>\n";
  &print_prompt($query);
  show_config($query) if $debug;
  print $query->end_html;
}

###
### Subroutines
###
sub init {
  require  "snipsweb-cfg.pl";		# in etcdir
  use CGI;	# also requires Base64.pm
  $query = new CGI;
  $debug= $query->param('debug') if $query->param('debug');
}

sub print_prompt {
  my($query) = @_;
  
  print $query->start_form;

  print "<P><EM>Which view?</EM><BR>\n",
    $query->radio_group(
			-name=>'view',
			-value=>['Critical', 'Error', 'Warning',
				  'Info'],
			-default=>'Critical');
  
  print "<P><EM>Which monitors do you want?</EM><BR>\n";
  print $query->checkbox_group(
			       -name=>'monpat',
			       -value=> \@monitors,
#			       -linebreak=>'yes',
			       -defaults=> \@monitors);
  
  print "<P><EM>Sound?</EM><BR>\n",
    $query->radio_group(
			-name=>'sound', 
			-value => ['yes', 'no'],
			-default=>'no' );


  print "<P><EM>Alternate Print Style?</EM><BR>\n",
    $query->radio_group(
			-name=>'altprint', 
			-value => ['yes', 'no'],
			-default=>'no' );

  print "<p>Refresh interval (in seconds)? \n",
    $query->textfield(-name=>'refresh',
		      -default=>$refresh,
		      -size=>5,
		      -maxlength=>10
		     ), " (overridden if events more than $max_table_rows)\n";


  print "<p>Sort order: ",
    $query->popup_menu(-name=>'sort',
		       -default=>'name',
		       -values=>[qw(name varname siteaddr varvalue monitor severe)]
		       ), "\n";
#  print "<p>Sort order (name, varname, siteaddr, varvalue, monitor, severe)? \n",
#    $query->textfield(-name=>'sort',
#		      -default=>'name',
#		      -size=>10,
#		      -maxlength=>15
#		     ), "\n";


  print "<p>file pattern? \n",
    $query->textfield(-name=>'filepat',
		      -default=>'',
		      -size=>15,
		      -maxlength=>20
		     ), "\n";


  print "<P>", $query->reset, "&nbsp; &nbsp;",
        $query->submit('Action','Reload'), "&nbsp; &nbsp;",
        $query->submit('Action','Submit'),
        $query->endform, "<HR>\n";
}

sub show_config {
  my($query) = @_;
  my(@values,$key);

  print "<H2>Here are the current settings in this form</H2>\n";
  
  foreach $key ($query->param) {
    print "<STRONG>$key</STRONG> -> ";
    @values = $query->param($key);
    print join(", ",@values),"<BR>\n";
  }
  print "<p>", make_url($query);
}

sub make_url {
  my($query) = @_;
  my(@values,$key);
  my $param;
  my $url=$genweb2_cgi;
  
  foreach $key ($query->param) {
    next if $key eq 'Action';
    @values = $query->param($key);
    my $sep=',';
    if ( $key eq 'monpat' ) {
      next if $#values == $#monitors;
      $sep='|';
    }
    next if (@values==1) && $values[0] eq 'no';
    my $value .= "$key=" . join($sep, @values);
    next if $value eq '';
    if ($param) {
      $param .= "&$value";
    } else {
      $param = $value;
    }
  }
  $url .= "?" . $param if $param;
  return $url;

}

# Sets default values for form.
sub set_defaults {
  my($query) = @_;
}


