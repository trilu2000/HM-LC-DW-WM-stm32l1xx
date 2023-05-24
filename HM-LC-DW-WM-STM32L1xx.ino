//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// HM-LC-DW-WM modified for the STM32L1xx CPU by trilu
//- -----------------------------------------------------------------------------------------------------------------------

#if __has_include("private.h") 
  #include "private.h"
#else
  #define DEVICE_ID {0x12, 0x34, 0x56}
  #define SERIAL_ID "HB01234567"
#endif

//#define HIDE_IGNORE_MSG
//#define DIMMER_EXTRA_DEBUG
#define RADIOWATCHDOG
#define RADIO_EXTRA_DEBUG
//#define NDEBUG
#undef NDEBUG
#define USE_CCA

// as we have no defined HW in STM32duino yet, we are working on base of the default STM32L152CB board
// which has a different pin map. deviances are handled for the moment in a local header file. 
// This might change when an own HW for AskSin is defined
#include "AskSin32Duino.h"

// Derive ID and Serial from the device UUID
//#define USE_HW_SERIAL

#include <SPI.h>
#include <AskSinPP.h>
#include <Dimmer.h>

// Pin definition of the specific device
#define CONFIG_BUTTON_PIN   PC15
#define LED1_PIN            PC14
#define LED2_PIN            PC13
#define DIMMER1_PIN         PB4
#define DIMMER2_PIN         PB5

// number of available peers per channel
#define PEERS_PER_CHANNEL 10

// all library classes are placed in the namespace 'as'
using namespace as;

// define all device properties
const struct DeviceInfo PROGMEM devinfo = {
  DEVICE_ID,              // Device ID
  SERIAL_ID,              // Device Serial
  
  {0x01,0x08},            // Device Model: HM-LC-DW-WM dual white LED dimmer
  //{0x00,0x67},          // Device Model: HM-LC-Dim1PWM-CV
  0x2C,                   // Firmware Version
  as::DeviceType::Dimmer, // Device Type
  {0x01,0x00}             // Info Bytes
};

// Configure the used hardware
typedef LibSPI<CC1101_CS_PIN> RadioSPI;
typedef Radio<RadioSPI, CC1101_GDO0_PIN> RadioType;
typedef DualStatusLed<LED1_PIN, LED2_PIN> LedType;
typedef AskSin<LedType, NoBattery, RadioType> HalType;
typedef DimmerChannel<HalType,PEERS_PER_CHANNEL> ChannelType;
typedef DimmerDevice<HalType, ChannelType, 6, 3> DimmerType;     // HM-LC-DW-WM dual white LED dimmer
//typedef DimmerDevice<HalType, ChannelType, 3, 3> DimmerType;   // HM-LC-Dim1PWM-CV

HalType hal;
DimmerType sdev(devinfo,0x20);
DualWhiteControl<HalType, DimmerType, PWM16<> > control(sdev);   // HM-LC-DW-WM dual white LED dimmer
//DimmerControl<HalType,DimmerType, PWM16<> > control(sdev);     // HM-LC-Dim1PWM-CV
ConfigToggleButton<DimmerType> cfgBtn(sdev);

// internal cpu reading function for STM32L1xx
class TempSens : public Alarm {
  uint16_t vref;
  uint16_t temp;
public:
  TempSens() : Alarm(0) {}
  virtual ~TempSens() {}

  void init() {
    DPRINT(F("internal temp sensor "));
    // read vref to see if it works
    vref = readVref();
    if (vref) {
      DPRINTLN(F("found"));
      set(seconds2ticks(10));
      sysclock.add(*this);
    }
    else {
      DPRINTLN(F("not available"));
    }
  }

  virtual void trigger(AlarmClock& clock) {
    vref = readVref();
    temp = readTempSensor(vref) * 10;
    DPRINT(F("tmp ")); DPRINTLN(temp);
    control.setTemperature(temp);
    set(seconds2ticks(120));
    clock.add(*this);
  }
};
TempSens tempsensor;


