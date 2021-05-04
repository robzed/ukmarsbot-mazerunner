/*
 * File: mouse.cpp
 * Project: mazerunner
 * File Created: Friday, 23rd April 2021 9:09:10 am
 * Author: Peter Harrison
 * -----
 * Last Modified: Friday, 23rd April 2021 11:31:13 am
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

#include "mouse.h"
#include "Arduino.h"
#include "encoders.h"
#include "maze.h"
#include "motion.h"
#include "motors.h"
#include "profile.h"
#include "reports.h"
#include "sensors.h"
#include "ui.h"

Mouse mouse;

char path[256];
char commands[256];
char mouseState __attribute__((section(".noinit")));

void mouseInit() {
  mouse.handStart = false;
  disable_steering();
  mouse.location = 0;
  mouse.heading = NORTH;
  mouseState = SEARCHING;
}

void turnIP180() {
  static int direction = 1;
  direction *= -1; // alternate direction each time it is called
  spin_turn(direction * 180, SPEEDMAX_SPIN_TURN, SPIN_TURN_ACCELERATION);
  mouse.heading = (mouse.heading + 2) & 0x03;
}

void turnIP90R() {
  spin_turn(-90, SPEEDMAX_SPIN_TURN, SPIN_TURN_ACCELERATION);
  mouse.heading = (mouse.heading + 1) & 0x03;
}

void turnIP90L() {
  spin_turn(90, SPEEDMAX_SPIN_TURN, SPIN_TURN_ACCELERATION);
  mouse.heading = (mouse.heading + 3) & 0x03;
}

void turnSS90L() {
  turn(90, 200, 2000);
  mouse.heading = (mouse.heading + 3) & 0x03;
}

void turnSS90R() {
  turn(-90, 200, 2000);
  mouse.heading = (mouse.heading + 1) & 0x03;
}

void move_forward(float distance, float top_speed, float end_speed) {
  forward.start(distance, top_speed, end_speed, SEARCH_ACCELERATION);
}

void mouseCheckWallSensors() {
  mouse.rightWall = (g_right_wall_present);
  mouse.leftWall = (g_left_wall_present);
  mouse.frontWall = (g_front_wall_present);
}

static void stopAndAdjust() {
  stop_at(180);
}

void mouseFollowTo(int target) {
  if (mouse.handStart) {
    mouse.handStart = false;
    forward.start(40, SPEEDMAX_EXPLORE, SPEEDMAX_EXPLORE, SEARCH_ACCELERATION);
  }
  while (mouse.location != target) {
    enable_steering();
    forward.start(40, SPEEDMAX_EXPLORE, SPEEDMAX_EXPLORE, SEARCH_ACCELERATION);
    wait_until_position(90);
    mouse.location = neighbour(mouse.location, mouse.heading);
    mouseCheckWallSensors();
    mouseUpdateMapFromSensors();
    if (mouse.location == target) {
      stopAndAdjust();
    } else if (!mouse.leftWall) {
      stopAndAdjust();
      turnIP90L();
    } else if (!mouse.frontWall) {
      wait_until_position(180);
    } else if (!mouse.rightWall) {
      stopAndAdjust();
      turnIP90R();
    } else {
      stopAndAdjust();
      turnIP180();
    }
  }
}

void mouseShowStatus() {
  // if (mouse.location < 16) {
  //   // debug << '0';
  // }
  // // debug << _HEX(mouse.location) << ':';
  // // debug << dirLetters[mouse.heading] << ' ' << 'W' << '=';
  // // debug << mouse.leftWall ? '1' : ' ' ;
  // // debug << mouse.frontWall ? '1' : ' ' ;
  // // debug << mouse.rightWall ? '1' : ' ' ;
  // // debug << endl;
}

/***
 * The mouse is assumed to be centrally placed in a cell and may be
 * stationary. The current location is known and need not be any cell
 * in particular.
 *
 * The walls for the current location are assumed to be correct in
 * the map.
 *
 * On execution, the mouse will search the maze until it reaches the
 * given target.
 *
 * The maze is mapped as each cell is entered. Mapping happens even in
 * cells that have already been visited. Walls are only ever added, not
 * removed.
 *
 * It is possible for the mapping process to make the mouse think it
 * is walled in with no route to the target.
 *
 * Returns  0  if the search is successful
 *         -1 if the maze has no route to the target.
 */
