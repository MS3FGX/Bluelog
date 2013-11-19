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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <syslog.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "classes.c"

#define VERSION	"1.0.5-RC1"
#define APPNAME "Bluelog"

#include <string.h>
#include <sqlite3.h>
#define TABLE "CREATE TABLE IF NOT EXISTS record(id INTEGER PRIMARY KEY, session_id INTEGER, mac VARCHAR(20), gathered_on DATETIME)"
#define TABLE_EVENT "CREATE TABLE IF NOT EXISTS session(id INTEGER PRIMARY KEY, window integer, started_on DATETIME)"
#define INDEX_MAC "CREATE INDEX IF NOT EXISTS `index_mac` ON record (`mac`)"
#define INDEX_DATE "CREATE INDEX IF NOT EXISTS `index_date` ON record (`gathered_on`)"


// Determine device-specific configs
// OpenWRT specific
#ifdef OPENWRT
#define VER_MOD "-WRT"
// Maximum number of devices in cache
#define MAX_DEV 200
// Toggle Bluelog Live
#define LIVEMODE 1
// Default log
#define OUT_PATH "/tmp/"
// Bluelog Live device list
#define LIVE_OUT "/tmp/live.log"
// Bluelog Live status info
#define LIVE_INF "/tmp/info.txt"
// PID storage
#define PID_FILE "/tmp/bluelog.pid"
// PWNPLUG specific
#elif PWNPLUG
#define VER_MOD "-PWN"
#define MAX_DEV 200
#define LIVEMODE 1
#define OUT_PATH "/dev/shm/"
#define LIVE_OUT "/tmp/live.log"
#define LIVE_INF "/tmp/info.txt"
#define PID_FILE "/tmp/bluelog.pid"
#else
// Generic x86
#define VER_MOD ""
#define MAX_DEV 200
#define LIVEMODE 1
#define OUT_PATH ""
#define LIVE_OUT "/tmp/live.log"
#define LIVE_INF "/tmp/info.txt"
#define PID_FILE "/tmp/bluelog.pid"
#endif

// Found device struct
struct btdev
{
	char name[248];
	char addr[18];
	char priv_addr[18];
	char time[20];
	uint8_t flags;
	uint8_t major_class;
	uint8_t minor_class;
	uint8_t seen;
};

// Global variables
FILE *outfile; // Output file
#ifdef SQLITE	/* Database variable */
sqlite3 *db;
#endif
FILE *infofile; // Status file
inquiry_info *results; // BlueZ scan results struct
int bt_socket; // HCI device
int showtime = 0; // Show timestamps in log		
int syslogonly = 0; // Log file enabled
int quiet = 0; // Print output normally
			
struct btdev new_device; 

char* get_localtime(int diff_seconds)
{
	// Time variables
	time_t now, diff;
	struct tm * timeinfo;
	static char time_string[20];
	
	// Find time and put it into time_string
	now = time (0);
	diff= now - diff_seconds; 
	timeinfo = localtime(&diff);
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
	if (showtime)
		fprintf(outfile,"[%s] Scan ended.\n", get_localtime(0));
	
	// Don't try to close a file that doesn't exist, kernel gets mad
	if (!syslogonly) {
		fclose(outfile);
		#ifdef SQLITE
		sqlite3_close(db);
		#endif
	}
	free(results);
	//free(dev_cache);
	close(bt_socket);
	// Delete PID file
	unlink(PID_FILE);
	printf("Done!\n");
	// Log shutdown to syslog
	syslog(LOG_INFO, "Shutdown OK.");
	exit(sig);
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
	if (!quiet)
		printf("Writing PID file: %s...", PID_FILE);
	if ((pid_file = fopen(PID_FILE,"w")) == NULL)
	{
		exit(1);
	}
	if (!quiet)	
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
	
	if (!quiet)
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
	if (hci_read_remote_name(bt_socket, addr, sizeof(name), name, 0) < 0) 
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
		
	// Only print this if Bluelog Live is enabled in build
	if (LIVEMODE)
		printf("\t-l                 Start \"Bluelog Live\", default is disabled\n");
	
	printf("\n");
	printf("Logging Options:\n"		
		"\t-n                 Write device names to log, default is disabled\n"
		"\t-c                 Write device class to log, default is disabled\n"
		"\t-f                 Use \"friendly\" device class, default is disabled\n"		
		"\t-t                 Write timestamps to log, default is disabled\n"
		"\t-x                 Obfuscate discovered MACs, default is disabled\n"	
		"\t-b                 Enable BlueProPro log format, see README\n");

	printf("\n");
	printf("Advanced Options:\n"			
		"\t-r <retries>       Name resolution retries, default is 3\n"
		"\t-a <minutes>       Amnesia, Bluelog will forget device after given time\n"
		"\t-s                 Syslog only mode, no log file. Default is disabled\n"
		"\n");
}

