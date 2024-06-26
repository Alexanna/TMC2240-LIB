// #include "TMCStepper.h"
#include "TMC2240XStepper.h"
#include "SF_SPI.h"

TMC2240Stepper::TMC2240Stepper(uint16_t pinCS, float RS) :
  _pinCS(pinCS),
  Rsense(RS)
  {}

TMC2240Stepper::TMC2240Stepper(uint16_t pinCS, uint16_t pinMOSI, uint16_t pinMISO, uint16_t pinSCK) :
  _pinCS(pinCS),
  Rsense(default_RS)
  {
    SF_SPIClass *SW_SPI_Obj = new SF_SPIClass(pinMOSI, pinMISO, pinSCK);
    TMC_SW_SPI = SW_SPI_Obj;
  }

TMC2240Stepper::TMC2240Stepper(uint16_t pinCS, float RS, uint16_t pinMOSI, uint16_t pinMISO, uint16_t pinSCK) :
  _pinCS(pinCS),
  Rsense(RS)
  {
    SF_SPIClass *SW_SPI_Obj = new SF_SPIClass(pinMOSI, pinMISO, pinSCK);
    TMC_SW_SPI = SW_SPI_Obj;
  }

void TMC2240Stepper::switchCSpin(bool state) {
  // Allows for overriding in child class to make use of fast io
  digitalWrite(_pinCS, state);
}
  // WRITE(TMC2240_SPI_SS_PIN, LOW);
	// tmc2240_spiSend(address|0x80);
	// tmc2240_spiSend((datagram >> 24) & 0xff);
	// tmc2240_spiSend((datagram >> 16) & 0xff);
	// tmc2240_spiSend((datagram >> 8) & 0xff);
	// tmc2240_spiSend(datagram & 0xff);
  // WRITE(TMC2240_SPI_SS_PIN, HIGH);

// uint32_t TMC2240Stepper::read(uint32_t add) {
//   uint32_t dummy = 0xff;
//   dummy = TMC_SW_SPI->transfer(0xff);
//   return dummy;
// }
uint32_t TMC2240Stepper::read() {
  char buf[4];
  uint32_t response = 0UL;
  memset(buf,0,sizeof(buf));
  // uint32_t dummy = ((uint32_t)DRVCONF_register.address<<17) | DRVCONF_register.sr;
  if (TMC_SW_SPI != nullptr) {
    switchCSpin(LOW);
    buf[0] = TMC_SW_SPI->transfer(0xFF);
    buf[1] = TMC_SW_SPI->transfer(0xFF);
    buf[2] = TMC_SW_SPI->transfer(0xFF);
    buf[3] = TMC_SW_SPI->transfer(0xFF);
  } else {
    SPI.beginTransaction(SPISettings(spi_speed, MSBFIRST, SPI_MODE3));
    switchCSpin(LOW);
    buf[0] = SPI.transfer(0xFF);
    buf[1] = SPI.transfer(0xFF);
    buf[2] = SPI.transfer(0xFF);
    buf[3] = SPI.transfer(0xFF);
    SPI.endTransaction();
  }
  switchCSpin(HIGH);
  return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}
void TMC2240Stepper::send(uint32_t data)
{
  if (TMC_SW_SPI != nullptr) {
    switchCSpin(LOW);
    TMC_SW_SPI->transfer(data);
  } else {
    SPI.beginTransaction(SPISettings(spi_speed, MSBFIRST, SPI_MODE3));
    switchCSpin(LOW);
    SPI.transfer(data);
    SPI.endTransaction();
  }
  switchCSpin(HIGH);
}

void TMC2240Stepper::write(uint8_t addressByte, uint32_t config) {
  // uint32_t data = (uint32_t)addressByte<<17 | config;
  if (TMC_SW_SPI != nullptr) {
    switchCSpin(LOW);
    TMC_SW_SPI->transfer(addressByte|0x80);
    TMC_SW_SPI->transfer((config >> 24) & 0xFF);
    TMC_SW_SPI->transfer((config >> 16) & 0xFF);
    TMC_SW_SPI->transfer((config >>  8) & 0xFF);
    TMC_SW_SPI->transfer(config & 0xFF);
  } else {
    SPI.beginTransaction(SPISettings(spi_speed, MSBFIRST, SPI_MODE3));
    switchCSpin(LOW);
    SPI.transfer(addressByte|0x80);
    SPI.transfer((config >> 24) & 0xFF);
    SPI.transfer((config >> 16) & 0xFF);
    SPI.transfer((config >>  8) & 0xFF);
    SPI.transfer(config & 0xFF);
    SPI.endTransaction();
  }
  switchCSpin(HIGH);
}

