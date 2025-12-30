/* Using with Arduino Uno
* 7348 byte (22%) with 32,256 byte max
* 802 byte (39%) with 2048 byte max
* Uses LM75B temperature sensors and read it every MEASURE_DELAY milliseconds and then
* it sends the value to the other device via kiss UART.
* The other device can change the delay.
*/
#include <Wire.h>
#include <SoftwareSerial.h>
#include "kissLIB.h"

// Configuration Constants
#define LM75B_I2C_ADDR 0x48   // Standard I2C address for LM75B
#define SOFT_RX_PIN    8      // Software Serial RX Pin
#define SOFT_TX_PIN    9      // Software Serial TX Pin

uint16_t MEASURE_DELAY = 1000;   // Time interval in milliseconds (1 second)

// Initialize SoftwareSerial on pins 8 and 9
SoftwareSerial softSerial(SOFT_RX_PIN, SOFT_TX_PIN);

// used for timer
unsigned long lastExecutionTime = 0;

// buffer for the kiss instance
uint8_t buffer[128];
// output frame array
uint8_t output[128];
// output frame array length variable
size_t index = 0;
// header for the incoming frame
uint8_t header;
// float uint8_t pointer for payload data to send
uint8_t* b;
// errors for kiss
int err = 0;


// kiss instance 
kiss_instance_t kiss;

void setup() 
{
  // Initialize I2C Communication
  Wire.begin();
  
  // Initialize Hardware Serial for local PC debugging
  Serial.begin(9600);
  
  // Initialize Software Serial for external data transmission
  softSerial.begin(9600);

  // initialization of the kiss instance
  kiss_init(&kiss, buffer, 128, 1, write, read, NULL, 0);
  
  // system initialized serial printing
  Serial.println(F("System Initialized..."));
  Serial.println(F("Reading LM75B and sending data via SoftwareSerial (Pins 8/9)"));
}

void loop() 
{
  // Non-blocking timer using millis()
  unsigned long currentMillis = millis();
  
  // if we need to update the temperature data
  if (currentMillis - lastExecutionTime >= MEASURE_DELAY) 
  {
    // save last execution time
    lastExecutionTime = currentMillis;

    // Read temperature from the I2C sensor
    float currentTemp = readTemperature();

    // Check if the reading is valid
    if (!isnan(currentTemp)) 
    {
        // float to 4 bytes pointer
        b = (uint8_t*)(void*)&currentTemp;

        // encode payload data
        err = kiss_encode(&kiss, b, 4, KISS_HEADER_DATA(0));

        if(err != 0)
        {
          // send frame 
          err = kiss_send_frame(&kiss);

          if(err != 0)
            kiss_send_frame(&kiss);
        }

    }
  }
  
  // if we receive a frame
  if(kiss_receive_frame(&kiss, 1) == 0)
  {

    delay(kiss.TXdelay*10);

    // try to decode it
    err = kiss_decode(&kiss, output, 128, &index, &header);

    // if we have no errors and we received a frame
    if(err == 0 && kiss.Status == KISS_STATUS_RECEIVED)
    {
      if(header == KISS_HEADER_DATA(0))
      { 
        // changing the measuring delay
        MEASURE_DELAY = ((output[1] & 0xFF) << 8) | ((output[0] & 0xFF) << 0);
      }
      else if(header == KISS_HEADER_PING)
      {
        // we received a ping and we responde with ack
        kiss_send_ack(&kiss);
      }
      else
      {
        // if the header is none of them we send back a nack, meaning we didn't understand what the other end sent us
        kiss_send_nack(&kiss);
      }
    }
    
  }

}

/**
 * Reads the temperature from the LM75B sensor via I2C.
 * Returns the temperature as a float.
 */
float readTemperature() 
{
  // Point to the Temperature Register (0x00)
  Wire.beginTransmission(LM75B_I2C_ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return NAN;

  // Request 2 bytes of data from the sensor
  Wire.requestFrom(LM75B_I2C_ADDR, 2);
  
  if (Wire.available() >= 2) {
    // LM75B stores temperature in an 11-bit format
    // Combine MSB and LSB
    int16_t rawData = (Wire.read() << 8) | Wire.read();
    
    // Shift right by 5 bits and multiply by resolution (0.125°C)
    return (rawData >> 5) * 0.125;
  }
  
  return NAN;
}

/**
 * Transmits a custom data packet through SoftwareSerial.
 * Protocol: [STX] [ID] [FLOAT_DATA] [ETX]
 */
int write(kiss_instance_t *kiss, uint8_t *data, size_t length)
{
  for(size_t i = 0; i < length; i++)
    if(softSerial.write(data[i]) != 1)
      return 1;
  return 0;
}

int read(kiss_instance_t *kiss, uint8_t *buffer, size_t dataLen, size_t *read)
{
  *read = 0;
  while(softSerial.available())
  {
    buffer[*read++] = (uint8_t)softSerial.read();
    if(*read >= dataLen)
      return 0;
  }
  return 0;
}

