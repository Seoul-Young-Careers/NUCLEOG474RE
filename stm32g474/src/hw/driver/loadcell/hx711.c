/*
 * loadcell.c
 *
 *  Created on: May 20, 2026
 *      Author: young
 */

#include <loadcell/hx711.h>

#ifdef _USE_HX711
typedef struct
{
  bool is_open;
  bool is_ready;
  int32_t offset;
  float scale;
  uint16_t sample_count;
} loadcell_tbl_t;


static loadcell_tbl_t loadcell_tbl[HX711_MAX];

static bool loadcellIsValidCh(uint8_t ch);
static bool loadcellReadRawHw(uint8_t ch, int32_t *p_raw);
static bool loadcellReadAverageRaw(uint8_t ch, uint16_t sample_count, int32_t *p_raw);

bool loadcellInit(void)
{
  for(uint8_t i = 0; i < HX711_MAX; i++)
  {
    loadcell_tbl[i].is_open      = false;
    loadcell_tbl[i].is_ready     = false;
    loadcell_tbl[i].offset       = 0;
    loadcell_tbl[i].scale        = LOADCELL_DEFAULT_SCALE;
    loadcell_tbl[i].sample_count = LOADCELL_DEFAULT_SAMPLE_COUNT;
  }

  return true;
}

bool loadcellOpen(uint8_t ch)
{
  if(loadcellIsValidCh(ch) != true) return false;
  if(loadcell_tbl[ch].is_open == true) return true;

  loadcell_tbl[ch].is_open      = true;
  loadcell_tbl[ch].is_ready     = false;
  loadcell_tbl[ch].offset       = 0;
  loadcell_tbl[ch].scale        = LOADCELL_DEFAULT_SCALE;
  loadcell_tbl[ch].sample_count = LOADCELL_DEFAULT_SAMPLE_COUNT;

  return true;
}

bool loadcellIsOpen(uint8_t ch)
{
  if(loadcellIsValidCh(ch) != true) return false;

  return loadcell_tbl[ch].is_open;
}

bool loadcellIsReady(uint8_t ch)
{
  if(loadcellIsValidCh(ch) != true) return false;
  if(loadcell_tbl[ch].is_open != true) return false;

  return loadcell_tbl[ch].is_ready;
}

bool loadcellReadRaw(uint8_t ch, int32_t *p_raw)
{
  if(loadcellIsValidCh(ch) != true) return false;
  if(loadcell_tbl[ch].is_open != true) return false;

  return loadcellReadAverageRaw(ch, loadcell_tbl[ch].sample_count, p_raw);
}

bool loadcellReadGram(uint8_t ch, float *p_gram)
{
  int32_t raw;

  if(p_gram == NULL) return false;
  if(loadcellReadRaw(ch, &raw) != true) return false;
  if(loadcell_tbl[ch].scale == 0.0f) return false;

  *p_gram = (float)(raw - loadcell_tbl[ch].offset) / loadcell_tbl[ch].scale;

  return true;
}

bool loadcellReadData(uint8_t ch, loadcell_data_t *p_data)
{
  int32_t raw;

  if(p_data == NULL) return false;
  if(loadcellReadRaw(ch, &raw) != true) return false;
  if(loadcell_tbl[ch].scale == 0.0f) return false;

  p_data->raw    = raw;
  p_data->offset = loadcell_tbl[ch].offset;
  p_data->scale  = loadcell_tbl[ch].scale;
  p_data->gram   = (float)(raw - loadcell_tbl[ch].offset) / loadcell_tbl[ch].scale;

  return true;
}

bool loadcellTare(uint8_t ch, uint16_t sample_count)
{
  int32_t raw;

  if(loadcellIsValidCh(ch) != true) return false;
  if(loadcell_tbl[ch].is_open != true) return false;
  if(sample_count == 0U) return false;

  if(loadcellReadAverageRaw(ch, sample_count, &raw) != true) return false;

  loadcell_tbl[ch].offset = raw;

  return true;
}

bool loadcellSetOffset(uint8_t ch, int32_t offset)
{
  if(loadcellIsValidCh(ch) != true) return false;
  if(loadcell_tbl[ch].is_open != true) return false;

  loadcell_tbl[ch].offset = offset;

  return true;
}

int32_t loadcellGetOffset(uint8_t ch)
{
  if(loadcellIsValidCh(ch) != true) return 0;

  return loadcell_tbl[ch].offset;
}

bool loadcellSetScale(uint8_t ch, float scale)
{
  if(loadcellIsValidCh(ch) != true) return false;
  if(loadcell_tbl[ch].is_open != true) return false;
  if(scale == 0.0f) return false;

  loadcell_tbl[ch].scale = scale;

  return true;
}

float loadcellGetScale(uint8_t ch)
{
  if(loadcellIsValidCh(ch) != true) return 0.0f;

  return loadcell_tbl[ch].scale;
}

bool loadcellSetSampleCount(uint8_t ch, uint16_t sample_count)
{
  if(loadcellIsValidCh(ch) != true) return false;
  if(sample_count == 0U) return false;

  loadcell_tbl[ch].sample_count = sample_count;

  return true;
}

uint16_t loadcellGetSampleCount(uint8_t ch)
{
  if(loadcellIsValidCh(ch) != true) return 0U;

  return loadcell_tbl[ch].sample_count;
}

static bool loadcellIsValidCh(uint8_t ch)
{
  return ch < HX711_MAX;
}

static bool loadcellReadRawHw(uint8_t ch, int32_t *p_raw)
{
  (void)ch;

  if(p_raw == NULL) return false;

  /*
   * Hardware access point:
   * - Wait until the loadcell ADC is ready.
   * - Read one signed raw ADC sample.
   * - Store it in *p_raw and return true.
   */

  return false;
}

static bool loadcellReadAverageRaw(uint8_t ch, uint16_t sample_count, int32_t *p_raw)
{
  int64_t sum = 0;
  int32_t raw;

  if(p_raw == NULL) return false;
  if(sample_count == 0U) return false;

  for(uint16_t i = 0; i < sample_count; i++)
  {
    if(loadcellReadRawHw(ch, &raw) != true)
    {
      return false;
    }

    sum += raw;
  }

  *p_raw = (int32_t)(sum / sample_count);

  return true;
}
#endif
