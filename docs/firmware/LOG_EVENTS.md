# Log events

Every log record is one event: what happened, when, on which part and how bad. The car stores each record in its own flash area, where it survives power-off, and prints it as one line on the USB serial port (115200 baud). Records and the fault table are defined in `lib/rideon_core/src/EventLog.h` and `FaultTable.h`; `tools/check_log_events.py` fails CI when this page and the code disagree.

Design and reasoning: [Logging, fault records and on-scene debugging](https://claude.ai/artifact/RovF9Ru6NaCksPPXNrguB5). For what each fault means and what to check, see [FAULT_CODES.md](FAULT_CODES.md).

## Reading a line

```
#4211 b42 +123.456s CRIT SAFETY FAULT OVERCUR comp=FL sym=OVER v=23.40 lim=20.00 mode=KID faults=0x0010
```

| Part | Meaning |
| --- | --- |
| `#4211` | Record number. It never repeats: it carries on across boots |
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
| `SYS` | `BOOT`, `SETUP_FAIL`, `SNAPSHOT`, `DROPPED`, `TABLE_FULL`, `LOG_MOUNT`, `CRASH_DUMP`, `CRASH_SAVED`, `DTC_CLEARED` |
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
| `LOG_MOUNT` | NOTICE, or WARN with torn slots | `records=` found on flash, `torn=` unreadable slots, `ready_sectors=` erased ahead, `formatted=1` if the log area was wiped | Every boot, after the log area is read. A torn slot is a record cut short by a power cut; one now and then is normal |
| `CRASH_DUMP` | ERR | `pc=` where the crash happened | After a `PANIC` or `WATCHDOG` reset, when a crash dump is in flash. See Crash dumps below |
| `CRASH_SAVED` | WARN | `cause=FAULT` and the fault, or `cause=RESET` and the reset reason; `samples=`, `crash_boot=` the boot it happened in | The crash recorder's last 5 s were kept in NVS. See Crash recorder below |
| `DTC_CLEARED` | NOTICE | `rows=` cleared | The fault table was cleared from the serial console (`clear dtc yes`) |

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

Rows never clear themselves. The table is kept in NVS and loaded at boot; a change is saved the next time the car is stopped with the drivers off. If the power goes before that, the trip is still in the log.

## Where it is stored

| Where | What | Size |
| --- | --- | --- |
| `evlog` flash partition | The log: a ring of 32-byte records with CRCs | 256 KB, 8192 records; the oldest 128 go when it wraps |
| NVS, namespace `evlog` | Boot counter (`boot`), fault table (`dtc`, layout version `dtcv`), crash recorder copy (`crash`, `crashv`) | A few KB |
| RTC memory | The crash recorder's live 5 s; survives a panic or watchdog reset, not a power cut | 1.5 KB |
| `coredump` flash partition | The ESP-IDF crash dump of the last panic | 64 KB |

The flash layout is in `partitions.csv`. Erasing flash stalls both CPU cores for tens of ms, so the firmware erases only at boot and while the car is stopped with the drivers off. It keeps 8 sectors (1024 records) erased ahead, so it never needs to erase while driving (`LOG_READY_SECTORS` in `config.h`). Writing one record while driving is a single 32-byte flash write.

The first boot after flashing this layout finds old bytes in the log area, wipes it (a few seconds, before the watchdog starts) and logs `LOG_MOUNT ... formatted=1`. Uploading new firmware with `pio run -t upload` keeps the log and the NVS; erasing the whole chip (`pio run -t erase`) clears both.

## Crash recorder

Like a car's event data recorder, the firmware keeps the last 5 s of driving: a freeze frame every 100 ms (mode, link quality, command, output, pedal, battery and bus volts, the five currents, box temperature), 50 in all. They live in RTC memory, which survives a panic or watchdog reset but not a power cut, and carry a CRC so a garbled copy is never trusted.

The 5 s are copied to NVS, replacing the previous copy, and `CRASH_SAVED` is logged:

- after a hard fault (the drive was cut) while driving. The trip itself is the last sample. The copy is written once the car is stopped with the drivers off. The kill switch is not a crash, and neither is the boot weld check waiting for the bus;
- at the next boot after a `PANIC`, `WATCHDOG` or `BROWNOUT` reset, if the car was driving in those 5 s.

A fault or reset while parked never replaces the copy, so the record of a real incident stays until a newer one. Read it with the `crash` console command.

## Serial console

Type a command in the serial monitor (115200 baud) and press Enter. Upper or lower case both work.

| Command | Prints or does |
| --- | --- |
| `help` | The commands |
| `status` | Boot number, uptime, mode, link, latched faults, battery and bus volts, current, output, log and fault-table size, whether a crash copy is kept |
| `log [n]` | The newest `n` log records from flash, oldest first (default 50) |
| `dtc` | The fault table: each row, then its first and last freeze frame |
| `crash` | The kept crash recorder copy: what caused it, then the 50 samples, oldest first |
| `clear dtc yes` | Clears the fault table and logs `DTC_CLEARED`. Only with the car Locked or Killed, stopped, drivers off. `clear dtc` alone says how |
| `test watchdog` | Hangs the loop so the watchdog resets the ESP32 (commissioning phase 3). Only with the car Locked or Killed, stopped, drivers off |

Nothing on the console can arm the car, drive it or change a limit. Long listings print a few lines per loop tick, only while the serial buffer has room, so they never slow the control loop; the 500 ms status line pauses while one prints. Typing a new command stops a listing.

## Crash dumps

After a panic or a watchdog reset the ESP32 saves a crash dump in the `coredump` partition, and the next boot logs `CRASH_DUMP pc=...` with the crashing task's name on serial. To read the whole dump (backtrace and variables) with the board on USB:

```sh
pip install esp-coredump
esp-coredump --port COMx info_corefile .pio/build/car/firmware.elf
```

The `.elf` must be from the same build that crashed.
