# Home DMX - Firmware

is a simple arduino based DMX controller to controll led lights installed in your home to be used with DMX controllers and relais.

The Hardware can be found in a extra git repository: [Home-DMX-Board](https://github.com/jonolt/Home-DMX-Board).

This Project started with [Home-DMX-Control](https://github.com/jonolt/Home-DMX-Control). Firmware and hardware are now in two repositories.


## Idea

The selection for the lighting scene in daily use should be as simple as possible. Therefore exactly two push buttons are used to move up and down in a list of lighting scenes. Since I am not a fan of idle transformers, a conventional light switch is used in my application to switch the whole system on and off. In the same way a corresponding scene could be set.

The individual scenes should be adjustable directly on the controller without additional hardware. The input of the scenes works like on lighting control desks without motor faders via potentiometer and buttons. To make the firmware easy (without programmer and co) and maintainable by everyone, an Arduino was used, because it has a USB converter and the IDE is easy to use and install.

## End User Usage

### Everyay Usage

Use the two buttons (connecte do SW2 and SW3 on pcb) to change sceene, by going up and down an scene list.

### Configuration

Exact User Interface depends on the project specific arduino programm. The brigness values are set with two analog potis (one for each line). They work like in Ligthning Cosoles. A arrow up/down means the current poti value is below/above the software value. Chhanfing the poti value will not change the software value until bot values where matched once. This is shown when only a pipe is displayed.

**External Power must be disconnected when programming the Ardino!**

#### Page 0

It is used to adjust the brightness of the display and to show the temperature measured by the I2C temperature sensor.

#### Page X (1, ..., 9)

Each page stores one light scene. In my project I have 3 zones of WW/CW LED stripes which should be switched independently. The zones are divided into columns. With SW1 a column can be selected and with RV1 and RV2 the brightness can be adjusted. To switch pages SW2 and SW3 are used which are not on the board. 

## Firmware

The scenes and the current page and current column are stored in the EPPROM. Each page/scene can store n 8-bit registers values to store DMX values.

Code is more or less self explaining (sorry for the a bit cryptic math logic for pages). Be aware of the pointers in the trace method.


## License and Copyright

Copyright (C) 2019, 2020 Johannes Nolte
 
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 2.1 only.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
