/**
 * @file prompt.h
 * @author giuliof (giulio@glgprograms.it)
 * @brief Debugging serial prompt interface. Must be used in combo with debug.h.
 * @version 0.1
 * @date 2022-03-06
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef _PROMPT_H_
#define _PROMPT_H_

// Initialize debugging serial interface.
void prompt_init();
// Prompt event loop. Handle commands from serial interface.
void prompt_main();

#endif