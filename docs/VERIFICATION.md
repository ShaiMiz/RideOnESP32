# Verification

What we check, how, and when, so a change that breaks a safety rule cannot reach `main` unnoticed. The ideas are borrowed from ISO 26262 part 6 and IEC 61508 part 3 (requirements-based tests, traceability, structural coverage, static analysis, fault injection). This is a toy car, not a certified product: we take the methods, not the paperwork.

## Scope

| Level | What | Where | When |
| --- | --- | --- | --- |
| Unit | Each rule in `lib/rideon_core` on its own | `test/test_*` | Every push (CI) |
| Whole tick | The car's whole decision path (`rideon::Controller`) driven tick by tick with the real `config.h` values: link loss, kill, E-stop, every fault, re-arm, Kid caps, and 200 000 ticks of random input with safety invariants checked every tick | `test/test_controller` | Every push (CI) |
| Mutation | 36 deliberate safety bugs; the tests must catch each one | `tools/mutation_check.py` | Every push (CI) |
| Static | Our code builds with no warnings (`-Werror`), cppcheck finds nothing, pin rules hold | `platformio.ini`, `tools/run_cppcheck.sh`, `include/pin_rules.h` | Every push (CI) |
| Bench | The real hardware: timing, wiring, sensors, flash, the kill paths with the ESP32 unplugged | [COMMISSIONING.md](COMMISSIONING.md) phases 2 to 5 | By hand, before the next phase |
| Ground | Stopping time and distance, measured | [COMMISSIONING.md](COMMISSIONING.md) phase 6 | By hand, before a child rides |

**Not covered by CI:** the hardware glue in `src/` (reading the receiver, the ADS1115s, the PCF8574, writing PWM and the kill lines) and anything about real timing. That is why `src/` stays thin: `main.cpp` only fills `ControlInputs`, calls `Controller::step()` and writes `ControlOutputs`. The bench gates cover the rest.

## What CI runs

Every push and pull request runs `.github/workflows/build.yml`. All jobs must pass before Shai merges.

| Job | Checks | Fails when |
| --- | --- | --- |
| `pins` | `tools/check_pin_docs.py`, `tools/check_log_events.py`, `tools/check_verification.py`, KiCad files regenerated | Pin docs, log docs or this page disagree with the code; a requirement has no test or gate; a test is written but never run |
| `test` | `pio test -e native` | Any unit test fails |
| `verify` | `tools/run_native_tests.sh` (every suite with `-Werror`, AddressSanitizer and UndefinedBehaviorSanitizer), `tools/check_coverage.py`, `tools/run_cppcheck.sh`, `tools/mutation_check.py` | A test fails; a memory error or undefined behaviour; coverage under the floor; any cppcheck finding; a mutant survives |
| `build` (car, radio_check, one_motor) | Each firmware builds, with `-Werror` on `src/` and `lib/rideon_core` | A compile error or warning in our code |
| `exploded-view` | The 3D view's data matches the BOM and wire list | Out of date |

Coverage floors (`tools/check_coverage.py`): `lib/rideon_core` at least 92 % of lines and 75 % of branches, and **every line** of the safety files (`Controller`, `SafetyRules`, `Faults.h`, `ModeMachine`, `LinkMonitor`, `Ramp`, `ArmingCheck`, `BatteryCheck`, `BoxTempCheck`, `ContactorCheck`, `ChannelMap.h`) run by a test. Raise the floors when coverage goes up; never lower them to get a PR through.

To run it all locally (Linux, or WSL on Windows; needs g++, gcovr and cppcheck):

```sh
tools/run_native_tests.sh && python3 tools/check_coverage.py && tools/run_cppcheck.sh && python3 tools/mutation_check.py
python3 tools/check_verification.py
```

## Requirements and how each is verified

Every safety requirement has an ID. Each row names the tests that verify it (`suite/test`) and the commissioning gates that check it on the real car. `tools/check_verification.py` fails CI if a row names a test that does not exist or is not run, if a requirement has neither a test nor a gate, or if a test or mutant names an ID that is not here. Sources: [SAFETY.md](../SAFETY.md), [CONTROL_MODES.md](firmware/CONTROL_MODES.md), [FAULT_CODES.md](firmware/FAULT_CODES.md).