static struct option main_options[] = {
	{ "interface", 1, 0, 'i' },
	{ "output",	1, 0, 'o' },
	{ "verbose", 0, 0, 'v' },
	{ "retry", 1, 0, 'r' },
	{ "window", 1, 0, 'w' },
	{ "time", 0, 0, 't' },
	{ "obfuscate", 0, 0, 'x' },
	{ "class", 0, 0, 'c' },
	{ "kill", 0, 0, 'k' },
	{ "friendly", 0, 0, 'f' },
	{ "bluepropro", 0, 0, 'b' },
	{ "name", 0, 0, 'n' },
	{ "help", 0, 0, 'h' },
	{ "daemonize", 0, 0, 'd' },
	{ "syslog", 0, 0, 's' },
	{ "quiet", 0, 0, 'q' },	
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
	
	// Time to scan. Scan time is roughly 1.28 seconds * scan_time
	// Originally this was always 8, now we adjust based on device:
	#ifdef OPENWRT
	int scan_time = 8;
	#elif PWNPLUG
	int scan_time = 5;
	#else
	int scan_time = 3;
	#endif
	#ifdef SQLITE
	/* Database variables */
	sqlite3 *db;
	sqlite3_stmt * stmt;
	sqlite3_stmt * query;
	char *zErrMsg = 0;
	const char * tail = 0;
	int rc;
	long int session_id;
	char * sSQL = 0;
	char * qSQL = 0;
	char * db_name = 0;
	#endif
	// Maximum number of devices per scan
	int max_results = 255;
	int num_results;
	
    // HCI cache setting
	int flags = IREQ_CACHE_FLUSH;
	
	// Strings to hold MAC and name
	char addr[19] = {0};
	char addr_buff[19] = {0};
	
	// String for time
	char cur_time[20];
	
	// Process ID read from PID file
	int ext_pid;
	
	// Settings
	int verbose = 0;
	int obfuscate = 0;
	int showclass = 0;
	int friendlyclass = 0;
	int daemon = 0;
	int bluepropro = 0;
	int getname = 0;
		
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
	int i, opt;
	
	// Record numbner of BlueZ errors
	int error_count = 0;
	
	// Kernel version info
	struct utsname sysinfo;
	uname(&sysinfo);
	
	while ((opt=getopt_long(argc,argv,"+o:i:r:a:w:vxcthldbfnksq", main_options, NULL)) != EOF)
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
		case 'w':
			scan_time =  atoi(optarg);
			break;
		case 'c':
			showclass = 1;
			break;
		case 'f':
			friendlyclass = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 't':
			showtime = 1;
			break;
		case 's':
			syslogonly = 1;
			break;
		case 'x':
			obfuscate = 1;
			break;
		case 'q':
			quiet = 1;
			break;			
		case 'b':
			bluepropro = 1;
			break;
		case 'd':
			daemon = 1;
			break;
		case 'n':
			getname = 1;
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
	
	// No verbose for daemon
	if (daemon)
		verbose = 0;
		
	// No Bluelog Live when running BPP, names on, syslog off
	if (bluepropro) {
		getname = 1;
		syslogonly = 0;
	}

