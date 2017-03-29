#include <DHT.h>
#include <EtherCard.h>

#define DEBUG 0

#define AGENT_VERS "1.2"
#define AGENT_TYPE "arzs"	// Arduio zabbix sensors
#define AGENT_HOST "test-2"
#define AGENT_PORT 10050
byte mymac[] = { 0x74, 0x12, 0x34, 0x56, 0x78, 0x01 };
const char host[] PROGMEM = "zabbix.local";	// zabbix host

#define LEDPIN A7	// Debug led

#define DHTTYPE DHT22
#define DHTCOUNT 6

#define DHTPIN1 2
#define DHTPIN2 3
#define DHTPIN3 4
#define DHTPIN4 5
#define DHTPIN5 6
#define DHTPIN6 7

DHT* sensors[DHTCOUNT] = {
#ifdef DHTPIN1
	new DHT(DHTPIN1, DHTTYPE),
#endif
#ifdef DHTPIN2
	new DHT(DHTPIN2, DHTTYPE),
#endif
#ifdef DHTPIN3
	new DHT(DHTPIN3, DHTTYPE),
#endif
#ifdef DHTPIN4
	new DHT(DHTPIN4, DHTTYPE),
#endif
#ifdef DHTPIN5
	new DHT(DHTPIN5, DHTTYPE),
#endif
#ifdef DHTPIN6
	new DHT(DHTPIN6, DHTTYPE),
#endif
};

byte Ethernet::buffer[512];
static BufferFiller bfill;
char header[9];
char intbuf[8];
char *json_data;

typedef void(*zabbix_cb)(BufferFiller &buf, String &cmd);

typedef struct {
	const char * cmd; // Name of the item
	const zabbix_cb callback; // Callback handler
} ZabbixConfig;

static const char cmd_agent_1[] PROGMEM = "agent.ping";
static const char cmd_agent_2[] PROGMEM = "agent.version";
static const char cmd_agent_3[] PROGMEM = "agent.type";
static const char cmd_agent_4[] PROGMEM = "agent.hostname";
static const char cmd_memfree[] PROGMEM = "memfree";
static const char cmd_discovery[] PROGMEM = "getids";

#ifdef DHTPIN1
static const char cmd_s_1_t[] PROGMEM = "t[1]";
static const char cmd_s_1_h[] PROGMEM = "h[1]";
#endif
#ifdef DHTPIN2
static const char cmd_s_2_t[] PROGMEM = "t[2]";
static const char cmd_s_2_h[] PROGMEM = "h[2]";
#endif
#ifdef DHTPIN3
static const char cmd_s_3_t[] PROGMEM = "t[3]";
static const char cmd_s_3_h[] PROGMEM = "h[3]";
#endif
#ifdef DHTPIN4
static const char cmd_s_4_t[] PROGMEM = "t[4]";
static const char cmd_s_4_h[] PROGMEM = "h[4]";
#endif
#ifdef DHTPIN5
static const char cmd_s_5_t[] PROGMEM = "t[5]";
static const char cmd_s_5_h[] PROGMEM = "h[5]";
#endif
#ifdef DHTPIN6
static const char cmd_s_6_t[] PROGMEM = "t[6]";
static const char cmd_s_6_h[] PROGMEM = "h[6]";
#endif

static const ZabbixConfig zabbix_config[] = {
	{ cmd_agent_1, &zbx_agent_ping },
	{ cmd_agent_2, &zbx_agent_version },
	{ cmd_agent_3, &zbx_agent_type },
	{ cmd_agent_4, &zbx_agent_host },
	{ cmd_memfree, &zbx_io_memfree },
	{ cmd_discovery, &zbx_discovery },
#ifdef DHTPIN1
	{ cmd_s_1_t, &zbx_dht },
	{ cmd_s_1_h, &zbx_dht },
#endif
#ifdef DHTPIN2
	{ cmd_s_2_t, &zbx_dht },
	{ cmd_s_2_h, &zbx_dht },
#endif
#ifdef DHTPIN3
	{ cmd_s_3_t, &zbx_dht },
	{ cmd_s_3_h, &zbx_dht },
#endif
#ifdef DHTPIN4
	{ cmd_s_4_t, &zbx_dht },
	{ cmd_s_4_h, &zbx_dht },
#endif
#ifdef DHTPIN5
	{ cmd_s_5_t, &zbx_dht },
	{ cmd_s_5_h, &zbx_dht },
#endif
#ifdef DHTPIN6
	{ cmd_s_6_t, &zbx_dht },
	{ cmd_s_6_h, &zbx_dht },
#endif
};

void sendZabbixResponse(BufferFiller &buf, const char* cmd) {
	size_t len = strlen(cmd);
	header[0] = 0x01;
	header[1] = len;

	buf = ether.tcpOffset();
	buf.emit_p(PSTR("ZBXD"));
	buf.emit_raw(header, 9);
	buf.emit_raw(cmd, len);
	ether.httpServerReply(buf.position());
#if (DEBUG == 1)
	Serial.print("\n<- ");
	Serial.println(cmd);
#endif // DEBUG
}

