/* 
 *  Copyright (C) 2019, 2020 Johannes Nolte
 *   
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 2.1 only.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  Firmware for Board V1.
 */

#include <Bounce2.h>  // https://github.com/thomasfredericks/Bounce2
#include <Arduino.h>
#include <SPI.h>
#include <EEPROM.h>
// use version 1.4! 1.5 may not work and needs testing
#include <DMXSerial.h>  //https://github.com/mathertel/DMXSerial
#include "dogm_7036.h"

// pin assignment
#define PIN_BACKLIGHT 11
#define PIN_LED_STATUS 13
#define PIN_ARDUINO_LED 12
#define PIN_POTI_KW A0
#define PIN_POTI_WW A1
#define PIN_INTERUPT 7
#define PIN_SW_UP A3
#define PIN_SW_DOWN A4
#define PIN_SW_COL A5
#define PIN_RELAIS A2
#define PIN_R232_REDE 4

// EEPROM/SYSVAR constants
#define NUMBER_OF_SYSVAR 3
#define EEPROM_BACKLIGHT 0
#define EEPROM_PAGE_START 1
#define EEPROM_PAGE_LAST 2

// channel and column numbers are 1 biased (similar to dmx)
#define NUMBER_OF_CHANELS 7  //>=2
#define NUMBER_OF_COLUMNS 3
#define NUMBER_OF_PAGES 5

// LCD instance and special characters
dogm_7036 DOG;
const byte DOG_ARROW_POS[] = {5, 9, 13};
const byte arrow_none[] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01};
const byte arrow_up[] = {0x01, 0x03, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01};
const byte arrow_down[] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x05, 0x03, 0x01};

//A quire Value
int trace_ww = false;
int trace_kw = false;

bool value_changed = true;
bool column_changed = true;
bool page_changed = true;

// Switches
Bounce switch_col = Bounce(); // Instantiate a Bounce object
Bounce switch_up = Bounce();
Bounce switch_down = Bounce();

// State Variables
byte cur_page_values[NUMBER_OF_CHANELS];
byte org_page_values[NUMBER_OF_CHANELS];
byte cur_col;
byte cur_page;
byte cur_sys[NUMBER_OF_SYSVAR];

//O thers
bool status_led = true;


// Function Set
void writeDMX();
void set_toogle_status_led();
void set_column_marker(int trace);
void set_backlight();

// Funtion Get
void trace(int reg_val, byte poti_val, int trace);
byte get_poti_ww_value();
byte get_poti_kw_value();
byte get_poti_value(int pin);

// Funtion Low Level
void clear_page(byte page);
void load_eeprom_ch_page();
void save_eeprom_ch_page();
byte get_eeprom_ch(byte ch, byte page = 255);
void set_eeprom_ch(byte ch, byte val, byte page = 255);
void load_eeprom_sys(byte var[]);
void save_eeprom_sys(byte var[]);
inline byte get_eeprom_sys(byte identifier);
inline void set_eeprom_sys(byte identifier, byte val);
void toogle_byte(byte *b);
int compute_reg(byte page, byte ch);


void setup() {

  //Setup Display
  DOG.initialize(5, 6, 8, 9, 10, 1, DOGM162); //byte p_cs, byte p_si, byte p_clk, byte p_rs, byte p_res, boolean sup_5V, byte lines
  DOG.displ_onoff(true);
  DOG.cursor_onoff(false);
  DOG.define_char(0, arrow_none);
  DOG.define_char(1, arrow_up);
  DOG.define_char(2, arrow_down);

  //Setup Pins
  pinMode(PIN_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_BACKLIGHT, LOW);

  pinMode(PIN_LED_STATUS, OUTPUT);
  digitalWrite(PIN_LED_STATUS, HIGH);
  pinMode(PIN_ARDUINO_LED, OUTPUT);
  digitalWrite(PIN_ARDUINO_LED, HIGH);

  pinMode(PIN_R232_REDE, OUTPUT);
  digitalWrite(PIN_R232_REDE, HIGH);

  pinMode(PIN_RELAIS, OUTPUT);
  digitalWrite(PIN_RELAIS, LOW);

  // Setup Switches
  switch_col.attach(PIN_SW_COL, INPUT);
  switch_col.interval(25);

  switch_up.attach(PIN_SW_UP, INPUT);
  switch_up.interval(25);

  switch_down.attach(PIN_SW_DOWN, INPUT);
  switch_down.interval(25);

  //Setup Communiication (DMX, Serial)
  DMXSerial.init(DMXController);

  // Load Data from EPPROM
  load_eeprom_sys(cur_sys);
  if (cur_sys[EEPROM_PAGE_START] == 0) {
    cur_page = cur_sys[EEPROM_PAGE_LAST];
  } else {
    cur_page = cur_sys[EEPROM_PAGE_START];
    load_eeprom_ch_page();
  }
  cur_col = 1;

}


