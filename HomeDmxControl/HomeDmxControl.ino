/*
    Copyright (C) 2019, 2020 Johannes Nolte

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 2.1 only.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    Firmware for Board V2.0. PWM out only, SW in only.
*/

#include <Bounce2.h>  // https://github.com/thomasfredericks/Bounce2
#include <Arduino.h>
#include <SPI.h>
#include <EEPROM.h>
#include "dogm_7036.h"

// pin assignment
#define PIN_LED_STATUS 13
#define PIN_POTI_1 A0
#define PIN_POTI_2 A1
#define PIN_INTERUPT 7
#define PIN_SW_2_UP A5
#define PIN_SW_3_DOWN A4
#define PIN_SW_1_COL A2
#define PIN_PWM_1 5
#define PIN_PWM_2 6
#define PIN_PWM_3 9
#define PIN_PWM_4 10
#define PIN_LCD_RESET 8
#define PIN_LCD_SCK 15
#define PIN_LCD_SI 16
#define PIN_LCD_RS 12
#define PIN_LCD_CS 17
#define PIN_LCD_BACKLIGHT 11

// EEPROM/SYSVAR constants
#define NUMBER_OF_SYSVAR 3
#define SYSVAR_BACKLIGHT 0
#define SYSVAR_PAGE_START 1
#define SYSVAR_PAGE_LAST 2

// channel and column numbers are 1 biased (similar to dmx)
#define NUMBER_OF_CHANELS 4  //>=2
#define NUMBER_OF_COLUMNS 2
#define NUMBER_OF_PAGES 5

// LCD instance and special characters
dogm_7036 DOG;
const byte DOG_ARROW_POS[] = {6, 13};
const byte arrow_none[] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01};
const byte arrow_up[] = {0x01, 0x03, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01};
const byte arrow_down[] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x05, 0x03, 0x01};
const byte hline[] = {0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03};

// Switches
Bounce switch1_col = Bounce();
Bounce switch2_up = Bounce();
Bounce switch3_down = Bounce();

// State Variables
byte cur_page_values[NUMBER_OF_CHANELS];
byte org_page_values[NUMBER_OF_CHANELS];
byte cur_col;
byte cur_page;
byte cur_sys[NUMBER_OF_SYSVAR];
byte org_sys[NUMBER_OF_SYSVAR];

int trace_poti1 = false;
int trace_poti2 = false;

bool changed_value = true;
bool changed_column = true;
bool changed_page = true;

// Others
bool status_led = true;


// Function Set
void set_toogle_status_led();
void set_column_marker(int col, int row, int trace);
void set_backlight();

// Funtion Get
void trace(int reg_val, byte poti_val, int trace);
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
  // p_si=p_clk=0 -> use hardware SPI
  DOG.initialize(PIN_LCD_CS, 0, 0, PIN_LCD_RS, PIN_LCD_RESET, 1, DOGM162); //byte p_cs, byte p_si, byte p_clk, byte p_rs, byte p_res, boolean sup_5V, byte lines
  DOG.displ_onoff(true);
  DOG.cursor_onoff(false);
  DOG.define_char(0, arrow_none);
  DOG.define_char(1, arrow_up);
  DOG.define_char(2, arrow_down);
  DOG.define_char(3, hline);

  //Setup Pins
  pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_LCD_BACKLIGHT, LOW);

  pinMode(PIN_LED_STATUS, OUTPUT);
  digitalWrite(PIN_LED_STATUS, HIGH);

  // Setup Switches
  switch1_col.attach(PIN_SW_1_COL, INPUT);
  switch1_col.interval(25);

  switch2_up.attach(PIN_SW_2_UP, INPUT);
  switch2_up.interval(25);

  switch3_down.attach(PIN_SW_3_DOWN, INPUT);
  switch3_down.interval(25);

  // Load Data from EPPROM
  load_eeprom_sys(cur_sys);
  load_eeprom_sys(org_sys);
  if (cur_sys[SYSVAR_PAGE_START] == 0) {
    cur_page = cur_sys[SYSVAR_PAGE_LAST];
  } else {
    cur_page = cur_sys[SYSVAR_PAGE_START];
    load_eeprom_ch_page();
  }
  cur_col = 1;

  Serial.begin(9600);

}


