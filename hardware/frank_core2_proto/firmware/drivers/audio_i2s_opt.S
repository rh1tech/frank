/*
 * frank-msx — fMSX for RP2350
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-msx
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * ARM Assembly optimized I2S audio volume processing
 * Step 4: Optimize volume shift loop in i2s_dma_write
 * 
 * Target: RP2350 Cortex-M33 @ 378-504 MHz
 */

    .syntax unified
    .cpu cortex-m33
    .thumb
    .section .time_critical.audio_i2s,"ax",%progbits
    .align 2

/*
 * Apply volume shift to packed stereo samples
 * void audio_volume_shift(uint32_t* dst, const uint32_t* src, 
 *                         uint32_t frames, uint32_t shift)
 * 
 * Args:
 *   r0 = dst (output uint32_t packed stereo)
 *   r1 = src (input uint32_t packed stereo)
 *   r2 = frames (number of stereo frames)
 *   r3 = shift (right shift amount for volume)
 * 
 * Original C code (from audio.c lines 101-109):
 *   for (uint32_t i = 0; i < frames; i++) {
 *     uint32_t v = src[i];
 *     int16_t l = (int16_t)(v >> 16);
 *     int16_t r = (int16_t)(v & 0xFFFF);
 *     l >>= shift;
 *     r >>= shift;
 *     dst[i] = ((uint32_t)(uint16_t)l << 16) | (uint16_t)r;
 *   }
 */
    .global audio_volume_shift
    .type audio_volume_shift, %function
    .thumb_func
audio_volume_shift:
    push    {r4-r7, lr}
    
    // Early exit if frames == 0
    cmp     r2, #0
    beq     .Lvol_done
    
    // Process 4 frames per iteration when possible
    cmp     r2, #4
    blt     .Lvol_remainder
    
.Lvol_loop_4x:
    // === Frame 0 ===
    ldr     r4, [r1], #4        // Load packed stereo
    asr     r5, r4, #16         // r5 = left (sign-extended)
    sxth    r6, r4              // r6 = right (sign-extended)
    asr     r5, r5, r3          // Apply volume shift to left
    asr     r6, r6, r3          // Apply volume shift to right
    lsl     r5, r5, #16         // left << 16
    uxth    r6, r6              // Zero-extend right to 16 bits
    orr     r5, r5, r6          // Combine
    str     r5, [r0], #4        // Store
    
    // === Frame 1 ===
    ldr     r4, [r1], #4
    asr     r5, r4, #16
    sxth    r6, r4
    asr     r5, r5, r3
    asr     r6, r6, r3
    lsl     r5, r5, #16
    uxth    r6, r6
    orr     r5, r5, r6
    str     r5, [r0], #4
    
    // === Frame 2 ===
    ldr     r4, [r1], #4
    asr     r5, r4, #16
    sxth    r6, r4
    asr     r5, r5, r3
    asr     r6, r6, r3
    lsl     r5, r5, #16
    uxth    r6, r6
    orr     r5, r5, r6
    str     r5, [r0], #4
    
    // === Frame 3 ===
    ldr     r4, [r1], #4
    asr     r5, r4, #16
    sxth    r6, r4
    asr     r5, r5, r3
    asr     r6, r6, r3
    lsl     r5, r5, #16
    uxth    r6, r6
    orr     r5, r5, r6
    str     r5, [r0], #4
    
    subs    r2, r2, #4          // frames -= 4
    cmp     r2, #4
    bge     .Lvol_loop_4x
    
.Lvol_remainder:
    // Handle remaining frames (< 4)
    cmp     r2, #0
    beq     .Lvol_done
    
.Lvol_loop_1x:
    ldr     r4, [r1], #4        // Load packed stereo
    asr     r5, r4, #16         // r5 = left (sign-extended)
    sxth    r6, r4              // r6 = right (sign-extended)
    asr     r5, r5, r3          // Apply volume shift
    asr     r6, r6, r3
    lsl     r5, r5, #16         // Pack back
    uxth    r6, r6
    orr     r5, r5, r6
    str     r5, [r0], #4
    
    subs    r2, r2, #1
    bne     .Lvol_loop_1x
    
.Lvol_done:
    pop     {r4-r7, pc}
    
    .size audio_volume_shift, .-audio_volume_shift
