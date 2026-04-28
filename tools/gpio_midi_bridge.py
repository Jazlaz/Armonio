#!/usr/bin/env python3
"""
GPIO MIDI bridge — buttons + 8 pots (one MCP3008, spidev0.0) → ALSA virtual port

ONE MCP3008 chip on CE0 (spidev0.0). All 8 pots live here.
C++ does NOT read SPI at all (spiFd0=spiFd1=-1).
Mode changes remap what the same 8 physical pots control.

GPIO (BCM):
  FUNC_PINS 17,27,22,5  — F1-F4, behaviour changes per mode
  MODE_PINS  6,13,23    — M1/M2/M3 mode selectors
  MUTE_PIN   26
  PANIC_PIN  24
"""

import RPi.GPIO as GPIO
import rtmidi
import spidev
import time

# ── GPIO pins ─────────────────────────────────────────────────────────────────
FUNC_PINS = [17, 27, 22, 5]
MODE_PINS = [6,  13, 23]
MUTE_PIN  = 26
PANIC_PIN = 24

MIDI_CHANNEL = 0
ADC_THRESHOLD = 8   # ignore jitter smaller than this (0-1023 scale)

# ── SPI — single MCP3008 on CE0 ───────────────────────────────────────────────
spi = spidev.SpiDev()
spi.open(0, 0)              # bus 0, CE0 = /dev/spidev0.0
spi.max_speed_hz = 1000000
spi.mode = 0

def read_adc(channel):
    """Read MCP3008 channel 0-7. Returns 0-1023."""
    r = spi.xfer2([1, (8 + channel) << 4, 0])
    return ((r[1] & 3) << 8) | r[2]

# ── Pot → CC map per mode ─────────────────────────────────────────────────────
# Each entry: (cc_number, adc_channel)
# Mode 1 = Hammond drawbars  (default, most-used)
# Mode 2 = Filter + Air
# Mode 3 = Harmonics + ADSR
# Pots 6 and 7 are intentionally unassigned in every mode.
POT_MAP = {
    0: [            # Mode 1 — Hammond drawbars
        (73, 0),    # pot 0 → +1 Octave
        (74, 1),    # pot 1 → Oct + Fifth (3rd)
        (75, 2),    # pot 2 → +2 Octaves
        (76, 3),    # pot 3 → 2 Oct + Third (5th)
        (79, 4),    # pot 4 → Quint (Fifth)
        (80, 5),    # pot 5 → Sub Octave
    ],
    1: [            # Mode 2 — Ladder filter + Air
        (91, 0),    # pot 0 → ladder cutoff
        (92, 1),    # pot 1 → ladder resonance
        (93, 2),    # pot 2 → ladder drive
        (94, 3),    # pot 3 → air high cut
        (83, 4),    # pot 4 → wind level (mix)
        (82, 5),    # pot 5 → wind low cut
    ],
    2: [            # Mode 3 — Harmonics + ADSR
        (88, 0),    # pot 0 → harmonics
        (81, 1),    # pot 1 → subharmonics
        (84, 2),    # pot 2 → attack
        (85, 3),    # pot 3 → decay
        (86, 4),    # pot 4 → sustain
        (87, 5),    # pot 5 → release
    ],
}

# ── Button CC layout per mode ─────────────────────────────────────────────────
BTN_MAP = {
    0: [                      # Mode 1 buttons (paired with Mode 3 pots-style controls)
        (23, 'toggle'),       # F1: Chime 2
        (24, 'toggle'),       # F2: Chime 3
        (25, 'toggle'),       # F3: Leslie on/off
        (26, 'toggle'),       # F4: Leslie speed slow/fast
    ],
    1: [                      # Mode 2
        (29, 'toggle'),       # F1: Filter on/off
        (30, 'step'),         # F2: Filter mode forward
        (31, 'step'),         # F3: Filter mode backward
        (32, 'gate'),         # F4: JI — tap toggles, hold + note picks new root
    ],
    2: [                      # Mode 3 buttons (paired with Mode 1 waveform-style controls)
        (20, 'momentary'),    # F1: Sine
        (21, 'momentary'),    # F2: Square
        (22, 'momentary'),    # F3: Triangle
        (28, 'toggle'),       # F4: LFO on/off
    ],
}

# ── Toggle states (persist across mode switches) ──────────────────────────────
toggle_states = {
    28: False, 29: False, 32: False,
    23: False, 24: False, 25: False, 26: False,
    27: False,  # mute
}


