# Radio channel map

The single list of what each radio channel does. `lib/rideon_core/src/ChannelMap.h` must match it, and so must the radio model ([radio/MODEL_SETUP.md](../../radio/MODEL_SETUP.md)).

Channels are numbered from 1, as EdgeTX and AlfredoCRSF number them.

| Channel | Constant | Control on the T-Lite (mode 2) | Values |
| --- | --- | --- | --- |
| 1 | `ch::STEERING` | Right stick sideways | 988 left, 1500 straight, 2012 right |
| 2 | `ch::THROTTLE` | Right stick up and down | 988 full reverse or brake, 1500 stop, 2012 full forward |
| 5 | `ch::KILL` | SC, left, 2-position: up = KILL, down = run | above 1700 = run; anything else = KILL |
| 6 | `ch::MODE` | SA, left, 3-position: up / middle / down | below 1300 Locked, 1300 to 1700 Kid, above 1700 Parent |
| 7 | `ch::SPEED_CAP` | SB, right, 3-position: up / middle / down | Kid speed cap 30 % / 60 % / 100 % |
| 8 | `ch::HORN` | Either T1 trim button (the pair under the left stick), while held | above 1700 = horn |

Channels 3 and 4 (the left stick) are not read by the firmware at all, so the left stick does nothing. The T-Lite's left throttle axis does not spring back to centre. SD (right, 2-position) is spare.

The radio model that sends these is [radio/edgetx/sdcard/MODELS/model42.yml](../../radio/edgetx/sdcard/MODELS/model42.yml); `tools/check_radio_files.py` fails CI if it and this table disagree.

## Rules

- A value outside 900 to 2100 µs is invalid. An invalid throttle, steering or kill channel raises `INPUT`. An invalid mode channel means Locked.
- The kill channel fails safe: only a clear "run" position allows driving.
- Channel values are trusted only while the link is up. When the link drops, AlfredoCRSF keeps the last values, so they are ignored.
