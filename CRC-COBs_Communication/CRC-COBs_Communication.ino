// CAPACITIVE SENSING CODE IS FROM MEMO

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <Arduino.h>
#include "driver/gptimer.h"
#include <Wire.h>
#include "FDC1004.h"
#include <ADS8688.h>

#define MAX_N 300
#define MAX_SIZE (MAX_N + MAX_N/254+2)
#define NUM_SENSORS 3

int receivedLength = 0; 
uint8_t receivedData[MAX_SIZE];

int currentTime;
int lastTime = 0;
int sampleTime = 12; // in milliseconds
uint64_t adc_timestamp; // going to read 64 bits but only use 40

const int adcReset = 2; // adc reset pin on the esp
uint16_t adcVal = 0; // value read from the potentiometer    
int timestampBytes = 5; // number of bytes of the timestamp that will be used
int nADC = 2; //bytes 
int nCap = 4; //bytes
const int to_encode_length = timestampBytes+nADC+nCap; // timestamp and data bytes that will be encoded
uint8_t toBeEncoded[11]; // **Need to manually input length = to_encode_length** array that will be hold the timestamp bytes and data bytes that will be encoded
bool ready = false;

//settings to configure capcitor measurement channel
uint8_t rate = 3;           //1 = 100 Hz, 2 = 200 Hz, 3 = 400 Hz Lower sample rate the higher the resolution
volatile int capdac = 9.5;
uint8_t measurements[NUM_SENSORS] = {1,2,3};
uint8_t sensors[NUM_SENSORS] = {1,2,3};
uint8_t capdacs[NUM_SENSORS] = {capdac,capdac,capdac};

uint8_t measurements_flag = 0b1110; //set bit for each of the sensors [1][2][3][4]

uint32_t capacitance[3];
int32_t capToSend;

// -----------------------------------------------------------------------------------------------------------
// Object to access library functions
// -----------------------------------------------------------------------------------------------------------
FDC1004 myFDC1004;
ADS8688 bank = ADS8688(1);               // Instantiate ADS8688 with PIN 1 as default CS

// initalize a timer 
gptimer_handle_t gptimer = NULL;
volatile int samplesReady = 0; //tells program to take a sample of data and send it

bool IRAM_ATTR gptimer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data){
  samplesReady = 1;
  return true;
}

// set up and start timer with a specified interval
void hardware_timer_setup(uint time_in_us){
  

  // step 1: create and configure timer
  gptimer_config_t timer_config = {
    .clk_src = GPTIMER_CLK_SRC_APB,  // APB clock (80MHz)
    .direction = GPTIMER_COUNT_UP,   // count up
    .resolution_hz = 1000000,    // 1MHz resolution (1 us per tick)
  };

  ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

  // step 2: register an interrupt 
  gptimer_event_callbacks_t cbs = {.on_alarm = gptimer_callback,};
  ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));

  // step 3: configure alarm interval
  gptimer_alarm_config_t alarm_config = {
    .alarm_count = time_in_us, // alarm after the desired time period
    .reload_count = 0, // starts counting from zero
    .flags={
      .auto_reload_on_alarm = true
    }
    };

  alarm_config.flags.auto_reload_on_alarm = true;
  ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

  // step 4: enable and start the timer
  ESP_ERROR_CHECK(gptimer_enable(gptimer));
  // ESP_ERROR_CHECK(gptimer_start(gptimer));
}

// states of the sensor
enum state {
  idle, // 0
  send, // 1
  debug // 2
};

// start the program with the esp32 in the idle state
enum state currentState = idle;

void setup() {
  pinMode(adcReset, OUTPUT);
  digitalWrite(adcReset, LOW);

  Wire.begin();
  
  //set up all four sensors for single measurement mode
  for(int i=0; i < NUM_SENSORS; i++)
  {
    myFDC1004.setupSingleMeasurement(measurements[i], sensors[i], capdacs[i]);
    
  }
  //start repeatedly measured all four measuremens at given rate
  myFDC1004.startRepeatMeasurement(measurements_flag, rate);
  
  // set up adc
  bank.setChannelSPD(0b00000001);       // set the channels to be read from 1 = read, 0 = power down
  bank.setGlobalRange(R1);              // set range for all channels R1 = 1.25*Vref= 1.25*4.096 = +- 5.12 V --> refer to datasheet for different ranges
  bank.autoRst();                       // reset auto sequence
  

  Serial.begin(115200);
  // bank.noOp();
  while (Serial.available() > 0) { //clear buffer
    Serial.read();
  }
  

  // set up timer
  hardware_timer_setup(sampleTime*1000); //sample time initially entered in ms
  // gptimer_stop(gptimer);

}


