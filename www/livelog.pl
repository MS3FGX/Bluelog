#!/usr/bin/perl
# livelog.pl - Perl script to generate Bluelog Live HTML
# Version 2.0

use strict;
# Bluelog scan result file
my $DEVFILE="/tmp/live.log";

# Variable for individual devices
my $DEVICE;

# Print majority of HTML here
print <<HEAD;
<!-- This page has been generated with livelog.pl -->
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html>
<head>
<title>Bluelog Live</title>
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1" />
<link href="style.css" type="text/css" rel="stylesheet" />
<link rel="icon" type="image/png" href="images/favicon.png" />
<link rel="shortcut icon" href="images/favicon.png" />
<META HTTP-EQUIV="refresh" CONTENT="20">
</head>
<body>
<div id="container">
<div id="header">
<span style="position:relative;left:15px;top:45px"><a href="http://www.digifail.com/" target="_blank" title="DigiFAIL"><img src="images/digifail_logo.png" border="0"></a></span>
</div>
<div id="sidebar">
<div id="sideobject">
<div id="boxheader">Info</div>
<div id="sidebox">
<div id="sidecontent">
HEAD

# Print out contents of info file
system("cat /tmp/info.txt");

# Print HTML for Discovered Devices counter
print "<div class=\"sideitem\">Discovered Devices: ";

# Count lines of file
system("cat /tmp/live.log | wc -l");

# Close DIV
print "</div>";

print <<TABLE;
</div>
</div>
</div>
</div>

<div id="content">
<table border="1" cellpadding="5" cellspacing="5" width="100%">
<tr>
<th>Time Discovered</th>
<th>MAC Address</th>
<th>Device Name</th>
<th>Device Class</th>
<th>Capabilities</th>
</tr>
TABLE

# Print scan results
open(DEVICES, "<$DEVFILE");
# This reverses files, but is it efficient?
my @RESULTS = reverse <DEVICES>;
foreach $DEVICE (@RESULTS)
{
	# Print individual line
	print $DEVICE;
}
# Close file
close(DEVLIST);

# Print the rest of the HTML
print <<TAIL;
</table>

</div>
<div class="footer">
<h4>	
<a href="about.html" onClick="window.open('about.html','About','toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=no,resizable=no,width=500,height=310,left=50,top=50'); return false;">About</a>
&nbsp;&nbsp;
<a href="contact.html" onClick="window.open('contact.html','Contact','toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=no,resizable=no,width=500,height=510,left=50,top=50'); return false;">Contact</a>
</h4>
</div>
</body>
</html>
TAIL

