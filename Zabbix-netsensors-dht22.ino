#include <EtherCard.h>
#include <DHT.h>

//#define DEBUG

#define AGENT_VERSION "Zas v1.1"	// Zabbix Arduino Sensors
#define AGENT_TYPE "arduino-zas"
#define AGENT_PORT 10050

#define LEDPIN A2	// Debug led

#define DHTPIN1 2
#define DHTPIN2 3
#define DHTPIN3 4
#define DHTPIN4 5
#define DHTPIN5 6
#define DHTPIN6 7

#define DHTCOUNT 3
#define DHTTYPE DHT22

DHT* sensors[DHTCOUNT] = {
	new DHT(DHTPIN1, DHTTYPE),
	new DHT(DHTPIN2, DHTTYPE),
	new DHT(DHTPIN3, DHTTYPE),
	//new DHT(DHTPIN4, DHTTYPE),
	//new DHT(DHTPIN5, DHTTYPE),
	//new DHT(DHTPIN6, DHTTYPE),
};

byte mymac[] = { 0x74, 0x12, 0x34, 0x56, 0x78, 0x02 };
const char host[] PROGMEM = "zabbix.local";	// zabbix host

byte Ethernet::buffer[512];
static BufferFiller bfill;

typedef void(*zabbix_cb)(BufferFiller &buf, String &cmd);

typedef struct {
	const char * cmd; // Name of the item
	const zabbix_cb callback; // Callback handler
} ZabbixConfig;

static const char cmd_agent_1[] PROGMEM = "agent.ping";
static const char cmd_agent_2[] PROGMEM = "agent.version";
static const char cmd_agent_3[] PROGMEM = "agent.type";

static const char cmd_s_1_t[] PROGMEM = "s.1.t";
static const char cmd_s_1_h[] PROGMEM = "s.1.h";

static const char cmd_s_2_t[] PROGMEM = "s.2.t";
static const char cmd_s_2_h[] PROGMEM = "s.2.h";

static const char cmd_s_3_t[] PROGMEM = "s.3.t";
static const char cmd_s_3_h[] PROGMEM = "s.3.h";
/*
static const char cmd_s_4_t[] PROGMEM = "s.4.t";
static const char cmd_s_4_h[] PROGMEM = "s.4.h";
static const char cmd_s_5_t[] PROGMEM = "s.5.t";
static const char cmd_s_5_h[] PROGMEM = "s.5.h";
static const char cmd_s_6_t[] PROGMEM = "s.6.t";
static const char cmd_s_6_h[] PROGMEM = "s.6.h";
*/


static const ZabbixConfig zabbix_config[] = {
	{ cmd_agent_1, &zbx_agent_ping },
	{ cmd_agent_2, &zbx_agent_version },
	{ cmd_agent_3, &zbx_agent_type },
	{ cmd_s_1_t, &zbx_dht },
	{ cmd_s_1_h, &zbx_dht },
	{ cmd_s_2_t, &zbx_dht },
	{ cmd_s_2_h, &zbx_dht },
	{ cmd_s_3_t, &zbx_dht },
	{ cmd_s_3_h, &zbx_dht },
	/*
	{ cmd_s_4_t, &zbx_dht },
	{ cmd_s_4_h, &zbx_dht },
	{ cmd_s_5_t, &zbx_dht },
	{ cmd_s_5_h, &zbx_dht },
	{ cmd_s_6_t, &zbx_dht },
	{ cmd_s_6_h, &zbx_dht },
	*/
};

void sendZabbixResponse(BufferFiller &buf, const char* cmd) {
	char header[9];
	byte i, len;

	len = strlen(cmd);

	// Create the header
	for (i = 0; i<9; i++) {
		switch (i) {
		case 0:
			header[i] = 0x01; break;
		case 1:
			header[i] = len; break;
		default:
			header[i] = 0;
		}
	}

	buf = ether.tcpOffset();
	buf.emit_p(PSTR("ZBXD"));
	buf.emit_raw(header, 9);
	buf.emit_raw(cmd, len);
	ether.httpServerReply(buf.position());
#ifdef DEBUG
	Serial.print("<< ");
	Serial.println(cmd);
#endif // DEBUG
}

