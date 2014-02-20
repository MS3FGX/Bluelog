/*
 *  readconfig.c - Read configuration file and
 *  load settings into struct.
 *
 */
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

// Size limits
#define MAX_LINE_LEN 256
#define MAX_VALUE_LEN 64

// Struct to hold configuration variables
struct cfg
{
	// Strings
	char *outfilename;
	
	// Basic
	int verbose;	
	int quiet;	
	int daemon;	
	int bluelive;	
	
	// Logging 
	int showtime;
	int obfuscate;
	int encode;
	int showclass;
	int friendlyclass;
	int bluepropro;
	int getname;
	int amnesia;
	int syslogonly;
	int getmanufacturer;
	
	// Advanced
	int retry_count;
	int scan_window;
	int hci_device;
	
	// Network
	int udponly;
	int server_port;
	int prefix;
	int banner;
	int hangup;
	char node_name[MAX_VALUE_LEN];
	char server_ip[MAX_VALUE_LEN];
	
	// System
	int bt_socket;
	int udp_socket;
	char addr[19];
};

// Define global struct, set default values
struct cfg config = 
{
	.verbose = 0,
	.quiet = 0,
	.daemon = 0,
	.bluelive = 0,
	.showtime = 0,
	.obfuscate = 0,
	.encode = 0,
	.showclass = 0,
	.friendlyclass = 0,
	.bluepropro = 0,
	.getname = 0,
	.amnesia = -1,
	.syslogonly = 0,
	.getmanufacturer = 0,
	.retry_count = 3,
	.scan_window = 8,
	.hci_device = 0,
	.udponly = 0,
	.udp_socket = -1,
	.server_port = 1234,
	.banner = 0,
	.prefix = 1,
	.hangup = 0,
	.server_ip = "NULL",
	.node_name = "NULL",
	.addr = "NULL",
};

// Determine if config file is present
int cfg_exists (void)
{
  struct stat buffer;   
  return (stat(CFG_FILE, &buffer) == 0);
}

// Trim leading spaces from values
// Probably better way to do this...
char* trim_space(char* s)
{
  while( *s==' ' )
    memmove(s,s+1,strlen(s));
  return s;
}

// Convert yes/no to 1/0
int eval_bool(char* value, int line)
{
	if (!strcasecmp(value, "YES"))
		return 1;
	else if (!strcasecmp(value, "NO"))
		return 0;
	else
	{
		printf("Invalid value in configuration file on line %i!\n", line);
		exit(1);
	}
}

// Make sure everybody plays nice
static void cfg_check (void)
{
	// Check for out of range values
	if ((config.retry_count < 0) || ((config.amnesia < 0) && (config.amnesia != -1)))
	{	
		printf("Error, arguments must be positive numbers!\n");
		exit(1);
	}
	
	// Make sure window is reasonable
	if (config.scan_window > MAX_SCAN || config.scan_window < MIN_SCAN)
	{
		printf("Scan window is out of range. See README.\n");
		exit(1);
	}	
	
	// Override some options that don't play nice with others
	// If retry is different from default, assume names are on.
	if (config.retry_count != 3)
		config.getname = 1;

	// No verbose for daemon
	if (config.daemon)
		config.verbose = 0;
		
	// No Bluelog Live when running BPP, names on, syslog off
	if (config.bluepropro)
	{
		config.bluelive = 0;
		config.getname = 1;
		config.syslogonly = 0;
	}

	// Showing raw class ID turns off friendly names
	if (config.showclass)
		config.friendlyclass = 0;
			
	// No timestamps for Bluelog Live, names on, syslog off
	if (config.bluelive)
	{
		config.showtime = 0;
		config.getname = 1;
		config.syslogonly = 0;
	}
	
	// No timestamps in syslog mode, disable other modes
	if (config.syslogonly)
	{
		config.showtime = 0;
		config.bluelive = 0;
		config.bluepropro = 0;
	}
	
	// UDP disables other modes
	if (config.udponly)
	{
		config.bluelive = 0;
		config.bluepropro = 0;
		config.syslogonly = 0;
	}

	// Encode trumps obfuscate
	if (config.encode)
		config.obfuscate = 0;
}

