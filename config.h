// Configuration file for Bluelog
#define VERSION	"1.1.1-dev"
#define APPNAME "Bluelog"

// Platform generic settings
#define MAX_SCAN 30
#define MIN_SCAN 3

// Determine device-specific configs
// OpenWRT specific
#ifdef OPENWRT
#define VER_MOD "-WRT"
// Maximum number of devices in cache
#define MAX_DEV 2048
// Toggle Bluelog Live
#define LIVEMODE 1
// Toggle OUI lookups
#define OUILOOKUP 0
// Default log
#define OUT_PATH "/tmp/"
// Bluelog Live device list
#define LIVE_OUT "/tmp/live.log"
// Bluelog Live status info
#define LIVE_INF "/tmp/info.txt"
// PID storage
#define PID_FILE "/tmp/bluelog.pid"
// File for OUI database
#define OUIFILE ""
// PWNPLUG specific
#elif PWNPLUG
#define VER_MOD "-PWN"
#define MAX_DEV 4096
#define LIVEMODE 1
#define OUILOOKUP 0
#define OUT_PATH "/dev/shm/"
#define LIVE_OUT "/tmp/live.log"
#define LIVE_INF "/tmp/info.txt"
#define PID_FILE "/tmp/bluelog.pid"
#define OUIFILE ""
#else
// Generic x86
#define VER_MOD ""
#define MAX_DEV 4096
#define LIVEMODE 1
#define OUILOOKUP 1
#define OUT_PATH ""
#define LIVE_OUT "/tmp/live.log"
#define LIVE_INF "/tmp/info.txt"
#define PID_FILE "/tmp/bluelog.pid"
#define OUIFILE "/usr/share/bluelog/MACLIST"
#endif
