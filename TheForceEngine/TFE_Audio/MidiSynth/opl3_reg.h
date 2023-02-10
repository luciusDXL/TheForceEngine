/*
 * Copyright by Hannu Savolainen 1993-1996
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

 // modified for TFE
 // heavily modified for audiolib at https://github.com/nukeykt/NBlood
 // original definitions found at http://www.cs.albany.edu/~sdc/Linux/linux-2.0/drivers/sound/opl3.h
 // it's from old Linux source but the license is pretty clearly 2-clause BSD.
#pragma once

#define OPL3_TEST_REGISTER                  0x01
#define     OPL3_ENABLE_WAVE_SELECT         0x20

#define OPL3_TIMER1_REGISTER                0x02
#define OPL3_TIMER2_REGISTER                0x03
#define OPL3_TIMER_CONTROL_REGISTER         0x04 // Left side
#define     OPL3_IRQ_RESET                  0x80
#define     OPL3_TIMER1_MASK                0x40
#define     OPL3_TIMER2_MASK                0x20
#define     OPL3_TIMER1_START               0x01
#define     OPL3_TIMER2_START               0x02

#define OPL3_CONNECTION_SELECT_REGISTER     0x04 // Right side
#define     OPL3_RIGHT_4OP_0                0x01
#define     OPL3_RIGHT_4OP_1                0x02
#define     OPL3_RIGHT_4OP_2                0x04
#define     OPL3_LEFT_4OP_0                 0x08
#define     OPL3_LEFT_4OP_1                 0x10
#define     OPL3_LEFT_4OP_2                 0x20

#define OPL3_MODE_REGISTER                  0x05 // Right side
#define     OPL3_ENABLE                     0x01
#define     OPL3_OPL4_ENABLE                0x02

#define OPL3_KBD_SPLIT_REGISTER             0x08 // Left side
#define     OPL3_COMPOSITE_SINE_WAVE_MODE   0x80
#define     OPL3_KEYBOARD_SPLIT             0x40

#define OPL3_PERCUSSION_REGISTER            0xbd // Left side only
#define     OPL3_TREMOLO_DEPTH              0x80
#define     OPL3_VIBRATO_DEPTH              0x40
#define     OPL3_PERCUSSION_ENABLE          0x20
#define     OPL3_BASSDRUM_ON                0x10
#define     OPL3_SNAREDRUM_ON               0x08
#define     OPL3_TOMTOM_ON                  0x04
#define     OPL3_CYMBAL_ON                  0x02
#define     OPL3_HIHAT_ON                   0x01

// Offsets to the register banks for operators. To get the
// register number just add the operator offset to the bank offset
// AM/VIB/EG/KSR/Multiple (0x20 to 0x35)
#define OPL3_AM_VIB                         0x20
#define     OPL3_TREMOLO_ON                 0x80
#define     OPL3_VIBRATO_ON                 0x40
#define     OPL3_SUSTAIN_ON                 0x20
#define     OPL3_KSR                        0x10 // Key scaling rate
#define     OPL3_MULTIPLE_MASK              0x0f // Frequency multiplier

// KSL/Total level (0x40 to 0x55)
#define OPL3_KSL_LEVEL                      0x40
#define     OPL3_KSL_MASK                   0xc0 // Envelope scaling bits
#define     OPL3_TOTAL_LEVEL_MASK           0x3f // Strength (volume) of OP

// Attack / Decay rate (0x60 to 0x75)
#define OPL3_ATTACK_DECAY                   0x60
#define     OPL3_ATTACK_MASK                0xf0
#define     OPL3_DECAY_MASK                 0x0f

// Sustain level / Release rate (0x80 to 0x95)
#define OPL3_SUSTAIN_RELEASE                0x80
#define     OPL3_SUSTAIN_MASK               0xf0
#define     OPL3_RELEASE_MASK               0x0f

// Wave select (0xE0 to 0xF5)
#define OPL3_WAVE_SELECT                    0xe0

// Offsets to the register banks for voices. Just add to the
// voice number to get the register number.
// F-Number low bits (0xA0 to 0xA8).
#define OPL3_FNUM_LOW                       0xa0

// F-number high bits / Key on / Block (octave) (0xB0 to 0xB8)
#define OPL3_KEYON_BLOCK                    0xb0
#define     OPL3_KEYON_BIT                  0x20
#define     OPL3_BLOCKNUM_MASK              0x1c
#define     OPL3_FNUM_HIGH_MASK             0x03

// Feedback / Connection (0xc0 to 0xc8)
// 
// These registers have two new bits when the OPL-3 mode
// is selected. These bits controls connecting the voice
// to the stereo channels. For 4 OP voices this bit is
// defined in the second half of the voice (add 3 to the
// register offset).
// 
// For 4 OP voices the connection bit is used in the
// both halfs (gives 4 ways to connect the operators).
#define OPL3_FEEDBACK_CONNECTION            0xc0
#define     OPL3_FEEDBACK_MASK              0x0e // Valid just for 1st OP of a voice
#define     OPL3_CONNECTION_BIT             0x01
#define     OPL3_STEREO_BITS                0x30 // OPL-3 only
#define         OPL3_VOICE_TO_LEFT          0x10
#define         OPL3_VOICE_TO_RIGHT         0x20