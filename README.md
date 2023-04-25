# OpenGBW

This Project extends and adapts the original by Guillaume Besson

More info: https://besson.co/projects/coffee-grinder-smart-scale


This mod will add GBW functionality to basically any coffe grinder that can be started and stopped manually.

The included 3D Models are adapted to the Eureka Mignon XL, but the electronics can be used for any Scale.

-----------

### Differences to the original

- added a rotary encoder to select weight and navigate menus
- made everything user configurable without having to compile your custom firmware
- dynamically adjust the weight offset after each grind
- added relay for greater compatibility
- added different ways to activate the grinder
- added scale only mode

-----------

### Getting started

1) 3D print the included models for a Eureka Mignon XL or design your own
2) flash the firmware onto an ESP32
3) connect the display, relay, load cell and rotary encoder to the ESP32 according to the wiring instructions
4) go into the menu by pressing the button of the rotary encoder and set your initial offset. -2g is a good enough starting value for a Mignon XL
5) if you're using the Mignon's push button to activate the grinder set grinding mode to impulse. If you're connected directly to the motor relay use continuous.
6) if you only want to use the scale to check your weight when single dosing, set scale mode to scale only. This will not trigger any relay switching and start a timer when the weight begins to increase. If you'd like to build your own brew scale with timer, this is also the mode to use.
7) calibrate your load cell by placing a 100g weight on it and following the instructions in the menu
8) set your dosing cup weight
5) exit the menu, set your desired weight and place your empty dosing cup on the scale. The first grind might be off by a bit - the accuracy will increase with each grind as the scale auto adjusts the grinding offset

-----------

### Wiring

#### Load Cell

| Load Cell  | HX711 | ESP32  |
|---|---|---|
| black  | E-  | |
| red  | E+  | |
| green  | A+  | |
| white  | A-  | |
|   | VCC  | VCC/3.3 |
|   | GND  | GND |
|   | SCK  | GPIO 18 |
|   | DT  | GPIO 19|

#### Display

| Display | ESP32 |
|---|---|
| VCC | VCC/3.3 |
| GND | GND |
| SCL | GPIO 22 |
| SDA | GPIO 21 |

#### Relay

| Relay | ESP32 | Grinder |
|---|---|---|
| + | VCC/3.3 | |
| - | GND | |
| S | GPIO 33 | |
| Middle Screw Terminal | | push button |
| NO Screw Terminal | | push button |

#### Rotary Encoder

| Encoder | ESP32 |
|---|---|
| VCC/+ | VCC/3.3 |
| GND | GND |
| SW | GPIO 34 |
| DT | GPIO 23 |
| CLK | GPIO 32 |

-----------

### BOM

1x ESP32  
1x HX711 load cell amplifier  
1x 0.9" OLED Display  
1x KY-040 rotary encoder  
1x 500g load cell 60 x 15,8 x 6,7 https://www.amazon.de/gp/product/B019JO9BWK/ref=ppx_yo_dt_b_asin_title_o02_s00?ie=UTF8&psc=1  

various jumper cables  
a few WAGO or similar connectors

-----------

### 3D Files

You can find the 3D STL models on thangs.com

Eureka XL: https://thangs.com/designer/jbear-xyz/3d-model/Eureka%20Mignon%20XL%20OpenGBW%20scale%20addon-834667?manualModelView=true

These _should_ fit any grinder in the Mignon line up as far as I can tell.

There's also STLs for a universal scale in the repo, though it is mostly meant as a starting off point to create your own. You can use the provided files, but you'll need to print an external enclosure for the ESP32, relay and any other components your setup might need.

### Todo:

- ~add option to change grind start/stop behaviour. Right now it pulses for 50ms, this works if its hooked up to the push button of a Eureka grinder. Other models might need constant input while grinding~ done
- add mounting options and cable routing channels to base
- more detailed instructions (with pictures!)
- other grinders?
- ???