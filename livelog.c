/*
 *  livelog - Simple CGI module for Bluelog Live mode
 * 
 *  Written by Tom Nardi (MS3FGX@gmail.com), released under the GPLv2.
 *  For more information, see: www.digifail.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>

// Defines
#define APPNAME "livelog.cgi"
#define VERSION "1.1"
#define MAXNUM 4096
#define MAXLINE 500
#define INFO "/tmp/info.txt"
#define LOG "/tmp/live.log"
#define PID_FILE "/tmp/bluelog.pid"

// Conditionals
#if defined OPENWRT || PWNPLUG
#define CSSPREFIX "/bluelog/"
#else
#define CSSPREFIX "../"
#endif

// Global variables
int mobile;

// Found device struct
struct btdev
{
	char time[20];
	char addr[18];
	char name[248];
	char class[248];
	char hwinfo[248];
};

// Global variables
// Device file
FILE *logfile;
// Status file
FILE *infofile; 
// Init device cache, index
struct btdev dev_cache[MAXNUM];
int device_index = 0;

// Experimental, print all HTML from CGI module
void print_html(char *CSSFILE)
{	
	// File info
	printf("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"\n"
		"\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n");
	
	// Boilerplate
	printf("<!--This file created by %s (v%s) by MS3FGX-->\n", APPNAME, VERSION);	
	
	printf("<html>\n"
		"<head>\n"
		"<title>Bluelog Live</title>\n"
		"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\" >\n");
	

	
	// CSS file prefix
	printf("<link href=\"%s%s\" type=\"text/css\" rel=\"stylesheet\" /></head><body>\n", CSSPREFIX,CSSFILE);

	printf("<link rel=\"icon\" type=\"image/png\" href=\"/images/favicon.png\" />\n"
		"<link rel=\"shortcut icon\" href=\"/images/favicon.png\" />\n"
		"<META HTTP-EQUIV=\"refresh\" CONTENT=\"20\">\n"
		"</head>\n"
		"<body>\n"
		"<div id=\"container\">\n"
		"<div id=\"header\">\n"
		"<span style=\"position:relative;left:15px;top:45px\">\n"
		"<a href=\"http://www.digifail.com/\" target=\"_blank\" title=\"DigiFAIL\"><img src=\"/images/digifail_logo.png\" border=\"0\"></a>\n"
		"</span>\n"
		"</div>\n");

	printf("<div class=\"footer\">\n"
		"<h4>\n"
		"<a href=\"about.html\" onClick=\"window.open('about.html','About','toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=no,resizable=no,width=500,height=320,left=50,top=50'); return false;\">About</a>\n"
		"&nbsp;&nbsp;\n"
		"<a href=\"contact.html\" onClick=\"window.open('contact.html','Contact','toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=no,resizable=no,width=500,height=510,left=50,top=50'); return false;\">Contact</a>\n"
		"</h4>\n"
		"</div>\n");
}

void print_header(char *CSSFILE)
{		
	// CGI header
	printf("Content-type: text/html\n\n");
	// Boilerplate
	printf("<!--This file created with %s (v%s) by MS3FGX-->\n", APPNAME, VERSION);
	// HTML head
	printf("<html><head><link href=\"%s%s\" type=\"text/css\" rel=\"stylesheet\" /></head><body>\n", CSSPREFIX,CSSFILE);
}

void setup_table()
{
	// Table setup
	puts("<table border=\"1\" cellpadding=\"5\" cellspacing=\"5\" width=\"100%\">\n");
	
	if (!mobile)
	{
		puts("<tr>\n"\
		"<th>Time Discovered</th>\n"\
		"<th>MAC Address</th>\n"\
		"<th>Device Name</th>\n"\
		"<th>Device Class</th>\n"\
		"<th>Hardware Info</th>\n"\
		"</tr>\n");
	}
	else
	{
		puts("<tr>\n"\
		"<th>Time</th>\n"\
		"<th>MAC</th>\n"\
		"<th>Name</th>\n"\
		"<th>Class</th>\n"\
		"</tr>\n");
	}
}

void print_info()
{
	// Line length
	char line [128];
	
	while (fgets(line,sizeof line,infofile))
		fputs(line,stdout);
}

void read_log()
{
	char line [MAXLINE];	
	while (fgets(line,sizeof line,logfile))
	{
		// Copy line to buffer
		char* line_buffer = strdup(line);
	
		// Populate cache entry from file
		strcpy(dev_cache[device_index].time,strsep(&line_buffer, ","));	
		strcpy(dev_cache[device_index].addr,strsep(&line_buffer, ","));	
		strcpy(dev_cache[device_index].name,strsep(&line_buffer, ","));
		strcpy(dev_cache[device_index].class,strsep(&line_buffer, ","));
		strcpy(dev_cache[device_index].hwinfo,strsep(&line_buffer, "\n"));
		
        // Increment index
		device_index++;
	}
}

void print_devices()
{
	while (device_index > 0)
	{	
		// Write out valid table HTML
		printf("<tr>");
		printf("<td>%s</td>",dev_cache[device_index-1].time);		
		printf("<td>%s</td>",dev_cache[device_index-1].addr);	
		
		// Before writing out device name to HTML, do some VERY basic sanitization (not safe, just to block obvious stuff)		
		if ((strstr(dev_cache[device_index-1].name, ">")) || (strstr(dev_cache[device_index-1].name, "</")))
			strcpy(dev_cache[device_index-1].name, "<p style='color:red;'>Blocked Possible Exploit</p>");

		printf("<td>%s</td>",dev_cache[device_index-1].name);	
		printf("<td>%s</td>",dev_cache[device_index-1].class);	
		
		if(!mobile)
			printf("<td>%s</td>",dev_cache[device_index-1].hwinfo);	
		
		printf("</tr>\n");
		device_index--;
	}
}

int read_pid()
{
	// Any error will return 0
	FILE *pid_file;
	int pid;

	if (!(pid_file=fopen(PID_FILE,"r")))
		return 0;
		
	if (fscanf(pid_file,"%d", &pid) < 0)
		pid = 0;

	fclose(pid_file);	
	return pid;
}

void TopBar()
{
	// Discovered devices display
	puts("<div id=\"content\">\n");
	printf("Discovered Devices: %i</div>\n",device_index);
	// Close up info pane
	puts("</div>\n");	
}

void SideBar()
{	
	// Start sidebar
	puts("<div id=\"sidebar\">\n");
	
	// Aux pane
	puts("<div id=\"sideobject\">\n"\
	"<div id=\"auxbox1\">\n"\
	"</div>\n"\
	"</div>\n");
	
	// Start info pane
	puts("<div id=\"sideobject\">\n"\
	"<div id=\"boxheader\">Info</div>\n"\
	"<div id=\"sidebox\">\n"\
	"<div id=\"sidecontent\">");
	
	// Populate sidebar from files
	print_info();
	
	// Close info pane
	puts("</div>\n"\
	"</div>\n"\
	"</div>\n");
	
	// Status pane
	puts("<div id=\"sideobject\">\n"\
	"<div id=\"boxheader\">Status</div>\n"\
	"<div id=\"sidebox\">\n"\
	"<div id=\"sidecontent\">");
	
	// Print current PID status
	puts("<div class=\"sideitem\">");
	puts("Bluelog: ");	
	if (read_pid() != 0)
		puts("<span style=\"color: #00ff00;\"><b>RUNNING</b></span></div>\n");
	else
		puts("<span style=\"color: #ff0000;\"><b>STOPPED</b></span></div>\n");	
	
	// Print number of discovered devices
	puts("<div class=\"sideitem\">");
	printf("Discovered Devices: %i</div>\n",device_index);
	
	// Close status pane
	puts("</div>\n"\
	"</div>\n"\
	"</div>\n");
	
	// Aux pane
	puts("<div id=\"sideobject\">\n"\
	"<div id=\"auxbox2\">\n"\
	"</div>\n"\
	"</div>\n");
	
	// Close sidebar
	puts("</div>\n");
}

void shut_down(void)
{
	// Close files
	fclose(infofile);
	fclose(logfile);
	exit(1);
}

static void help(void)
{
	printf("%s (v%s) by MS3FGX\n", APPNAME, VERSION);
	printf("----------------------------------------------------------------\n");
	printf("This is a CGI module used to create a live webpage of Bluelog\n"
		"results. This module simply parses the log files created by\n"	
		"Bluelog, it has no scanning or logging capability of its own.\n");
	printf("\n");
	printf("For more information, see www.digifail.com\n");
	printf("\n");
	printf("Options:\n"
		"\t-m              Mobile format\n"
		"\t-h              Display help\n"
		"\t-d              Print Debug Info\n"
		"\t-v              Print Version Info\n"		
		"\n");
}

static void debug(void)
{
printf("System Variables\n");
printf("--------------------------\n");
printf("Module Version: %s\n", VERSION);
printf("Max Devices: %i\n",MAXNUM);
printf("Max Line Length: %i\n",MAXLINE);
printf("\n");
printf("File Locations\n");
printf("--------------------------\n");
printf("Info File: %s\n",INFO);
printf("Log File: %s\n", LOG);
printf("CSS Prefix: %s\n",CSSPREFIX);
}

static struct option main_options[] = {
	{ "help", 0, 0, 'h' },
	{ "debug", 0, 0, 'd' },
	{ "version", 0, 0, 'v' },
	{ "mobile", 0, 0, 'm' },
	{ 0, 0, 0, 0 }
};
 
int main(int argc, char *argv[])
{		
	// Variables	
	// Default CSS
	char CSSFILE[12]="style.css";

	// Pointers to filenames
	char *infofilename = INFO;
	char *logfilename = LOG;
	
	// Handle arguments
	int opt;
	while ((opt=getopt_long(argc, argv, "dvhm", main_options, NULL)) != EOF)
	{
		switch (opt)
		{
		case 'h':
			help();
			exit(0);
		case 'd':
			debug();
			exit(0);
		case 'v':
			printf("%s (v%s) by MS3FGX\n", APPNAME, VERSION);
			exit(0);
			break;
		case 'm':
			strcpy(CSSFILE, "mobile.css");
			mobile = 1;
			break;
		default:
			printf("Unknown option.\n");
			exit(0);
		}
	}
	
	// Bail out if we are root, except on WRT
	#ifndef OPENWRT
	if(getuid() == 0)
	{
		syslog(LOG_ERR,"CGI module refusing to run as root!");
		
		// Make sure error message is themed
		print_header(CSSFILE);
		puts("<div id=\"content\">");
		puts("Server attempting to run CGI module as root, bailing out!");
		puts("<p>");
		puts("Check your web server configuration and try again.");
		puts("</body></html>");
		exit(1);
	}
	#endif

	// Read in environment variable
	// Comment this to fix compiler warning until ready to implement
	//char* env_string;
	//env_string=getenv("QUERY_STRING");

	// Print HTML head
	print_header(CSSFILE);
	//print_html(CSSFILE);
	
	// Start container div
	if (!mobile)
		puts("<div id=\"container\">\n");

	// Open files
	if ((infofile = fopen(infofilename, "r")) == NULL)
	{
		syslog(LOG_ERR,"Error while opening %s!",infofilename);
		puts("<div id=\"content\">");
		printf("Error while opening %s!\n",infofilename);
		puts("</body></html>");
		exit(1);
	}
	
	if ((logfile = fopen(logfilename, "r")) == NULL)
	{
		syslog(LOG_ERR,"Error while opening %s!",logfilename);
		puts("<div id=\"content\">");
		printf("Error while opening %s!\n",logfilename);
		puts("</body></html>");
		exit(1);
	}
	
	// Draw sidebar\topbar
	read_log();
	if (!mobile)
		SideBar();
	else
		TopBar();
	
	// Content window
	puts("<div id=\"content\">");
	
	if (device_index > 0)
	{
		// Print results
		setup_table();
		print_devices();
		puts("</table>\n");
	}
	
	// Close content, body, and HTML
	puts("</div></body></html>");
	
	// Close files and exit
	shut_down();
	return 0;
}

