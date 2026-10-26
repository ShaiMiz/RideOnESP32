# Log events

Every log record is one event: what happened, when, on which part and how bad. The car prints each record as one line on the USB serial port (115200 baud). Records and the fault table are defined in `lib/rideon_core/src/EventLog.h` and `FaultTable.h`; `tools/check_log_events.py` fails CI when this page and the code disagree.

Design and reasoning: [Logging, fault records and on-scene debugging](https://claude.ai/artifact/RovF9Ru6NaCksPPXNrguB5). For what each fault means and what to check, see [FAULT_CODES.md](FAULT_CODES.md).

## Reading a line

```
#4211 b42 +123.456s CRIT SAFETY FAULT OVERCUR comp=FL sym=OVER v=23.40 lim=20.00 mode=KID faults=0x0010
```

| Part | Meaning |
| --- | --- |
| `#4211` | Record number since boot (on flash it will never repeat) |
| `b42` | Boot number: goes up by one at every power-up or reset, kept in NVS |
| `+123.456s` | Time since that boot. The car has no calendar clock, so boot number plus uptime is the timestamp |
| `CRIT` | Severity, numbered as in RFC 5424 syslog |
| `SAFETY` | Category |
| `FAULT` | Event, then its values as `key=value`, units in the key |
| `mode=KID` | Mode when it happened |
| `faults=0x0010` | Latched fault mask when it happened (bits in `Faults.h`) |

A line is printed only when something changes, plus a `SNAPSHOT` after 60 s of quiet. The older 500 ms status line still prints alongside for the bench.

## Severities

| Severity | Number | Used for |
| --- | --- | --- |
| `CRIT` | 2 | A hard fault: drivers cut at once. Also a part that failed to start |
| `ERR` | 3 | A fault that brakes the car to a stop; battery at cutoff or its sensor wrong |
| `WARN` | 4 | Still driving but degraded: weak or lost link, low battery, records dropped, fault table full |
| `NOTICE` | 5 | Normal but important: boot, mode change, re-arm, a fault's condition ending, link back |
| `INFO` | 6 | `SNAPSHOT`. The first to be dropped when the queue is full |

## Categories

| Category | Events |
| --- | --- |
| `SYS` | `BOOT`, `SETUP_FAIL`, `SNAPSHOT`, `DROPPED`, `TABLE_FULL` |
| `SAFETY` | `FAULT`, `FAULT_GONE`, `REARM` |
| `MODE` | `MODE` |
| `LINK` | `LINK` |
| `POWER` | `BATTERY` |
| `DRIVE`, `STEER`, `SENSE`, `CFG` | Reserved for later events |

## Events

| Event | Severity | Values | When |
| --- | --- | --- | --- |
| `BOOT` | NOTICE | `reset=` `POWER_ON`, `SOFTWARE`, `PANIC`, `WATCHDOG`, `BROWNOUT` or `OTHER` | Every boot. `PANIC`, `WATCHDOG` or `BROWNOUT` means an unexpected restart: find the cause before driving |
| `SETUP_FAIL` | CRIT | `comp=` the part | A part did not start at boot. The car stays locked until a reboot (`SETUP` fault) |
| `FAULT` | CRIT or ERR | fault code, `comp=`, `sym=`, `v=` measured, `lim=` limit | A fault condition starts on a part. CRIT when it is a hard fault |
| `FAULT_GONE` | NOTICE | fault code, `comp=`, `lasted_s=` | That condition ended. The fault stays latched until a re-arm |
| `REARM` | NOTICE | `cleared=` mask of the faults cleared | The parent re-armed from Locked and latched faults cleared |
| `MODE` | NOTICE | `from=`, `to=` | Mode changed |
| `LINK` | WARN or NOTICE | `state=` `LOST`, `WEAK` or `OK`, `lq=` % | Link state changed (WARN unless it is back to OK) |
| `BATTERY` | NOTICE, WARN or ERR | `state=` `OK`, `LOW`, `CUTOFF` or `SENSOR`, `bat_V=` | Battery state changed |
| `SNAPSHOT` | INFO | `bat_V=`, `I_A=` total drive current, `out=` drive output -1..1, `lq=` | After 60 s with no other record |
| `DROPPED` | WARN | `n=` | Records were dropped because the queue was full (least severe first) |
| `TABLE_FULL` | WARN | fault code, `comp=` | A new fault and part did not fit in the fault table (24 rows) |

Two conditions are normal at boot and are not logged as `FAULT` or counted: the radio link before it first comes up, and `NO_BUS` while the boot weld check waits for the bus to drain (`sym=WAITING`). The `LINK` record still shows the link coming up.

## Components

| Component | Part |
| --- | --- |
| `-` | No particular part |
| `FL` | Front-left drive wheel: motor, driver and current sense |
| `FR` | Front-right drive wheel |
| `RL` | Rear-left drive wheel |
| `RR` | Rear-right drive wheel |
| `STEER` | Steering motor and driver |
| `WHEELS` | All four drive wheels together (total current) |
| `BUS` | Motor bus after the contactor (its ADS1115 reading) |
| `K1` | Main contactor |
| `BATT` | Battery and its voltage divider |
| `RADIO` | Receiver, link and radio channels |
| `ESTOP` | E-stop button and its sense line |
| `PCF` | PCF8574 switch expander |
| `PCA` | PCA9685 PWM board |
| `ADS` | ADS1115 boards (at boot) |
| `BOX` | Box temperature sensor |
| `IMU` | MPU-6050 |

## Symptoms

| Symptom | Meaning |
| --- | --- |
| `-` | None given |
| `OVER` | Above its limit for longer than the hold time |
| `UNDER` | Below its limit for longer than the hold time |
| `STALE` | No fresh reading |
| `STUCK_HIGH` | A driver IS line pinned high: the driver reports a fault |
| `RANGE` | Reading or channel value impossible |
| `NO_ANSWER` | Device not answering |
| `LOST` | Radio link lost |
| `ACTIVE` | A switch or button in its stop position (kill switch, E-stop) |
| `WAITING` | A boot check still running, not a failure |
| `WELDED` | Contactor welded shut |
| `MISSING` | Expected but not there |

## Fault table

Besides the log, the car keeps one row per fault and part that has ever tripped, like a car's diagnostic trouble codes (ISO 14229). Each row has:

- the status bits: failing now, failed this boot, confirmed, failed since the table was cleared (numbered as in the ISO 14229 status byte);
- how many times it tripped, and the worst value seen (highest, or lowest for `UNDER`);
- a freeze frame of the first and of the latest trip: boot, time, mode, link quality, command, output, pedal, battery and bus volts, the five currents and the box temperature.

Rows never clear themselves. Today the table lives in RAM and starts empty at each boot; the next step saves it, and the log, to flash, adds serial commands to read and clear it, and adds a 5 s crash recorder.