int cfg_read (void)
{
	FILE* cfgfile;
	char line[MAX_LINE_LEN + 1];
	char* token;
	char* value;
	int linenum = 1;
        
	// Open file, return error if something goes wrong
	if ((cfgfile = fopen(CFG_FILE, "r")) == NULL)
		return(1);
		
	// Continue until file is done
	while(fgets(line, MAX_LINE_LEN, cfgfile) != NULL)
	{
		token = strtok(line, "\t =\n\r");
		if(token != NULL && token[0] != '#')
		{			
			// Get token's associated value
			value = trim_space(strtok(NULL, "\t;=\n\r"));
								
			if (value != NULL)
			{
				// Is it too large?
				if ((sizeof (value)) > MAX_VALUE_LEN)
				{
					printf("FAILED\n");
					printf("Value too large on line %i!\n", linenum);
					exit(1);
				}
				
				// See if token matches something
				if (strcmp(token, "VERBOSE") == 0)
					config.verbose = eval_bool(value, linenum);
				else if (strcmp(token, "QUIET") == 0)
					config.quiet = eval_bool(value, linenum);
				else if (strcmp(token, "DAEMON") == 0)
					config.daemon = eval_bool(value, linenum);
				else if (strcmp(token, "LIVEMODE") == 0)
					config.bluelive = eval_bool(value, linenum);
				else if (strcmp(token, "SHOWTIME") == 0)
					config.showtime = eval_bool(value, linenum);		
				else if (strcmp(token, "OBFUSCATE") == 0)
					config.obfuscate = eval_bool(value, linenum);
				else if (strcmp(token, "ENCODE") == 0)
					config.encode = eval_bool(value, linenum);				
				else if (strcmp(token, "SHOWCLASS") == 0)
					config.showclass = eval_bool(value, linenum);
				else if (strcmp(token, "FRIENDLYCLASS") == 0)
					config.friendlyclass = eval_bool(value, linenum);
				else if (strcmp(token, "BLUEPROPRO") == 0)
					config.bluepropro = eval_bool(value, linenum);
				else if (strcmp(token, "GETNAME") == 0)
					config.getname = eval_bool(value, linenum);
				else if (strcmp(token, "AMNESIA") == 0)
					config.amnesia = (atoi(value));
				else if (strcmp(token, "SYSLOGONLY") == 0)
					config.syslogonly = eval_bool(value, linenum);
				else if (strcmp(token, "GETMANUFACTURER") == 0)
					config.getmanufacturer = eval_bool(value, linenum);			
				else if (strcmp(token, "SCANWINDOW") == 0)
					config.scan_window = (atoi(value));
				else if (strcmp(token, "RETRYCOUNT") == 0)
					config.retry_count = (atoi(value));
				else if (strcmp(token, "HCIDEVICE") == 0)
					config.hci_device = (atoi(value));
				else if (strcmp(token, "UDPONLY") == 0)
					config.udponly = eval_bool(value, linenum);
				else if (strcmp(token, "SERVERIP") == 0)
					strcpy(config.server_ip, value);
				else if (strcmp(token, "SERVERPORT") == 0)
					config.server_port = (atoi(value));					
				else if (strcmp(token, "NODENAME") == 0)
					strcpy(config.node_name, value);
				else if (strcmp(token, "BANNER") == 0)
					config.banner = eval_bool(value, linenum);
				else if (strcmp(token, "HANGUP") == 0)
					config.hangup = eval_bool(value, linenum);
				else if (strcmp(token, "PREFIX") == 0)
					config.prefix = eval_bool(value, linenum);
				else
				{
					printf("FAILED\n");
					printf("Syntax error or unknown option in configuration file on line %i!\n", linenum);
					exit(1);
				}
			}
			else
			{
				printf("FAILED\n");
				printf("Value missing in configuration file on line %i!\n", linenum);
				exit(1);
			}
		}
	// Increment line number
	linenum++;
	}

	return (0);
}