void setup () {
  delay(1000);
  DINIT(57600,ASKSIN_PLUS_PLUS_IDENTIFIER);
  //storage().setByte(0, 0);

  bool first = control.init(hal, DIMMER1_PIN, DIMMER2_PIN);      // HM-LC-DW-WM dual white LED dimmer
  //bool first = control.init(hal, DIMMER1_PIN);                 // HM-LC-Dim1PWM-CV
  buttonISR(cfgBtn, CONFIG_BUTTON_PIN);

  if (first == true) {
    sdev.channel(1).peer(cfgBtn.peer());
    sdev.channel(2).peer(cfgBtn.peer());
    sdev.channel(3).peer(cfgBtn.peer());
    sdev.channel(4).peer(cfgBtn.peer());
    sdev.channel(5).peer(cfgBtn.peer());
    sdev.channel(6).peer(cfgBtn.peer());
  }

  tempsensor.init();
  sdev.initDone();
  DDEVINFO(sdev);
}

void loop() {
  bool worked = hal.runready();
  bool poll = sdev.pollRadio();
  if( worked == false && poll == false ) {
    //hal.activity.savePower<Idle<true> >(hal);
  }
}


#ifdef RADIO_EXTRA_DEBUG
/* Use serial input function to debug CC1101 status at runtime
*  Input format: $<info> 's' or 'r' followed by a return
*  There are two types available, s = CC1101_STATUS, c = CC1101_CONFIG
* 
*  CC1101_STATUS:
*  CC1101_PARTNUM          0x30               (0x00)  Chip ID
*  CC1101_VERSION          0x31               (0x04)  Chip ID
*  CC1101_FREQEST          0x32               (0x00)  Frequency Offset Estimate from Demodulator
*  CC1101_LQI              0x33               (0x00)  Demodulator Estimate for Link Quality
*  CC1101_RSSI             0x34               (0x00)  Received Signal Strength Indication
*  CC1101_MARCSTATE        0x35               (0x00)  Main Radio Control State Machine State
*  CC1101_WORTIME1         0x36               (0x00)  High Byte of WOR Time
*  CC1101_WORTIME0         0x37               (0x00)  Low Byte of WOR Time
*  CC1101_PKTSTATUS        0x38               (0x00)  Current GDOx Status and Packet Status
*  CC1101_VCO_VC_DAC       0x39               (0x00)  Current Setting from PLL Calibration Module
*  CC1101_TXBYTES          0x3A               (0x00)  Underflow and Number of Bytes
*  CC1101_RXBYTES          0x3B               (0x00)  Overflow and Number of Bytes
*  CC1101_RCCTRL1_STATUS   0x3C               (0x00)  Last RC Oscillator Calibration Result
*  CC1101_RCCTRL0_STATUS   0x3D               (0x00)  Last RC Oscillator Calibration Result
*
*/

#include "inputparser.h"
InputParser parser(50, (InputParser::Commands*)cmdTab);

const InputParser::Commands cmdTab[] PROGMEM = {
//  { 'h', 0, showHelp },
  { 'r', 1, readCCstatus },
  { 's', 7, sendCmdStr },
  { 'p', 0, listPeers },
//  { 'f', 2, writeEEprom },
//  { 'c', 0, clearEEprom },
//  { 't', 0, testConfig },

//  { 'b', 1, buttonSend },
//  { 'a', 0, stayAwake },

//  { 'r', 0, resetDevice },
  { 0 }
};

RadioSPI spi;