	// Showing raw class ID turns off friendly names
	if (showclass)
		friendlyclass = 0;
			
	// No timestamps in syslog mode, disable other modes
	if (syslogonly)
	{
		showtime = 0;
		bluepropro = 0;
	}

	// Boilerplate
	if (!quiet)
	{
		printf("%s (v%s%s) by MS3FGX\n", APPNAME, VERSION, VER_MOD);
		// That's right, this kind of thing bothers me. Problem?
		#if defined OPENWRT || PWNPLUG
			printf("----");
		#endif
		printf("---------------------------\n");
	}

	// Init Hardware
	ba2str(&bdaddr, addr);
	if (!strcmp(addr, "00:00:00:00:00:00"))
	{
		if (!quiet)
			printf("Autodetecting device...");
		device = hci_get_route(NULL);
		// Put autodetected device MAC into addr
		hci_devba(device, &bdaddr);
		ba2str(&bdaddr, addr);
	}
	else
	{
		if (!quiet)
			printf("Initializing device...");
		device = hci_devid(addr);
	}
	
	// Open device and catch errors
	bt_socket = hci_open_dev(device); 
	if (device < 0 || bt_socket < 0) {
		//sprintf(stdio, "\n");	// Failed to open device, that can't be good
		exit(1);
	}
	
	// If we get here the device should be online.
	if (!quiet)
		printf("OK\n");
	
	// Status message for BPP
	if (!quiet)
		if (bluepropro)
			printf("Output formatted for BlueProPro.\n"
				   "More Info: www.hackfromacave.com\n");
	
	// Open output file
	if (!syslogonly)
	{
		if (!quiet)		
			printf("Opening output file: %s...", outfilename);
		if ((outfile = fopen(outfilename, filemode)) == NULL)
		{
			printf("\n");
			printf("Error opening output file!\n");
			exit(1);
		}
		#ifdef SQLITE
		/* Open and manage SQLITE database */
		db_name = malloc(snprintf(NULL, 0, "%s.db", outfilename) + 1);
		sprintf(db_name, "%s.db", outfilename);
		sqlite3_open(db_name, &db);
		if (!quiet)		
			printf("Opening sqlite db: %s...\n", db_name);
		free(db_name);
		sqlite3_exec(db, TABLE, NULL, NULL, &zErrMsg);
		sqlite3_exec(db, TABLE_EVENT, NULL, NULL, &zErrMsg);
		sqlite3_exec(db, INDEX_MAC, NULL, NULL, &zErrMsg);
        sqlite3_exec(db, INDEX_DATE, NULL, NULL, &zErrMsg);
		sqlite3_exec(db, "PRAGMA synchronous = OFF", NULL, NULL, &zErrMsg);
		rc = sqlite3_exec(db, "PRAGMA journal_mode = MEMORY", NULL, NULL, &zErrMsg);
	 	if( rc ){
			fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
			sqlite3_close(db);
			exit(1);
		}
		/* Log start time and parameters */
		sSQL = malloc( 200 );
		sprintf(sSQL, "INSERT INTO session VALUES (NULL, '%d', '%s')", scan_time, get_localtime(0) );
		sqlite3_exec(db, sSQL, NULL, NULL, &zErrMsg);
		session_id = sqlite3_last_insert_rowid(db);
		sprintf(sSQL, "INSERT INTO record VALUES (NULL, '%ld', @sMAC, @sDATE)", session_id);
		//sSQL = "INSERT INTO records VALUES (NULL, @sMAC, @sDATE)";
		rc = sqlite3_prepare_v2(db, sSQL, strlen(sSQL), &stmt, &tail);
		if (rc) {
			fprintf(stderr, "Can't prepare query: %s\n", sqlite3_errmsg(db));
			sqlite3_close(db);
			exit(1);
		}
		qSQL = "SELECT COUNT(*) FROM record WHERE mac = @sMAC and gathered_on > @sDATE";
		rc = sqlite3_prepare_v2(db, qSQL, strlen(qSQL), &query, NULL);
		if (rc) {
			fprintf(stderr, "Can't prepare query: %s\n", sqlite3_errmsg(db));
			sqlite3_close(db);
			exit(1);
		}
		
		printf("Sqlite statement ready\n");
		#endif
		if (!quiet)
			printf("OK\n");
	}
	else
	{
		if (!quiet)
			printf("In syslog mode, log file disabled.\n");
	}
	