int mouseSearchTo(int target) {
  flood_maze(target);
  mouseShowStatus();
  // // debug << F("  searching to: ") << target << endl;
  if (cost[mouse.location] == MAX_COST) {
    return -1;
  }
  unsigned char newHeading;
  if (mouse.handStart) { // implies that the heading is correct
    mouse.handStart = false;
    // move to the cell centre
    forward.start(40, SPEEDMAX_EXPLORE, SPEEDMAX_EXPLORE, SEARCH_ACCELERATION);
  } else {
    newHeading = direction_to_smallest(mouse.location, mouse.heading);
    mouseTurnToFace(newHeading);
  }
  while (mouse.location != target) {
    // here the mouse is always at the center of the cell and may be
    // stationary or moving
    enable_steering();
    forward.start(25, SPEEDMAX_EXPLORE, SPEEDMAX_EXPLORE, SEARCH_ACCELERATION);
    wait_until_position(90);
    // now we are at the cell boundary
    mouse.location = neighbour(mouse.location, mouse.heading);
    mouseCheckWallSensors();
    mouseShowStatus();
    mouseUpdateMapFromSensors();
    flood_maze(target);
    if (mouse.location == target) {
      stopAndAdjust();
      break;
    }
    if (cost[mouse.location] == MAX_COST) { // are we walled in
      stopAndAdjust();
      return -1;
    }
    newHeading = direction_to_smallest(mouse.location, mouse.heading);
    unsigned char hdgChange = (newHeading - mouse.heading) & 0x3;
    switch (hdgChange) {
      case 0: // ahead
        wait_until_position(180);
        break;
      case 1: // right
        stopAndAdjust();
        turnIP90R();
        break;
      case 2: // behind
        stopAndAdjust();
        turnIP180();
        break;
      case 3: // left
        stopAndAdjust();
        turnIP90L();
        break;
    }
  }
  return 0;
}

//--------------------------------------------------------------------------
// assume the maze is flooded and that a simple path string has been generated
// then run the mouse along the path.
// run-length encoding of straights is done on the fly.
// turns are in-place so the mouse stops after each straight.
//--------------------------------------------------------------------------
void mouseRunInplaceTurns(int topSpeed) { //TODO
  pathExpand(path);
  // debug << path << endl;
  // debug << commands << endl;
  // "HRH": in place right
  // "HLH": in place left
  // "HH":  half a cell forward
  // "HS":  end after half a cell
  int index = 0;
  while (commands[index] != 'S') {
    if (button_pressed()) {
      break;
    }
    if (commands[index] == 'B') {
      index++;
    } else if (commands[index] == 'H' && commands[index + 1] == 'R' && commands[index + 2] == 'H') {

      move_forward(90, topSpeed, 0);
      turnIP90R();
      move_forward(90, topSpeed, topSpeed);
      index += 3;
    } else if (commands[index] == 'H' && commands[index + 1] == 'L' && commands[index + 2] == 'H') {
      move_forward(90, topSpeed, 0);
      turnIP90L();
      move_forward(90, topSpeed, topSpeed);
      index += 3;
    } else if (commands[index] == 'H' && commands[index + 1] == 'H') {
      move_forward(90, topSpeed, topSpeed);
      index++;
    } else if (commands[index] == 'H' && commands[index + 1] == 'S') {
      move_forward(90, topSpeed, 0);
      index++;
    } else {
      // debug << F("Instruction error!\n");
      break;
    }
  }
  // assume we succeed
  mouse.location = GOAL;
  mouseShowStatus();
}

//--------------------------------------------------------------------------
// Assume the maze is flooded and that a path string already exists.
// Convert that to half-cell straights for easier processing
// next, convert all HRH and HLH occurences to the corresponding smooth turns
// then run the mouse along the path.
// run-length encoding of straights is done on the fly.
// turns are smooth and care is taken to deal with the path end.
//--------------------------------------------------------------------------
void mouseRunSmoothTurns(int topSpeed) {
  pathExpand(path);
  // "HRH": smooth right
  // "HLH": smooth left
  // "HH":  half a cell forward
  // "HS":  end after half a cell
  int index = 0;
  while (commands[index] != 'S') {
    if (button_pressed()) {
      break;
    }
    if (commands[index] == 'B') {
      index++;
    } else if (commands[index] == 'H' && commands[index + 1] == 'R' && commands[index + 2] == 'H') {
      move_forward(20, topSpeed, SPEEDMAX_SMOOTH_TURN);
      // debug << 'R';
      turnSS90R();
      move_forward(20, topSpeed, topSpeed);
      index += 3;
    } else if (commands[index] == 'H' && commands[index + 1] == 'L' && commands[index + 2] == 'H') {
      move_forward(20, topSpeed, SPEEDMAX_SMOOTH_TURN);
      // debug << 'L';
      turnSS90L();
      move_forward(20, topSpeed, topSpeed);
      index += 3;
    } else if (commands[index] == 'H' && commands[index + 1] == 'H') {
      // debug << 'H';
      move_forward(90, topSpeed, topSpeed);
      ;
      index++;
    } else if (commands[index] == 'H' && commands[index + 1] == 'S') {
      // debug << 'H';
      move_forward(90, topSpeed, 0);
      index++;
    } else {
      // debug << F("Instruction error!\n");
      break;
    }
  }
  // debug << 'S' << endl;
  // assume we succeed
  mouse.location = GOAL;
  mouseShowStatus();
}

