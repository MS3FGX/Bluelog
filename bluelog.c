/*
 *  Bluelog - Fast Bluetooth scanner with optional Web frontend
 * 
 *  Bluelog is a Bluetooth site survey tool, designed to tell you how
 *  many discoverable devices there are in an area as quickly as possible.
 *  As the name implies, its primary function is to log discovered devices
 *  to file rather than to be used interactively. Bluelog could run on a
 *  system unattended for long periods of time to collect data. In addition
 *  to basic scanning, Bluelog also has a unique feature called "Bluelog Live",
 *  which puts results in a constantly updating Web page which you can serve up
 *  with your HTTP daemon of choice.
 * 
 *  Bluelog uses code from a number of GPL projects. See README for more info.
 *
 *  Written by Tom Nardi (MS3FGX@gmail.com), released under the GPLv2.
 *  For more information, see: www.digifail.com
 */

#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

// Load configuration
#include "config.h"

// Bluelog-specific includes
#include "classes.c"
#include "libmackerel.c"
#include "readconfig.c"
#include "udp.c"

// Found device struct
struct btdev
{
	char name[248];
	char addr[18];
	char priv_addr[18];
	char time[20];
	uint64_t epoch;
	uint8_t flags;
	uint8_t major_class;
	uint8_t minor_class;
	uint8_t print;
	uint8_t seen;
};

// Global variables
FILE *outfile; // Output file
FILE *infofile; // Status file
inquiry_info *results; // BlueZ scan results struct
			
struct btdev dev_cache[MAX_DEV]; // Init device cache

