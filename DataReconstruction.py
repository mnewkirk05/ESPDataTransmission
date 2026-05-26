import pathlib
import sys
import time

import pandas as pd
import serial
import numpy as np

MAX_N = 300 # large value that should never be reached, but used as a check
MAX_SIZE = MAX_N + MAX_N//254 + 2

SAMPLING_PERIOD = 10e-3
SAMPLING_RATE = 1/SAMPLING_PERIOD


# Serial port variables:
port = "COM6"
baudRate_serial = 115200  # Measure of data speed
timeout_serial = 2  # in seconds  # Number of seconds to wait for serial data

# Sensor dependent variables
data_length = 9 # timestamp bytes + data bytes
encoded_length = 13 # data_length + OHB + CRC + end marker
sensor_resolution = 12 # used to convert ADC value to a voltage

DataFileName = "Voltage divider-2"



def SerialEnhancer(port, buf, endMarker):
    # check if the buffer being passed in has an end marker in it
    i = buf.find(endMarker)
    if i >= 0: #going to return the data if an endMarker has been found
        r = buf[:i+1] #add the information to the array that will be returned
        del buf[:i+1] #delete the values that are being returned from the buffer array so they are not counted twice
        return r
    
    # now that the buffer does not contain an end marker, read more data from the sensor
    while True:
        n = max(1, min(2048,port.in_waiting)) #determine how many bytes are to be read
        data = port.read(n) #read the calculated number of bytes

        i = data.find(endMarker) #find location of the endMarker 
        if i>=0:
            r = buf + data[:i+1] #store and return the data in the buffer and the current data packet up to and including the endMarker
            buf[:] = data[i+1:] #store all data after the endMarker in the buffer to be sent with the next data packet
            return r
        else: #i=-1, meaning no endMarker was found in this data packet and need to read the next to find it
            buf.extend(data) #store the current data set in the buffer to be added once the endMarker is found



def makeLookUpTable():
    polynomial = 0x1021

    for dividend in range (256):
        currentByte = dividend << 8

        for bit in range (8):
            if (currentByte & 0x8000) != 0:
                currentByte = (currentByte<<1) & 0xFFFF
                currentByte = (currentByte ^ polynomial) & 0xFFFF
            else:
                currentByte = (currentByte<<1) & 0xFFFF
        crctable16[dividend] = currentByte

def crc (data, numBytes): #data is an array of bytes
    crc = 0xFFFF
    
    for B in range (0, numBytes):
        pos = ((crc>>8) ^ data[B]) & 0xFF
        crc = ((crc << 8) ^ crctable16[pos]) & 0xFFFF

    return crc



    
def decode(encoded_data, data_length, decoded_data):
  # decode the data into the output array, plus the 2 bytes for the crc code
    
    decoded_length = COBs_Decode(encoded_data, data_length, decoded_data)
    if (decoded_length < 2):
        return 0 #// won't be able to do the next line of code if this error is made
    highCRCrec = decoded_data[decoded_length-2]
    lowCRCrec = decoded_data[decoded_length-1]

    actualCRC = (highCRCrec << 8) | lowCRCrec
    sentCRC = crc(decoded_data, decoded_length-2) # find the crc with a length not including the initial crc
    sentCRC = sentCRC & 0xFFFF # make sure that any overflow is discarded

    # print(f"actual = {actualCRC:02X} sent = {sentCRC:02X}")
    valid = 0
    if (actualCRC == sentCRC):
      valid = 1
        
    return decoded_length, valid


def COBs_Decode(cobs,length,output):
    cobs_index = 0
    output_index = 0

    while (cobs_index < length): #add a safety in case there's an error and 0x00 is never reached

        code = cobs[cobs_index] # read the data from the encoded list

        if (code == 0x00):
            break #reached the delimiter, exit the loop

        cobs_index += 1 # only increase once sure that the end of the array has not be reached
        

        #  copy over the cob values for this data block
        for i in range(1, code):
            output[output_index] = cobs[cobs_index]
            output_index += 1
            cobs_index += 1
        

        #  check if a zero needs to be put back into the output array
        #  if the code value is 0xff, it means this byte is an added OHB so there should be no zero added to the new array
        #  also ensure that the cobs_index is not pointing at the end of the array, as the delimited should not be copied over
        #  also make sure we are not at the delimiter
        if (code != 0xFF and cobs_index<length and cobs[cobs_index]!=0):
            output[output_index] = 0x00
            output_index += 1    
    return output_index


# Function from from one of Memo's programs 
def build_data_headers(headers: dict, custom_metadata: dict = None) -> dict:
    """
    Builds a forceHeaders dictionary by combining metadata and headers.

    Parameters:
    - headers (dict): A dictionary where keys are column letters and values are header names.
    - custom_metadata (dict): Optional. A dictionary with specific metadata lists for certain columns.

    Returns:
    - dict: A dictionary where each key maps to a list of metadata + header.
    """
    if custom_metadata is None:
        custom_metadata = {}

    header_spacing = 1  # spacing between metadata and headers

    # Determine the maximum metadata length
    max_meta_len = max([len(v) for v in custom_metadata.values()], default=0) + header_spacing

    # Pad default metadata
    padded_default_metadata = [None] * (max_meta_len)

    # Pad all custom metadata entries
    padded_custom_metadata = {col: meta + [None] * (max_meta_len - len(meta)) for col, meta in custom_metadata.items()}

    # Build the final dictionary
    return {col: padded_custom_metadata.get(col, padded_default_metadata) + [header] for col, header in headers.items()}



