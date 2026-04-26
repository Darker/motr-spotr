#pragma once
#include <stdint.h>

#include <stddef.h>


constexpr size_t ADC_BUFFER_LEN = 2048;

constexpr size_t ADC_BUFFER_SIZE_BYTES = ADC_BUFFER_LEN * sizeof(uint16_t);
// a sample is 2 bytes, and we want half of the buffer, so /2 in total
constexpr size_t ADC_BUFFER_HALF_SIZE_BYTES = ADC_BUFFER_SIZE_BYTES / (2);
// 2 bytes per sample, so num samples is bytes / 2
constexpr size_t ADC_BUFFER_HALF_SIZE_SAMPLES = ADC_BUFFER_HALF_SIZE_BYTES / 2;
// Each sample takes 1.5 bytes, or 3/2
constexpr size_t ADC_PACKED_CHUNK_LEN = (ADC_BUFFER_HALF_SIZE_SAMPLES * 3) / 2;
constexpr uint8_t MAGIC_BEGIN[] = {0xDE, 0xAD, 0xBE, 0xEF};
constexpr uint8_t MAGIC_END[] = {0x00, 0xDA, 0x7A};
// data len is uint32 which describes size AFTER the header
constexpr size_t HEADER_SIZE_LEN = 4;

constexpr size_t HEADER_OFFSET_DATA_LEN = sizeof(MAGIC_BEGIN);
constexpr size_t HEADER_OFFSET_DATA = HEADER_OFFSET_DATA_LEN + HEADER_SIZE_LEN;
constexpr size_t HEADER_OFFSET_MAGIC_END = HEADER_OFFSET_DATA + ADC_PACKED_CHUNK_LEN;
constexpr size_t USB_BUFFER_LEN = sizeof(MAGIC_BEGIN) + HEADER_SIZE_LEN + ADC_PACKED_CHUNK_LEN + sizeof(MAGIC_END);