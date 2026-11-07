-- TNS|RideOn ELRS setup|TNE
-- RideOn ELRS setup: sets the T-Lite's internal ELRS module to the RideOn settings,
-- reads the exact name and firmware version of the module and the car's receiver,
-- and writes them to /RideOn-radio.txt on the SD card (radio/install_radio.py reads it).
--
-- Run it from SYS > Tools > "RideOn ELRS setup" with the RideOn model selected:
--   1. Car OFF: sets packet rate, switch mode, telemetry ratio and power.
--      (ELRS refuses to change packet rate or switch mode while connected.)
--   2. Car ON (receiver bound): turns Model Match on, so the receiver learns this
--      model's number, and reads the receiver's version.
-- Safety: it only runs with SC on KILL and SA on Locked.
--
-- It speaks the same CRSF parameter protocol as the ExpressLRS Lua script (elrsV3.lua):
-- 0x28 ping, 0x29 device info, 0x2C read, 0x2B entry, 0x2D write, 0x2E status flags.
-- Setting names and options checked against ExpressLRS 3.6.4 and 4.1.0 source.
-- Wanted values: radio/ELRS_SETUP.md. Tested in CI against a simulated module
-- (tools/test_radio_scripts.py), not yet on the real radio.

local TX = 0xEE
local RX = 0xEC
local ME = 0xEF          -- ELRS Lua handset address
local SELECT = 9         -- CRSF text selection field
local REPORT = "/RideOn-radio.txt"

-- In the order they are applied. Model Match goes first while the car is on, and the
-- rate goes last: a rate change makes the link drop and come back.
local WANT = {
  { name = "Model Match", pick = { "On" }, needLink = true },
  { name = "Max Power", pick = { "100" } },
  { name = "Dynamic", pick = { "Dyn" } },
  { name = "Telem Ratio", pick = { "1:8" } },
  { name = "Switch Mode", pick = { "Hybrid" }, noLink = true },
  { name = "Packet Rate", pick = { "150Hz", "100Hz Full" }, noLink = true },
}

local state = "safety"
local devices = {}       -- [id] = { name=, version=, fields= }
local fields = {}        -- [name] = { id=, type=, options=, value= }
local fieldCount = 0
local flags = nil        -- ELRS status flags; bit 0 = receiver connected
local queue = {}         -- field ids to load
local chunk = 0
local buffer = nil
local timeout = 0
local tries = 0
local started = 0
local step = 1
local results = {}       -- [name] = text shown and reported
local message = ""
local reportDone = false

local function now() return getTime() end

local function connected() return flags ~= nil and bit32.btest(flags, 1) end

local function readString(data, offset)
  local s = ""
  while data[offset] ~= nil and data[offset] ~= 0 do
    s = s .. string.char(data[offset])
    offset = offset + 1
  end
  return s, offset + 1
end

