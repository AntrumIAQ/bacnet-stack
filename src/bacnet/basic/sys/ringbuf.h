/**
 * @file
 * @brief API for a ring buffer library for deeply embedded system.
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date 2004
 * @copyright SPDX-License-Identifier: MIT
 */
#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>
#include <stdbool.h>
/* BACnet Stack defines - first */
#include "bacnet/bacdef.h"

/**
 * ring buffer power of two alignment macro
 *
 * @{
 */
#ifndef NEXT_POWER_OF_2
/* clang-format off */
#define B2(x)    (   (x) | (   (x) >> 1) )
#define B4(x)    ( B2(x) | ( B2(x) >> 2) )
#define B8(x)    ( B4(x) | ( B4(x) >> 4) )
#define B16(x)   ( B8(x) | ( B8(x) >> 8) )
#define B32(x)   (B16(x) | (B16(x) >>16) )
#define NEXT_POWER_OF_2(x) (B32((x)-1) + 1)
/* clang-format on */
#endif
/** @} */

/**
 * ring buffer data structure
 *
 * @{
 */
struct ring_buffer_t {
    /** block of memory or array of data */
    volatile uint8_t *buffer;
    /** how many bytes for each chunk */
    unsigned element_size;
    /** number of chunks of data */
    unsigned element_count;
    /** where the writes go */
    volatile unsigned head;
    /** where the reads come from */
    volatile unsigned tail;
    /* maximum depth reached */
    volatile unsigned depth;
};
typedef struct ring_buffer_t RING_BUFFER;
/** @} */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

BACNET_STACK_EXPORT
unsigned Ringbuf_Count(RING_BUFFER const *b);
BACNET_STACK_EXPORT
unsigned Ringbuf_Depth(RING_BUFFER const *b);
BACNET_STACK_EXPORT
unsigned Ringbuf_Depth_Reset(RING_BUFFER *b);
BACNET_STACK_EXPORT
unsigned Ringbuf_Size(RING_BUFFER const *b);
BACNET_STACK_EXPORT
bool Ringbuf_Full(RING_BUFFER const *b);
BACNET_STACK_EXPORT
bool Ringbuf_Empty(RING_BUFFER const *b);
/* tail */
BACNET_STACK_EXPORT
volatile void *Ringbuf_Peek(RING_BUFFER const *b);
BACNET_STACK_EXPORT
bool Ringbuf_Pop(RING_BUFFER *b, uint8_t *data_element);
BACNET_STACK_EXPORT
bool Ringbuf_Pop_Element(
    RING_BUFFER *b, const uint8_t *this_element, uint8_t *data_element);
BACNET_STACK_EXPORT
bool Ringbuf_Put_Front(RING_BUFFER *b, const uint8_t *data_element);
/* head */
BACNET_STACK_EXPORT
bool Ringbuf_Put(RING_BUFFER *b, const uint8_t *data_element);
/* pair of functions to use head memory directly */
BACNET_STACK_EXPORT
volatile void *Ringbuf_Data_Peek(RING_BUFFER *b);
BACNET_STACK_EXPORT
volatile void *
Ringbuf_Peek_Next(RING_BUFFER const *b, const uint8_t *data_element);
BACNET_STACK_EXPORT
bool Ringbuf_Data_Put(RING_BUFFER *b, const volatile uint8_t *data_element);
BACNET_STACK_EXPORT
unsigned Ringbuf_Data_Size(RING_BUFFER const *b);
/* Note: element_count must be a power of two */
BACNET_STACK_EXPORT
bool Ringbuf_Init(
    RING_BUFFER *b,
    volatile uint8_t *buffer,
    unsigned element_size,
    unsigned element_count);
BACNET_STACK_EXPORT
bool Ringbuf_Initialize(
    RING_BUFFER *b,
    volatile uint8_t *buffer,
    unsigned buffer_size,
    unsigned element_size,
    unsigned element_count);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
