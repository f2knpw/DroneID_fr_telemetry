
local function run()
  -- Called periodically while the Special Function switch is on
  -- switch on serial port 1 : serialSetPower(port_nr, value)
	serialSetPower(1, 1)
end


return { run=run }