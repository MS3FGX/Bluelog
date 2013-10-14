/*
 *  libmackerel - Library to manipulate MAC strings
 * 
 *  Written by Tom Nardi (MS3FGX@gmail.com), released under the GPLv2.
 *  For more information, see: www.digifail.com
 */

#define MACKERELVERSION "1.1"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// For OUI lookup
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

// OUI file parameters
// Point to valid file (this is done in config.h for Bluelog)
//#define OUIFILE "oui.txt"
// Characters until vendor name starts
#define TXTOFFSET 9

// CRC parameters (default values are for CRC-32):
const int order = 32;
const unsigned long polynom = 0x4c11db7;
const int direct = 1;
const unsigned long crcinit = 0xffffffff;
const unsigned long crcxor = 0xffffffff;
const int refin = 1;
const int refout = 1;

// Global values:
unsigned long crcmask;
unsigned long crchighbit;
unsigned long crcinit_direct;
unsigned long crcinit_nondirect;
unsigned long crctab[256];

// Get things ready
int mac_init ()
{
	// Make sure PRNG is seeded for MAC gen
	srand(time(NULL));
	
	// This sets up CRC encoding
	int i;
	unsigned long bit, crc;

	crcmask = ((((unsigned long)1<<(order-1))-1)<<1)|1;
	crchighbit = (unsigned long)1<<(order-1);

	crcinit_direct = crcinit;
	crc = crcinit;
	for (i=0; i<order; i++)
	{
		bit = crc & 1;
		if (bit) crc^= polynom;
			crc >>= 1;

		if (bit) crc|= crchighbit;	
	}	
	crcinit_nondirect = crc;
	return(0);
}

// Verify string is (probably) a MAC
int mac_verify (char* test_mac)
{	
	int i;
		
	// Make sure it isn't null
	if (test_mac == NULL)
		return(1);
	
	// See if it's long enough
	if (strlen(test_mac) != 17)
		return(1);
		
	// See if every 3rd character is a colon or dash
	for (i = 2; i < 17; i += 3)
	{
		if ((int)test_mac[i] != 58 && (int)test_mac[i] != 45)
			return(1);
	}
		
	// If we get here, it's (maybe) a MAC
	return(0);
}

// Generate random MAC address
// Adapted from "SpoofTooph" by JP Dunning (.ronin)
char* mac_rand ()
{	
	char addr_part[3] = {0};
	static char addr[18] = {0};
	int i = 0;
	
	// Fill in the middle
	while ( i < 14)
	{
		sprintf(addr_part, "%02X", (rand() % 254));	
		addr[i++] = addr_part[0];
		addr[i++] = addr_part[1];
		addr[i++] = ':';
	}

	// Tack 2 more random characters to finish it
	sprintf(addr_part, "%02X", (rand() % 254));	
	addr[i++] = addr_part[0];
	addr[i++] = addr_part[1];

	return(addr);
}

// Generate half of a random MAC address
char* mac_rand_half (void)
{
	static char half_buffer[9] = {0};
	char *full_buffer = {0};
	
	// Get a new random MAC
	full_buffer = mac_rand();
	
	// Copy half of it to buffer
	strncpy(half_buffer, full_buffer, 8);

	return(half_buffer);
}

// Return MAC OUI segment
char* mac_get_oui (char* full_mac)
{
	// Verify first
	if (mac_verify(full_mac))
		return("INVALID_MAC");	
	
	static char half_buffer[9] = {0};
	
	// Copy half of full into buffer, return
	strncpy(half_buffer, full_mac, 8);
	return(half_buffer);
}

// Return MAC in hex notation
char* mac_get_hex (char* full_mac)
{
	// Verify first
	if (mac_verify(full_mac))
		return("INVALID_MAC");	
	
	int i;
	static char addr_buffer[18] = {0};
	
	// Copy full MAC into buffer
	strncpy(addr_buffer, full_mac, 17);
	
	// Replace every 3rd character with -
	for (i = 2; i < 17; i += 3)
		addr_buffer[i] = 45;
	
	return(addr_buffer);
}

// X out the device-specific part of MAC
char* mac_obfuscate (char* full_mac)
{
	// Verify first
	if (mac_verify(full_mac))
		return("INVALID_MAC");
		
	static char addr_buffer[18] = {0};
	
	// Terminate to prevent duplicating previous results
	memset(addr_buffer, '\0', sizeof(addr_buffer));
	
	// Copy half of full MAC into buffer
	strncpy(addr_buffer, full_mac, 9);
	
	// Replace trimmed characters with XX
	strcat(addr_buffer, "XX:XX:XX");
	
	return(addr_buffer);
}

// Return vendor from OUI database file
// Modified from functions in oui.c from BlueZ
char* mac_get_vendor (char* full_mac)
{
	char oui[9] = {0};
			
	// Return OUI (also verify)
	strncpy(oui, mac_get_oui(full_mac), 9);
	
	struct stat st;
	char *str, *map, *off, *end;
	int fd;

	fd = open(OUIFILE, O_RDONLY);
	if (fd < 0)
		return("NO_OUI_FILE");

	if (fstat(fd, &st) < 0)
	{
		close(fd);
		return("CANT_STAT_FILE");
	}

	str = malloc(128);
	if (!str)
	{
		close(fd);
		return("MALLOC_FAILED");
	}

	memset(str, 0, 128);

	map = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (!map || map == MAP_FAILED)
	{
		free(str);
		close(fd);
		return("MMAP_FAILED");
	}

	off = strstr(map, oui);
	if (off)
	{
		off += TXTOFFSET;
		end = strpbrk(off, "\r\n");
		strncpy(str, off, end - off);
	}
	else
	{
		free(str);
		str = "No Record";
	}

	munmap(map, st.st_size);
	close(fd);
	return str;
}

/*
 * CRC functions based on CRC tester 1.3 written by Sven Reifegerste
 * Available at: http://www.zorc.breitbandkatze.de/crc.html
*/

// Used internally for CRC
unsigned long reflect (unsigned long crc, int bitnum)
{
	unsigned long i, j=1, crcout=0;
	for (i=(unsigned long)1<<(bitnum-1); i; i>>=1) {
		if (crc & i) crcout|=j;
		j<<= 1;
	}
	return (crcout);
}

// Encode MAC with CRC32
char* mac_encode (char* full_mac)
{
	unsigned long i, j, c, bit;
	unsigned long crc = crcinit_direct;
	
	// Static length, could be trouble
	int len = 17;
	
	// For return formatting
	static char addr[18] = {0};

	// Verify first
	if (mac_verify(full_mac))
		return("INVALID_MAC");
	
	for (i=0; i < len; i++)
	{
		c = (unsigned long)*full_mac++;
		if (refin) c = reflect(c, 8);

		for (j=0x80; j; j>>=1) {

			bit = crc & crchighbit;
			crc<<= 1;
			if (c & j) bit^= crchighbit;
			if (bit) crc^= polynom;
		}
	}	

	if (refout) crc=reflect(crc, order);
	crc^= crcxor;
	crc&= crcmask;

	// Format it
	sprintf(addr,"%.8X",(unsigned int)crc);
	return(addr);
}
