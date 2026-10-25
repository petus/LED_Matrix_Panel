/**
 * @file    console.h
 * @brief   Single-key test commands over the Serial Monitor.
 *
 * Active only when DEBUG_ENABLED and DEBUG_CONSOLE are both 1,
 * otherwise update() compiles to an empty function.
 *
 * Commands (type a character and press Enter / Send):
 *   m       screen mode AUTO / CLOCK / PLANE / RADAR
 *   r       download the radar now
 *   + / -   brightness up / down
 *   i       print system info
 *   h       help
 */
#pragma once

namespace Console {

  /// Print the help text (called once at start-up).
  void begin();

  /// Call from loop() – reads and executes pending serial commands.
  void update();

}  // namespace Console
