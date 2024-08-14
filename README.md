# UnitCamS3_timelapse_pir
Takeing JPG images by timelapse/PIR trigger and storing them into its microSD.

# Features
It takes JPG images by timelapse/PIR trigger and stores into its microSD.And the trigger(interval, PIR, and both) is able to choose by JSON file in the microSD.Also you can custom the timelapse interval by the JSON.

# Hardware requirement
* Unit CamS3 Wi-Fi Camera (OV2640)[SKU: U174] - mandatory
* microSD - mandatory
* USB TypeC 2Grove Unit[SKU: U151] -mandatory. But if you use another your own wire structure, optional
* PIR Motion Sensor (AS312)[SKU: U004] - mandatory, if you use PIR mode

# Usage
You must place 'Pref.json' into root of microSD.The elements are;  
* Key:"interval"
** It has a number object. It describes interval of timelapse by microsecond.
* Key:"type"
** It has a string object. It describes the what trigger is enabled.

* An example of 'triggered by interval & PIR, and capturing each 1sec'
    `{`
    `  {"interval":1000000}`
    `  {"type":"both"}`
    `}`

# License
"UnitCamS3_timelapse_pir" is under [MIT license](https://en.wikipedia.org/wiki/MIT_License).