/***
 * inelegant but simple solution to the problem
 */
void mouseTurnToFace(unsigned char newHeading) {
  // debug << dirLetters[mouse.heading] << '>' << dirLetters[newHeading] << endl;
  switch (mouse.heading) {
    case NORTH:
      if (newHeading == EAST) {
        turnIP90R();
      } else if (newHeading == SOUTH) {
        turnIP180();
      } else if (newHeading == WEST) {
        turnIP90L();
      }
      break;
    case EAST:
      if (newHeading == SOUTH) {
        turnIP90R();
      } else if (newHeading == WEST) {
        turnIP180();
      } else if (newHeading == NORTH) {
        turnIP90L();
      }
      break;
    case SOUTH:
      if (newHeading == WEST) {
        turnIP90R();
      } else if (newHeading == NORTH) {
        turnIP180();
      } else if (newHeading == EAST) {
        turnIP90L();
      }
      break;
    case WEST:
      if (newHeading == NORTH) {
        turnIP90R();
      } else if (newHeading == EAST) {
        turnIP180();
      } else if (newHeading == SOUTH) {
        turnIP90L();
      }
      break;
  }
}

void mouseUpdateMapFromSensors() {
  switch (mouse.heading) {
    case NORTH:
      if (mouse.frontWall) {
        set_wall_present(mouse.location, NORTH);
      }
      if (mouse.rightWall) {
        set_wall_present(mouse.location, EAST);
      }
      if (mouse.leftWall) {
        set_wall_present(mouse.location, WEST);
      }
      break;
    case EAST:
      if (mouse.frontWall) {
        set_wall_present(mouse.location, EAST);
      }
      if (mouse.rightWall) {
        set_wall_present(mouse.location, SOUTH);
      }
      if (mouse.leftWall) {
        set_wall_present(mouse.location, NORTH);
      }
      break;
    case SOUTH:
      if (mouse.frontWall) {
        set_wall_present(mouse.location, SOUTH);
      }
      if (mouse.rightWall) {
        set_wall_present(mouse.location, WEST);
      }
      if (mouse.leftWall) {
        set_wall_present(mouse.location, EAST);
      }
      break;
    case WEST:
      if (mouse.frontWall) {
        set_wall_present(mouse.location, WEST);
      }
      if (mouse.rightWall) {
        set_wall_present(mouse.location, NORTH);
      }
      if (mouse.leftWall) {
        set_wall_present(mouse.location, SOUTH);
      }
      break;
    default:
      // This is an error. We should handle it.
      break;
  }
  walls[mouse.location] |= VISITED;
}

/***
 * The mouse is expected to be in the start cell heading NORTH
 * The maze may, or may not, have been searched.
 * There may, or may not, be a solution.
 *
 * This simple searcher will just search to goal, turn around and
 * search back to the start. At that point there will be a route
 * but it is unlikely to be optimal.
 *
 * the mouse can run this route by creating a path that does not
 * pass through unvisited cells.
 *
 * A better searcher will continue until a path generated through all
 * cells, regardless of visited state,  does not pass through any
 * unvisited cells.
 *
 * The walls can be saved to EEPROM after each pass. It left to the
 * reader as an exercise to do something useful with that.
 */
int mouseSearchMaze() {
  wait_for_front_sensor();
  //                                             motorsEnable();
  mouse.location = 0;
  mouse.heading = NORTH;
  int result = mouseSearchTo(GOAL);
  if (result != 0) {
    panic(1);
  }
  //  EEPROM.put(0, walls);
  // digitalWrite(RED_LED, 1);
  delay(200);
  result = mouseSearchTo(0);
  stop_motors();
  if (result != 0) {
    panic(1);
  }
  //    EEPROM.put(0, walls);
  delay(200);
  return 0;
}