void loop() {
  // currentState = send; // use if wanting to test the arudino code alone, without python communication

  if (Serial.available() > 0){
    char signal = Serial.read();

    if (signal == 's'){ // when s (start) is sent from python, start sending data
      currentState = send;
        // need to send the first start bit since all other start bits will be the end bit of the previous package
        Serial.write(0x00);
        ESP_ERROR_CHECK(gptimer_start(gptimer));
        // gptimer_set_raw_count(gptimer, 0);
    }
    else{ // go to idle when python wants to end data transmission and the esp32 needs to stop sending data
      currentState = idle;
      ESP_ERROR_CHECK(gptimer_stop(gptimer));
      
    }
  }
  switch(currentState){
      case idle: // wait in this state until python send the command to start collecting data
        break;

      case send:
        if (samplesReady> 0){//get data based on the desired sampling time
          samplesReady = 0; // set back to zero, ***might end up skipping data if samplesReady > 1, but should received data at the appropriate time for the next sample
          
          // get the timestamp and sensor value
          adc_timestamp = esp_timer_get_time(); // 8 bytes, only using 5
          // gptimer_get_raw_count(gptimer, &adc_timestamp);
          adcVal = bank.noOp(); // 2 bytes

          // store the repeat data
          myFDC1004.getRepeatRawData(measurements_flag, capacitance);
          //make into a loop when reading from more channels
          capToSend = capacitance[0]; 

          // fill the array to be encoded with the timestamp and data --> need to shift the information to the appropriate position in the array
          for (int i=0; i<timestampBytes; i++){
            toBeEncoded[i] = (uint8_t)((adc_timestamp >> (8*(timestampBytes-1-i))) & 0xFF); 
          }

          // fill the array starting at where the timestamp byte ends
          for (int i = 0; i < nADC; i++){
            toBeEncoded[timestampBytes+i] = (uint8_t)((adcVal >> (8*(nADC-1-i))) & 0xFF);
          }

          // fill the rest of the array with the capcitive sensing data, starting where the ADC data ends
          for (int i = 0; i < nCap; i++){
            toBeEncoded[timestampBytes+nADC+i] = (uint8_t)((capToSend >> (8*(nCap-1-i))) & 0xFF);
          }

          // TEST TO SEE WHAT WILL BE ENCODED
          // for (int i = 0; i < to_encode_length; i++){
          //   Serial.print(toBeEncoded[i]);
          //   Serial.print(" ");
          // }
          // Serial.println();

          // Encode the data
          uint8_t encodeCobs[MAX_SIZE]; // initialize an array for the encoded data
          int encoded_length = encode(toBeEncoded, to_encode_length, encodeCobs);  // length of the COBs encoded value, does not included crc
          
          // send the rest of the encoded data
          for (int i = 0; i<encoded_length; i++){
            Serial.write(encodeCobs[i]); // send the encoded array to python one byte at a time
          }

        }
        break;
      case debug:
        if (samplesReady> 0){//get data based on the desired sampling time
          samplesReady=0;
          adcVal = bank.noOp();
          for (int i = 0; i < nADC; i++){
            toBeEncoded[i] = (uint8_t)((adcVal >> (8*(nADC-1-i))) & 0xFF);
          }
          for (int i = 0; i < nADC; i++){
            Serial.print(toBeEncoded[i]);
            Serial.print(" ");
          }
          Serial.println();
        }
        break;
    }
}



int encode(uint8_t data[], int data_length, uint8_t cobs[]){
    uint16_t crc_val = crc(data, data_length);

    // separate the high and low bytes in the crc code
    uint8_t highCRCval = (crc_val>>8) & 0XFF;
    uint8_t lowCRCval = crc_val & 0xFF;

    int newLength = data_length + 2;

    uint8_t dataCRC[newLength]; // array that will contain the two extra bytes for the CRC value
    // append the crc code to the second array
    for (int i = 0; i < data_length; i++){
        dataCRC[i] = data[i];
    }
    //add the high and low bytes of the crc value to the array
    dataCRC[data_length]=highCRCval;
    dataCRC[data_length+1]=lowCRCval;    

    // encode the data
    int cobsLength = COBs(dataCRC, newLength, cobs);

    return cobsLength; // return length not including the crc values
}



// more efficient algorithm of obtaining the crc value
uint16_t crc (uint8_t data[], int length){ //data is an array of bytes

  uint8_t x;
  uint16_t crc = 0xFFFF;
  int data_pointer = 0;

  while (length--){
      x = crc >> 8 ^ data[data_pointer++];
      x ^= x>>4;
      crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);
  }

  return crc;
}

//pass in the array that will be filled with the cobs encoding
int COBs (uint8_t data[], int n, uint8_t cobs[]){

    int data_index = 0; // start at the begin of the data array passed in
    int cobs_index = 1; // start at the second location in cobs to leave space for the first OHB
    int distance_index = 0; // index of the current zeros needing to be changed
    uint8_t distance = 1; // unless updated, the next zero will be one location away

    while (data_index < n){
        if (data[data_index] == 0x00){ //need to overwrite this value
            cobs[distance_index] = distance; 
            distance = 1; // reset the distance 
            distance_index = cobs_index++;

            data_index++;
        }
        else{
            // copy and increase the data and data index if there is a non-zero term
            cobs[cobs_index++] = data[data_index++]; 
            distance++; // keep track of non-zero terms

            // if 0xff is reached in the code block, treat it like a zero has been found
            // but without progressing through the data array
            if (distance == 0xFF){ 
                cobs[distance_index] = distance;
                distance = 1;
                distance_index = cobs_index++;
            }
        }
    }
    // add the distance to the delimiter
    cobs[distance_index] = distance;
    cobs[cobs_index++] = 0x00; // delimiter
    return cobs_index; // want to know the length after encoding
}