	// Write PID file
	if (!daemon)
		write_pid(getpid());
	
	// Get and print time to console and file
	strcpy(cur_time, get_localtime(0));
		
	if (!daemon) {
		printf("Scan started at [%s] on %s.\n", cur_time, addr);
	}
	
	if (showtime) {
		fprintf(outfile,"[%s] Scan started on %s\n", cur_time, addr);
		// Make sure this gets written out
		fflush(outfile);
	}
		
	// Log success to this point
	syslog(LOG_INFO,"Init OK!");
	
	// Daemon switch
	if (daemon) {
		daemonize();
	} else {
		if (!quiet)
			printf("Hit Ctrl+C to end scan.\n");
	}
	
	// Check for kernel 3.0.x
	if (!strncmp("3.0.",sysinfo.release,4)) {
		printf("\n");
		printf("-----------------------------------------------------\n");
		printf("Device scanning may fail, since you are running a 3.0.x\n");
		printf("Linux kernel. This potential failure is probably due to the\n");
		printf("following kernel bug:\n");
		printf("\n");
		printf("http://marc.info/?l=linux-kernel&m=131629118406044\n");
		printf("\n");
		printf("You will need to upgrade your kernel to at least the\n");
		printf("the 3.1 series to continue.\n");
		printf("-----------------------------------------------------\n");
	}
 
	// Init result struct
	results = (inquiry_info*)malloc(max_results * sizeof(inquiry_info));

