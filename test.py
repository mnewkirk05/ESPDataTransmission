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
