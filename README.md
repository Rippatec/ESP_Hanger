# ESP_Hanger

This shows a fast WiFi connect method I found on pokewithastick.net

Wake from deep-sleep, read sensor, UDP post, back to sleep.

Time taken on my router is typicaly 250ms.

The sensor is a HTU21D for the measurement of temperature and humidity. The Heat-index is then calculated.
Also included in the UDP packet is the battery voltage and the WiFi connect time.

The UDP packet is then sent by broadcast packet in the local subnet to be picked-off by apropriate devices (logger, display).

I dont use a regulator, just 2 x D-cells. They start at about 3.2V and it works down to 2.8~ish. 
Not particularly efficient but this is just a proof of concept and it works just fine.

