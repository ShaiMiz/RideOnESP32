// What the car firmware makes of the channels: a port of the decoding in
// lib/rideon_core/src/ChannelMap.h, ModeMachine.cpp, ArmingCheck.cpp and the mode / cap part of
// Controller.cpp, with no faults and the pedal released. It is a picture of the rules for the
// view, not the firmware itself: the C++ and its tests stay the authority.

export function channelValid(us, l) {
  return us >= l.minValid && us <= l.maxValid;
}

export function stickFraction(us, l) {
  if (!channelValid(us, l)) return 0;
  const d = us - l.center;
  if (d > -l.deadband && d < l.deadband) return 0;
  const span = d > 0 ? l.high - l.center - l.deadband : l.center - l.low - l.deadband;
  const v = (d > 0 ? d - l.deadband : d + l.deadband) / span;
  return Math.max(-1, Math.min(1, v));
}

/** 0 = low, 1 = middle, 2 = high, -1 = invalid */
export function switchPosition(us, l) {
  if (!channelValid(us, l)) return -1;
  if (us < l.switchLowBelow) return 0;
  if (us > l.switchHighAbove) return 2;
  return 1;
}

const near = (us, l) => Math.abs(us - l.center) <= l.armTolerance;

export class ModeMachine {
  constructor() {
    this.mode = 'LOCKED';
    this.needCycle = true; // at boot the switch must be seen at Locked first
  }

  update({ killSwitch, request, armingOk }) {
    if (killSwitch) {
      this.mode = 'KILLED';
      this.needCycle = true;
      return this.mode;
    }
    if (this.mode === 'KILLED') this.mode = 'LOCKED';
    if (request === 'INVALID') {
      this.mode = 'LOCKED';
      this.needCycle = true;
      return this.mode;
    }
    if (request === 'LOCKED') {
      this.needCycle = false;
      this.mode = 'LOCKED';
      return this.mode;
    }
    const wanted = request === 'KID' ? 'KID' : 'PARENT';
    if (this.mode === 'LOCKED') {
      if (!this.needCycle && armingOk) this.mode = wanted;
    } else if (this.mode !== wanted) {
      this.mode = armingOk ? wanted : 'LOCKED';
    }
    return this.mode;
  }
}

const limitCommand = (cmd, cap, reverseCap) => Math.max(-cap * reverseCap, Math.min(cap, cmd));

/**
 * One firmware step. us = channel microseconds indexed by channel number (1-based),
 * link = { up, lq }. Returns what the car reads and does.
 */
export function decode(us, ch, l, link, machine) {
  const linkUsable = link.up && link.lq >= l.lqLostBelow;
  const killUs = us[ch.KILL];
  const killPos = switchPosition(killUs, l);
  const killSwitch = linkUsable && killPos !== 2;
  const modePos = switchPosition(us[ch.MODE], l);
  const request = !linkUsable ? 'LOCKED' : ['LOCKED', 'KID', 'PARENT'][modePos] ?? 'INVALID';
  const throttle = linkUsable ? stickFraction(us[ch.THROTTLE], l) : 0;
  const steering = linkUsable ? stickFraction(us[ch.STEERING], l) : 0;
  const armingOk = near(us[ch.THROTTLE], l) && near(us[ch.STEERING], l);
  const inputFault =
    linkUsable && (!channelValid(us[ch.THROTTLE], l) || !channelValid(us[ch.STEERING], l) || killPos < 0);
  // With no link the firmware brakes and holds Locked; the kill switch can't be read.
  const mode = linkUsable ? machine.update({ killSwitch, request, armingOk }) : (machine.update({ killSwitch: false, request: 'LOCKED', armingOk: true }), 'LOCKED');
  const capPos = switchPosition(us[ch.SPEED_CAP], l);
  const kidCap = capPos >= 0 ? l.kidCaps[capPos] : l.kidCaps[0];
  const linkCap = linkUsable && link.lq < l.lqDegradedBelow ? l.degradedCap : 1;
  let command = 0;
  let note = '';
  if (mode === 'KID') {
    const parentBrake = throttle <= l.kidBrakeOverride;
    command = 0; // the pedal drives in Kid mode; the view keeps it released
    note = parentBrake ? 'parent brake: stick pulled back, pedal ignored' : `pedal drives, capped at ${Math.round(kidCap * linkCap * 100)} %`;
  } else if (mode === 'PARENT') {
    command = limitCommand(throttle, l.parentCap * linkCap, l.reverseCap);
    note = `right stick drives, cap ${Math.round(l.parentCap * linkCap * 100)} %, reverse ${Math.round(l.parentCap * linkCap * l.reverseCap * 100)} %`;
  } else if (mode === 'KILLED') {
    note = 'kill switch: contactor open, brakes';
  } else {
    note = !linkUsable ? 'no link: brakes to a stop' : machine.needCycle && request !== 'LOCKED' ? 'waiting for Locked first' : !armingOk && request !== 'LOCKED' ? 'centre both sticks to arm' : 'stopped';
  }
  const horn = linkUsable && switchPosition(us[ch.HORN], l) === 2;
  return {
    linkUsable,
    killPos,
    request,
    mode,
    armingOk,
    throttle,
    steering: mode === 'KID' || mode === 'PARENT' ? steering : 0,
    command,
    kidCap,
    linkCap,
    horn,
    inputFault,
    note,
  };
}