void loop() {

  // Check Switches, updating cur_page and cur_col, toogle ch7
  switch_col.update();
  switch_up.update();
  switch_down.update();

  if ( switch_col.rose() ) {
    if (switch_col.previousDuration() > 1000) {
      if (cur_page > 0) {
        toogle_byte(&cur_page_values[7 - 1]);
        value_changed = true;
      } else {
        //toogle_byte(&cur_page_values[7-1]);
      }
    } else {
      if (cur_page > 0) {
        cur_col++;
        if (cur_col > NUMBER_OF_COLUMNS) {
          cur_col = 1;
        }
        column_changed = true;
      } else {
        cur_sys[EEPROM_PAGE_START]++;
        if (cur_sys[EEPROM_PAGE_START] > NUMBER_OF_PAGES) {
          cur_sys[EEPROM_PAGE_START] = 0;
        }
        value_changed = true;
      }
    }
  }

  int new_page = cur_page;
  if (switch_up.rose()) {
    new_page++;
  }
  if (switch_down.rose()) {
    new_page--;
  }

  if (new_page > NUMBER_OF_PAGES) {
    new_page = 0;
  }
  if (new_page < 0) {
    new_page = NUMBER_OF_PAGES;
  }

  if (new_page != cur_page) {
    if (cur_page > 0) {
      save_eeprom_ch_page();
    }
    else {
      save_eeprom_sys(cur_sys);
    }
    cur_page = byte(new_page);
    load_eeprom_ch_page();

    if (cur_sys[EEPROM_PAGE_START] == 0) {
      cur_sys[EEPROM_PAGE_LAST] = cur_page;
      // must be written to eeprom since it cant be saved by page change
      set_eeprom_sys(EEPROM_PAGE_LAST, cur_page);
    }
    page_changed = true;
  }

  // Check Poti Tracing
  if (page_changed || column_changed) {
    trace_ww = 999;
    trace_kw = 999;
  }

  if (cur_page == 0) {
    digitalWrite(PIN_ARDUINO_LED, HIGH);
    trace(&cur_sys[EEPROM_BACKLIGHT], get_poti_kw_value(), &trace_kw);
  }
  else
  {
    digitalWrite(PIN_ARDUINO_LED, LOW);
    trace(&cur_page_values[2 * cur_col - 1 - 1], get_poti_ww_value(), &trace_ww);
    trace(&cur_page_values[2 * cur_col - 1], get_poti_kw_value(), &trace_kw);
  }

  // update LCD only when values have chnaged
  // value changed flag is set by set_eeprom funtions
  if (value_changed || column_changed || page_changed) {

    if (cur_page == 0) {
      DOG.position(1, 1);
      DOG.string("S pagestart:");
      char buf[3];
      sprintf(buf, " %03d   ", cur_sys[EEPROM_PAGE_START]);
      DOG.string(buf);
      DOG.position(1, 2);
      DOG.string("0 intesity :");
      sprintf(buf, " %03d   ", cur_sys[EEPROM_BACKLIGHT]);
      DOG.string(buf);
    } else {
      // refresh display without the column marker
      String strW;
      if (cur_page_values[7 - 1] == 255) {
        strW = "SxWW";
      } else {
        strW = "S WW";
      }

      String strK = String(cur_page);
      if (!compare_cur_vs_org()) {
        strK.concat("*KW");
      }
      else
      {
        strK.concat(" KW");
      }

      for (int i = 1; i <= NUMBER_OF_COLUMNS; i++) {
        char buf[3];
        sprintf(buf, " %03d", cur_page_values[2 * (i - 1)]);
        strW.concat(buf);
        sprintf(buf, " %03d", cur_page_values[2 * (i - 1) + 1]);
        strK.concat(buf);
      }
      DOG.position(1, 1);
      DOG.string(strW.c_str());
      DOG.position(1, 2);
      DOG.string(strK.c_str());
    }

    writeDMX();
    digitalWrite(PIN_RELAIS, cur_page_values[7 - 1] & 1);
    set_backlight();
  }

  // update the column marker at every cycle
  if (cur_page > 0) {
    DOG.position(DOG_ARROW_POS[cur_col - 1], 1);
    set_column_marker(trace_ww);
    DOG.position(DOG_ARROW_POS[cur_col - 1], 2);
    set_column_marker(trace_kw);
  }
  else {
    DOG.position(13, 2);
    set_column_marker(trace_kw);
  }

  set_toogle_status_led();

  value_changed = false;
  page_changed = false;
  column_changed = false;

  //    Serial.print(cur_page);
  //    Serial.print(" | ");
  //    for (int i = 0; i < 3; i++) {
  //      Serial.print(cur_sys[i]);
  //      Serial.print(" ");
  //    }
  //    Serial.print(" | ");
  //    for (int i = 0; i < 40; i++) {
  //      Serial.print(EEPROM.read(i));
  //      Serial.print(" ");
  //    }
  //    Serial.println(" ");

  delay(20);

}

