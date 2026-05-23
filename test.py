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