| ID | Requirement | Tests | Gates |
| --- | --- | --- | --- |
| SR-01 | The car boots Locked and drives only after the mode switch has been seen at Locked | `test_controller/test_boots_locked_and_needs_switch_cycle`, `test_mode_machine/test_boots_locked_and_will_not_drive_without_switch_cycle` | Phase 4 |
| SR-02 | Link loss is judged by the receiver's link flag, packet age and link quality, never by channel values. On loss the car brakes to zero, the drive enable held while braking and dropped once stopped, within the firmware budget below | `test_controller/test_link_loss_at_full_speed_brakes_to_zero_within_budget`, `test_controller/test_link_loss_ignores_frozen_channels`, `test_link_monitor/test_lost_within_200ms_even_if_flag_still_up`, `test_link_monitor/test_lost_when_flag_down`, `test_link_monitor/test_link_quality_bands`, `test_link_monitor/test_millis_wraparound`, `test_safety_rules/test_kill_line_truth_table` | Phase 2, Phase 4, Phase 5 |
| SR-03 | Every fault latches. A fault that clears on its own never restarts the car, even with the stick or pedal held | `test_controller/test_link_back_does_not_restart_without_rearm`, `test_controller/test_every_fault_stops_the_car_its_way`, `test_faults/test_latch_holds_after_condition_clears`, `test_mode_machine/test_fault_that_goes_away_stays_latched_until_rearm` | Phase 4 |
| SR-04 | Link quality between 40 and 70 %: still drives, at the degraded cap | `test_controller/test_degraded_link_caps_speed`, `test_safety_rules/test_degraded_link_is_not_a_fault` | |
| SR-05 | Kill switch: Killed, drive enable and contactor cut the same tick. A kill channel out of range is kill. Releasing it goes to Locked | `test_controller/test_kill_switch_cuts_same_tick`, `test_controller/test_kill_release_needs_rearm`, `test_controller/test_invalid_kill_channel_is_kill`, `test_mode_machine/test_kill_switch_wins_and_needs_full_rearm` | Phase 4, Phase 5 |
| SR-06 | E-stop pressed: drive cut, contactor held open; releasing it never restarts the car | `test_controller/test_estop_release_does_not_restart`, `test_safety_rules/test_estop`, `test_faults/test_estop_coasts_and_holds_contactor_open` | Phase 3, Phase 5 |
| SR-07 | A hard fault (kill, over-current, driver, tilt, setup, E-stop, no bus, weld) cuts the drive enable the tick it latches; the car coasts | `test_controller/test_every_fault_stops_the_car_its_way`, `test_safety_rules/test_kill_line_truth_table`, `test_safety_rules/test_fault_classes_are_consistent` | Phase 5 |
| SR-08 | A contactor fault (kill, setup, E-stop, driver, over-current, weld) opens K1, after the drive enable has dropped | `test_controller/test_every_fault_stops_the_car_its_way`, `test_safety_rules/test_kill_line_truth_table`, `test_safety_rules/test_fault_classes_are_consistent`, `test_contactor/test_driver_and_overcurrent_open_k1` | Phase 4 |
| SR-09 | Current and battery limits trip only after their hold time: inrush and battery sag do not | `test_controller/test_overcurrent_hold_time`, `test_controller/test_total_current_limit`, `test_controller/test_battery_sag_is_not_cutoff`, `test_safety_rules/test_overcurrent_per_wheel_after_hold`, `test_safety_rules/test_overcurrent_exactly_at_limit_is_allowed`, `test_safety_rules/test_total_overcurrent_names_all_wheels`, `test_safety_rules/test_driver_fault_after_hold_on_wheel_and_steering`, `test_faults/test_hold_timer_ignores_short_spikes` | Phase 4 |
| SR-10 | A re-arm needs the mode switch at Locked, both sticks centred, the pedal released and the fault's cause gone | `test_controller/test_rearm_needs_sticks_centred_and_pedal_released`, `test_controller/test_rearm_does_not_clear_a_present_fault`, `test_mode_machine/test_will_not_arm_with_stick_or_pedal_held`, `test_mode_machine/test_fault_locks_and_does_not_rearm_while_present`, `test_link_monitor/test_arming_check` | Phase 4, Phase 5 |
| SR-11 | Kid to Parent with something held, or an invalid mode switch, goes to Locked | `test_controller/test_mode_change_while_held_goes_locked`, `test_controller/test_invalid_mode_switch_locks`, `test_mode_machine/test_kid_to_parent_needs_centred_sticks`, `test_mode_machine/test_invalid_mode_switch_locks` | |
| SR-12 | The contactor stays open until the boot weld check passes; a welded contactor locks the car until a reboot; no bus with K1 closed is a fault | `test_controller/test_contactor_open_until_weld_check_passes`, `test_controller/test_welded_contactor_never_clears`, `test_contactor/test_closing_waits_for_a_drained_bus`, `test_contactor/test_bus_still_live_after_timeout_is_welded`, `test_contactor/test_bus_that_does_not_come_up`, `test_contactor/test_weld_fault_is_permanent_and_opens_k1`, `test_contactor/test_invalid_readings_never_pass_or_fail`, `test_safety_rules/test_contactor_checks` | Phase 3 |
| SR-13 | No link at boot: Locked | `test_controller/test_no_link_at_boot_keeps_locked`, `test_link_monitor/test_lost_before_first_packet` | Phase 2 |
| SR-14 | A part that failed at setup locks the car until a reboot | `test_controller/test_setup_failure_is_permanent`, `test_faults/test_setup_fault_is_permanent`, `test_mode_machine/test_setup_fault_never_clears` | |
| SR-15 | Kid mode: speed cap from CH7, slowest when low or invalid; reverse at most `REVERSE_CAP` of it | `test_controller/test_kid_speed_caps`, `test_controller/test_kid_reverse_cap`, `test_ramp/test_reverse_cap` | Phase 6 |
| SR-16 | Kid mode: the parent's stick-back brake beats the pedal; a lost switch expander is an `INPUT` fault | `test_controller/test_parent_brake_override_in_kid_mode`, `test_controller/test_kid_mode_needs_the_pedal_expander`, `test_safety_rules/test_lost_expander_is_a_fault_only_in_kid_mode` | Phase 5 |
| SR-17 | Gentle acceleration (gentler in Kid mode), firm braking, no change of direction until stopped | `test_controller/test_kid_ramp_is_gentler`, `test_controller/test_direction_change_stops_first`, `test_ramp/test_accelerates_gently`, `test_ramp/test_brakes_firmly`, `test_ramp/test_no_reverse_until_stopped` | Phase 5 |
| SR-18 | Under any input sequence: a hard fault or Killed means enable LOW and zero output; driving only with no fault latched; K1 closed only with no contactor fault and the weld check passed; the output moves one ramp step per tick at most and never reverses in one tick | `test_controller/test_invariants_hold_under_random_input` | |
| SR-19 | An impossible battery reading is a fault, never "OK"; cutoff needs its hold time and recovers only above the recover voltage | `test_safety_rules/test_battery_sensor_and_cutoff`, `test_faults/test_battery_sensor_fault_is_not_ok`, `test_faults/test_battery_cutoff_needs_hold_and_hysteresis`, `test_faults/test_battery_high_warning` | Phase 5 |
| SR-20 | A current-sense or bus reading that stops updating is a fault; unsensed channels are not judged | `test_safety_rules/test_stale_readings_name_the_part`, `test_safety_rules/test_unsensed_channels_are_not_checked`, `test_sensing/test_reading_stale` | Phase 4 |
| SR-21 | Box over-temperature with hold time and hysteresis; a lost sensor is HOT once required | `test_box_temp/test_hot_after_hold_and_until_cooled`, `test_box_temp/test_short_spike_is_not_hot`, `test_box_temp/test_lost_sensor_only_hot_when_required`, `test_box_temp/test_decode_rejects_bad_reads`, `test_safety_rules/test_box_and_tilt` | Phase 5 |
| SR-22 | The event log survives power cuts: torn records are detected and skipped, the newest record is never erased, and nothing that drives the car waits on flash | `test_log_ring/test_survives_power_cycle`, `test_log_ring/test_torn_record_is_skipped_and_never_read`, `test_log_ring/test_maintain_never_erases_the_sector_being_written`, `test_event_log/test_record_is_sealed_and_torn_records_are_caught`, `test_event_log/test_full_queue_drops_least_severe_first` | Phase 3 |
| SR-23 | The pin map follows the ESP32's rules: no GPIO used twice, kill lines quiet at boot, outputs not on input-only pins, analog on ADC1 | `test_pins/test_no_gpio_used_twice`, `test_pins/test_kill_lines_quiet_at_boot`, `test_pins/test_outputs_can_drive`, `test_pins/test_analog_on_adc1`, `test_pins/test_strapping_pins_match` | |
| SR-24 | The key and the E-stop cut motor power with the ESP32 unplugged | | Phase 3, Phase 5 |
| SR-25 | An ESP32 reset, crash or hang opens the contactor and drops the enables (pull-downs, watchdog) | `test_pins/test_kill_lines_quiet_at_boot` | Phase 3 |
| SR-26 | Turning the radio off stops the car within 0.5 s, measured at the highest speed cap | | Phase 4, Phase 6 |
| SR-27 | The logged current agrees with a meter | | Phase 4 |
| SR-28 | Stopping distance measured at each speed cap with the child's weight | | Phase 6 |
| SR-30 | The crash recorder keeps the last 5 s of driving after a hard fault or an unexpected reset; a garbled copy is never trusted, and a fault or reset while parked, the kill switch or the boot weld wait never replaces a kept copy | `test_crash_console/test_watchdog_reset_while_driving_keeps_the_ring`, `test_crash_console/test_hard_fault_while_driving_freezes_with_the_trip_last`, `test_crash_console/test_corrupted_ring_is_caught`, `test_crash_console/test_reset_while_parked_or_with_a_corrupt_ring_keeps_nothing`, `test_crash_console/test_kill_switch_weld_wait_and_soft_faults_do_not_freeze`, `test_crash_console/test_hard_fault_while_parked_does_not_freeze` | Phase 5 |
| SR-31 | The serial console cannot arm, drive or change a limit; it accepts only its listed commands. The Bluetooth module only listens (no wire back), and its horn-hold report fires only with the car parked | `test_crash_console/test_parse_commands`, `test_crash_console/test_parse_rejects_anything_else`, `test_crash_console/test_line_reader`, `test_crash_console/test_report_trigger_fires_once_after_a_parked_hold`, `test_crash_console/test_report_trigger_needs_the_car_parked_and_a_steady_hold` | Phase 3 |
| SR-29 | Receiver frames are found and timestamped correctly, so packet age is right | `test_crsf_frames/test_detects_rc_frame`, `test_crsf_frames/test_rejects_bad_crc`, `test_crsf_frames/test_resyncs_after_false_start`, `test_crsf_frames/test_idle_gap_drops_partial_frame` | Phase 2 |