void TMC2240Stepper::begin() {
  //set pins
  if (TMC_SW_SPI != nullptr) {
    TMC_SW_SPI->init();
  }else{
    SPI.begin();
  }
  pinMode(_pinCS, OUTPUT);
  switchCSpin(HIGH);
}

bool TMC2240Stepper::isEnabled() { return 0/*toff() > 0*/; }

uint8_t TMC2240Stepper::test_connection() {
  uint32_t drv_status = 0/*DRVSTATUS()*/;
  switch (drv_status) {
      case 0xFFCFF: return 1;
      case 0: return 2;
      default: return 0;
  }
}

/*
  Requested current = mA = I_rms/1000
  Equation for current:
  I_rms = (CS+1)/32 * V_fs/R_sense * 1/sqrt(2)
  Solve for CS ->
  CS = 32*sqrt(2)*I_rms*R_sense/V_fs - 1

  Example:
  vsense = 0b0 -> V_fs = 0.310V //Typical
  mA = 1650mA = I_rms/1000 = 1.65A
  R_sense = 0.100 Ohm
  ->
  CS = 32*sqrt(2)*1.65*0.100/0.310 - 1 = 24,09
  CS = 24
*/

uint16_t TMC2240Stepper::cs2rms(uint8_t CS) {
  // return (float)(CS+1)/32.0 * (vsense() ? 0.165 : 0.310)/(Rsense+0.02) / 1.41421 * 1000;
}

uint16_t TMC2240Stepper::rms_current() {

  uint8_t cur_run = run();
  return (cur_run * 3000 / 32);
  // return cs2rms(cs());
}
void TMC2240Stepper::rms_current(uint16_t mA) {

    float rel_ma = mA * 1.414;
    uint16_t cur_ma = 0;
    uint16_t set_cur = 0;
    uint32_t set_data = 0;
    cur_ma = mA;

    set_cur = (rel_ma * 32 / 3000);

    set_data = 0x00060005;

    set_data = set_data | (set_cur << 8);

    IHOLD_IRUN(set_data);
}


void TMC2240Stepper::push() {
  DEVCONF(DEVCONF_register.sr);
  IHOLD_IRUN(IHOLD_IRUN_register.sr);
  CHOPCONF(CHOPCONF_register.sr);
  TPOWERDOWM(TPOWERDOWM_register.sr);
  TPWMTHRS(TPWMTHRS_register.sr);
  SG4_THRS(SG4_THRS_register.sr);
  GCONF(GCONF_register.sr);
  COOLCONF(COOLCONF_register.sr);
  TCOOLTHRS(TCOOLTHRS_register.sr);
  GSTAT(GSTAT_register.sr);
  PWMCONF(PWMCONF_register.sr);
}

void TMC2240Stepper::hysteresis_end(int8_t value) { hend(value+3); }
int8_t TMC2240Stepper::hysteresis_end() { return hend()-3; };

void TMC2240Stepper::hysteresis_start(uint8_t value) { hstrt(value-1); }
uint8_t TMC2240Stepper::hysteresis_start() { return hstrt()+1; }

void TMC2240Stepper::microsteps(uint16_t ms) {
  switch(ms) {
    case 256: mres(0); break;
    case 128: mres(1); break;
    case  64: mres(2); break;
    case  32: mres(3); break;
    case  16: mres(4); break;
    case   8: mres(5); break;
    case   4: mres(6); break;
    case   2: mres(7); break;
    case   0: mres(8); break;
    default: break;
  }
}

uint16_t TMC2240Stepper::microsteps() {
  switch(mres()) {
    case 0: return 256;
    case 1: return 128;
    case 2: return  64;
    case 3: return  32;
    case 4: return  16;
    case 5: return   8;
    case 6: return   4;
    case 7: return   2;
    case 8: return   0;
  }
  return 0;
}

void TMC2240Stepper::blank_time(uint8_t value) {
  switch (value) {
    case 16: tbl(0b00); break;
    case 24: tbl(0b01); break;
    case 36: tbl(0b10); break;
    case 54: tbl(0b11); break;
  }
}

uint8_t TMC2240Stepper::blank_time() {
  switch (tbl()) {
    case 0b00: return 16;
    case 0b01: return 24;
    case 0b10: return 36;
    case 0b11: return 54;
  }
  return 0;
}


