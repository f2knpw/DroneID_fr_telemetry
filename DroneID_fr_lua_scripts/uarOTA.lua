local function init()
  -- Called once when the script is loaded
end

local function run()
  -- Called periodically while the Special Function switch is on
--{"RTC":1693769182, "lat":435128245, "lon":14970095, "GSpd":0, "Hdg":193.33999633789, "GAlt":183.60000610352}
	serialSetPower(1, 1)
	serialWrite('{"OTA": 1}\n')
end


local function background()
  -- Called periodically while the Special Function switch is off
end

return { run=run, background=background, init=init }