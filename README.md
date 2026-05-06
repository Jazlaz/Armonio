# Armonio

**Armonio** is a polyphonic additive synthesizer built with JUCE, designed to run fully headless on a **Raspberry Pi** as a standalone DAWless instrument.

It features Hammond-style drawbar registers, a Leslie rotary effect, per-voice ADSR, LFO, and USB and NTS MIDI input — all controllable from a hardware MIDI controller with no screen required.

Here is a Video Demo: https://www.youtube.com/watch?v=JYwFRLIfnu8

---

## Download

| Platform | File |
|----------|------|
| Windows VST3 | [Armonio-v1.0-Windows-VST3.zip](https://github.com/Jazlaz/Armonio/releases/download/v1.0/Armonio-v1.0-Windows-VST3.zip) |
| Raspberry Pi Zero 2 W (Standalone) | [Armonio-v1.0-Pi-Zero2W.zip](https://github.com/Jazlaz/Armonio/releases/download/v1.0/Armonio-v1.0-Pi-Zero2W.zip) |

---

## Features

### Synthesis
- **Three waveforms** — Sine, Square, Triangle (additive)
- **Harmonic & subharmonic control** — up to 16 harmonics, 8 subharmonics
- **Per-voice ADSR** envelope

<img width="799" height="536" alt="envelop_withHarmonics" src="https://github.com/user-attachments/assets/3bb5f4db-f55d-41ec-bac7-71399ac7eb60" />

### Hammond Organ Section
- **8 drawbar registers**: +1 Oct, Oct+Fifth, +2 Oct, 2Oct+Third, 2Oct+Fifth, +3 Oct, Fifth, Sub Octave
- **Percussive chimes** — 2nd and 3rd harmonic transients with independent envelope
- **LFO** with 5 modes, depth and speed control

<img width="766" height="383" alt="hamondCloseup" src="https://github.com/user-attachments/assets/ec84ffb2-2d5f-488d-b2e5-eb35f95835e2" />

### Leslie Rotary Effect
- Two-band rotary simulation (horn + drum) with crossover at 800 Hz
- **Slow / Fast** speed switch with smooth acceleration/deceleration ramp
- Doppler pitch modulation + stereo panning sweep

<img width="254" height="236" alt="LeslieCU" src="https://github.com/user-attachments/assets/21851e24-19f4-4640-8278-4397ab8dbf2c" />

### Voice Management
- **12-voice polyphony** with artifact-free voice stealing (crossfade on steal)
- Output normalization scales dynamically with active Hammond registers
- Soft-clip limiter on master output

### Raspberry Pi / DAWless
- Runs headlessly as a **systemd service** — boot and play, no screen needed
- **Auto-detects all MIDI input devices** on startup and hot-plug (no configuration)
- Tested on **Raspberry Pi Zero 2 W** with **PCM5122 DAC HAT**
- Cross-compiled from Windows (WSL) using a Pi toolchain

- [Video demonstration](https://www.youtube.com/watch?v=WYv2V_M6UVY)

<img width="460" height="284" alt="piwithminilabsmaller" src="https://github.com/user-attachments/assets/68296cd8-f00f-4739-8f74-4906ded6e7cb" />

---

## Hardware

| Component | Model used |
|-----------|-----------|
| SBC | Raspberry Pi Zero 2 W |
| DAC HAT | IQaudIO PCM5122 |
| MIDI Controller | Any class-compliant MIDI controller |

---

## MIDI CC Mapping

| CC | Control |
|----|---------|
| 20 | Waveform → Sine |
| 21 | Waveform → Square |
| 22 | Waveform → Triangle |
| 23 | Chime 2 on/off |
| 24 | Chime 3 on/off |
| 25 | Rotary on/off |
| 26 | Rotary Slow/Fast |
| 27 | Mute |
| 73–80 | Hammond drawbars (in tab order) |
| 81 | Harmonics |
| 82 | Subharmonics |
| 84–87 | Attack / Decay / Sustain / Release |

---

## Building

### Prerequisites
- [JUCE](https://juce.com/) — clone separately and point `JUCE_PATH` to it
- CMake 3.22+
- C++17 compiler

### Desktop (Windows / macOS / Linux)

```bash
cmake -S . -B build -DJUCE_PATH=/path/to/JUCE
cmake --build build --config Release
```

### Raspberry Pi (cross-compile from WSL)

Requires a Pi sysroot at `~/pi-sysroot` and an aarch64 cross-compiler.
Sync sysroot from your Pi once:

```bash
rsync -avzL user@<pi-ip>:/lib/aarch64-linux-gnu/ ~/pi-sysroot/lib/aarch64-linux-gnu/
rsync -avzL user@<pi-ip>:/usr/lib/aarch64-linux-gnu/ ~/pi-sysroot/usr/lib/aarch64-linux-gnu/
rsync -avzL user@<pi-ip>:/usr/include/ ~/pi-sysroot/usr/include/
```

Then build and deploy:

```bash
cmake -S . -B ~/build-pimonio \
      -DCMAKE_TOOLCHAIN_FILE=/path/to/RaspberryPiToolchain.cmake \
      -DJUCE_PATH=/path/to/JUCE \
      -DCMAKE_BUILD_TYPE=Release

cmake --build ~/build-pimonio --config Release -j4

scp ~/build-pimonio/Armonio_artefacts/Release/Standalone/Armonio \
    user@<pi-ip>:~/
```

---

## Running Headless on Pi

Copy the binary to your Pi, then set it up as a service:

```bash
sudo nano /etc/systemd/system/armonio.service
```

```ini
[Unit]
Description=Armonio Synth
After=sound.target

[Service]
User=<your-username>
ExecStart=/home/<your-username>/Armonio
Restart=on-failure
RestartSec=3

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable armonio
sudo systemctl start armonio
```

Armonio will now launch automatically on every boot with all connected MIDI devices enabled.

---

## PCM5122 DAC — Eliminating Clicks

The PCM5122 has a hardware auto-mute that causes audible clicks between notes. Disable it on the Pi:

```bash
amixer -c 0 sset "Auto Mute" 0
amixer -c 0 sset "Auto Mute Mono" 0
sudo alsactl store
```

---

