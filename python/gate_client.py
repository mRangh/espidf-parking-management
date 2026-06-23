'''
 * ============================================================================
 * @file        gate_client.py
 *
 * @author      Marco Antônio Ranghetti
 * @github      github.com/mRangh
 * @email       marcoantonioranghetti@gmail.com
 * @academic    d2026008956@unifei.edu.br
 *
 * @version     1.0.0
 * @date        2026-06-22
 * @license     Apache License 2.0
 * ============================================================================
 '''

import serial, sys, time
from typing import Callable
import callbacks



class GateClient:

    esp32: serial.Serial | None = None

    def __init__(self, port='/dev/ttyUSB0', baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.esp32 = None
        self.activation_string = ''



    def connect(self):
        try:
            
            print(f"[PYTHON]: Connecting to gate on {self.port}")

            self.esp32 = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                bytesize=serial.EIGHTBITS,
                timeout=0.1
            )
            try:
                self.esp32.dtr = False
                self.esp32.rts = False
            except OSError:
                print("[PYTHON_INFO]: Virtual port detected, bypassing DTR/RTS control.")

            time.sleep(0.5)
            print(f"[PYTHON]: Connected to {self.port} and listening")
            return True

        except serial.SerialException as e:
            print(f"[ERR] Serial comm failed: {e}", file=sys.stderr)
            return False



    def send_release(self):
        if self.esp32 is not None and self.esp32.is_open:
            try:
                command = b"RELEASE_GATE\n"
                print("[PYTHON]: Sending 'RELEASE_GATE' command...")
                self.esp32.write(command)
                self.esp32.flush()
            except Exception as e:
                print(f"[ERROR]: Failed to write to serial: {e}", file=sys.stderr)
        else:
            print("[ERROR]: Cannot send release, serial is not open.", file=sys.stderr)



    def run_event_loop(self, entry_callback: Callable[[], bool], exit_callback: Callable[[], bool]):
        if not self.connect(): return
        print('[PYTHON]: Starting event loop...')

        try:
            while True:
                if self.esp32 is not None and self.esp32.in_waiting > 0:
                    line_bytes = self.esp32.readline()
                    if line_bytes:
                        line = line_bytes.decode('utf-8', errors='ignore').strip()
                        print(line)
                        if "Requesting processment..." in line:
                            self.activation_string = line
                            print(f'[PYTHON]: Listened {self.activation_string}. Starting processment.')
                        
                            avante_cavalaria = False

                            if "ENTRY_GATE" in line:
                                print('[PYTHON]: Running ENTRY_GATE')
                                avante_cavalaria = entry_callback()
                            elif "EXIT_GATE" in line:
                                print('[PYTHON]: Running EXIT_GATE')
                                avante_cavalaria = exit_callback()
                            
                            if avante_cavalaria:
                                self.send_release()
                            else:
                                print('[PYTHON]: Processment declined.')
                time.sleep(0.1)

        except KeyboardInterrupt:
            print('\n[PYTHON]: Stopping listening.')
        
        finally:
            if self.esp32 is not None and self.esp32.is_open:
                self.esp32.close()

if __name__ == '__main__':
    gate_client = GateClient()
    gate_client.connect()
    if gate_client.connect():
        gate_client.run_event_loop(callbacks.entry_callback, callbacks.exit_callback)