char* get_localtime()
{
	// Time variables
	time_t rawtime;
	struct tm * timeinfo;
	static char time_string[20];
	
	// Find time and put it into time_string
	time (&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(time_string,20,"%D %T",timeinfo);
	
	// Send it back
	return(time_string);
}

char* file_timestamp()
{
	// Time variables
	time_t rawtime;
	struct tm * timeinfo;
	static char time_string[20];
	static char filename[40];
	
	// Find time and put it into time_string
	time (&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(time_string,20,"%F-%H%M",timeinfo);
	
	sprintf(filename,"bluelog-%s.log",time_string);
	
	// Send it back
	return(filename);
}

void shut_down(int sig)
{
	// Close up shop
	printf("\n");
	printf("Closing files and freeing memory...");
	// Only show this if timestamps are enabled
	if (config.showtime && (outfile != NULL))
		fprintf(outfile,"[%s] Scan ended.\n", get_localtime());
	
	// Don't try to close a file that doesn't exist, kernel gets mad
	if (outfile != NULL)
		fclose(outfile);
	
	// UDP cleanup
	if (config.udponly)
	{
		// Send message if configured
		if (config.hangup)
			send_udp_msg("Disconnect\n");
		
		// Close socket
		close(config.udp_socket);
	}
	
	// Always close these
	free(results);
	close(config.bt_socket);
	
	// Delete PID file
	unlink(PID_FILE);
	printf("Done!\n");
	
	// Log shutdown to syslog
	syslog(LOG_INFO, "Shutdown OK.");
	exit(sig);
}

void live_entry(int index)
{
	// Local variables 
	char local_name[248];
	char local_class[64];
	char local_capabilities[64];
	
	//Populate the local variables
	strcpy(local_name, dev_cache[index].name);
	strcpy(local_class, device_class(dev_cache[index].major_class, dev_cache[index].minor_class));
	strcpy(local_capabilities, device_capability(dev_cache[index].flags));
		
	// Let's format these a little nicer
	if (!strcmp(local_name, "VOID"))
		strcpy(local_name, "No Response");
	if (!strcmp(local_class, "VOID"))
		strcpy(local_class, "Unclassified");
	if (!strcmp(local_capabilities, "VOID"))
		strcpy(local_capabilities, "Not Reported");
		
	// Write out log
	fprintf(outfile,"%s,", dev_cache[index].time);
	fprintf(outfile,"%s,", dev_cache[index].addr);
	fprintf(outfile,"%s,", local_name);
	fprintf(outfile,"%s,", local_class);
	
	// Last field is variable
	if (config.getmanufacturer)
		fprintf(outfile,"%s", mac_get_vendor(dev_cache[index].priv_addr));
	else
		fprintf(outfile,"%s", local_capabilities);
		
	fprintf(outfile,"\n");
}

int read_pid (void)
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

static void write_pid (pid_t pid)
{
	FILE *pid_file;
	
	// Open PID file
	if (!config.quiet)
		printf("Writing PID file: %s...", PID_FILE);
	if ((pid_file = fopen(PID_FILE,"w")) == NULL)
	{
		printf("\n");
		printf("Error opening PID file!\n");
		exit(1);
	}
	if (!config.quiet)	
		printf("OK\n");
	
	// If open, write PID and close	
	fprintf(pid_file,"%d\n", pid);
	fclose(pid_file);
}

static void daemonize (void)
{
	// Process and Session ID
	pid_t pid, sid;
	
	syslog(LOG_INFO,"Going into daemon mode...");
 
	// Fork off process
	pid = fork();
	if (pid < 0)
		exit(EXIT_FAILURE);
	else if (pid > 0)
		exit(EXIT_SUCCESS);
			
	// Change umask
	umask(0);
 
	// Create a new SID for the child process
	sid = setsid();
	if (sid < 0)
		exit(EXIT_FAILURE);

	// Change current working directory
	if ((chdir("/")) < 0)
		exit(EXIT_FAILURE);
		
	// Write PID file
	write_pid(sid);
	
	if (!config.quiet)
		printf("Going into background...\n");
		
	// Close file descriptors
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
}

char* namequery (const bdaddr_t *addr)
{
	// Response to pass back
	static char name[248];
	
	// Terminate to prevent duplicating previous results
	memset(name, 0, sizeof(name));
	
	// Attempt to read device name
	if (hci_read_remote_name(config.bt_socket, addr, sizeof(name), name, 0) < 0) 
		strcpy(name, "VOID");
		
	return (name);
}

static void help(void)
{
	printf("%s (v%s%s) by Tom Nardi \"MS3FGX\" (MS3FGX@gmail.com)\n", APPNAME, VERSION, VER_MOD);
	printf("----------------------------------------------------------------\n");
	printf("Bluelog is a Bluetooth site survey tool, designed to tell you how\n"
		"many discoverable devices there are in an area as quickly as possible.\n"
		"As the name implies, its primary function is to log discovered devices\n"
		"to file rather than to be used interactively. Bluelog could run on a\n"
		"system unattended for long periods of time to collect data.\n");
	printf("\n");
	
	// Only print this if Bluelog Live is enabled in build
	if (LIVEMODE)
	{
		printf("Bluelog also includes a mode called \"Bluelog Live\" which creates a\n"
			"webpage of the results that you can serve up with your HTTP daemon of\n"
			"choice. See the \"README.LIVE\" file for details.\n");
		printf("\n");
	}
	
	printf("For more information, see: www.digifail.com\n");
	printf("\n");
	printf("Basic Options:\n"
		"\t-i <interface>     Sets scanning device, default is \"hci0\"\n"
		"\t-o <filename>      Sets output filename, default is \"devices.log\"\n"
		"\t-v                 Verbose, prints discovered devices to the terminal\n"		
		"\t-q                 Quiet, turns off nonessential terminal outout\n"
		"\t-d                 Enables daemon mode, Bluelog will run in background\n"
		"\t-k                 Kill an already running Bluelog process\n");
	printf("\n");
	printf("Logging Options:\n"		
		"\t-n                 Write device names to log, default is disabled\n");
	
	// Only print this if OUI lookup is enabled in build
	if (OUILOOKUP)
		printf("\t-m                 Write device manufacturer to log, default is disabled\n");

	printf("\t-c                 Write device class to log, default is disabled\n"
		"\t-f                 Use \"friendly\" device class, default is disabled\n"		
		"\t-t                 Write timestamps to log, default is disabled\n"
		"\t-x                 Obfuscate discovered MACs, default is disabled\n"
		"\t-e                 Encode discovered MACs with CRC32, default disabled\n"
		"\t-a <minutes>       Amnesia, Bluelog will forget device after given time\n");

	printf("\n");
	printf("Output Options:\n");
	
	// Only print this if Bluelog Live is enabled in build
	if (LIVEMODE)
		printf("\t-l                 Start \"Bluelog Live\", default is disabled\n");

	printf("\t-b                 Enable BlueProPro log format, see README\n"
		"\t-s                 Syslog only mode, no log file. Default is disabled\n");	

	printf("\n");
	printf("Advanced Options:\n"			
		"\t-r <retries>       Name resolution retries, default is 3\n"
		"\t-w <seconds>       Scanning window in seconds, see README\n"		
		"\n");
}

static struct option main_options[] = {
	{ "interface", 1, 0, 'i' },
	{ "output",	1, 0, 'o' },
	{ "verbose", 0, 0, 'v' },
	{ "retry", 1, 0, 'r' },
	{ "amnesia", 1, 0, 'a' },
	{ "window", 1, 0, 'w' },	
	{ "time", 0, 0, 't' },
	{ "obfuscate", 0, 0, 'x' },
	{ "class", 0, 0, 'c' },
	{ "live", 0, 0, 'l' },
	{ "kill", 0, 0, 'k' },
	{ "friendly", 0, 0, 'f' },
	{ "bluepropro", 0, 0, 'b' },
	{ "name", 0, 0, 'n' },
	{ "help", 0, 0, 'h' },
	{ "daemonize", 0, 0, 'd' },
	{ "syslog", 0, 0, 's' },
	{ "encode", 0, 0, 'e' },
	{ "quiet", 0, 0, 'q' },
	{ "manufacturer", 0, 0, 'm' },
	{ 0, 0, 0, 0 }
};

int main(int argc, char *argv[])
{	
	// Handle signals
	signal(SIGINT,shut_down);
	signal(SIGHUP,shut_down);
	signal(SIGTERM,shut_down);
	signal(SIGQUIT,shut_down);

	// HCI device number, MAC struct
	int device = 0;
	bdaddr_t bdaddr;
	bacpy(&bdaddr, BDADDR_ANY);
	
	// Time to scan. Scan time is roughly 1.28 seconds * scan_window
	// Originally this was always 8, now we adjust based on device:
	#ifdef OPENWRT
	int scan_window = 8;
	#elif PWNPLUG
	int scan_window = 5;
	#else
	int scan_window = 3;
	#endif
	
	// Maximum number of devices per scan
	int max_results = 255;
	int num_results;
	
	// Device cache and index
	int cache_index = 0;

	// HCI cache setting
	int flags = IREQ_CACHE_FLUSH;
	
	// Strings to hold MAC and name
	char addr[19] = {0};
	char addr_buff[19] = {0};
	
	// String for time
	char cur_time[20];
	
	// Process ID read from PID file
	int ext_pid;
	
	// Pointers to filenames
	char *infofilename = LIVE_INF;
	
	// Change default filename based on date
	char OUT_FILE[1000] = OUT_PATH;
	strncat(OUT_FILE, file_timestamp(),sizeof(OUT_FILE)-strlen(OUT_FILE)-1);	
	char *outfilename = OUT_FILE;
	
	// Mode to open output file in
	char *filemode = "a+";
	
	// Output buffer
	char outbuffer[500];
	
	// Misc Variables
	int i, ri, opt;
	
	// Record numbner of BlueZ errors
	int error_count = 0;
	
	// Current epoch time
	long long int epoch;
	
	// Kernel version info
	struct utsname sysinfo;
	uname(&sysinfo);
	
	while ((opt=getopt_long(argc,argv,"+o:i:r:a:w:vxcthldbfenksmq", main_options, NULL)) != EOF)
	{
		switch (opt)
		{
		case 'i':
			if (!strncasecmp(optarg, "hci", 3))
				hci_devba(atoi(optarg + 3), &bdaddr);
			else
				str2ba(optarg, &bdaddr);
			break;
		case 'o':
			outfilename = strdup(optarg);
			break;
		case 'r':
			config.retry_count = atoi(optarg);
			break;
		case 'a':
			config.amnesia = atoi(optarg);
			break;	
		case 'w':
			config.scan_window = round((atoi(optarg) / 1.28));
			break;	
		case 'c':
			config.showclass = 1;
			break;
		case 'e':
			config.encode = 1;
			break;			
		case 'f':
			config.friendlyclass = 1;
			break;
		case 'v':
			config.verbose = 1;
			break;
		case 't':
			config.showtime = 1;
			break;
		case 's':
			config.syslogonly = 1;
			break;
		case 'x':
			config.obfuscate = 1;
			break;
		case 'q':
			config.quiet = 1;
			break;			
		case 'l':
			if(!LIVEMODE)
			{
				printf("Live mode has been disabled in this build. See documentation.\n");
				exit(0);
			}
			else
				config.bluelive = 1;
			break;
		case 'b':
			config.bluepropro = 1;
			break;
		case 'd':
			config.daemon = 1;
			break;
		case 'n':
			config.getname = 1;
			break;
		case 'm':
			if(!OUILOOKUP)
			{
				printf("Manufacturer lookups have been disabled in this build. See documentation.\n");
				exit(0);
			}
			else
				config.getmanufacturer = 1;
			break;
		case 'h':
			help();
			exit(0);
		case 'k':
			// Read PID from file into variable
			ext_pid = read_pid();
			if (ext_pid != 0)
			{
				printf("Killing Bluelog process with PID %i...",ext_pid);
				if(kill(ext_pid,15) != 0)
				{
					printf("ERROR!\n");
					printf("Unable to kill Bluelog process. Check permissions.\n");
					exit(1);
				}
				else
					printf("OK.\n");
				
				// Delete PID file
				unlink(PID_FILE);
			}
			else
				printf("No running Bluelog process found.\n");
				
			exit(0);
		default:
			printf("Unknown option. Use -h for help, or see README.\n");
			exit(1);
		}
	}
	
	// See if there is already a process running
	if (read_pid() != 0)
	{
		printf("Another instance of Bluelog is already running!\n");
		printf("Use the -k option to kill a running Bluelog process.\n");
		exit(1);
	}
	
	// Load config from file if no options given on command line
	if(cfg_exists() && argc == 1)
	{		
		if (cfg_read() != 0)
		{
			printf("Error opening config file!\n");
			exit(1);
		}
		// Put interface into BT struct
		hci_devba(config.hci_device, &bdaddr);
	}
		
	// Perform sanity checks on varibles
	cfg_check();

	// Setup libmackerel
	mac_init();	
	
	// Boilerplate
	if (!config.quiet)
	{
		printf("%s (v%s%s) by MS3FGX\n", APPNAME, VERSION, VER_MOD);
		#if defined OPENWRT || PWNPLUG
			printf("----");
		#endif
		printf("---------------------------\n");
	}
	
	// Show notification we loaded config from file
	if(cfg_exists() && argc == 1 && !config.quiet)
		printf("Config loaded from: %s\n", CFG_FILE);

	// Init Hardware
	ba2str(&bdaddr, config.addr);
	if (!strcmp(config.addr, "00:00:00:00:00:00"))
	{
		if (!config.quiet)
			printf("Autodetecting device...");
		device = hci_get_route(NULL);
		// Put autodetected device MAC into addr
		hci_devba(device, &bdaddr);
		ba2str(&bdaddr, config.addr);
	}
	else
	{
		if (!config.quiet)
			printf("Initializing device...");
		device = hci_devid(config.addr);
	}
	
	// Open device and catch errors
	config.bt_socket = hci_open_dev(device); 
	if (device < 0 || config.bt_socket < 0)
	{
		// Failed to open device, that can't be good
		printf("\n");
		printf("Error initializing Bluetooth device!\n");
		exit(1);
	}
	
	// If we get here the device should be online.
	if (!config.quiet)
		printf("OK\n");

	// Status message for BPP
	if (!config.quiet)
		if (config.bluepropro)
			printf("Output formatted for BlueProPro.\n"
				   "More Info: www.hackfromacave.com\n");
				   	
	// Open socket 
	if (config.udponly)
		open_udp_socket();

	// Open output file, unless in networking mode
	if (!config.syslogonly && !config.udponly)
	{
		if (config.bluelive)
		{
			// Change location of output file
			outfilename = LIVE_OUT;
			filemode = "w";
			if (!config.quiet)
				printf("Starting Bluelog Live...\n");
		}
		if (!config.quiet)		
			printf("Opening output file: %s...", outfilename);
		if ((outfile = fopen(outfilename, filemode)) == NULL)
		{
			printf("\n");
			printf("Error opening output file!\n");
			exit(1);
		}
		if (!config.quiet)
			printf("OK\n");
	}
	else
		if (!config.quiet)
			printf("Network mode enabled, not creating log file.\n");
	
	// Open status file
	if (config.bluelive)
	{
		if (!config.quiet)		
			printf("Opening info file: %s...", infofilename);
		if ((infofile = fopen(infofilename,"w")) == NULL)
		{
			printf("\n");
			printf("Error opening info file!\n");
			exit(1);
		}
		if (!config.quiet)
			printf("OK\n");
	}
	
	// Write PID file
	if (!config.daemon)
		write_pid(getpid());
	
	// Get and print time to console and file
	strcpy(cur_time, get_localtime());
	
	if (!config.daemon)
		printf("Scan started at [%s] on %s\n", cur_time, config.addr);
	
	if (config.showtime && (outfile != NULL))
	{
		fprintf(outfile,"[%s] Scan started on %s\n", cur_time, config.addr);
		// Make sure this gets written out
		fflush(outfile);
	}
		
	// Write info file for Bluelog Live
	if (config.bluelive)
	{
		fprintf(infofile,"<div class=\"sideitem\">%s Version: %s%s</div>\n", APPNAME, VERSION, VER_MOD);
		fprintf(infofile,"<div class=\"sideitem\">Device: %s</div>\n", config.addr);
		fprintf(infofile,"<div class=\"sideitem\">Started: %s</div>\n", cur_time);
		
		// Think we are done with you now
		fclose(infofile);
	}
	
	// Log success to this point
	syslog(LOG_INFO,"Init OK!");
	
	// Daemon switch
	if (config.daemon)
		daemonize();
	else
		if (!config.quiet)
			#if defined PWNPAD
			printf("Close this window to end scan.\n");
			#else
			printf("Hit Ctrl+C to end scan.\n");
			#endif
		
	// Init result struct
	results = (inquiry_info*)malloc(max_results * sizeof(inquiry_info));	
	
	// Start scan, be careful with this infinite loop...
	for(;;)
	{
		// Flush results buffer
		memset(results, '\0', max_results * sizeof(inquiry_info)); 
		
		// Scan and return number of results
		num_results = hci_inquiry(device, scan_window, max_results, NULL, &results, flags);
		
		// A negative number here means an error during scan
		if(num_results < 0)
		{
			// Increment error count
			error_count++;
			
			// Ignore occasional errors on Pwn Plug and OpenWRT
			#if !defined PWNPLUG || OPENWRT
			// All other platforms, print error and bail out
			syslog(LOG_ERR,"Received error from BlueZ!");
			printf("Scan failed!\n");
			// Check for kernel 3.0.x
			if (!strncmp("3.0.",sysinfo.release,4))
			{
				printf("\n");
				printf("-----------------------------------------------------\n");
				printf("Device scanning failed, and you are running a 3.0.x\n");
				printf("Linux kernel. This failure is probably due to the\n");
				printf("following kernel bug:\n");
				printf("\n");
				printf("http://marc.info/?l=linux-kernel&m=131629118406044\n");
				printf("\n");
				printf("You will need to upgrade your kernel to at least the\n");
				printf("the 3.1 series to continue.\n");
				printf("-----------------------------------------------------\n");

			}
			shut_down(1);
			#else
			// Exit on back to back errors
			if (error_count > 5)
			{
				printf("Scan failed!\n");				
				syslog(LOG_ERR,"BlueZ not responding, unrecoverable!");
				shut_down(1);
			}
			
			// Otherwise, throttle back a bit, might help
			sleep(1);
			#endif
		}
		else
		{
			// Clear error counter
			error_count = 0;
		}
		
		// Check if we need to reset device cache
		if ((cache_index + num_results) >= MAX_DEV)
		{
			syslog(LOG_INFO,"Resetting device cache...");
			memset(dev_cache, 0, sizeof(dev_cache));
			cache_index = 0;
		}
			
		// Loop through results
		for (i = 0; i < num_results; i++)
		{	
			// Return current MAC from struct
			ba2str(&(results+i)->bdaddr, addr);
			
			// Compare to device cache
			for (ri = 0; ri <= cache_index; ri++)
			{				
				// Determine if device is already logged
				if (strcmp (addr, dev_cache[ri].priv_addr) == 0)
				{		
					// This device has been seen before
			
					// Increment seen count, update printed time
					dev_cache[ri].seen++;
					strcpy(dev_cache[ri].time, get_localtime());
					
					// If we don't have a name, query again
					if ((dev_cache[ri].print == 3) && (dev_cache[ri].seen > config.retry_count))
					{
						syslog(LOG_INFO,"Unable to find name for %s!", addr);
						dev_cache[ri].print = 1;
					}
					else if ((dev_cache[ri].print == 3) && (dev_cache[ri].seen < config.retry_count))
					{
						// Query name
						strcpy(dev_cache[ri].name, namequery(&(results+i)->bdaddr));
						
						// Did we get one?
						if (strcmp (dev_cache[ri].name, "VOID") != 0)
						{
							syslog(LOG_INFO,"Name retry for %s successful!", addr);
							// Force print
							dev_cache[ri].print = 1;
						}
						else
							syslog(LOG_INFO,"Name retry %i for %s failed!",dev_cache[ri].seen, addr);
					}
					
					// Amnesia mode
					if (config.amnesia >= 0)
					{
						// Find current epoch time
						epoch = time(NULL);
						if ((epoch - dev_cache[ri].epoch) >= (config.amnesia * 60))
						{
							// Update epoch time
							dev_cache[ri].epoch = epoch;
							// Set device to print
							dev_cache[ri].print = 1;
						}
					}
					
					// Unless we need to get printed, move to next result
					if (dev_cache[ri].print != 1)
						break;
				}
				else if (strcmp (dev_cache[ri].addr, "") == 0) 
				{
					// Write new device MAC (visible and internal use)
					strcpy(dev_cache[ri].addr, addr);
					strcpy(dev_cache[ri].priv_addr, addr);					
					
					// Query for name
					if (config.getname)
						strcpy(dev_cache[ri].name, namequery(&(results+i)->bdaddr));
					else
						strcpy(dev_cache[ri].name, "IGNORED");

					// Get time found
					strcpy(dev_cache[ri].time, get_localtime());
					dev_cache[ri].epoch = time(NULL);
					
					// Class info
					dev_cache[ri].flags = (results+i)->dev_class[2];
					dev_cache[ri].major_class = (results+i)->dev_class[1];
					dev_cache[ri].minor_class = (results+i)->dev_class[0];
					
					// Init misc variables
					dev_cache[ri].seen = 1;
					
					// Increment index	
					cache_index++;	
					
					// If we have a device name, get printed
					if (strcmp (dev_cache[ri].name, "VOID") != 0)
						dev_cache[ri].print = 1;
					else
					{
						// Found with no name.
						// Print message to syslog, prevent printing, and move on
						syslog(LOG_INFO,"Device %s discovered with no name, will retry", dev_cache[ri].addr);
						dev_cache[ri].print = 3;
						break;
					}											
				}
							
				// Ready to print?
				if (dev_cache[ri].print == 1) 
				{	
					// Encode MAC
					if (config.encode || config.obfuscate)
					{
						// Clear buffer
						memset(addr_buff, '\0', sizeof(addr_buff));

						if (config.obfuscate)
							strcpy(addr_buff, mac_obfuscate(dev_cache[ri].priv_addr));
						
						if (config.encode)
							strcpy(addr_buff, mac_encode(dev_cache[ri].priv_addr));

						// Copy to cache
						strcpy(dev_cache[ri].addr, addr_buff);
					}
					
					// Print everything to console if verbose is on, optionally friendly class info
					if (config.verbose)
					{
						if (config.friendlyclass)
						{
							printf("[%s] %s,%s,%s,(%s)\n",\
								dev_cache[ri].time, dev_cache[ri].addr,\
								dev_cache[ri].name, device_class(dev_cache[ri].major_class,\
								dev_cache[ri].minor_class), device_capability(dev_cache[ri].flags));						
						}
						else
						{
							printf("[%s] %s,%s,0x%02x%02x%02x\n",\
								dev_cache[ri].time, dev_cache[ri].addr,\
								dev_cache[ri].name, dev_cache[ri].flags,\
								dev_cache[ri].major_class, dev_cache[ri].minor_class);
						}
					}
											
					if (config.bluelive)
					{
						// Write result with live function
						live_entry(ri);
					}
					else if (config.bluepropro)
					{
						// Set output format for BlueProPro
						fprintf(outfile,"%s", dev_cache[ri].addr);
						fprintf(outfile,",0x%02x%02x%02x", dev_cache[ri].flags,\
						dev_cache[ri].major_class, dev_cache[ri].minor_class);
						fprintf(outfile,",%s\n", dev_cache[ri].name);
					}
					else 
					{
						// Flush buffer
						memset(outbuffer, 0, sizeof(outbuffer));
						
						// Print time first if enabled
						if (config.showtime)
							sprintf(outbuffer,"[%s],", dev_cache[ri].time);
							
						// Always output MAC
						sprintf(outbuffer+strlen(outbuffer),"%s", dev_cache[ri].addr);
						
						// Optionally output class
						if (config.showclass)					
							sprintf(outbuffer+strlen(outbuffer),",0x%02x%02x%02x", dev_cache[ri].flags,\
							dev_cache[ri].major_class, dev_cache[ri].minor_class);
							
						// "Friendly" version of class info
						if (config.friendlyclass)					
							sprintf(outbuffer+strlen(outbuffer),",%s,(%s)",\
							device_class(dev_cache[ri].major_class, dev_cache[ri].minor_class),\
							device_capability(dev_cache[ri].flags));
						
						// Get manufacturer
						if (config.getmanufacturer)
							sprintf(outbuffer+strlen(outbuffer),",%s", mac_get_vendor(dev_cache[ri].priv_addr));
							
						// Append the name
						if (config.getname)
							sprintf(outbuffer+strlen(outbuffer),",%s", dev_cache[ri].name);
													
						// Send buffer, else file. File needs newline
						if (config.syslogonly)
							syslog(LOG_INFO,"%s", outbuffer);
						else if (config.udponly)
						{
							// Append newline to socket, kind of hacky
							sprintf(outbuffer+strlen(outbuffer),"\n");
							send_udp_msg(outbuffer);
						}
						else
							fprintf(outfile,"%s\n",outbuffer);
					}
					dev_cache[ri].print = 0;
					break;
				}
				// If we make it this far, it means we will check next stored device
			}
			// If there's a file open, write changes
			if (outfile != NULL)
				fflush(outfile);
		}
	}
	// If we get here, shut down
	shut_down(0);
	// STFU
	return (1);
}
