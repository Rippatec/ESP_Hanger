# ESP_Hanger
Arduino sketch for an minimal ESP8266 with a HTU21D sensor.

This shows a fast WiFi connect and send method as outlined on pokewithastick.net

Wake from deep-sleep, read sensor, UDP post, back to sleep.
Time taken on my router is typicaly 250ms.

The sensor is a HTU21D to measure temperature and humidity. The Heat-index is then calculated from the sampled data.
Also included in the UDP packet is the battery voltage and the WiFi connect time.

The UDP packet is then sent by broadcast packet in the local subnet to be picked-off by apropriate devices (Raspberry-Pi logger, display).

I dont use a regulator, just 2 x D-cells. They start at about 3.2V and it works down to 2.8~ish. 
Not particularly efficient but this is just a proof of concept and it works just fine.