void loop() {

  // Check Switches, updating cur_page and cur_col, toogle ch7
  switch1_col.update();
  switch2_up.update();
  switch3_down.update();

  if ( switch1_col.rose() ) {
    if (switch1_col.previousDuration() > 1000) {
      if (cur_page > 0) {
        toogle_byte(&cur_page_values[7 - 1]);
        changed_value = true;
      } else {
        //toogle_byte(&cur_page_values[7-1]);
      }
    } else {
      if (cur_page > 0) {
        cur_col++;
        if (cur_col > NUMBER_OF_COLUMNS) {
          cur_col = 1;
        }
        changed_column = true;
      } else {
        cur_sys[SYSVAR_PAGE_START]++;
        if (cur_sys[SYSVAR_PAGE_START] > NUMBER_OF_PAGES) {
          cur_sys[SYSVAR_PAGE_START] = 0;
        }
        changed_value = true;
      }
    }
  }

  int new_page = cur_page;
  if (switch2_up.rose()) {
    new_page++;
  }
  if (switch3_down.rose()) {
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
      load_eeprom_sys(org_sys);
    }
    cur_page = byte(new_page);
    load_eeprom_ch_page();

    if (cur_sys[SYSVAR_PAGE_START] == 0) {
      cur_sys[SYSVAR_PAGE_LAST] = cur_page;
      // must be written to eeprom since it cant be saved by page change
      set_eeprom_sys(SYSVAR_PAGE_LAST, cur_page);
    }
    changed_page = true;
  }

  // Check Poti Tracing
  if (changed_page || changed_column) {
    trace_poti1 = 999;
    trace_poti2 = 999;
  }

  if (cur_page == 0) {
    trace(&cur_sys[SYSVAR_BACKLIGHT], get_poti_value(PIN_POTI_1), &trace_poti2);
  }
  else
  {
    trace(&cur_page_values[2 * cur_col - 1 - 1], get_poti_value(PIN_POTI_1), &trace_poti1);
    trace(&cur_page_values[2 * cur_col - 1], get_poti_value(PIN_POTI_2), &trace_poti2);
  }

  // update LCD only when values have chnaged (without column markers)
  // value changed flag is set by set_eeprom funtions
  if (changed_value || changed_column || changed_page) {
    String str_row_1 = "S ";
    String str_row_2 = String(cur_page);
    if (!compare_page_cur_vs_org() || !compare_sys_cur_vs_org()) {
      str_row_2.concat("*");
    }
    else
    {
      str_row_2.concat(" ");
    }
    Serial.println(str_row_2);
    if (cur_page == 0) {
      DOG.position(1, 1);
      str_row_1.concat("pagestart:");
      char buf[3];
      sprintf(buf, " %03d   ", cur_sys[SYSVAR_PAGE_START]);
      str_row_1.concat(buf);
      DOG.position(3, 2);
      str_row_2.concat("intesity :");
      sprintf(buf, " %03d   ", cur_sys[SYSVAR_BACKLIGHT]);
      str_row_2.concat(buf);
    } else {
      for (int i = 1; i <= NUMBER_OF_COLUMNS; i++) {
        char buf[5];
        byte ch = 2 * i - 1;
        sprintf(buf, "%c%1d: %03d", 0x03, ch, get_channel(ch));
        str_row_1.concat(buf);
        sprintf(buf, "%c%1d: %03d", 0x03, ch + 1, get_channel(ch + 1));
        str_row_2.concat(buf);
      }

    }
    DOG.position(1, 1);
    DOG.string(str_row_1.c_str());
    DOG.position(1, 2);
    DOG.string(str_row_2.c_str());

    write_output();
    set_backlight();
  }

  // update the column marker at every cycle
  if (cur_page > 0) {
    set_column_marker(DOG_ARROW_POS[cur_col - 1], 1, trace_poti1);
    set_column_marker(DOG_ARROW_POS[cur_col - 1], 2, trace_poti2);
  }
  else {
    set_column_marker(13, 2, trace_poti2);
  }

  set_toogle_status_led();

  changed_value = false;
  changed_page = false;
  changed_column = false;

  delay(20);

}

void write_output() {
  analogWrite(PIN_PWM_1, 255 - get_channel(1));
  analogWrite(PIN_PWM_2, 255 - get_channel(2));
  analogWrite(PIN_PWM_3, 255 - get_channel(3));
  analogWrite(PIN_PWM_4, 255 - get_channel(4));
}

// write column marker at current curser pusition of LCD
void set_column_marker(int col, int row, int trace) {
  DOG.position(col, row);
  if (trace == 0) {
    DOG.ascii(0);
  } else if (trace < 0) {
    DOG.ascii(1);
  } else {
    DOG.ascii(2);
  }
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
    changed_value = true;
  } else {
    if (abs(dif) <= 1) {
      *trace = 0;
    }
    else {
      *trace = dif;
    }
  }
}

// toggle between 0 and 255 (usefull for dmx)
void toogle_byte(byte *b) {
  if (*b == 255) {
    *b = 0;
  } else {
    *b = 255;
  }
}

//returns a 8bit value for the poti, if new read to close to the ne one the
inline byte get_poti_value(int pin) {
  return byte(analogRead(pin) / 4);
}

void set_backlight() {
  analogWrite(PIN_LCD_BACKLIGHT, cur_sys[SYSVAR_BACKLIGHT]);
}

// return true if arrays are identical, else false
bool compare_page_cur_vs_org() {
  for (byte i = 0; i < NUMBER_OF_CHANELS; i++) {
    if (cur_page_values[i] != org_page_values[i]) {
      return false;
    }
  }
  return true;
}

// return true if arrays are identical, else false
bool compare_sys_cur_vs_org() {
  for (byte i = 0; i < NUMBER_OF_SYSVAR; i++) {
    if (cur_sys[i] != org_sys[i]) {
      return false;
    }
  }
  return true;
}

byte get_channel(byte ch) {
  return cur_page_values[ch - 1];
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


// Setter for SYSVAR_BACKLIGHT, EEPROM_PAGE, EPROME_COL ect.
inline byte get_eeprom_sys(int identifier) {
  return EEPROM.read(identifier);
}

// Getter for SYSVAR_BACKLIGHT, EEPROM_PAGE, EPROME_COL ect.
inline void set_eeprom_sys(int identifier, byte val) {
  return EEPROM.update(identifier, val);
  changed_value = true;
}

// Provides one place where the register value gets generated
// from the page and the channel number.
// Maximum page number is 255 as page is save in one register.
int compute_reg(byte page, byte ch) {
  // First 10 registeres are reserved + 10 channels per page + ch
  return NUMBER_OF_SYSVAR + (page - 1) * NUMBER_OF_CHANELS + ch;
}
