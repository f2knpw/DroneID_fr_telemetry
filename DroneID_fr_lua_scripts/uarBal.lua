local function init()
  -- Called once when the script is loaded
	mGPS =  getFieldInfo("GPS").id    -- id of the field "GPS". GPS is a "table" with GPS.lat and GPS.lon  
	mGSpd =  getFieldInfo("GSpd").id  -- id of the field "GSpd"
	mHdg =  getFieldInfo("Hdg").id    -- id of the field "Hdg"
	mGAlt =  getFieldInfo("GAlt").id  -- id of the field "GAlt"
end

local function run()
  -- Called periodically while the Special Function switch is on
--{"OTA":0, "lat":43.5128245, "lon":1.4970095, "GSpd":0, "Hdg":193.33999633789, "GAlt":183.60000610352}
	serialSetPower(1, 1) -- setPower port AUX2 on --> serialSetPower(port_nr, value)
    serialWrite('{"OTA": 0, "lat":'..(getValue(mGPS).lat)..', "lon":'..(getValue(mGPS).lon)..', "GSpd":'..getValue(mGSpd)..', "Hdg":'..getValue(mHdg)..', "GAlt":'..getValue(mGAlt)..'}\n')
end


local function background()
  -- Called periodically while the Special Function switch is off
end

return { run=run, background=background, init=init }




local function background()
  -- Called periodically while the Special Function switch is off
end

return { run=run, background=background, init=init }