int memoryTest() {
	int byteCounter = 0;
	byte *byteArray;
	while ((byteArray = (byte*)malloc(byteCounter * sizeof(byte))) != NULL)
	{
		byteCounter++;
		free(byteArray);
	}
	free(byteArray);
	return byteCounter;
}

void zbx_io_memfree(BufferFiller &buf, String &cmd) {
	sprintf(intbuf, "%d", memoryTest());
	sendZabbixResponse(buf, intbuf);
}

void zbx_agent_ping(BufferFiller &buf, String &cmd) {
	sendZabbixResponse(buf, "1");
}

void zbx_agent_version(BufferFiller &buf, String &cmd) {
	sendZabbixResponse(buf, AGENT_VERS);
}

void zbx_agent_type(BufferFiller &buf, String &cmd) {
	sendZabbixResponse(buf, AGENT_TYPE);
}

void zbx_agent_host(BufferFiller &buf, String &cmd) {
	sendZabbixResponse(buf, AGENT_HOST);
}

void zbx_discovery(BufferFiller &buf, String &cmd) {
	sendZabbixResponse(buf, json_data);
}

void zbx_dht(BufferFiller &buf, String &cmd) {
	// t[1] = Sensor, #1, Temperature
	char type = cmd.charAt(0);
	char tmp = cmd.charAt(2);
	int num = strtol(&tmp, NULL, 10);	// 10 - base (2, 8, 10, 16)

	if (type == 't')
		dtostrf(sensors[num - 1]->readTemperature(), 2, 1, intbuf);
	else
		dtostrf(sensors[num - 1]->readHumidity(), 2, 1, intbuf);

	sendZabbixResponse(buf, intbuf);
}

void serviceZabbixRequest(BufferFiller &buf, word pos) {
	byte i;
	String cmd = String((char *)Ethernet::buffer + pos);
#if (DEBUG == 1)
	Serial.print("\n-> ");
	Serial.print(cmd);	// newline already in cmd string.
#endif // DEBUG
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
		cmd.remove(cmd.indexOf('\n'));	// FIX: Enc28j60 error in some network (random bytes at end of tcp packet)

		if (cmd == check) {
			(zabbix_config[i].callback)(buf, cmd);
			return;
		}
	}

	sendZabbixResponse(buf, "ZBX_NOTSUPPORTED\0");
}

static void gotPinged(byte* ptr) {
#if (DEBUG == 1)
	ether.printIp("PING: ", ptr);
#endif // DEBUG
}

void setup() {
#if (DEBUG == 1)
	Serial.begin(115200);		// 57600
	Serial.println("[SETUP]");
#endif // DEBUG

	// Setup led
	pinMode(LEDPIN, OUTPUT);
	digitalWrite(LEDPIN, HIGH);

	if (ether.begin(sizeof Ethernet::buffer, mymac, 10) == 0) {
#if (DEBUG == 1)
		Serial.println(F("ERR: Eth"));
#endif // DEBUG
	}

	if (!ether.dhcpSetup()) {
#if (DEBUG == 1)
		Serial.println(F("ERR: Dhcp"));
#endif // DEBUG
	}

	ether.hisport = AGENT_PORT;
	ether.dnsLookup(host);

#if (DEBUG == 1)
	ether.printIp("Ip: ", ether.myip);
	ether.printIp("Gw: ", ether.gwip);
	ether.printIp("Dns: ", ether.dnsip);
	ether.printIp("Srv: ", ether.hisip);
#endif // DEBUG

	ether.registerPingCallback(gotPinged);
	ether.persistTcpConnection(false);

	// var set
	memset(header, 0, sizeof header);

	for (byte i = 0; i < DHTCOUNT; i++)
	{
		char buff[16];
		sensors[i]->begin();	// Start sensors.

#if (DEBUG == 1)
		Serial.print(F("DHT_"));
		Serial.println(i);
#endif // DEBUG
	}

	json_data = PSTR("{\"data\":["
#ifdef DHTPIN1
		"{\"{#SID}\":1}"
#endif
#ifdef DHTPIN2
		",{\"{#SID}\":2}"
#endif
#ifdef DHTPIN3
		",{\"{#SID}\":3}"
#endif
#ifdef DHTPIN4
		",{\"{#SID}\":4}"
#endif
#ifdef DHTPIN5
		",{\"{#SID}\":5}"
#endif
#ifdef DHTPIN6
		",{\"{#SID}\":6}"
#endif
		"]}");

	delay(2000);	// 2 sec for sensors init
	digitalWrite(LEDPIN, LOW);	// End of config, disable led.
#if (DEBUG == 1)
	Serial.print(F("JS: "));
	Serial.println(json_data);
	Serial.println(F("DONE"));
#endif // DEBUG
}

void loop() {
	word len = ether.packetReceive();
	word pos = ether.packetLoop(len);

	if (len) {
		if ((pos = ether.accept(10050, len))) {
			digitalWrite(LEDPIN, HIGH);
			serviceZabbixRequest(bfill, pos);
			digitalWrite(LEDPIN, LOW);
		}
	}
}
