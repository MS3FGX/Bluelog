/*
 * The following is based on code from "Inquisition", a Bluetooth
 * scanner written by Michael John Wensley and released under the GPLv2
 * 
 * More info can be found at: http://www.wensley.org.uk/
 */

// Device classes
#define ENT(e) (sizeof(e)/sizeof(char*))
static char *majors[] = {" Misc", " Computer", " Phone", " Network", " A/V",\
                         " Peripheral", " Imaging", " Wearable", " Toy"};

static char* computers[] = {"Misc", "Desktop", "Server", "Laptop", "Handheld",\
                            "Palm", "Wearable"};

static char* phones[] = {"Misc", "Cell", "Cordless", "Smart", "Wired",\
                         "ISDN", "Sim Card Reader For "};

static char* av[] = {"Misc", "Headset", "Handsfree", "Reserved", "Microphone",\
                "Loudspeaker", "Headphones", "Portable", "Car", "STB", "HiFi",\
                "VCR", "Video Camera", "Camcorder",\
                "Video Display and Loudspeaker", "Video Conferencing",\
                "Reserved", "Game / Toy"};

static char* peripheral[] = {"Misc", "Joystick", "Gamepad", "Remote control",\
                "Sensing device", "Digitiser Tablet", "Card Reader"};

static char* wearable[] = {"Misc", "Wrist Watch", "Pager", "Jacket", "Helmet",\
                           "Glasses"};

static char* toys[] = {"Misc", "Robot", "Vehicle", "Character", "Controller",\
                       "Game"};
// End device classes

// Return device capabilities
char* device_capability(uint8_t flags)
{
	// Response to pass back
	static char response_string[64];
	
	// Terminate to prevent duplicating previous results
	memset(response_string, '\0', sizeof(response_string));

	if (flags & 0x1)
		strcat(response_string, "Position ");
	if (flags & 0x2)
		strcat(response_string, "Net ");
	if (flags & 0x4)
		strcat(response_string, "Render ");
	if (flags & 0x8)
		strcat(response_string, "Capture ");
	if (flags & 0x10)
		strcat(response_string, "OBEX ");
	if (flags & 0x20)
		strcat(response_string, "Audio ");
	if (flags & 0x40)
		strcat(response_string, "Phone ");
	//if (flags & 0x80) strcat(response_string, ", Info");
	
	// If all else fails, give it something to show
	if (flags == 0)
		strcpy(response_string, "VOID");
	else
		// Remove trailing space from response
		response_string[(strlen(response_string)-1)] = '\0';
	
	return(response_string);
}

// Return device class
char* device_class(uint8_t major, uint8_t minor)
{
	// Response to pass back
	static char response_string[64];
	
	// Terminate to prevent duplicating previous results
	memset(response_string, '\0', sizeof(response_string));
	
	// Convert minor
	minor = minor >> 2;
	
	// Parse major class
	switch (major)
	{
	case 1:
		if (minor <= ENT(computers))
			strcat(response_string, computers[minor]);
		break;
	case 2:
		if (minor <= ENT(phones))
			strcat(response_string, phones[minor]);
		break;
	case 3:
		// Huh?
		//printf(" Usage %d/56", minor);
		break;
	case 4:
		if (minor <= ENT(av))
			strcat(response_string, av[minor]);
		break;
	case 5:
		if ((minor & 0xF) <= ENT(peripheral))
			strcat(response_string, peripheral[(minor & 0xF)]);
		if (minor & 0x10)
			strcat(response_string, " with keyboard");
		if (minor & 0x20)
			strcat(response_string, " with pointer");
		break;
	case 6:
		if (minor & 0x2)
			strcat(response_string, " with display");
		if (minor & 0x4)
			strcat(response_string, " with camera");
		if (minor & 0x8)	
			strcat(response_string, " with scanner");
		if (minor & 0x10)
			strcat(response_string, " with printer");
		break;
	case 7:
		if (minor <= ENT(wearable)) strcat(response_string, wearable[minor]);
		break;
	case 8:
		if (minor <= ENT(toys)) strcat(response_string, toys[minor]);
		break;
	default:
		// Handle unknown devices, leave early.
		strcat(response_string, "VOID");
		return(response_string);
	}
	strcat(response_string, majors[major]);
	return(response_string);
}

