# Esp8266 Based Plant Moisture sensor

This code is largely based off of https://github.com/dmainmon/ESP8266-Soil-Moisture-Sensor right now

## Changes made so far:
* Moved to platform.io
* Adjusted values for my captive sensors
* Migrated wifi information to an external constants file


## Planned improvements:
* Have the monitor report on moisture to something
 * Google sheets might be an option
* Setup a dashboard in smartthings to show plants
* Work out a way to ask for more water
	* Perhaps on a configurable basis depending on plant
* Runtime calibration