def send_cc(mout, cc, val):
    mout.send_message([0xB0 | MIDI_CHANNEL, cc, val])


def send_panic(mout):
    for ch in range(16):
        mout.send_message([0xB0 | ch, 123, 0])
        mout.send_message([0xB0 | ch,  64, 0])
    print("[panic] All notes off")


def broadcast_pots(mout, mode):
    """Read all 8 pots and send their current values for the given mode."""
    for cc, ch in POT_MAP[mode]:
        raw  = read_adc(ch)
        norm = int(raw / 1023.0 * 127.0)
        send_cc(mout, cc, norm)


def main():
    mout = rtmidi.MidiOut()
    mout.open_virtual_port("GPIO MIDI")
    print("Virtual MIDI port 'GPIO MIDI' created")

    GPIO.cleanup()
    GPIO.setmode(GPIO.BCM)
    all_pins = FUNC_PINS + MODE_PINS + [MUTE_PIN, PANIC_PIN]
    for pin in all_pins:
        GPIO.setup(pin, GPIO.IN, pull_up_down=GPIO.PUD_UP)

    current_mode = 0
    last = {pin: True for pin in all_pins}
    last_raw = [-1] * 8   # -1 forces all pots to broadcast on first tick

    print("Ready — Mode 1 (Hammond) active. Ctrl+C to exit.")

    try:
        while True:
            # ── Mode selectors ────────────────────────────────────────────────
            for i, pin in enumerate(MODE_PINS):
                now = bool(GPIO.input(pin))
                if not now and last[pin]:
                    if i != current_mode:
                        current_mode = i
                        # Snapshot current physical pot positions so the synth's
                        # existing parameter values for this mode's CCs are NOT
                        # overwritten — a pot only sends when it's actually moved
                        # past the threshold from where it sits right now.
                        last_raw = [read_adc(ch) for ch in range(8)]
                        print(f"→ Mode {i + 1}")
                last[pin] = now

            # ── Mute ──────────────────────────────────────────────────────────
            now = bool(GPIO.input(MUTE_PIN))
            if not now and last[MUTE_PIN]:
                toggle_states[27] = not toggle_states[27]
                send_cc(mout, 27, 127 if toggle_states[27] else 0)
                print(f"Mute {'ON' if toggle_states[27] else 'OFF'}")
            last[MUTE_PIN] = now

            # ── Panic ─────────────────────────────────────────────────────────
            now = bool(GPIO.input(PANIC_PIN))
            if not now and last[PANIC_PIN]:
                send_panic(mout)
            last[PANIC_PIN] = now

            # ── Function buttons ──────────────────────────────────────────────
            for i, pin in enumerate(FUNC_PINS):
                now = bool(GPIO.input(pin))
                cc, btn_type = BTN_MAP[current_mode][i]

                if not now and last[pin]:           # falling edge = press
                    if btn_type == 'momentary':
                        send_cc(mout, cc, 127)
                        print(f"[M{current_mode+1}] F{i+1}: CC{cc}=127")
                    elif btn_type == 'toggle':
                        toggle_states[cc] = not toggle_states[cc]
                        val = 127 if toggle_states[cc] else 0
                        send_cc(mout, cc, val)
                        print(f"[M{current_mode+1}] F{i+1}: CC{cc}={val}")
                    elif btn_type == 'step':
                        send_cc(mout, cc, 127)
                        print(f"[M{current_mode+1}] F{i+1}: CC{cc}=127")
                    elif btn_type == 'gate':
                        send_cc(mout, cc, 127)
                        print(f"[M{current_mode+1}] F{i+1}: CC{cc}=127 (gate down)")
                elif now and not last[pin]:         # rising edge = release
                    if btn_type == 'gate':
                        send_cc(mout, cc, 0)
                        print(f"[M{current_mode+1}] F{i+1}: CC{cc}=0 (gate up)")

                last[pin] = now

            # ── Pots (mode-dependent) ─────────────────────────────────────────
            for slot, (cc, ch) in enumerate(POT_MAP[current_mode]):
                raw = read_adc(ch)
                if abs(raw - last_raw[slot]) > ADC_THRESHOLD:
                    last_raw[slot] = raw
                    norm = int(raw / 1023.0 * 127.0)
                    send_cc(mout, cc, norm)

            time.sleep(0.02)   # 50 Hz

    except KeyboardInterrupt:
        send_panic(mout)
        print("Bye")
    finally:
        spi.close()
        GPIO.cleanup()


if __name__ == "__main__":
    main()