## Link-loss stop time

SAFETY.md asks for the car to stop within **0.5 s** of the radio going off. With today's `config.h` the firmware alone needs up to about **0.72 s** at full speed: up to `LINK_TIMEOUT_MS` (200 ms) to call the link lost, then `1 / BRAKE_PER_S` (0.5 s) to brake from full output to zero, plus up to two 10 ms ticks. `test_link_loss_at_full_speed_brakes_to_zero_within_budget` checks that budget, not 0.5 s. At the Kid caps the brake is shorter (0.6 cap: about 0.5 s total). Whether the requirement is "output zero" or "wheels stopped", and which to change (a shorter timeout, a firmer link-loss brake, or the requirement), is Shai's call; until then the phase 4 and 6 gates measure it on the car.

## Adding or changing a requirement

1. Add or change its row above, in the same PR as the code.
2. Write the test first, name the ID in a comment above it (`// SR-12: ...`), and list it in the row.
3. If the rule can be broken by a one-line slip, add a mutant for it to `tools/mutation_check.py`.
4. If only the real car can show it, name the commissioning phase whose gate checks it.

## Settings only Shai can change

In GitHub, Settings, Branches, the rule for `main`: tick "Require status checks to pass before merging" and select `pins`, `test`, `verify`, and `build (car)`, `build (radio_check)`, `build (one_motor)`. Without that, a red CI run is a warning, not a block.