void zbx_agent_ping(BufferFiller &buf, String &cmd) {
	sendZabbixResponse(buf, "1");
}

void zbx_agent_version(BufferFiller &buf, String &cmd) {
	sendZabbixResponse(buf, AGENT_VERSION);
}

void zbx_agent_type(BufferFiller &buf, String &cmd) {
	sendZabbixResponse(buf, AGENT_TYPE);
}

void zbx_dht(BufferFiller &buf, String &cmd) {
	char buffer[8];
	// s.1.t = Sensor, #1, Temperature
	char type = cmd.charAt(4);
	char tmp = cmd.charAt(2);
	int num = strtol(&tmp, NULL, 10);	// 10 - base (2, 8, 10, 16)

	if (type == 't')
		dtostrf(sensors[num - 1]->readTemperature(), 2, 1, buffer);
	else
		dtostrf(sensors[num - 1]->readHumidity(), 2, 1, buffer);

	sendZabbixResponse(buf, buffer);
}

void serviceZabbixRequest(BufferFiller &buf, word pos) {
	byte i;
	String cmd = String((char *)Ethernet::buffer + pos);
	String check;
	const char *ptr;
	char c;
	for (i = 0; i<(sizeof zabbix_config) / (sizeof(ZabbixConfig)); i++) {
		// read from the eeprom
		check = "";
		ptr = zabbix_config[i].cmd;
		while ((c = pgm_read_byte(ptr++))) {
			check += c;
		}
		check += '\n'; // every command ends with a newline
					   // now check contains the command name

					   // Check it agains the current command
		if (cmd == check) {
#ifdef DEBUG
			Serial.print(">> ");
			Serial.print(cmd);	// newline already in cmd string.
#endif // DEBUG
			(zabbix_config[i].callback)(buf, cmd);
			return;
		}
	}

	sendZabbixResponse(buf, "ZBX_NOTSUPPORTED");
}

static void gotPinged(byte* ptr) {
#ifdef DEBUG
	ether.printIp(">> ping from: ", ptr);
#endif // DEBUG
}

void setup() {
#ifdef DEBUG
	Serial.begin(57600);		// 57600
	Serial.println("[SETUP]");
#endif // DEBUG

	// Setup led
	pinMode(LEDPIN, OUTPUT);
	digitalWrite(LEDPIN, HIGH);	// Config start, enable led

								// Setup ethernet
	if (ether.begin(sizeof Ethernet::buffer, mymac, 10) == 0) {
#ifdef DEBUG
		Serial.println(F("ETH: NO ACCESS"));
#endif // DEBUG
	}

	if (!ether.dhcpSetup()) {
#ifdef DEBUG
		Serial.println(F("ETH: NO DHCP"));
#endif // DEBUG
	}

	ether.hisport = AGENT_PORT;
	ether.dnsLookup(host);

#ifdef DEBUG
	ether.printIp("Eth.Ip: ", ether.myip);
	ether.printIp("Eth.Gw: ", ether.gwip);
	ether.printIp("Eth.Dns: ", ether.dnsip);
	ether.printIp("Eth.Srv: ", ether.hisip);
#endif // DEBUG

	ether.registerPingCallback(gotPinged);
	ether.persistTcpConnection(false);

	for (int i = 0; i < DHTCOUNT; i++)
	{
		sensors[i]->begin();	// Start sensors.
	}

	delay(2000);	// 2 sec for sensors init
	digitalWrite(LEDPIN, LOW);	// End of config, disable led.
}

void loop() {
	word len = ether.packetReceive();
	//word pos;
	word pos = ether.packetLoop(len);

	if (len) {
		if ((pos = ether.accept(10050, len))) {
			digitalWrite(LEDPIN, HIGH);
			serviceZabbixRequest(bfill, pos);
			digitalWrite(LEDPIN, LOW);
		}
	}
}
