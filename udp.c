/*
 *  udp.c - Establish UDP communications with a remote server
 *
 * Based on code submitted by Ian Macdonald, released under the GPLv2.
 */
 
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>

// Global UDP struct
struct sockaddr_in adr_srvr; 	

// Make sure network config is valid, otherwise, bail out.
static void net_cfg_check (void)
{
	// Bro, do you even config?
	if (!strcmp (config.server_ip, "NULL"))
	{
		printf("No server IP configured! See README.NET\n");
		exit(1);
	}
	
	// If no node name has been configured, use hostname
	if (!strcmp (config.node_name, "NULL"))
	{
		// Get hostname
		char hostname[64];
		hostname[63] = '\0';
		gethostname(hostname, 63);
		strcpy(config.node_name, hostname);
	}
}

// Send string over UDP socket
int send_udp_msg (char* msg_string)
{  				
	if (config.prefix)
	{
		// Send node name first, no newline
		sendto(config.udp_socket, config.node_name, strlen(config.node_name), 0, (struct sockaddr *)&adr_srvr, (sizeof adr_srvr));
		sendto(config.udp_socket, ": ", strlen(": "), 0, (struct sockaddr *)&adr_srvr, (sizeof adr_srvr));
	}
	
	if ((sendto(config.udp_socket, msg_string, strlen(msg_string), 0, (struct sockaddr *)&adr_srvr, (sizeof adr_srvr))) < 0)
	{
		printf("Failed to send UDP message!\n");
		exit(1);
	}
	
	return 0;
}

// Open a UDP socket to configured IP/port
int open_udp_socket (void)
{
	// Verify config
	net_cfg_check();
	
	// Setup socket
	memset(&adr_srvr,0,sizeof adr_srvr);
	adr_srvr.sin_family = AF_INET;
	adr_srvr.sin_port = htons(config.server_port);
	adr_srvr.sin_addr.s_addr =  inet_addr(config.server_ip);

	if (!config.quiet)
		printf("Opening UDP socket to %s:%i...", config.server_ip, config.server_port);
	
	// Valid IP?
	if (adr_srvr.sin_addr.s_addr == INADDR_NONE)
	{
		printf("Invalid IP Address!\n");
		exit(1);
	}

	// Create a UDP socket to use
	if ((config.udp_socket = socket(AF_INET,SOCK_DGRAM, 0)) == -1)
	{
		printf("Error opening socket!\n");
		exit(1);
	}

	// Announce we've connected
	if (config.banner)
	{
		char MsgBuffer[256] = {0};
		sprintf(MsgBuffer+strlen(MsgBuffer),"Version: %s Device: %s\n" , VERSION, config.addr);
		send_udp_msg(MsgBuffer);
	}
	
	// All good
	printf("OK\n");
	return 0;
}
