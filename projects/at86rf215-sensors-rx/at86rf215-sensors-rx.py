#!/usr/bin/python


import argparse
import signal
import struct
import time


import serial as sl


finished = False
timeout = 0.1


def signal_handler(sig, frame):
    global finished
    finished = True


def program(port = None, baudrate = None):
    global finished
    
    while (not finished): # Repeat until finish condition (ctrl+c)  
        
        try: 
            # Create and start Serial manager
            with sl.Serial(port=port, baudrate=baudrate, timeout=timeout) as sr:
            
                # Try to receive a Serial message
                message = sr.read(15)
                if (len(message) > 0):
                    try: 
                        eui48, counter, rssi = struct.unpack(">6sIb", message[1:12])
                        print("------------------------------------------")
                        print("OpenMote Address: {}\nPacket counter: {}\nRSSI: {}".format(eui48, counter, rssi))
                        print("------------------------------------------")
                    except:
                        print("program: Error unpacking.")
        except:
            print("program: Wait for OpenMote.")
            time.sleep(5)
            
    if (finished):
        # Stop the serial port
        sr.close()


def main():

    # Set up SIGINT signal
    signal.signal(signal.SIGINT, signal_handler)

    # Create argument parser
    parser = argparse.ArgumentParser(description="")
    parser.add_argument("-p", "--port", type=str, required=True)
    parser.add_argument("-b", "--baudrate", type=int, default=115200)

    # Parse arguments
    args = parser.parse_args()

    # Execute program
    program(port=args.port, baudrate=args.baudrate)
    

if __name__ == "__main__":
    main()