local function splitOptions(s)
  local list = {}
  for opt in string.gmatch(s .. ";", "([^;]*);") do list[#list + 1] = opt end
  return list
end

-- "150Hz(-112dBm)" -> "150Hz"
local function label(opt)
  local l = string.gsub(opt, "%(.-%)", "")
  l = string.match(l, "^%s*(.-)%s*$")
  return l
end

local function matches(opt, want)
  local l = label(opt)
  if string.match(want, "^%d+$") then
    return tonumber(string.match(l, "^(%d+)")) == tonumber(want)
  end
  return string.lower(l) == string.lower(want)
end

local function wantedIndex(field, w)
  for _, want in ipairs(w.pick) do
    for i, opt in ipairs(field.options) do
      if opt ~= "" and matches(opt, want) then return i - 1 end
    end
  end
  return nil
end

local function current(field)
  if field.options and field.options[field.value + 1] then
    return label(field.options[field.value + 1])
  end
  return tostring(field.value)
end

local function safeSwitches()
  return getValue("sc") < 0 and getValue("sa") < -512
end

local function parseDeviceInfo(data)
  local id = data[2]
  local name, o = readString(data, 3)
  local version = string.format("%d.%d.%d", data[o + 9] or 0, data[o + 10] or 0, data[o + 11] or 0)
  devices[id] = { name = name, version = version, fields = data[o + 12] or 0 }
  if id == TX then fieldCount = devices[id].fields end
end

local function parseField(payload)
  local f = {}
  f.parent = payload[1]
  f.type = bit32.band(payload[2] or 0, 0x7f)
  local o
  f.name, o = readString(payload, 3)
  if f.type == SELECT then
    local opts
    opts, o = readString(payload, o)
    f.options = splitOptions(opts)
    f.value = payload[o] or 0
  end
  return f
end

local function parseEntry(data)
  if data[2] ~= TX or data[3] ~= queue[#queue] then return end
  local remaining = data[4]
  buffer = buffer or {}
  for i = 5, #data do buffer[#buffer + 1] = data[i] end
  if remaining > 0 then
    chunk = chunk + 1
    tries = 0
    timeout = 0                    -- ask for the next chunk at once
    return
  end
  local f = parseField(buffer)
  f.id = queue[#queue]
  if f.name ~= "" then fields[f.name] = f end
  queue[#queue] = nil
  buffer, chunk, tries, timeout = nil, 0, 0, 0
end

local function poll()
  while true do
    local command, data = crossfireTelemetryPop()
    if command == nil then return end
    if command == 0x29 then parseDeviceInfo(data)
    elseif command == 0x2B then parseEntry(data)
    elseif command == 0x2E and data[2] == TX then flags = data[6] or 0
    end
  end
end

local function loadAll()
  queue = {}
  for id = fieldCount, 1, -1 do queue[#queue + 1] = id end
  chunk, buffer, tries, timeout = 0, nil, 0, 0
end

local function requestNext()
  if #queue == 0 then return true end
  if now() >= timeout then
    tries = tries + 1
    if tries > 5 then           -- give up on this field and move on
      queue[#queue] = nil
      buffer, chunk, tries = nil, 0, 0
      return #queue == 0
    end
    crossfireTelemetryPush(0x2C, { TX, ME, queue[#queue], chunk })
    timeout = now() + 50
  end
  return false
end

local function versionLine(id, what)
  local d = devices[id]
  if not d then return what .. ": not found" end
  return what .. ": " .. d.name .. " " .. d.version
end

local function writeReport()
  local f = io.open(REPORT, "w")
  if not f then return false end
  local ver, radio, maj, minor, rev = getVersion()
  local dt = getDateTime()
  io.write(f, "RideOn radio report\n")
  io.write(f, string.format("date: %04d-%02d-%02d %02d:%02d\n", dt.year, dt.mon, dt.day, dt.hour, dt.min))
  io.write(f, string.format("edgetx: %s (%s)\n", tostring(ver), tostring(radio)))
  io.write(f, versionLine(TX, "elrs_tx") .. "\n")
  io.write(f, versionLine(RX, "elrs_rx") .. "\n")
  io.write(f, "receiver_connected: " .. (connected() and "yes" or "no") .. "\n")
  io.write(f, "model_match_ok: " .. ((connected() and not bit32.btest(flags, 4)) and "yes" or "unknown") .. "\n")
  for _, w in ipairs(WANT) do
    io.write(f, w.name .. ": " .. (results[w.name] or "?") .. "\n")
  end
  io.close(f)
  return true
end

local function applyStep()
  local w = WANT[step]
  if not w then return true end
  step = step + 1
  local f = fields[w.name]
  if not f or f.type ~= SELECT then
    results[w.name] = "not found"
    return false
  end
  local idx = wantedIndex(f, w)
  if idx == nil then
    results[w.name] = current(f) .. " (no " .. w.pick[1] .. ")"
  elseif idx == f.value then
    results[w.name] = current(f) .. " ok"
  elseif w.needLink and not connected() then
    results[w.name] = current(f) .. ", run again car ON"
  elseif w.noLink and connected() then
    results[w.name] = current(f) .. ", run again car OFF"
  else
    crossfireTelemetryPush(0x2D, { TX, ME, f.id, idx })
    f.value = idx
    results[w.name] = current(f) .. " set"
    return "wrote"
  end
  return false
end

local function init()
  started = now()
end

local function draw()
  lcd.clear()
  lcd.drawText(0, 0, "RideOn ELRS setup", INVERS)
  if state == "safety" then
    lcd.drawText(0, 12, "Set SC to KILL", 0)
    lcd.drawText(0, 22, "and SA to Locked", 0)
    lcd.drawText(0, 40, "(both switches up)", SMLSIZE)
    return
  end
  if state ~= "done" then
    lcd.drawText(0, 12, message, 0)
    return
  end
  local y = 9
  lcd.drawText(0, y, versionLine(TX, "TX"), SMLSIZE); y = y + 7
  lcd.drawText(0, y, connected() and versionLine(RX, "RX") or "RX: car off / not bound", SMLSIZE); y = y + 7
  for _, w in ipairs(WANT) do
    lcd.drawText(0, y, w.name .. ": " .. (results[w.name] or "?"), SMLSIZE)
    y = y + 7
  end
  lcd.drawText(0, 57, reportDone and "Saved " .. REPORT or "Report not saved", SMLSIZE)
end

local function run(event)
  if event == EVT_VIRTUAL_EXIT then return 1 end
  poll()
  local t = now()

  if state == "safety" then
    if safeSwitches() then
      state = "ping"
      started = t
      timeout = 0
    end
  elseif state ~= "done" and not safeSwitches() then
    state = "safety"      -- a switch moved off KILL or Locked: stop changing things
  elseif state == "ping" then
    message = "Looking for ELRS..."
    if t >= timeout then
      crossfireTelemetryPush(0x28, { 0x00, 0xEA })
      timeout = t + 100
    end
    -- Wait 1.5 s after the module answers, so the receiver can answer too.
    if devices[TX] and t > started + 150 then
      state = "status"
      timeout, tries = 0, 0
    elseif t > started + 1000 then
      message = "No ELRS module answers"
    end
  elseif state == "status" then
    message = "Reading link status..."
    if flags ~= nil then
      state = "load"
      loadAll()
    elseif t >= timeout then
      tries = tries + 1
      crossfireTelemetryPush(0x2D, { TX, ME, 0x00, 0x00 })
      timeout = t + 50
      if tries > 6 then flags = 0 end   -- no answer: treat as not connected
    end
  elseif state == "load" then
    message = "Reading settings " .. (fieldCount - #queue) .. "/" .. fieldCount
    if requestNext() then
      state = "apply"
      step = 1
    end
  elseif state == "apply" then
    message = "Applying..."
    if t >= timeout then
      local r = applyStep()
      if r == true then
        state = "settle"
        timeout = t + 100
      elseif r == "wrote" then
        timeout = t + 30   -- give the module time to save before the next write
      end
    end
  elseif state == "settle" then
    message = "Checking..."
    if t >= timeout then
      -- Read everything back, so the report shows what the module really holds.
      flags = nil
      crossfireTelemetryPush(0x2D, { TX, ME, 0x00, 0x00 })
      state = "verify"
      loadAll()
      timeout = t + 50
    end
  elseif state == "verify" then
    message = "Checking " .. (fieldCount - #queue) .. "/" .. fieldCount
    if requestNext() then
      for _, w in ipairs(WANT) do
        local f = fields[w.name]
        if f and f.type == SELECT then
          local idx = wantedIndex(f, w)
          local r = results[w.name] or ""
          if idx ~= nil and idx == f.value then
            results[w.name] = current(f) .. " ok"
          elseif string.find(r, "run again") == nil then
            results[w.name] = current(f) .. " NOT SET"
          end
        end
      end
      if flags == nil then flags = 0 end
      reportDone = writeReport()
      state = "done"
    end
  end

  draw()
  return 0
end

return { init = init, run = run }
