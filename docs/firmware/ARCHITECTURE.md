# Firmware architecture

The firmware has two layers. **`lib/rideon_core`** holds every safety and control decision as plain C++ with no Arduino calls, so it runs and is tested on a PC (`pio test -e native`). **`src/`** holds thin modules that talk to hardware and pass plain values into the core.

## One loop iteration (100 Hz)

```
 RadioInput ──► channels, link flag, packet age, LQ ──► LinkMonitor ──┐
 ManualInput ─► pedal, shifter, horn ───────────────────────────────┤
 PowerMonitor ► battery and bus volts, 5 currents, driver flags ────┤
 ContactorCheck (bus vs battery), BoxTempCheck (DS18B20) ───────────┤
                                                                    ▼
                                         SafetyManager.evaluate ─► active faults
                                                                    ▼
                                                       FaultLatch (latches them)
                                                                    ▼
             ArmingCheck (sticks centred, pedal up) ─► ModeMachine ─► Locked / Kid / Parent / Killed
                                                                    ▼
                                main.cpp: command from the mode, speed cap, reverse cap
                                                                    ▼
                SafetyManager.apply ─► KillLines (drive EN, steer EN, contactor)
                                                                    ▼
                          DriveController ─► Ramp ─► 4 × BTS7960Motor PWM
```

Between ticks, `loop()` keeps draining the receiver's serial port so no frames are lost.

## Modules

| Module | Layer | Job |
| --- | --- | --- |
| `Faults.h` | core | Fault bits, `FaultLatch`, `HoldTimer` |
| `ModeMachine` | core | Locked, Kid, Parent and Killed rules |
| `ArmingCheck` | core | Sticks centred and pedal released |
| `CrsfFrameDetector` | core | Finds and timestamps RC frames in the receiver's bytes, resyncing like AlfredoCRSF |
| `LinkMonitor` | core | Link Ok, Degraded or Lost from the flag, packet age and LQ |
| `ChannelMap.h` | core | Channel numbers and stick and switch decoding |
| `Ramp` | core | Accel and brake ramps, no reverse until stopped, caps |
| `BatteryCheck` | core | Voltage bands with hold time and hysteresis |
| `ContactorCheck` | core | Boot weld check and missing-bus check from the motor-bus voltage |
| `BoxTempCheck`, `Ds18b20.h` | core | Box over-temperature with hysteresis; DS18B20 scratchpad CRC and decoding |
| `Pid` | core | Steering loop (phase 5) |
| `main.cpp` | src | Setup order, fixed-rate loop, command selection |
| `safety/KillLines` | src | GPIO21, GPIO2, GPIO12 |
| `safety/Watchdog` | src | Task watchdog on the loop, used by `car` and `one_motor` |
| `safety/SafetyManager` | src | Turns sensor readings into fault conditions; sets the kill lines |
| `input/RadioInput` | src | AlfredoCRSF on UART2, plus a tap that timestamps RC frames |
| `input/ManualInput` | src | Pedal ADC and PCF8574 switches |
| `drive/BTS7960Motor` | src | Signed speed to RPWM/LPWM (from v2.2) |
| `drive/DriveController` | src | Four wheels on one ramp, each flipped by `WHEEL_REVERSED` |
| `drive/SteeringController` | src | Stub: holds steering disabled until the AS5600 is fitted |
| `sensing/PowerMonitor` | src | Battery volts; ADS1115 currents and motor-bus volts, one channel per tick |
| `sensing/BoxTemp` | src | DS18B20 on GPIO15, own 1-Wire timing, one small step per tick |
| `accessories/Accessories` | src | Box fans full on through the PCA9685; lights and sound later |
| `sensing/Imu`, `WheelSpeed` | src | Stubs |
| `telemetry/Log` | src | Non-blocking serial lines |
| `telemetry/CrsfTelemetry`, `Dash` | src | Stubs |

## Where things live

- Pins: only `include/pins.h`.
- Limits, rates, thresholds: only `include/config.h`.
- Radio channels: `ChannelMap.h`, documented in [CHANNEL_MAP.md](CHANNEL_MAP.md).

## Build environments

| Env | Builds | Used in |
| --- | --- | --- |
| `car` | `src/` except `src/bench/` | Phase 5 onward |
| `radio_check` | `src/bench/radio_check.cpp` + RadioInput + KillLines | Phase 2 |
| `one_motor` | `src/bench/one_motor.cpp` + one motor, radio, power, log, watchdog | Phase 4 |
| `native` | `lib/rideon_core` + `test/` on the PC | Every change |

## Adding a feature

1. Put any decision logic in `lib/rideon_core` with a unit test.
2. Keep the hardware module in `src/` thin.
3. Add new pins to `pins.h` and new limits to `config.h`, nowhere else.
4. If it can stop the car, give it a fault bit and a row in [FAULT_CODES.md](FAULT_CODES.md).
5. Bench-test it on its own before it goes in the car.