void readCCstatus() {
  uint8_t ret, info;
  parser >> info;
  ret = spi.readReg(info, CC1101_STATUS);
  DPRINT(F("\nread status: 0x")); DHEX(info); DPRINT(F(": 0x")); DHEX(ret); DPRINT('\n');

  switch (info) {
  case 0x33:  // LQI
    DPRINT(F("LQI: ")); DPRINT(ret & 0x7F);
    if (ret & 0x80) DPRINT(F(", CRC_OK"));
    DPRINT('\n');
    break;
  case 0x35:  // Marcstate
    DPRINT(F("MARCSTATE_"));
    if      (ret == 0x00) DPRINT(F("SLEEP"));
    else if (ret == 0x01) DPRINT(F("IDLE"));
    else if (ret == 0x02) DPRINT(F("XOFF"));
    else if (ret == 0x03) DPRINT(F("VCOON_MC"));
    else if (ret == 0x04) DPRINT(F("REGON_MC"));
    else if (ret == 0x05) DPRINT(F("MANCAL"));
    else if (ret == 0x06) DPRINT(F("VCOON"));
    else if (ret == 0x07) DPRINT(F("REGON"));
    else if (ret == 0x08) DPRINT(F("STARTCAL"));
    else if (ret == 0x09) DPRINT(F("BWBOOST"));
    else if (ret == 0x0a) DPRINT(F("FS_LOCK"));
    else if (ret == 0x0b) DPRINT(F("IFADCON"));
    else if (ret == 0x0c) DPRINT(F("ENDCAL"));
    else if (ret == 0x0d) DPRINT(F("RX"));
    else if (ret == 0x0e) DPRINT(F("RX_END"));
    else if (ret == 0x0f) DPRINT(F("RX_RST"));
    else if (ret == 0x10) DPRINT(F("TXRX_SWITCH"));
    else if (ret == 0x11) DPRINT(F("RXFIFO_OVERFLOW"));
    else if (ret == 0x12) DPRINT(F("FSTXON"));
    else if (ret == 0x13) DPRINT(F("TX"));
    else if (ret == 0x14) DPRINT(F("TX_END"));
    else if (ret == 0x15) DPRINT(F("RXTX_SWITCH"));
    else if (ret == 0x16) DPRINT(F("TXFIFO_UNDERFLOW"));
    DPRINT('\n');
    break;
  case 0x38:  // PKTSTATUS
    DPRINT(F("PKTSTATUS"));
    if (ret & 0x01) DPRINT(F(", GDO0"));
    if (ret & 0x04) DPRINT(F(", GDO2"));
    if (ret & 0x08) DPRINT(F(", SFD"));
    if (ret & 0x10) DPRINT(F(", CCA"));
    if (ret & 0x20) DPRINT(F(", PQT_REACHED"));
    if (ret & 0x40) DPRINT(F(", CS"));
    if (ret & 0x80) DPRINT(F(", CRC_OK"));
    DPRINT('\n');
    break;
  case 0x3a:  // TXBYTES
    DPRINT(F("TXBYTES: ")); DPRINT(ret & 0x7F);
    if (ret & 0x80) DPRINT(F(", TXFIFO_UNDERFLOW"));
    DPRINT('\n');
    break;
  case 0x3b:  // RXBYTES
    DPRINT(F("RXBYTES: ")); DPRINT(ret & 0x7F);
    if (ret & 0x80) DPRINT(F(", RXFIFO_OVERFLOW"));
    DPRINT('\n');
    break;
  default:
    break;
  }
  DPRINT('\n');
}

void sendCmdStr() {																
  Message msg;

  msg.init(0x0b, parser.buffer[1], parser.buffer[3], parser.buffer[2], parser.buffer[10], parser.buffer[11]);
  msg.from(&parser.buffer[4]);
  msg.to(&parser.buffer[7]);

  uint8_t dlen = parser.buffer[0] - 0x0b;
  if (dlen) msg.append(&parser.buffer[12], dlen);
  //msg.dump();
  sdev.send(msg);
}

void listPeers() {
  uint8_t cnlcnt = sdev.channels();
  DPRINTLN(F("\nlist peers"));
  for (uint8_t i = 1; i <= cnlcnt; i++) {
    uint8_t pcnt = sdev.channel(i).peers();
    DPRINT(F("CNL: ")); DPRINT(i); DPRINT(F(", PEERS: ")); DPRINTLN(pcnt);
    
    for (uint8_t j = 0; j < pcnt; j++) {
      DHEX(j); DPRINT(' '); DHEXLN(sdev.channel(i).peerat(j));
    }
  }
  DPRINT('\n');
}


void serialEventRun(void) {
  parser.poll();
}

#endif
