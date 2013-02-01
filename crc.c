/* Based on CRC tester 1.3 written by Sven Reifegerste (zorc/reflex)
 * Available at: http://www.zorc.breitbandkatze.de/crc.html
*/

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

// Setup
void init_crc ()
{
	int i;
	unsigned long bit, crc;

	// at first, compute constant bit masks for whole CRC and CRC high bit
	crcmask = ((((unsigned long)1<<(order-1))-1)<<1)|1;
	crchighbit = (unsigned long)1<<(order-1);

	// compute missing initial CRC value
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
}

unsigned long reflect (unsigned long crc, int bitnum) {

	// reflects the lower 'bitnum' bits of 'crc'

	unsigned long i, j=1, crcout=0;

	for (i=(unsigned long)1<<(bitnum-1); i; i>>=1) {
		if (crc & i) crcout|=j;
		j<<= 1;
	}
	return (crcout);
}

unsigned int crc_hash (unsigned char* p, unsigned long len) {

	// fast bit by bit algorithm without augmented zero bytes.
	// does not use lookup table, suited for polynom orders between 1...32.

	unsigned long i, j, c, bit;
	unsigned long crc = crcinit_direct;

	for (i=0; i<len; i++) {

		c = (unsigned long)*p++;
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

	return(crc);
}

