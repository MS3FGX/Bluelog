#!/usr/bin/perl                                                                
# Simple Perl UDP server for Bluelog

use strict;
use warnings;
use Socket;

socket(UDP, PF_INET, SOCK_DGRAM, getprotobyname("udp"));
bind(UDP, sockaddr_in(1123, INADDR_ANY));
print $_ while (<UDP>);
