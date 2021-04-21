/*
 * File: ui.cpp
 * Project: mazerunner
 * File Created: Sunday, 28th March 2021 2:44:24 pm
 * Author: Peter Harrison
 * -----
 * Last Modified: Thursday, 8th April 2021 10:04:41 am
 * Modified By: Peter Harrison
 * -----
 * MIT License
 *
 * Copyright (c) 2021 Peter Harrison
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "ui.h"
#include "digitalWriteFast.h"
#include "maze.h"
#include "reports.h"
#include "sensors.h"
#include "settings.h"
#include "tests.h"
#include "user.h"
#include <Arduino.h>

#define MAX_DIGITS 8

const int MAX_ARGC = 16;
struct Args {
  const char *argv[MAX_ARGC];
  int argc;
};

const int INPUT_BUFFER_SIZE = 32;
char s_input_line[INPUT_BUFFER_SIZE];
uint8_t s_index = 0;

//***************************************************************************//

/***
 * Scan a character array for an integer.
 * Begin scn at line[pos]
 * Assumes no leading spaces.
 * Stops at first non-digit.
 * MODIFIES pos so that it points to the first non-digit
 * MODIFIES value ONLY IF a valid integer is converted
 * RETURNS  boolean status indicating success or error
 *
 * optimisations are possible but may not be worth the effort
 */
uint8_t read_integer(const char *line, int &value) {
  char *ptr = (char *)line;
  char c = *ptr++;
  bool is_minus = false;
  uint8_t digits = 0;
  if (c == '-') {
    is_minus = true;
    c = *ptr++;
  }
  int32_t number = 0;
  while (c >= '0' and c <= '9') {
    if (digits++ < MAX_DIGITS) {
      number = 10 * number + (c - '0');
    }
    c = *ptr++;
  }
  if (digits > 0) {
    value = is_minus ? -number : number;
  }
  return digits;
}
//***************************************************************************//

/***
 * Scan a character array for a float.
 * This is a much simplified and limited version of the library function atof()
 * It will not convert exponents and has a limited range of valid values.
 * They should be more than adequate for the robot parameters however.
 * Begin scan at line[pos]
 * Assumes no leading spaces.
 * Only scans MAX_DIGITS characters
 * Stops at first non-digit, or decimal point.
 * MODIFIES pos so that it points to the first character after the number
 * MODIFIES value ONLY IF a valid float is converted
 * RETURNS  boolean status indicating success or error
 *
 * optimisations are possible but may not be worth the effort
 */
uint8_t read_float(const char *line, float &value) {

  char *ptr = (char *)line;
  char c = *ptr++;
  uint8_t digits = 0;

  bool is_minus = false;
  if (c == '-') {
    is_minus = true;
    c = *ptr++;
  }

  uint32_t a = 0.0;
  int exponent = 0;
  while (c >= '0' and c <= '9') {
    if (digits++ < MAX_DIGITS) {
      a = a * 10 + (c - '0');
    }
    c = *ptr++;
  };
  if (c == '.') {
    c = *ptr++;
    while (c >= '0' and c <= '9') {
      if (digits++ < MAX_DIGITS) {
        a = a * 10 + (c - '0');
        exponent = exponent - 1;
      }
      c = *ptr++;
    }
  }
  float b = a;
  while (exponent < 0) {
    b *= 0.1;
    exponent++;
  }
  if (digits > 0) {
    value = is_minus ? -b : b;
  }
  return digits;
}

int cli_run_test(const Args args) {
  if (args.argc < 2) {
    run_test(get_switches());
    return T_OK;
  }
  int test_number = -1;
  read_integer(args.argv[1], test_number);
  if (test_number < 0) {
    return T_UNEXPECTED_TOKEN;
  }
  run_test(test_number);
  return T_OK;
}

int cli_run_user(const Args args) {
  if (args.argc < 2) {
    run_mouse(get_switches());
    return T_OK;
  }
  int test_number = -1;
  read_integer(args.argv[1], test_number);
  if (test_number < 0) {
    return T_UNEXPECTED_TOKEN;
  }
  run_mouse(test_number);
  return T_OK;
}

int move_test(const Args args) {
  if (args.argc != 2) {
    return T_UNEXPECTED_TOKEN;
  }
  int distance = -1;
  read_integer(args.argv[1], distance);
  Serial.print("Move ");
  Serial.println(distance);
  if (distance < 0) {
    return T_UNEXPECTED_TOKEN;
  }

  extern void move_test(int distance);
  move_test(distance);

  return T_OK;
}