// write column marker at current curser pusition of LCD
void set_column_marker(int trace) {
  if (trace == 0) {
    DOG.ascii(0);
  } else if (trace < 0) {
    DOG.ascii(1);
  } else {
    DOG.ascii(2);
  }
}

// send values from EPPROM to dimmer
void writeDMX() {
  //RegalUnten
  DMXSerial.write(1, cur_page_values[0]);
  DMXSerial.write(2, cur_page_values[1]);
  //RegalOben
  DMXSerial.write(3, cur_page_values[2]);
  DMXSerial.write(4, cur_page_values[3]);
  //Glasswand
  DMXSerial.write(5, cur_page_values[4]);
  DMXSerial.write(6, cur_page_values[4]);
  DMXSerial.write(7, cur_page_values[5]);
  DMXSerial.write(8, cur_page_values[5]);
}

//togles the status LED indicating working code
void set_toogle_status_led() {
  //Toogle Statu LED (blinking)
  if (status_led) {
    status_led = false;
  } else {
    status_led = true;
  }
  digitalWrite(PIN_LED_STATUS, status_led);
}

// Compare register value with value of poti. If the values are
// once matched update the register value with the poti value.
void trace(byte* reg_val, byte poti_val, int* trace) {
  int dif = poti_val - *reg_val;
  if (*trace == 0) {
    *reg_val = poti_val;
    value_changed = true;
  } else {
    if (abs(dif) <= 1) {
      *trace = 0;
    }
    else {
      *trace = dif;
    }
  }
}

// toggle between 0 and 255
void toogle_byte(byte *b) {
  if (*b == 255) {
    *b = 0;
  } else {
    *b = 255;
  }
}

byte get_poti_ww_value() {
  //returns a 8bit value for the ww (RV1) poti
  return get_poti_value(PIN_POTI_WW);
}

byte get_poti_kw_value() {
  //returns a 8bit value for the kw (RV2) poti
  return get_poti_value(PIN_POTI_KW);
}

//returns a 8bit value for the poti, if new read to close to the ne one the
inline byte get_poti_value(int pin) {
  return byte(analogRead(pin) / 4);
}

void set_backlight() {
  analogWrite(PIN_BACKLIGHT, cur_sys[EEPROM_BACKLIGHT]);
}

// return true if arrays are identical, else false
bool compare_cur_vs_org() {
  for (byte i = 0; i < NUMBER_OF_CHANELS; i++) {
    if (cur_page_values[i] != org_page_values[i]) {
      return false;
    }
  }
  return true;
}

void load_eeprom_ch_page() {
  for (byte i = 1; i <= NUMBER_OF_CHANELS; i++) {
    cur_page_values[i - 1] = get_eeprom_ch(i, cur_page);
    org_page_values[i - 1] = cur_page_values[i - 1];
  }
}

void save_eeprom_ch_page() {
  for (byte i = 1; i <= NUMBER_OF_CHANELS; i++) {
    set_eeprom_ch(i, cur_page_values[i - 1], cur_page);
  }
}

void load_eeprom_sys(byte var[]) {
  for (int i = 0; i < NUMBER_OF_SYSVAR; i++) {
    var[i] = get_eeprom_sys(i);
  }
}

void save_eeprom_sys(byte var[]) {
  for (int i = 0; i < NUMBER_OF_SYSVAR; i++) {
    set_eeprom_sys(i, var[i]);
  }
}

/// #### EEPMROM

// Set all channels in page to 0.
void clear_page(byte page) {
  for (byte ch = 1; ch < NUMBER_OF_CHANELS; ch++) {
    EEPROM.update(compute_reg(page, ch), 0);
  }
}

// Getter fo channel value at given page
// If page is negativ, current page number is used.
byte get_eeprom_ch(byte ch, byte page) {
  if (page == 255) {
    page = cur_page;
  }
  if (page == 0) {
    return 0;
  }
  return EEPROM.read(compute_reg(page, ch));
}

// Setter fo channel value at given page.
// If page is negativ, current page number is used.
void set_eeprom_ch(byte ch, byte val, byte page) {
  if (page == 255) {
    page = cur_page;
  }
  if (page == 0) {
    return;
  }
  EEPROM.update(compute_reg(page, ch), val);
}


// Setter for EEPROM_BACKLIGHT, EEPROM_PAGE, EPROME_COL ect.
inline byte get_eeprom_sys(int identifier) {
  return EEPROM.read(identifier);
}

// Getter for EEPROM_BACKLIGHT, EEPROM_PAGE, EPROME_COL ect.
inline void set_eeprom_sys(int identifier, byte val) {
  return EEPROM.update(identifier, val);
  value_changed = true;
}

// Provides one place where the register value gets generated
// from the page and the channel number.
// Maximum page number is 255 as page is save in one register.
int compute_reg(byte page, byte ch) {
  // First 10 registeres are reserved + 10 channels per page + ch
  return NUMBER_OF_SYSVAR + (page - 1) * NUMBER_OF_CHANELS + ch;
}
