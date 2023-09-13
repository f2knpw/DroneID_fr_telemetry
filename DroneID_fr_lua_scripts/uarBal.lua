local function init()
  -- Called once when the script is loaded
	mlat =  getFieldInfo("lat").id    -- here we get the numerical id of the filed "lat"
	mlon =  getFieldInfo("lon").id    
	mGSpd =  getFieldInfo("GSpd").id    
	mHdg =  getFieldInfo("Hdg").id 
	mGAlt =  getFieldInfo("GAlt").id 	
end

local function run()
  -- Called periodically while the Special Function switch is on
--{"OTA":0, "lat":435128245, "lon":14970095, "GSpd":0, "Hdg":193.33999633789, "GAlt":183.60000610352}
	serialSetPower(1, 1) -- setPower port AUX2 on --> serialSetPower(port_nr, value)
	serialWrite('{"OTA": 0, "lat":'..getValue(mlat)..', "lon":'..getValue(mlon)..', "GSpd":'..getValue(mGSpd)..', "Hdg":'..getValue(mHdg)..', "GAlt":'..getValue(mGAlt)..'}\n')
end


local function background()
  -- Called periodically while the Special Function switch is off
end

return { run=run, background=background, init=init }