// Configuration file for Bluelog
#define VERSION	"1.1.3-dev"
#define APPNAME "Bluelog"

// Generic 
#define MAX_SCAN 30
#define MIN_SCAN 3

// Device specific

// OpenWRT
#ifdef OPENWRT
#define VER_MOD "-WRT"
#define MAX_DEV 2048
#define LIVEMODE 1
#define OUILOOKUP 0
#define OUT_PATH "/tmp/"
#define LIVE_OUT "/tmp/live.log"
#define LIVE_INF "/tmp/info.txt"
#define PID_FILE "/tmp/bluelog.pid"
#define OUIFILE ""
#define CFG_FILE "/etc/bluelog/bluelog.conf"
// Pwnie Express Pwn Plug
#elif PWNPLUG
#define VER_MOD "-PWN"
#define MAX_DEV 2048
#define LIVEMODE 1
#define OUILOOKUP 0
#define OUT_PATH "/dev/shm/"
#define LIVE_OUT "/tmp/live.log"
#define LIVE_INF "/tmp/info.txt"
#define PID_FILE "/tmp/bluelog.pid"
#define OUIFILE ""
#define CFG_FILE "/etc/bluelog/bluelog.conf"
// Pwnie Express Pwn Pad
#elif PWNPAD
#define VER_MOD "-PAD"
#define MAX_DEV 2048
#define LIVEMODE 0
#define OUILOOKUP 1
#define OUT_PATH "/opt/pwnpad/captures/bluetooth/"
#define LIVE_OUT ""
#define LIVE_INF ""
#define PID_FILE "/tmp/bluelog.pid"
#define OUIFILE "/usr/share/bluelog/oui.txt"
#define CFG_FILE "/etc/bluelog/bluelog.conf"
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
#define OUIFILE "/etc/bluelog/oui.txt"
#define CFG_FILE "/etc/bluelog/bluelog.conf"
#endif

// These override the platform-specific options.
// Live
#ifdef NOLIVE
#define LIVEMODE 0
#endif

// OUI Lookups
#ifdef NOOUI
#define OUILOOKUP 0
#endif
