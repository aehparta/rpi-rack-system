# Rack setup system for Raspberry Pi

## v1.0

Raspberry Pi and similar compatible rack installation system.

Basic holder:
* *10x10 cm*, cheap PCB size
* Supplies power to attached Raspberry Pi or similar through GPIO header
* Supports:
  * Raspberry Pi 2/3/4 and other boards that have matching size and GPIO header
  * Orange Pi R1 (suitable as a router with dual built-in 100 Mbps ethernet)

Single backplane module:
* *10x10 cm*, cheap PCB size
* Takes 4 holder boards using 2x10 card edge connector (3.96 mm pitch)

## First installation with single modular piece without frame

<img src="https://github.com/aehparta/rpi-rack-system/blob/master/images/rpi-rack-first-installation.jpg" width="60%">

## Power supply

Each Raspberry Pi holder has it's own DC-DC converter which uses Ti TPS5450 and is able to supply *5 V* and *3 A* from *5.5 to 36 V* input.

## v2.0

Well, as it turns out, PCBs get cheaper and cheaper.

---
<img src="https://github.com/aehparta/rpi-rack-system/blob/master/images/holder-empty.jpg" width="48%"> <img src="https://github.com/aehparta/rpi-rack-system/blob/master/images/holder-with-rpi.jpg" width="48%">

Holder for Raspberry Pi compatible boards and Orange Pi compatible boards.

---
<img src="https://github.com/aehparta/rpi-rack-system/blob/master/images/connectors.jpg" width="48%"> <img src="https://github.com/aehparta/rpi-rack-system/blob/master/images/fans.jpg" width="48%">

Front and back view of assembled backplane connected to enclosure attachment brackets.

---
<img src="https://github.com/aehparta/rpi-rack-system/blob/master/images/caps.jpg" width="48%"> <img src="https://github.com/aehparta/rpi-rack-system/blob/master/images/active-bridge.jpg" width="48%">

Some big caps and view to active rectifier circuit (four FETs on heatsinks in left image and 8-pin DIP on right image) and big 45A schottky diodes for automatic transfer from AC to battery on AC power loss.

---
<img src="https://github.com/aehparta/rpi-rack-system/blob/master/images/control.jpg" width="48%"> <img src="https://github.com/aehparta/rpi-rack-system/blob/master/images/current-measurement-circuit.jpg" width="48%">

Some slot control and slot specific current measurement circuitry.

---
![installed-slots](https://github.com/aehparta/rpi-rack-system/blob/master/images/installed-slots.jpg)

Populating the slots for software development.

---
<img src="https://github.com/aehparta/rpi-rack-system/blob/master/images/ui-overview.png" width="50%"><img src="https://github.com/aehparta/rpi-rack-system/blob/master/images/ui-slot-view.png" width="50%">

The control UI.
