
local function run()
  -- Called periodically while the Special Function switch is on
  -- switch off serial port 1 : serialSetPower(port_nr, value)
	serialSetPower(1, 0)
end



return { run=run }