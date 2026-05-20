/*
 * loadcell.h
 *
 *  Created on: May 20, 2026
 *      Author: young
 */

#ifndef SRC_COMMON_HW_INCLUDE_LOADCELL_LOADCELL_H_
#define SRC_COMMON_HW_INCLUDE_LOADCELL_LOADCELL_H_

#include "hw_def.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOADCELL_MAX										HW_LOADCELL_MAX

#define LOADCELL_DEFAULT_SAMPLE_COUNT   10U
#define LOADCELL_DEFAULT_SCALE          1.0f



bool loadcellInit(void);                                                // Reset loadcell module state
bool loadcellOpen(uint8_t ch);                                          // Open selected loadcell channel

bool loadcellIsOpen(uint8_t ch);                                        // Check channel open state
bool loadcellIsReady(uint8_t ch);                                       // Check sensor data-ready state

bool loadcellReadRaw(uint8_t ch, int32_t *p_raw);                       // Read averaged raw ADC count
bool loadcellReadGram(uint8_t ch, float *p_gram);                       // Read converted weight in grams
bool loadcellReadData(uint8_t ch, loadcell_data_t *p_data);             // Read raw and converted data snapshot

bool loadcellTare(uint8_t ch, uint16_t sample_count);                   // Set tare offset from averaged samples
bool loadcellSetOffset(uint8_t ch, int32_t offset);                     // Apply tare offset manually
int32_t loadcellGetOffset(uint8_t ch);                                  // Get current tare offset

bool loadcellSetScale(uint8_t ch, float scale);                         // Apply raw-count-to-gram scale factor
float loadcellGetScale(uint8_t ch);                                     // Get current scale factor

bool loadcellSetSampleCount(uint8_t ch, uint16_t sample_count);         // Set default averaging sample count
uint16_t loadcellGetSampleCount(uint8_t ch);                            // Get default averaging sample count

#ifdef __cplusplus
}
#endif

#endif /* SRC_COMMON_HW_INCLUDE_LOADCELL_LOADCELL_H_ */
