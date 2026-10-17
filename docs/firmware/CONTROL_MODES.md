# Control modes

The parent's radio always outranks the child, and anything unknown means stop. The rules are in `lib/rideon_core/src/ModeMachine.cpp` and tested in `test/test_mode_machine`.

| Mode | Who drives | Drive enable | Steering |
| --- | --- | --- | --- |
| Locked | Nobody. The car brakes to a stop, then the drivers are disabled | HIGH while braking, then LOW | Disabled |
| Kid | Pedal and shifter, up to the speed cap on CH7 | HIGH | Disabled (freewheels, never fights the child's hands) |
| Parent | Radio throttle; the pedal is ignored | HIGH | Phase 5: held at the stick's angle by the AS5600 loop. Disabled until then |
| Killed | Nobody. Drivers and contactor cut at once; the car coasts | LOW | Disabled |

## Getting into a driving mode

At power-on, and after every stop, the car is Locked. To drive:

1. Kill switch to run.
2. Mode switch to **Locked**. Faults whose cause is gone clear at this point, but only with the sticks centred and the pedal released.
3. Sticks centred, pedal released.
4. Mode switch to Kid or Parent.

If the mode switch is already at Kid or Parent at power-on, nothing happens until it passes through Locked.

## What stops the car

| Event | Result | Back to driving |
| --- | --- | --- |
| Kill switch | Killed: drive enable and contactor cut at once | Kill switch to run, then the steps above |
| Radio link lost (no packet for 200 ms, link quality under 40 %, or link flag down) | Locked: brakes to a stop in about 0.5 s | When the link is back, the steps above |
| Link quality 40 to 70 % | Still drives, at half the speed cap | Automatic when the link improves |
| Over-current, driver fault, tilt | Locked, drive enable cut at once (coast) | The steps above, once the cause is gone |
| E-stop pressed | Killed in hardware; the firmware also locks, cuts the drive enable and holds the contactor open | Release the E-stop, then the steps above |
| Battery cutoff, battery sensor fault, input fault, current sensing lost | Locked, brakes to a stop | The steps above, once the cause is gone |
| Setup failure | Locked, contactor open | Reboot only |

See [FAULT_CODES.md](FAULT_CODES.md) for each fault.

## Changing modes while driving

Kid to Parent, or back, switches directly if the sticks are centred and the pedal is released. If not, the car goes to Locked and brakes. It moves to the new mode as soon as the sticks and pedal are released.

## Kid mode details

- The command is pedal × shifter direction (forward +1, reverse −1, both or neither = neutral), capped by CH7, with reverse capped at half.
- **Parent brake:** pulling the right stick back past 25 % brakes the car, whatever the pedal does.
- If the switch expander (shifter, horn) stops answering, Kid mode raises `INPUT` and locks. Parent mode still works.
- The radio link is required in Kid mode too. If the parent's radio dies, the car stops.

## Smooth driving

- Acceleration: 0 to full in 2 s (Parent), about 3 s (Kid).
- Braking: full to 0 in 0.5 s, with both driver inputs low, which turns on both low-side switches and brakes the motor.
- No change of direction until the car has stopped.
- Reverse is capped at 50 % of the current speed cap.
