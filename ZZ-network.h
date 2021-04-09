


const char* ssid = "Linksys";
const char* password = "typhoon22";

// change to suit your local network
#define IPA  IPAddress(192, 168, 3, 150)		
#define GATE IPAddress(192, 168, 3, 1)			
#define MASK IPAddress(255, 255, 255, 0)		
#define DNS  IPAddress(192, 168, 3, 1)

// the MAC address of the wifi router
uint8_t target_mac[6] = { 0xE8, 0x94 , 0xF6 ,0x5C ,0xC2 ,0x2F };
int channel = 6;								// the wifi channel to be used
