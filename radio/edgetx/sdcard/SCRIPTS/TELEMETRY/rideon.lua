-- RideOn telemetry screen and alarms for the Jumper T-Lite V2 (EdgeTX, 128x64).
-- Copied to SCRIPTS/TELEMETRY/rideon.lua by radio/install_radio.py.
-- Screen: long-press PAGE (or TELE) on the main screen.
-- The alarms in background() run whenever the RideOn model is loaded, screen or not.
--
-- Thresholds follow include/config.h; tools/check_radio_files.py checks they agree.
local BATT_WARN_V = 22.0   -- BATTERY_LOW_WARN_V
local LQ_WARN = 70         -- LQ_DEGRADED_BELOW: below this the car halves its speed cap
local BATT_REPEAT = 1000   -- 10 s between battery warnings (getTime() ticks are 10 ms)
local LQ_REPEAT = 500      -- 5 s between weak-link warnings

local fm = ""              -- last FM text from the car, "" when no telemetry
local linkUp = false
local nextBatt = 0
local nextLq = 0

-- "LOCKED ESTOP" -> "LOCKED", "ESTOP". "KILLED LINK+" -> "KILLED", "LINK+".
local function splitFm(text)
  local mode, fault = string.match(text, "^(%S+)%s+(.+)$")
  if mode then return mode, fault end
  return text, ""
end

local function telemetryText(name)
  local v = getValue(name)
  if type(v) == "string" then return v end
  return ""
end

local function number(name)
  local v = getValue(name)
  if type(v) == "number" then return v end
  return 0
end

local function beeps(n, freq)
  for _ = 1, n do
    playTone(freq, 120, 80, PLAY_BACKGROUND)
  end
end

local function onFmChange(old, new)
  local oldMode, oldFault = splitFm(old)
  local newMode, newFault = splitFm(new)
  if newFault ~= "" and newFault ~= oldFault then
    -- A new latched fault: long buzz and a falling alarm tone.
    playHaptic(300, 100, PLAY_NOW)
    playHaptic(300, 0)
    playTone(1800, 400, 100, PLAY_NOW, -10)
    playTone(1200, 400, 0)
  elseif newMode ~= oldMode then
    -- Mode change: 1 low beep Locked, 2 Kid, 3 Parent, a long low tone Killed.
    if newMode == "LOCKED" then beeps(1, 600)
    elseif newMode == "KID" then beeps(2, 1000)
    elseif newMode == "PARENT" then beeps(3, 1400)
    elseif newMode == "KILLED" then
      playTone(400, 600, 0, PLAY_NOW)
      playHaptic(200, 0)
    end
  end
end

local function background()
  local now = getTime()
  -- RQly is 0 when telemetry is not streaming.
  local lq = number("RQly")
  local up = lq > 0
  if up ~= linkUp then
    linkUp = up
    if not up then
      -- EdgeTX also says "telemetry lost". On this car that means it is braking to a stop.
      playHaptic(500, 0, PLAY_NOW)
      fm = ""
    end
  end
  if not up then return end

  local text = telemetryText("FM")
  if text ~= "" and text ~= fm then
    onFmChange(fm, text)  -- also on (re)connect, so a fault latched meanwhile is not missed
    fm = text
  end

  local v = number("RxBt")
  if v > 5 and v < BATT_WARN_V and now >= nextBatt then
    nextBatt = now + BATT_REPEAT
    playTone(1000, 200, 100, PLAY_BACKGROUND)
    playNumber(math.floor(v * 10 + 0.5), UNIT_VOLTS, PREC1)
  end

  if lq < LQ_WARN and now >= nextLq then
    nextLq = now + LQ_REPEAT
    playTone(2200, 100, 50, PLAY_BACKGROUND)
    playTone(2200, 100, 0, PLAY_BACKGROUND)
    playHaptic(100, 0)
  end
end

-- What the radio is asking for, read from its own switches (CHANNEL_MAP.md).
local function switchText()
  local kill = getValue("sc") > 0 and "RUN" or "KILL"
  local sa = getValue("sa")
  local mode = (sa < -512 and "Lock") or (sa > 512 and "Parent") or "Kid"
  local sb = getValue("sb")
  local cap = (sb < -512 and "30%") or (sb > 512 and "100%") or "60%"
  return kill, mode, cap
end

local function run(event)
  background()
  lcd.clear()
  lcd.drawText(0, 0, "RideOn", INVERS)
  local kill, mode, cap = switchText()
  lcd.drawText(40, 0, kill .. " " .. mode .. " " .. cap, SMLSIZE)

  if not linkUp then
    lcd.drawText(16, 18, "NO LINK", DBLSIZE + BLINK)
    lcd.drawText(0, 48, "Car off, or out of range.", SMLSIZE)
    lcd.drawText(0, 56, "Car brakes by itself.", SMLSIZE)
    return 0
  end

  local carMode, fault = splitFm(fm)
  if fault ~= "" then
    lcd.drawText(0, 10, carMode, MIDSIZE)
    lcd.drawText(0, 24, fault, MIDSIZE + INVERS + BLINK)
  else
    lcd.drawText(0, 12, carMode ~= "" and carMode or "...", DBLSIZE)
  end

  local v = number("RxBt")
  local battAttr = (v > 5 and v < BATT_WARN_V) and (INVERS + BLINK) or 0
  lcd.drawText(0, 40, string.format("%.1fV", v), battAttr)
  lcd.drawText(42, 40, string.format("%.1fA", number("Curr")))
  lcd.drawText(84, 40, string.format("%dmAh", number("Capa")), SMLSIZE)

  local lq = number("RQly")
  local lqAttr = lq < LQ_WARN and (INVERS + BLINK) or 0
  lcd.drawText(0, 54, string.format("LQ %d%%", lq), lqAttr)
  lcd.drawText(46, 54, string.format("%ddBm", number("1RSS")), SMLSIZE)
  lcd.drawText(90, 54, string.format("%dmW", number("TPWR")), SMLSIZE)
  return 0
end

return { run = run, background = background }