/***
 * Search the maze until there is a solution then make a path and run it
 * First with in-place turns, then with smooth turns;
 *
 * The mouse can be placed into any of the possible stated before
 * calling this function so that individual actions can be tested.
 *
 * If you do not want to search exhaustively then do a single search
 * out and back again. Then block off all the walls in any cells that
 * are unvisited. Now any path generated will succeed even if it is
 * not optimal.
 */
int mouseRunMaze() {
  // motorsEnable();
  if (mouseState == SEARCHING) {
    wait_for_front_sensor();
    mouse.handStart = true;
    enable_steering();
    mouse.location = 0;
    mouse.heading = NORTH;
    mouseSearchTo(GOAL);
    mouseSearchTo(0);
    mouseTurnToFace(NORTH);
    delay(200);
    mouseState = INPLACE_RUN;
  }
  if (mouseState == INPLACE_RUN) {
    flood_maze(GOAL);
    pathGenerate(0);
    // debug << F("Maze is searched\nwaiting inplace for start\n");
    wait_for_front_sensor();
    Serial.println(F("Running in place"));
    mouseRunInplaceTurns(SPEEDMAX_STRAIGHT);
    Serial.println(F("Returning"));
    mouseSearchTo(0);
    Serial.println(F("Done"));
    mouseState = SMOOTH_RUN;
  }
  if (mouseState == SMOOTH_RUN) {
    // now try with smooth turns;
    flood_maze(GOAL);
    pathGenerate(0);
    mouseTurnToFace(direction_to_smallest(mouse.location, mouse.heading));
    delay(200);
    // debug << F("waiting for smooth run start\n");
    wait_for_front_sensor();
    Serial.println(F("Running smooth"));
    mouseRunSmoothTurns(SPEEDMAX_STRAIGHT);
    Serial.println(F("Returning"));
    mouseSearchTo(0);
    Serial.println(F("Finished"));
    mouseState = FINISHED;
  }
  stop_motors();
  return 0;
}

/***
 * Assumes the maze is already flooded to a single target cell and so
 * every cell will have a cost that decreases as the target is approached.
 *
 * Starting at the given cell, the algorithm repeatedly looks for the
 * smallest available neighbour and records the action taken to reach it.
 *
 * The process starts by assuming the mouse is heading NORTH in the start
 * cell since that is what would be the case at the start of a speed run.
 *
 * At each cell, the preference is to move forwards if possible
 *
 * If the pathfinder is called from any other cell, the mouse must first
 * turn to face to the smallest neighbour of that cell using the same
 * method as in this function.
 *
 * The resulting path is a simple string, null terminated, that can be
 * printed to the Serial.to make it easy to compare paths using different
 * flooding or path generating methods.
 *
 * The characters in the path string are:
 * 	'B' : always the first character, it marks the path start.
 * 	'F' : move forwards a full cell
 * 	'H' : used in speedruns to indicate movement of half a cell forwards
 * 	'R' : turn right in this cell
 * 	'L' : turn left in this cell
 * 	'A' : turn around (should never happen in a speedrun path)
 * 	'S' : the last character in the path, telling the mouse to stop
 *
 * For example, the Japan2007 maze, flooded with a simple Manhattan
 * flood, should produce the path string:
 *
 * BFFFRLLRRLLRRLLRFFRRFLLFFLRFRRLLRRLLRFFFFFFFFFRFFFFFRLRLLRRLLRRFFRFFFLFFFS
 *
 * The path string is processed by the mouse directly to make it move
 * along the path. At its simplest, this is just a case of executing
 * a single movement for each character in the string, using in-place turns.
 *
 * I would strongly recommend this style of path string. Not only can the
 * strings be used to compare routes very easily, they can be printed and
 * visually compared or followed by hand.
 *
 * Path strings are easily translated into more complex paths using
 * smooth turns an they are relatively easy to turn into a set of
 * commands that will represent a diagonal path.
 *
 * Further, short path strings  can be hand-generated to test the movement
 * of the mouse or to test the setup of different turn types.
 *
 * The pathGenerator is not terribly efficient and can take up to 20ms to
 * generate a path depending on the maze, start cell and target.
 *
 */