int circle_test(const Args args) {
  if (args.argc != 2) {
    return T_UNEXPECTED_TOKEN;
  }
  int diameter = -1;
  read_integer(args.argv[1], diameter);
  Serial.print("Circle ");
  Serial.println(diameter);
  if (diameter < 0) {
    return T_UNEXPECTED_TOKEN;
  }

  extern void test_circle(float circle_diameter_in_mm);
  test_circle(diameter);

  return T_OK;
}

int cli_settings_command(const Args &args) {
  if (args.argc == 1) {
    dump_settings(5);
    return T_OK;
  }
  if (strlen(args.argv[1]) == 1) {
    char cmd = args.argv[1][0];
    switch (cmd) {
      case '$':
        for (int i = 0; i < get_settings_count(); i++) {
          print_setting_details(i, 5);
          Serial.println(';');
        }
        return T_OK;
        break;
      case '!':
        save_settings_to_eeprom();
        return T_OK;
        break;
      case '@':
        load_settings_from_eeprom();
        return T_OK;
        break;
      case '#':
        restore_default_settings();
        return T_OK;
        break;
    }
  }
  // it must have been a parameter get/set
  int param = -1;
  read_integer(args.argv[1], param);
  if (param < 0 || param >= get_settings_count()) {
    return T_UNEXPECTED_TOKEN;
  }
  if (args.argc == 2) {
    print_setting(param);
    return T_OK;
  }
  float value = NAN;
  if (!read_float(args.argv[2], value)) {
    return T_UNEXPECTED_TOKEN;
  }
  write_setting(param, value);
  print_setting(param);
  return T_OK;
}

void cli_clear_input() {
  s_index = 0;
  s_input_line[s_index] = 0;
}

int cli_read_line() {
  while (Serial.available()) {
    char c = Serial.read();
    // TODO : add single character priority commands like Abort
    if (c == '\n') {
      Serial.println();
      return 1;
    } else if (c == 8) {
      if (s_index > 0) {
        s_input_line[s_index] = 0;
        s_index--;
        Serial.print(c);
        Serial.print(' ');
        Serial.print(c);
      }
    } else if (isPrintable(c)) {
      c = toupper(c);
      Serial.print(c);
      if (s_index < INPUT_BUFFER_SIZE - 1) {
        s_input_line[s_index++] = c;
        s_input_line[s_index] = 0;
      }
    } else {
      // drop the character silently
    }
  }
  return 0;
}

Args cli_split_line() {
  Args args = {0};
  char *line = s_input_line;
  char *token;
  // special case for the single character settings commands
  if (s_input_line[0] == '$') {
    args.argv[args.argc] = "$";
    args.argc++;
    line++;
  }
  for (token = strtok(line, " ,="); token != NULL; token = strtok(NULL, " ,=")) {
    args.argv[args.argc] = token;
    args.argc++;
    if (args.argc == MAX_ARGC)
      break;
  }
  return args;
}

void cli_prompt() {
  Serial.print('\n');
  Serial.print('>');
  Serial.print(' ');
}

void cli_interpret(const Args &args) {
  if (strlen(args.argv[0]) == 1) {
    // These are all single-character commands
    char c = args.argv[0][0]; //  first character of first token
    switch (c) {
      case '$':
        cli_settings_command(args);
        break;
      case 'w':
        print_maze_plain();
        break;
      case 'R':
        print_maze_with_directions();
        break;
      case 'S':
        enable_sensors();
        delay(10);
        report_wall_sensors();
        disable_sensors();
        break;
      case 'T':
        cli_run_test(args);
        break;
      case 'U':
        cli_run_user(args);
        break;

      case 'M':
        move_test(args);

      case 'C':
        circle_test(args);

      default:
        break;
    }
    return;
  }
  // parse multi-character commands here
  //TODO - remove these debugging lines
  for (int i = 0; i < args.argc; i++) {
    Serial.println(args.argv[i]);
  }
}

void cli_run() {
  if (cli_read_line() > 0) {
    Args args = cli_split_line();
    cli_interpret(args);
    cli_clear_input();
    cli_prompt();
  }
}

/***
 * just sit in a loop, flashing lights waiting for the button to be pressed
 */
void panic(uint16_t n = 0xAAAA) {
  uint16_t p = n;
  while (!button_pressed()) {
    uint16_t b = p & 1;
    digitalWriteFast(LED_LEFT, b);
    digitalWriteFast(LED_RIGHT, b);
    p = (p >> 1) | (b << 15);
    delay(100);
  }
  wait_for_button_release();
  digitalWriteFast(LED_LEFT, 0);
  digitalWriteFast(LED_RIGHT, 0);
}
