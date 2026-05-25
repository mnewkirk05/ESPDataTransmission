#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_N 300
#define MAX_SIZE (MAX_N + MAX_N/254+2)

int receivedLength = 0; 
uint8_t receivedData[MAX_SIZE];

bool newData = false;
bool valid = false;

int currentTime;
int lastTime = 0;
int sampleTime = 1000; // in milliseconds
uint64_t adc_timestamp; // going to read 64 bits but only use 40

const int cPotPin = 32; // pin attached to the potentiometer for sample sensor data
uint32_t potVal = 0; // value read from the potentiometer    
int timestampBytes = 5; // number of bytes of the timestamp that will be used
int nData = 4; //bytes
const int to_encode_length = timestampBytes+nData; // timestamp and data bytes that will be encoded
uint8_t toBeEncoded[9]; // array that will be hold the timestamp bytes and data bytes that will be encoded


// states of the sensor
enum state {
  idle, // 0
  send // 1
};

// start the program with the esp32 in the idle state
enum state currentState = idle;

void setup() {
  Serial.begin(115200);
  Serial.println("Ready to start");
  while (Serial.available() > 0) {
    Serial.read();
  }

  // need to send the first start bit since all other start bits will be the end bit of the previous package
  Serial.write(0x00);
  pinMode(cPotPin, INPUT);
}

void loop() {

  currentTime = millis();

  // currentState = send; // while not communicating with python

  if (Serial.available() > 0){
    char signal = Serial.read();

    if (signal == 's'){ // when s (start) is sent from python, start sending data
      currentState = send;
    }
    else{ // go to idle when there is an error and the esp32 needs to stop sending data
      currentState = idle;
    }
  }
  switch(currentState){
      case idle:

        break;
      case send:
        if (currentTime - lastTime > sampleTime){ //get data based on the desired sampling time
          lastTime = currentTime;

            // get the timestamp and sensor value
          adc_timestamp = esp_timer_get_time(); // 8 bytes, only using 5
          potVal = analogRead(cPotPin); // 4 bytes


          // fill the array to be encoded with the timestamp and data
          for (int i=0; i<timestampBytes; i++){
            toBeEncoded[i] = (uint8_t)((adc_timestamp >> (8*(timestampBytes-1-i))) & 0xFF); // shift the timestamp to each position in the array
          }

          for (int i = timestampBytes; i < to_encode_length; i++){
            toBeEncoded[i] = (uint8_t)((potVal >> (8*(timestampBytes-1-i))) & 0xFF);
          }

          for (int i = 0; i < to_encode_length; i++){
            Serial.print(toBeEncoded[i]);
            Serial.print(" ");
          }
          Serial.println();

          // Encode the data
          uint8_t encodeCobs[MAX_SIZE]; // initialize an array for the encoded data
          int encoded_length = encode(toBeEncoded, to_encode_length, encodeCobs);  
          // Serial.print("Encoded length: ");       
          // Serial.println(encoded_length); //send the length of the COBs encoded value, does not included crc
          // send the rest of the encoded data
          for (int i = 0; i<encoded_length; i++){
            Serial.write(encodeCobs[i]); // send the encoded array to python one byte at a time
            // Serial.print(encodeCobs[i]);
            // Serial.print(" ");
          }
          // Serial.println();
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

int decode(uint8_t encoded_data[], int data_length, uint8_t decoded_data[]){
  // decode the data into the output array, plus the 2 bytes for the crc code
    
    int decoded_length = COBs_Decode(encoded_data, MAX_SIZE, decoded_data); 
    if (decoded_length < 2){return 0;}// won't be able to do the next line of code if this error is made
    uint8_t highCRCrec = decoded_data[decoded_length-2];
    uint8_t lowCRCrec = decoded_data[decoded_length-1];

    uint16_t actualCRC = ((uint16_t)highCRCrec << 8) | lowCRCrec;
    uint16_t sentCRC = crc(decoded_data, decoded_length-2); // find the crc with a length not including the initial crc


    valid = false;
    if (actualCRC == sentCRC){
      valid = true;
    }
    Serial.print("Valid = ");
    Serial.println(valid);
    
    return decoded_length;
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


int COBs_Decode(uint8_t cobs[], int length, uint8_t output[]){
    int cobs_index = 0;
    int output_index = 0;

    while (cobs_index < length){

        uint8_t code = cobs[cobs_index];

        if (code == 0x00){break;} //reached the delimiter, exit the loop

        cobs_index++; // only increase once sure that the end of the array has not be reached
        

        // copy over the cob values for this data block
        for (int i = 1; i < code; i++){
            output[output_index++] = cobs[cobs_index++];
        }

        // check if a zero needs to be put back into the output array
        // if the code value is 0xff, it means this byte is an added OHB so there should be no zero added to the new array
        // also ensure that the cobs_index is not pointing at the end of the array, as the delimited should not be copied over
        // also make sure we are not at the delimiter
        if (code != 0xFF && cobs_index<length && cobs[cobs_index]!=0){
            output[output_index++] = 0x00;
        }        
    }
    return output_index;
}