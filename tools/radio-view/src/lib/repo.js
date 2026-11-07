// Reads the numbers the view needs out of the repo's own files, so they can't drift:
// channel numbers from lib/rideon_core/src/ChannelMap.h, limits from include/config.h.

export function channelNumbers(channelMapH) {
  const out = {};
  for (const m of channelMapH.matchAll(/constexpr uint8_t (\w+) = (\d+);/g)) {
    if (m[1] !== 'COUNT') out[m[1]] = Number(m[2]);
  }
  for (const k of ['STEERING', 'THROTTLE', 'KILL', 'MODE', 'SPEED_CAP', 'HORN']) {
    if (!out[k]) throw new Error(`ChannelMap.h: ch::${k} not found`);
  }
  return out;
}

export function configValues(configH) {
  const num = (name) => {
    const m = new RegExp(`\\b${name}\\s*=\\s*(-?[\\d.]+)f?`).exec(configH);
    if (!m) throw new Error(`config.h: ${name} not found`);
    return Number(m[1]);
  };
  return {
    minValid: num('CH_MIN_VALID'),
    maxValid: num('CH_MAX_VALID'),
    low: num('CH_LOW'),
    center: num('CH_CENTER'),
    high: num('CH_HIGH'),
    deadband: num('STICK_DEADBAND_US'),
    armTolerance: num('ARM_CENTER_TOLERANCE_US'),
    switchLowBelow: num('SWITCH_LOW_BELOW'),
    switchHighAbove: num('SWITCH_HIGH_ABOVE'),
    kidCaps: [num('KID_CAP_MIN'), num('KID_CAP_MID'), num('KID_CAP_MAX')],
    parentCap: num('PARENT_CAP'),
    reverseCap: num('REVERSE_CAP'),
    kidBrakeOverride: num('KID_BRAKE_OVERRIDE'),
    lqDegradedBelow: num('LQ_DEGRADED_BELOW'),
    lqLostBelow: num('LQ_LOST_BELOW'),
    degradedCap: num('LQ_DEGRADED_CAP'),
    batteryLowWarnV: num('BATTERY_LOW_WARN_V'),
  };
}

/** What each firmware channel is called, for labels. */
export const ROLE_LABEL = {
  STEERING: 'Steering',
  THROTTLE: 'Throttle',
  KILL: 'Kill',
  MODE: 'Mode',
  SPEED_CAP: 'Kid speed cap',
  HORN: 'Horn',
};