	// Seamless loop
	for(;;)
	{
		memset( results, '\0', max_results * sizeof(inquiry_info) ); 
		// Scan and return number of results
		num_results = hci_inquiry(device, scan_time, max_results, NULL, &results, flags);
		
		// A negative number here means an error during scan
		if (num_results < 0) {
			// Increment error count
			error_count++;
			
			// All other platforms, print error and bail out
			syslog(LOG_ERR,"Scan failed, received error from BlueZ!");
			
			syslog(LOG_ERR,"Resetting USB device\n");
			int rc = ioctl( bt_socket, HCIDEVRESET, device);
			if (rc < 0) {
				syslog(LOG_ERR,"Error in ioctl");
				shut_down(1);
			} else {
				printf("Reset successful\n");
			}
			
			// Exit on back to back errors
			if (error_count > 5) {
				syslog(LOG_ERR,"BlueZ not responding, unrecoverable!");
				shut_down(1);
			}
			
			// Otherwise, throttle back a bit, might help
			sleep(1);
			continue;
		}
		else {
			// Clear error counter
			error_count = 0;
		}
		
		/* Explicitly begin a transaction once for all gathered macs at each 
		*  turn; wrapping a sequence of insert by a single transaction makes
		*  inserts faster rather than a transaction for each insert (default)
		*/
		#ifdef SQLITE
		sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &zErrMsg);	
		#endif
		// Loop through results
		for (i = 0; i < num_results; i++) {	
			// Return current MAC from struct
			ba2str(&(results+i)->bdaddr, addr);
            sqlite3_bind_text(query, 1, addr, strlen(addr), SQLITE_STATIC);
       		sqlite3_bind_text(query, 2, get_localtime(20), -1, SQLITE_STATIC);    // 30minutes = 1800seconds
       		int s = sqlite3_step(query);
       		if (s != SQLITE_ROW) {
       		    printf("Unexpected response from sqlite3_step(query)");
       			shut_down(0);
       		}
       		int n_element  = sqlite3_column_int(query, 0);
            sqlite3_clear_bindings(query);
            sqlite3_reset(query);

            if (n_element != 0) {
                printf("%s already logged (%d)\n", addr, n_element);
                break;
            }
            printf("%s) logging (%s)\n", get_localtime(0), addr);
            // Log the new device
			memset(&new_device, 0, sizeof(new_device));
			// Write new device 
			strcpy(new_device.addr, addr);
			
			// Query for name
			if (getname)
				strcpy(new_device.name, namequery(&(results+i)->bdaddr));
			else
				strcpy(new_device.name, "IGNORED");

			// Get time found
			strcpy(new_device.time, get_localtime(0));
			
			// Class info
			new_device.flags = (results+i)->dev_class[2];
			new_device.major_class = (results+i)->dev_class[1];
			new_device.minor_class = (results+i)->dev_class[0];
			
		
			// If we have a device name, get printed
			if (strcmp (new_device.name, "VOID") == 0) {
				// Found with no name.
				// Print message to syslog, prevent printing, and move on
				syslog(LOG_INFO,"Device %s discovered with no name", new_device.addr);
				break;
			}											
				
			// Obfuscate MAC
			if (obfuscate) {
				// Preserve real MAC
				strcpy(new_device.priv_addr, new_device.addr);

				// Split out OUI, replace device with XX
				strncpy(addr_buff, new_device.addr, 9);
				strcat(addr_buff, "XX:XX:XX");
				
				// Copy to DB, clear buffer for next device
				strcpy(new_device.addr, addr_buff);
				memset(addr_buff, '\0', sizeof(addr_buff));
			}
			
			// Write to syslog if we are daemon
			if (daemon);
				if (syslogonly)
					syslog(LOG_INFO,"Found new device: %s",new_device.addr);
			
			// Print everything to console if verbose is on
			if (verbose)
				printf("[%s] %s,%s,0x%02x%02x%02x\n",\
				new_device.time, new_device.addr,\
				new_device.name, new_device.flags,\
				new_device.major_class, new_device.minor_class);
									
            if (bluepropro) {
				// Set output format for BlueProPro
				fprintf(outfile,"%s", new_device.addr);
				fprintf(outfile,",0x%02x%02x%02x", new_device.flags,\
				new_device.major_class, new_device.minor_class);
				fprintf(outfile,",%s\n", new_device.name);
			} else {
				// Flush buffer
				memset(outbuffer, 0, sizeof(outbuffer));
				
				// Print time first if enabled
				if (showtime) {
					sprintf(outbuffer,"[%s] ", new_device.time);
				}
					
				// Always output MAC
				sprintf(outbuffer+strlen(outbuffer),"%s", new_device.addr);
				// Optionally output class
				if (showclass)					
					sprintf(outbuffer+strlen(outbuffer),",0x%02x%02x%02x", new_device.flags,\
					new_device.major_class, new_device.minor_class);
					
				// "Friendly" version of class info
				if (friendlyclass)					
					sprintf(outbuffer+strlen(outbuffer),",%s,(%s)",\
					device_class(new_device.major_class, new_device.minor_class),\
					device_capability(new_device.flags));
					
				// Append the name
				if (getname)
					sprintf(outbuffer+strlen(outbuffer),",%s", new_device.name);
				
				// Send buffer, else file. File needs newline
				if (syslogonly)
					syslog(LOG_INFO,"%s", outbuffer);
				else
					fprintf(outfile,"%s\n",outbuffer);
			}

			#ifdef SQLITE
			sqlite3_bind_text(stmt, 1, new_device.addr, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, new_device.time, -1, SQLITE_STATIC); 
			// run the insert
			sqlite3_step(stmt);
			// Clear stmt for the next insert
			sqlite3_clear_bindings(stmt);
			sqlite3_reset(stmt);
			#endif
			// Write any new changes
			if (!syslogonly)
				fflush(outfile);
			
		} // for to parse results 
		#ifdef SQLITE
		sqlite3_exec(db, "END TRANSACTION", NULL, NULL, &zErrMsg);
		#endif

	}// for seamless
	// Clear out results buffer
	free(results);
	// If we get here, shut down
	shut_down(0);
	// STFU
	return (1);
}

