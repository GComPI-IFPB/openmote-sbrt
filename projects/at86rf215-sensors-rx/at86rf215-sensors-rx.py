#!/usr/bin/python

'''
@file       at86rf215-ping-pong.py
@author     Pere Tuset-Peiro  (peretuset@openmote.com)
@version    v0.1
@date       February, 2019
@brief      

@copyright  Copyright 2019, OpenMote Technologies, S.L.
            This file is licensed under the GNU General Public License v2.
'''

import os
import sys
pwd = os.path.abspath(__file__)
pwd = os.path.dirname(os.path.dirname(os.path.dirname(pwd)))
pwd = os.path.join(pwd, 'python')
sys.path.append(pwd)

import argparse
import signal
import struct
import logging
import time
import random
import string
import json

import serial as sl


finished = False
timeout = 0.1


def signal_handler(sig, frame):
    global finished
    finished = True


def program(port = None, baudrate = None):
    global finished

    # Create and start Serial manager
    with sl.Serial(port=port, baudrate=baudrate, timeout=timeout) as sr:
        while (not finished): # Repeat until finish condition (ctrl+c)
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

        if (finished):
            # Stop the serial port
            sr.close()


def main():
    # Set-up logging back-end
    logging.basicConfig(level=logging.DEBUG)

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
