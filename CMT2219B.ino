
uint8_t RFPDK[0x60] = {
  //            0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F
  /* 0x00 */ 0x00, 0x66, 0xEC, 0x1C, 0x70, 0x80, 0x14, 0x08, 0x91, 0x02, 0x02, 0xD0, 0xAE, 0xFA, 0x34, 0x9F,
  /* 0x10 */ 0x00, 0x2D, 0x00, 0x0D, 0x20, 0x26, 0x10, 0x81, 0x42, 0x71, 0xCE, 0x1C, 0x42, 0x5B, 0x1C, 0x1C,
  /* 0x20 */ 0x95, 0xC1, 0x20, 0x20, 0xE2, 0x36, 0x16, 0x0A, 0x9F, 0x4B, 0x29, 0x29, 0xC0, 0x4A, 0x05, 0x53,
  /* 0x30 */ 0x10, 0x00, 0xB4, 0x00, 0x00, 0x01, 0x00, 0x00, 0x12, 0x08, 0x00, 0xAA, 0x01, 0x00, 0x00, 0x00,
  /* 0x40 */ 0x00, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63,
  /* 0x50 */ 0xFF, 0x00, 0x00, 0x1F, 0x10, 0x50, 0x0E, 0x16, 0x00, 0x07, 0x70, 0x00, 0x0C, 0x01, 0x3F, 0x7F
};

void writeReg(uint8_t address, uint8_t val) {
  digitalWrite(P_CSB, LOW);
  shiftOut(P_SDIO, P_SCLK, MSBFIRST, address);
  shiftOut(P_SDIO, P_SCLK, MSBFIRST, val);
  digitalWrite(P_CSB, HIGH);
  digitalWrite(P_SDIO, LOW);
}

uint8_t readReg(uint8_t address) {
  digitalWrite(P_CSB, LOW);
  shiftOut(P_SDIO, P_SCLK, MSBFIRST, address | 0x80);

  pinMode(P_SDIO, INPUT);

  uint8_t response = shiftIn(P_SDIO, P_SCLK, MSBFIRST);
  digitalWrite(P_CSB, HIGH);
  digitalWrite(P_SDIO, LOW);

  pinMode(P_SDIO, OUTPUT);

  return response;
}

uint8_t readFifo() {
  pinMode(P_SDIO, INPUT);
  digitalWrite(P_FCSB, LOW);
  uint8_t response = shiftIn(P_SDIO, P_SCLK, MSBFIRST);
  digitalWrite(P_FCSB, HIGH);
  pinMode(P_SDIO, OUTPUT);

  return response;
}

bool initCmt2219b() {
  pinMode(P_CSB, OUTPUT);
  pinMode(P_SCLK, OUTPUT);
  pinMode(P_FCSB, OUTPUT);
  pinMode(P_SDIO, OUTPUT);
  pinMode(P_GPIO2, INPUT);

  digitalWrite(P_SCLK, LOW);
  digitalWrite(P_SDIO, HIGH);
  digitalWrite(P_CSB, HIGH);
  digitalWrite(P_FCSB, HIGH);

  // STEP 1: Soft reset & wait for 20ms
  writeReg(0x7F, 0xFF);
  delay(20);
  
  // STEP 2: Confirm reset completed and the chip stays in SLEEP state
  uint8_t status = readReg(0x61) & 0x0F;
  if (status != 0x01) {
    Serial.println("CMT2219B is not in SLEEP state!");
    return false;
  }

  // STEP 3: Send "go stand by" command and confirm the chip enters STBY state
  writeReg(0x60, 0x02); // go standby
  status = readReg(0x61) & 0x0F;
  if (status != 0x02) {
    Serial.println("CMT2219B is not in STBY state!");
    return false;
  }

  // STEP 4: write RFPDK values
  for (int i = 0; i < 0x60; i++)
    writeReg(i, RFPDK[i]);

  // STEP 5: set last 3 bits of CUS_CMT10 (0x09) to 0b010
  uint8_t cusCmt10 = readReg(0x09); // CUS_CMT10
  cusCmt10 = (cusCmt10 & ~0x07) | 0x02; 
  writeReg(0x09, cusCmt10);

  // STEP 6: set lfosc bits to 0 if SLEEP_TIMER is not required
  uint8_t lfosc = readReg(0x0D); // CUS_SYS2
  lfosc &= ~0xE0; // disable LFOSC_RECAL_EN, LFOSC_CAL1_EN, LFOSC_CAL2_EN
  writeReg(0x0D, lfosc);

  // SKIP STEP 7: manual freq hopping

  // STEP 8: set registers between 0x60 and 0x6A
  // enable GPIO2
  writeReg(0x65, 0x04);

  // GPIO2.function = HIGH ON PACKET RECEIVED
  uint8_t int2Ctl = readReg(0x67); // CUS_INT2_CTL
  int2Ctl &= ~0x1F;
  int2Ctl |=  0x07;
  writeReg(0x67, int2Ctl);

  // STEP 8 part 2: set RSTIN_EN to 0
  uint8_t state = readReg(0x61); // CUS_MODE_STA
  state &= ~0x20; // disable RSTN_IN_EN
  writeReg(0x61, state);

  // STEP 8 part 3: set LOCKING_EN to 1
  uint8_t enCtl = readReg(0x62); // CUS_EN_CTL
  enCtl |= 0x20; // enable LOCKING_EN
  writeReg(0x62, enCtl);

  // STEP 9: set CUS_MODE_STA.CFG_RETAIN to 1 ([0x61][4])
  state = readReg(0x61); // CUS_MODE_STA
  state |=  0x10; // enable CFG_RETAIN
  writeReg(0x61, state);

  // STEP 10: send "go sleep"
  writeReg(0x60, 0x10); // go sleep

  //clear interrupt flags
  writeReg(0x6A, 0x00);
  writeReg(0x6B, 0x00);
  writeReg(0x68, 0x01); // enable PACKET_RECEPTION (right or wrong) interrupt

  writeReg(0x60, 0x02); // go stand by
  writeReg(0x69, 0x00); // FIFO_AUTO_CLR_DIS = 0, clear FIFO before entering RX state
                        // FIFO_MERGE_EN = 0    , FIFO size is 32 bytes
  writeReg(0x6C, 0x02); // clear FIFO

  writeReg(0x60, 0x10); // go sleep

  return true;
}

// Thanks to:
// https://www.hackster.io/jsmsolns/arduino-tpms-tyre-pressure-display-b6e544
// https://github.com/merbanan/rtl_433/blob/master/src/devices/tpms_truck.c
uint8_t rawData[9];
bool readTpmsData(TPMSData *data) {
  writeReg(0x60, 0x02); // go stand by
  uint8_t interruptFlags = readReg(0x6D);
  bool packetOk = interruptFlags & 0x01;
  if (packetOk) { // PKT_OK
    for (int i = 0; i < 9; i++)
      rawData[i] = readFifo();
    
    data->id = 0;
    for (int i = 0; i < 4; i++) {
      data->id = data->id << 8;
      data->id = data->id | rawData[i];
    }

    data->wheel = rawData[4];
    data->status = rawData[5] >> 4;
    data->pressure = (uint8_t)((double)(((uint16_t)rawData[5] & 0x0f) << 8 | rawData[6]) / 6.895);
    data->temperature = (int8_t)(rawData[7]);
  }

  writeReg(0x6C, 0x02); // clear FIFO
  writeReg(0x6A, 0x00); // clear interrupt flag
  writeReg(0x6B, 0x00); // clear interrupt flag
  writeReg(0x60, 0x10); // go sleep

  return packetOk;
}