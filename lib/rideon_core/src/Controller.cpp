#include "Controller.h"

namespace rideon {

float Controller::kidCap(uint16_t speedCapUs) const {
  int pos = switchPosition(speedCapUs, cfg_.channels);
  return pos >= 0 ? cfg_.kidCaps[pos] : cfg_.kidCaps[0];  // invalid: slowest
}

ControlOutputs Controller::step(const ControlInputs& in) {
  ControlOutputs out = {};
  const ChannelLimits& chl = cfg_.channels;

  out.link = evaluateLink(in.link, cfg_.link);
  bool linkUsable = out.link != LinkState::Lost;

  // Channel values are trusted only while the link is up; they freeze when it drops.
  int killPos = switchPosition(in.killUs, chl);
  bool killSwitch = linkUsable && killPos != 2;  // only a clear "run" position allows driving
  ModeRequest request = linkUsable ? modeRequestFromSwitch(switchPosition(in.modeUs, chl)) : ModeRequest::Locked;
  bool channelsValid =
      !linkUsable || (channelValid(in.throttleUs, chl) && channelValid(in.steeringUs, chl) && killPos >= 0);
  float throttle = linkUsable ? stickFraction(in.throttleUs, chl) : 0.0f;

  SafetyInputs s = {out.link,
                    killSwitch,
                    channelsValid,
                    in.manualOk,
                    in.manualOk && in.estopPressed,
                    request == ModeRequest::Kid,
                    in.batteryVolts,
                    in.current,
                    cfg_.wheelsSensed,
                    cfg_.steerSensed,
                    cfg_.busSensed,
                    in.contactorChecked,
                    in.busMissing,
                    in.busWaiting,
                    in.k1Welded,
                    in.boxHot,
                    in.boxSensorLost,
                    in.boxCelsius,
                    in.tilted,
                    in.nowMs};
  faults_.update(rules_.evaluate(s, cfg_.safety));

  bool arming = armingOk({in.throttleUs, in.steeringUs, in.pedal}, cfg_.arming);
  out.mode = modes_.update({killSwitch, request, arming}, faults_);

  float linkCap = out.link == LinkState::Degraded ? cfg_.degradedCap : 1.0f;
  kidRamp_ = out.mode == Mode::Kid;
  if (out.mode == Mode::Kid) {
    bool parentBrake = throttle <= cfg_.kidBrakeOverride;
    float pedal = in.manualOk ? in.pedal * in.direction : 0.0f;
    out.command = parentBrake ? 0.0f : limitCommand(pedal, kidCap(in.speedCapUs) * linkCap, cfg_.reverseCap);
  } else if (out.mode == Mode::Parent) {
    out.command = limitCommand(throttle, cfg_.parentCap * linkCap, cfg_.reverseCap);
  }

  out.lines = killLines(out.mode, faults_, ramp_.stopped(rampParams()), in.contactorAllowed);
  if (out.lines.cut) {
    ramp_.reset();
  } else {
    ramp_.step(out.command, in.dtS, rampParams());
  }
  out.speed = ramp_.value();
  out.steering = linkUsable && !out.lines.cut ? stickFraction(in.steeringUs, chl) : 0.0f;
  out.horn = (out.mode == Mode::Kid && in.kidHorn) || (linkUsable && switchPosition(in.hornUs, chl) == 2);
  return out;
}

}  // namespace rideon
