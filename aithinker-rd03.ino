#include <Arduino.h>
#include <SoftwareSerial.h>
#include <MiniShell.h>

#include "rd03-protocol.h"

#define RADAR_BAUD_RATE 115200
#define PIN_RX  D2
#define PIN_TX  D3
#define printf Serial.printf

static MiniShell shell(&Serial);
static SoftwareSerial radar(PIN_RX, PIN_TX);
static RD03Protocol proto_ack = RD03Protocol(0xFAFBFCFD, 0x01020304);
static RD03Protocol proto_rsp = RD03Protocol(0xF1F2F3F4, 0xF5F6F7F8);
static bool debug = false;

static void printhex(const char *prefix, const uint8_t *buf, size_t len)
{
    printf(prefix);
    for (size_t i = 0; i < len; i++) {
        printf(" %02X", buf[i]);
    }
    printf("\n");
}

static int do_reboot(int argc, char *argv[])
{
    ESP.restart();
    return 0;
}

static int do_debug(int argc, char *argv[])
{
    debug = !debug;
    printf("debug: %s\n", debug ? "ON" : "OFF");
    return 0;
}

static bool radar_command_raw(uint16_t cmd, size_t cmd_len, const uint8_t *cmd_data)
{
    uint8_t buf[32];

    // send
    size_t len = proto_ack.build_command(buf, cmd, cmd_len, cmd_data);
    printhex("CMD <", buf, len);
    radar.write(buf, len);

    // wait
    unsigned long begin = millis();
    while ((millis() - begin) < 1000) {
        if (radar.available()) {
            char c = radar.read();
            if (proto_ack.process_rx(c)) {
                int len = proto_ack.get_data(buf);
                printhex("ACK <", buf, len);
                return true;
            }
        }
    }
    return false;
}

static bool radar_open(void)
{
    uint8_t data[2];

    data[0] = 1;
    data[1] = 0;
    return radar_command_raw(0x00FF, 2, data);
}

static bool radar_close(void)
{
    return radar_command_raw(0x00FE, 0, NULL);
}

static bool radar_command(uint16_t cmd, size_t cmd_len, const uint8_t *cmd_data)
{
    // open
    if (!radar_open()) {
        printf("radar_open failed!\n");
        return -1;
    }
    delay(100);

    // command
    if (!radar_command_raw(cmd, cmd_len, cmd_data)) {
        printf("radar_command_raw() failed!\n");
    }
    delay(100);

    // close
    radar_close();
    return 0;
}

static int do_mode(int argc, char *argv[])
{
    if (argc < 2) {
        return -1;
    }
    int mode = atoi(argv[1]);

    uint8_t data[6];
    data[0] = 0;
    data[1] = 0;
    data[2] = mode;
    data[3] = 0;
    data[4] = 0;
    data[5] = 0;
    if (!radar_command(0x0012, sizeof(data), data)) {
        printf("radar_command() failed!\n");
    }
    return 0;
}

// example data 01 78 00 AA 0C 51 02 25 00 34 00 22 00 12 00 11 00 50 00 3A 00 22 00 1D 00 1D 00 28 00 25 00 28 00 11 00
static void decode_report(size_t len, const uint8_t *buf)
{
    if (len != 0x23) {
        return;
    }

    int index = 0;
    bool present = buf[index++];
    uint16_t range = buf[index++];
    range += buf[index++] << 8;
    printf("%3s %5ucm", present ? "ON" : "OFF", range);
    for (int i = 0; i < 16; i++) {
        uint16_t energy = buf[index++];
        energy += buf[index++] << 8;
        printf(" %5u", energy);
    }
    printf("\n");
}

static const cmd_t commands[] = {
    { "reboot", do_reboot, "Reboot" },
    { "debug", do_debug, "Toggle debug mode" },
    { "mode", do_mode, "Set report mode (0=debug,4=binary,100=ascii)" },
    { NULL, NULL, NULL }
};

void setup(void)
{
    Serial.begin(115200);
    radar.begin(RADAR_BAUD_RATE);
}

void loop(void)
{
    uint8_t buf[256];

    shell.process(">", commands);

    // process incoming data from radar
    while (radar.available()) {
        uint8_t c = radar.read();
        if (debug) {
            printf(" %02X", c);
        }
        // run receive state machine
        if (proto_rsp.process_rx(c)) {
            size_t len = proto_rsp.get_data(buf);
            printhex("RSP <", buf, len);
            decode_report(len, buf);
        }
    }
}
