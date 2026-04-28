#!/usr/bin/env python3
"""
MIDI monitor for GPIO 15 (UART RX = /dev/ttyAMA0).
Reads raw bytes at 31250 baud and prints them as hex + parsed MIDI.
Run on the Pi: python3 midi_gpio15_monitor.py
Stop with Ctrl+C.
"""

import serial
import sys
import time

DEVICE   = '/dev/ttyAMA0'
BAUDRATE = 31250

STATUS_NAMES = {
    0x80: 'Note Off',
    0x90: 'Note On',
    0xA0: 'Aftertouch',
    0xB0: 'CC',
    0xC0: 'Program Change',
    0xD0: 'Chan Pressure',
    0xE0: 'Pitch Bend',
    0xF0: 'SysEx',
    0xF2: 'Song Pos',
    0xF3: 'Song Select',
    0xF6: 'Tune Request',
    0xF7: 'SysEx End',
    0xF8: 'Clock',
    0xFA: 'Start',
    0xFB: 'Continue',
    0xFC: 'Stop',
    0xFE: 'Active Sense',
    0xFF: 'Reset',
}

# How many data bytes each status expects (high nibble → count)
DATA_BYTES = {
    0x8: 2, 0x9: 2, 0xA: 2, 0xB: 2,
    0xC: 1, 0xD: 1, 0xE: 2,
}

NOTE_NAMES = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B']

def note_name(n):
    return f"{NOTE_NAMES[n % 12]}{n // 12 - 1}"

def parse_midi(buf):
    """Return a human-readable string for a complete MIDI message."""
    if not buf:
        return ''
    status = buf[0]
    high   = (status & 0xF0) >> 4
    ch     = (status & 0x0F) + 1

    if high == 0x9 and len(buf) >= 3:
        vel = buf[2]
        kind = 'Note On' if vel > 0 else 'Note Off (vel=0)'
        return f"{kind}  ch={ch}  note={note_name(buf[1])}({buf[1]})  vel={vel}"

    if high == 0x8 and len(buf) >= 3:
        return f"Note Off  ch={ch}  note={note_name(buf[1])}({buf[1]})  vel={buf[2]}"

    if high == 0xB and len(buf) >= 3:
        return f"CC  ch={ch}  cc={buf[1]}  val={buf[2]}"

    if high == 0xC and len(buf) >= 2:
        return f"Program Change  ch={ch}  prog={buf[1]}"

    if high == 0xE and len(buf) >= 3:
        lsb, msb = buf[1], buf[2]
        value = ((msb << 7) | lsb) - 8192
        return f"Pitch Bend  ch={ch}  val={value:+d}"

    if high == 0xA and len(buf) >= 3:
        return f"Aftertouch  ch={ch}  note={note_name(buf[1])}  pressure={buf[2]}"

    # System messages
    if status >= 0xF0:
        name = STATUS_NAMES.get(status, f'0x{status:02X}')
        if len(buf) == 1:
            return name
        data = ' '.join(f'{b:02X}' for b in buf[1:])
        return f"{name}  [{data}]"

    return ''


def main():
    print(f"Opening {DEVICE} at {BAUDRATE} baud...")
    try:
        ser = serial.Serial(DEVICE, BAUDRATE, timeout=0.05)
    except serial.SerialException as e:
        print(f"ERROR: {e}")
        print("Is uart-midi.service running? Stop it first: sudo systemctl stop uart-midi.service")
        sys.exit(1)

    print("Listening for MIDI on GPIO 15 (UART RX). Press Ctrl+C to stop.\n")
    print(f"{'TIME':>8}  {'HEX BYTES':<24}  PARSED")
    print("-" * 70)

    buf        = []
    expected   = 0   # data bytes still expected
    start_time = time.time()
    byte_count = 0
    msg_count  = 0

    try:
        while True:
            raw = ser.read(ser.in_waiting or 1)
            if not raw:
                continue

            for byte in raw:
                byte_count += 1
                t = time.time() - start_time

                is_status    = (byte & 0x80) != 0
                is_realtime  = byte >= 0xF8

                # Real-time bytes: print immediately, don't touch message state
                if is_realtime:
                    name = STATUS_NAMES.get(byte, f'0x{byte:02X}')
                    if byte not in (0xF8, 0xFE):   # suppress clock / active sense spam
                        print(f"{t:8.3f}  {byte:02X}{'':22}  {name}")
                    continue

                # New status byte — flush any incomplete message first
                if is_status:
                    if buf and expected > 0:
                        hex_str = ' '.join(f'{b:02X}' for b in buf)
                        print(f"{t:8.3f}  {hex_str:<24}  [INCOMPLETE - {expected} byte(s) missing]")

                    buf      = [byte]
                    high     = (byte & 0xF0) >> 4
                    expected = DATA_BYTES.get(high, 0)

                    # Single-byte system messages
                    if byte in (0xF6, 0xF7):
                        parsed = parse_midi(buf)
                        print(f"{t:8.3f}  {byte:02X}{'':22}  {parsed}")
                        msg_count += 1
                        buf = []; expected = 0

                else:
                    # Data byte
                    if not buf:
                        # Running status not implemented — show orphan byte
                        print(f"{t:8.3f}  {byte:02X}{'':22}  [orphan data byte]")
                        continue

                    buf.append(byte)
                    expected = max(0, expected - 1)

                    if expected == 0:
                        hex_str = ' '.join(f'{b:02X}' for b in buf)
                        parsed  = parse_midi(buf)
                        print(f"{t:8.3f}  {hex_str:<24}  {parsed}")
                        msg_count += 1
                        buf = []
                        # expected stays 0; next status byte will reset

    except KeyboardInterrupt:
        elapsed = time.time() - start_time
        print(f"\n\nDone. {byte_count} bytes / {msg_count} messages in {elapsed:.1f}s")
        ser.close()


if __name__ == '__main__':
    main()