if __name__ == "__main__":
    data = [] #2D array that will hold each set of data to be processed at the end
    crctable16 = [0] * 256 # going to have all possible combinations for input byte values from 0-255
    decoded_data = [] #2D array that will hold the decoded data packets
    esp32_timestamp_bytes = []

    running = 0 #boolean variable that turns to 1 if the first start 0 byte is read

    makeLookUpTable() # fill in the look up table for crc 16
    endMarker = b"\x00" #value replaced in the cobs encoding ** if changed, change in Arduino code as well
    rxBuff = bytearray() #each packet of data that will be stored in the data array

    

    # Initialize serial communication with the ESP32
    try:
        # If a port is specified, the serial connection opens automatically
        serialPort = serial.Serial(port, baudrate=baudRate_serial, timeout=timeout_serial)
    except serial.SerialException:
        print("\nError (Serial Communication): Check the communication port \n")
        sys.exit(1)  # Exit the program if the serial connection fails


    # clear the buffer
    serialPort.reset_input_buffer()
    
    # wait for the first start byte to beginning running the program
    while (running==0):
        if serialPort.read(1) == b'\x00':
            running = 1 #if 0 is not read, don't start data collection

            # Have the esp32 begin sending data and initialize the buffer array
            serialPort.write(b's') 
            buf = bytearray() 
            print("Beginning data collection...")



    # collect data as long as a keyboard interrupt does not occur
    try:
       while (1):
        rxBuff = SerialEnhancer(serialPort, buf, endMarker)
        data.append(bytes(rxBuff)) # want the list added as one item
    except KeyboardInterrupt: # stop collecting data when ctrl+c is pressed, after this command, if ctrl+c is pressed, it will close the program
        # if the program was at the end of reading a packet when the KeyboardInterrupt occurred, it can be added to the data array, otherwise ignore the data
        if (len(rxBuff)> 0 and rxBuff[-1] == endMarker): 
            data.append(bytes(rxBuff))
        serialPort.write(b'e') # tell esp32 to stop sending data 

    # Once data collection has stopped, begin processing it
    for rxBuff in data:
        packet = rxBuff[:-1] #remove the delimiter
        decodeData = bytearray(MAX_SIZE) # re-initalize each time

        decode_length, valid = decode(packet,len(packet), decodeData) #pass in encoded_length as the length to be decoded and the function should return the original data length + 2 (includes crc)
        decode_length -= 2 #don't include the 2 crc bytes in the comparison of data length bytes
        # data was corrupt if the crc values do not match, or the decoded length is not equal to the data length sent
        if (not valid) or (decode_length!=data_length): 
            decoded_data.append(np.nan) 
        else: # if the data is valid, prepare it to be written to the csv file (not including crc)
            payload = decodeData[:decode_length]
            timestamp = int.from_bytes(payload[0:5],'big') #convert the time bytes
            esp32_timestamp_bytes.append(timestamp)

            #convert the sensor data bytes
            sensor_data_raw = int.from_bytes(payload[5:9], 'big') #the sensor data will be after the 5 timestamp bytes and up to the decoded data length (not including crc)
            sensor_data = (sensor_data_raw*3.3)/(2**sensor_resolution)
            decoded_data.append(sensor_data)

    # Convert the time given from arudino to times starting at zero
    esp32_timestamp = [x - esp32_timestamp_bytes[0] for x in esp32_timestamp_bytes]

    # Create a list for the sample number of each piece of data collection
    sample_number = list(range(1,len(esp32_timestamp) + 1))

    serialPort.close()
    # ------------------------------------------------------------------------------------------------------------------
    print(f"\nSaving data, please wait...")

    # Create the output folder if it doesn't exist
    dataFolderNamePath = pathlib.Path(__file__).parent.joinpath("Sensor Data")
    if not dataFolderNamePath.is_dir():
        dataFolderNamePath.mkdir()


    # Create the csv file
    _dataHeaders = {
        "A": "Sample No.",
        "B": "Timestamp (us)",
        "C": "Sensor Voltage from Potentiometer (V)",
    }

    # Data to be included at the top of the CSV file
    dataMetadata = {
        "A": ["Sensor Sample rate (Hz):"],
        "B": [SAMPLING_RATE],
    }

    # Main data content
    dataBody = {
        "A": sample_number,
        "B": esp32_timestamp,
        "C": decoded_data
    }

    # Build the full header section with metadata
    dataHeaders = build_data_headers(_dataHeaders, dataMetadata)

    # Create DataFrames for headers and data
    header_df = pd.DataFrame(dataHeaders)
    dataBody_df = pd.DataFrame(dataBody)

    # Combine header and data into a single DataFrame
    capacitanceData_df = pd.concat([header_df, dataBody_df], ignore_index=True)

    # Save the DataFrame to a CSV file
    capacitanceData_df.to_csv(
        dataFolderNamePath.joinpath(DataFileName + ".csv"),
        index=False,
        header=False,
    )

    print(f"\nDone saving data")