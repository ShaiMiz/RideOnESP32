# Radio channel map

The single list of what each radio channel does. `lib/rideon_core/src/ChannelMap.h` must match it, and so must the radio model in [radio/MODEL_SETUP.md](../../radio/MODEL_SETUP.md).

Channels are numbered from 1, as EdgeTX and AlfredoCRSF number them.

| Channel | Constant | Control | Values |
| --- | --- | --- | --- |
| 1 | `ch::STEERING` | Right stick horizontal | 988 left, 1500 straight, 2012 right |
| 2 | `ch::THROTTLE` | Right stick vertical | 988 full reverse or brake, 1500 stop, 2012 full forward |
| 5 | `ch::KILL` | 2-position switch | above 1700 = run; anything else = KILL |
| 6 | `ch::MODE` | 3-position switch | below 1300 Locked, 1300 to 1700 Kid, above 1700 Parent |
| 7 | `ch::SPEED_CAP` | 3-position switch | Kid speed cap 30 % / 60 % / 100 % |
| 8 | `ch::HORN` | Momentary | above 1700 = horn |

Channels 3 and 4 (the left stick) are not used. The T-Lite's left throttle axis does not spring back to centre.

## Rules

- A value outside 900 to 2100 µs is invalid. An invalid throttle, steering or kill channel raises `INPUT`. An invalid mode channel means Locked.
- The kill channel fails safe: only a clear "run" position allows driving.
- Channel values are trusted only while the link is up. When the link drops, AlfredoCRSF keeps the last values, so they are ignored.
