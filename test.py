# data = []

# def fillArray(data):
#     for i in range (11):
#         if ((i % 2) ==0):
#             data.append(i)
# print(data)
# fillArray(data)
# print (data)


# import time

# def printHello():
#     print('hello')
#     time.sleep(2.5)  # Delay for 2.5 seconds
# try:
#     while (1):
#         printHello()
# except KeyboardInterrupt:
#     print("out")

# print('continue to the rest of the program')




# Test a python listener
from pynput import keyboard
import threading
import time

# class StopReadingInterrupt(Exception):
#     pass

# stop_event = threading.Event()

# def on_press(key):
#     try:
#         if key.char == 'e':
#             print("E pressed - stopping")
#             stop_event.set()
#     except AttributeError:
#         pass

# listener = keyboard.Listener(on_press=on_press)
# listener.start()



def printHello():
    print('hello')
    time.sleep(2.5)  # Delay for 2.5 seconds


try:
    while (1):
        
        # if stop_event.is_set():
        #     raise StopReadingInterrupt()

        printHello()
except KeyboardInterrupt:
    print("out")

while(1):
    print('continue to the rest of the program')



# Code from main program when threading was being used

# from pynput import keyboard
# import threading

# class StopReadingInterrupt(Exception):
#     pass


# # initialize the listener to watch for e to be pressed
# stop_event = threading.Event()

# # if e is pressed, set the 
# def on_press(stop_event, key):
#     try:
#         if key.char == 'e':
#             print("E pressed - stopping")
#             stop_event.set() #change the stop event to be true
#     except AttributeError: #ignore any other case with the keyboard
#         pass

# listener = keyboard.Listener(on_press=on_press) #initialize the keyboard listener with the on_press function to watch for e
# listener.start() #start the listener

# using the listener, the stop event flag will be set as soon as e is pressed,
# but the program will not stop until the next loop through the while loop
# #check if the stop event has been set and raise the interrupt
# if stop_event.is_set(): 
#     raise StopReadingInterrupt()





# Decode from arduino (not needed since done in python)
# int decode(uint8_t encoded_data[], int data_length, uint8_t decoded_data[]){
#   // decode the data into the output array, plus the 2 bytes for the crc code
    
#     int decoded_length = COBs_Decode(encoded_data, data_length, decoded_data); 
#     if (decoded_length < 2){return 0;}// won't be able to do the next line of code if this error is made
#     uint8_t highCRCrec = decoded_data[decoded_length-2];
#     uint8_t lowCRCrec = decoded_data[decoded_length-1];

#     uint16_t actualCRC = ((uint16_t)highCRCrec << 8) | lowCRCrec;
#     uint16_t sentCRC = crc(decoded_data, decoded_length-2); // find the crc with a length not including the initial crc


#     valid = false;
#     if (actualCRC == sentCRC){
#       valid = true;
#     }
#     Serial.print("Valid = ");
#     Serial.println(valid);
    
#     return decoded_length;
# }


# int COBs_Decode(uint8_t cobs[], int length, uint8_t output[]){
#     int cobs_index = 0;
#     int output_index = 0;

#     while (cobs_index < length){

#         uint8_t code = cobs[cobs_index];

#         if (code == 0x00){break;} //reached the delimiter, exit the loop

#         cobs_index++; // only increase once sure that the end of the array has not be reached
        

#         // copy over the cob values for this data block
#         for (int i = 1; i < code; i++){
#             output[output_index++] = cobs[cobs_index++];
#         }

#         // check if a zero needs to be put back into the output array
#         // if the code value is 0xff, it means this byte is an added OHB so there should be no zero added to the new array
#         // also ensure that the cobs_index is not pointing at the end of the array, as the delimited should not be copied over
#         // also make sure we are not at the delimiter
#         if (code != 0xFF && cobs_index<length && cobs[cobs_index]!=0){
#             output[output_index++] = 0x00;
#         }        
#     }
#     return output_index;
# }