bool pathGenerate(unsigned char startCell) {
  bool solved = true;
  ;
  unsigned char cell = startCell;
  int nextCost = cost[cell] - 1; // assumes manhattan flood
  unsigned char commandIndex = 0;
  path[commandIndex++] = 'B';
  unsigned char direction = direction_to_smallest(cell, NORTH);
  while (nextCost >= 0) {
    unsigned char cmd = 'S';
    switch (direction) {
      case NORTH:
        if (is_exit(cell, NORTH) && neighbour_cost(cell, NORTH) == nextCost) {
          cmd = 'F';
          break;
        }
        if (is_exit(cell, EAST) && neighbour_cost(cell, EAST) == nextCost) {
          cmd = 'R';
          direction = DtoR[direction];
          break;
        }
        if (is_exit(cell, WEST) && neighbour_cost(cell, WEST) == nextCost) {
          cmd = 'L';
          direction = DtoL[direction];
          break;
        }
        if (is_exit(cell, SOUTH) && neighbour_cost(cell, SOUTH) == nextCost) {
          cmd = 'A';
          direction = DtoB[direction];
        }
        break;
      case EAST:
        if (is_exit(cell, EAST) && neighbour_cost(cell, EAST) == nextCost) {
          cmd = 'F';
          break;
        }
        if (is_exit(cell, SOUTH) && neighbour_cost(cell, SOUTH) == nextCost) {
          cmd = 'R';
          direction = DtoR[direction];
          break;
        }
        if (is_exit(cell, NORTH) && neighbour_cost(cell, NORTH) == nextCost) {
          cmd = 'L';
          direction = DtoL[direction];
          break;
        }
        if (is_exit(cell, WEST) && neighbour_cost(cell, WEST) == nextCost) {
          cmd = 'A';
          direction = DtoB[direction];
          break;
        }
        break;
      case SOUTH:
        if (is_exit(cell, SOUTH) && neighbour_cost(cell, SOUTH) == nextCost) {
          cmd = 'F';
          break;
        }
        if (is_exit(cell, WEST) && neighbour_cost(cell, WEST) == nextCost) {
          direction = DtoR[direction];
          cmd = 'R';
          break;
        }
        if (is_exit(cell, EAST) && neighbour_cost(cell, EAST) == nextCost) {
          cmd = 'L';
          direction = DtoL[direction];
          break;
        }
        if (is_exit(cell, NORTH) && neighbour_cost(cell, NORTH) == nextCost) {
          cmd = 'A';
          direction = DtoB[direction];
          break;
        }
        break;
      case WEST:
        if (is_exit(cell, WEST) && neighbour_cost(cell, WEST) == nextCost) {
          cmd = 'F';
          break;
        }
        if (is_exit(cell, NORTH) && neighbour_cost(cell, NORTH) == nextCost) {
          cmd = 'R';
          direction = DtoR[direction];
          break;
        }
        if (is_exit(cell, SOUTH) && neighbour_cost(cell, SOUTH) == nextCost) {
          cmd = 'L';
          direction = DtoL[direction];
          break;
        }
        if (is_exit(cell, EAST) && neighbour_cost(cell, EAST) == nextCost) {
          cmd = 'A';
          direction = DtoB[direction];
          break;
        }
        break;
      default:
        // this is an error. We should handle it.
        break;
    }
    cell = neighbour(cell, direction);
    if ((walls[cell] & VISITED) != VISITED) {
      solved = false;
    }
    nextCost--;
    path[commandIndex] = cmd;
    commandIndex++;
  }
  path[commandIndex] = 'S';
  commandIndex++;
  path[commandIndex] = '\0';
  return solved;
}

/***
 * Assumes that the maze is flooded and a path string has been generated.
 *
 * Convert  the simple path string to a set of commands using half-cell
 * moves instead of full-cell moves so that a sequence like
 * 		BFRFLS
 * becomes
 * 		BHHRHHHHLHHS
 *
 * The turns all have an implied full cell forward movement after them
 * The only real advantage of this is that it is easier to convert to smooth
 * turns by looking for patterns like HRH and HLH
 *

 */
void pathExpand(char *pathString) {
  int pathIndex = 0;
  int commandIndex = 0;
  commands[commandIndex++] = 'B';
  while (char c = pathString[pathIndex]) {
    switch (c) {
      case 'F':
        commands[commandIndex++] = 'H';
        commands[commandIndex++] = 'H';
        pathIndex++;
        break;
      case 'R':
        commands[commandIndex++] = 'R';
        commands[commandIndex++] = 'H';
        commands[commandIndex++] = 'H';
        pathIndex++;
        break;
      case 'L':
        commands[commandIndex++] = 'L';
        commands[commandIndex++] = 'H';
        commands[commandIndex++] = 'H';
        pathIndex++;
        break;
      case 'S':
        commands[commandIndex++] = 'S';
        pathIndex++;
        break;
      case 'B':
      case ' ': // ignore these so we can write easy-to-read path strings
        pathIndex++;
        break;
      default:
        // TODO: this is an error. We should handle it.
        break;
    }
  }
  commands[commandIndex] = '\0';
}
