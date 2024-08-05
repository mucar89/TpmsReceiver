// Standard SPI pins (11..13) are not used because CMT2219B uses single pin for MOSI and MISO.

#define P_SCLK 5  // Clock
#define P_SDIO 3  // Data In & Out
#define P_CSB 6   // CS_Registers
#define P_FCSB 4  // CS_FIFO
#define P_GPIO2 2 // Interrupt (Must support interrupt!)

// Used documents:
// AN161-CMT2219BQuickStartGuide-EN-V1.0-171121.pdf, Page 17, Section 3.5    (initializing)
// AN167-CMT2219B_FIFO_and_Packet_Format_Operation_Guide-EN-v1.0-171121.pdf  (FIFO and interrupt handling)

typedef struct TPMSData {
  uint32_t id;
  uint8_t wheel;
  uint8_t status;
  uint8_t pressure; // psi
  int8_t temperature; // celcius
};

void setup() {
  Serial.begin(115200);
  Serial.println("Started.");

  attachInterrupt(digitalPinToInterrupt(P_GPIO2), onData, RISING);

  if (!initCmt2219b()) {
    while (true) {
      Serial.println("CMT2219B cannot be initialized. Check wiring and try again.");
      delay(1000);
    }
  }
}

bool dataReceived = false;
void onData() {
  dataReceived = true;
}

TPMSData tpmsData;
void loop() {
  if (dataReceived) {
    bool valid = readTpmsData(&tpmsData);
    if (valid) {
      Serial.print("ID: ");
      Serial.print(tpmsData.id, HEX);
      Serial.print(" Wheel: ");
      Serial.print(tpmsData.wheel);
      Serial.print(" Status: ");
      Serial.print(tpmsData.status);
      Serial.print(" Pressure: ");
      Serial.print(tpmsData.pressure);
      Serial.print(" Temperature: ");
      Serial.println(tpmsData.temperature);
    } else {
      Serial.println("Invalid packet received.");
    }

    dataReceived = false;
  }
}