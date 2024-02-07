/*
 *  Copyright (c) 2016 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>
#include <assert.h>

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "vpx/vpx_integer.h"
#include "vpx_dsp/arm/mem_neon.h"
#include "vpx_dsp/arm/transpose_neon.h"
#include "vpx_ports/mem.h"

static INLINE int32x4_t highbd_convolve8_4(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x8_t filters) {
  const int16x4_t filters_lo = vget_low_s16(filters);
  const int16x4_t filters_hi = vget_high_s16(filters);
  int32x4_t sum;

  sum = vmull_lane_s16(s0, filters_lo, 0);
  sum = vmlal_lane_s16(sum, s1, filters_lo, 1);
  sum = vmlal_lane_s16(sum, s2, filters_lo, 2);
  sum = vmlal_lane_s16(sum, s3, filters_lo, 3);
  sum = vmlal_lane_s16(sum, s4, filters_hi, 0);
  sum = vmlal_lane_s16(sum, s5, filters_hi, 1);
  sum = vmlal_lane_s16(sum, s6, filters_hi, 2);
  sum = vmlal_lane_s16(sum, s7, filters_hi, 3);
  return sum;
}

static INLINE uint16x8_t
highbd_convolve8_8(const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
                   const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
                   const int16x8_t s6, const int16x8_t s7,
                   const int16x8_t filters, const uint16x8_t max) {
  const int16x4_t filters_lo = vget_low_s16(filters);
  const int16x4_t filters_hi = vget_high_s16(filters);
  int32x4_t sum0, sum1;
  uint16x8_t d;

  sum0 = vmull_lane_s16(vget_low_s16(s0), filters_lo, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), filters_lo, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), filters_lo, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), filters_lo, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), filters_hi, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), filters_hi, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s6), filters_hi, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s7), filters_hi, 3);
  sum1 = vmull_lane_s16(vget_high_s16(s0), filters_lo, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), filters_lo, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), filters_lo, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), filters_lo, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), filters_hi, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), filters_hi, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s6), filters_hi, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s7), filters_hi, 3);
  d = vcombine_u16(vqrshrun_n_s32(sum0, 7), vqrshrun_n_s32(sum1, 7));
  d = vminq_u16(d, max);
  return d;
}

void vpx_highbd_convolve8_horiz_neon(const uint16_t *src, ptrdiff_t src_stride,
                                     uint16_t *dst, ptrdiff_t dst_stride,
                                     const InterpKernel *filter, int x0_q4,
                                     int x_step_q4, int y0_q4, int y_step_q4,
                                     int w, int h, int bd) {
  if (x_step_q4 != 16) {
    vpx_highbd_convolve8_horiz_c(src, src_stride, dst, dst_stride, filter,
                                 x0_q4, x_step_q4, y0_q4, y_step_q4, w, h, bd);
    return;
  }

  assert((intptr_t)dst % 4 == 0);
  assert(dst_stride % 4 == 0);

  const int16x8_t filters = vld1q_s16(filter[x0_q4]);
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

  src -= 3;

  if (w == 4) {
    const int16_t *s = (const int16_t *)src;
    uint16_t *d = dst;

    do {
      int16x4_t s0[8], s1[8], s2[8], s3[8];
      load_s16_4x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                   &s0[4], &s0[5], &s0[6], &s0[7]);
      load_s16_4x8(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                   &s1[4], &s1[5], &s1[6], &s1[7]);
      load_s16_4x8(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                   &s2[4], &s2[5], &s2[6], &s2[7]);
      load_s16_4x8(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                   &s3[4], &s3[5], &s3[6], &s3[7]);

      int32x4_t d0 = highbd_convolve8_4(s0[0], s0[1], s0[2], s0[3], s0[4],
                                        s0[5], s0[6], s0[7], filters);
      int32x4_t d1 = highbd_convolve8_4(s1[0], s1[1], s1[2], s1[3], s1[4],
                                        s1[5], s1[6], s1[7], filters);
      int32x4_t d2 = highbd_convolve8_4(s2[0], s2[1], s2[2], s2[3], s2[4],
                                        s2[5], s2[6], s2[7], filters);
      int32x4_t d3 = highbd_convolve8_4(s3[0], s3[1], s3[2], s3[3], s3[4],
                                        s3[5], s3[6], s3[7], filters);
      uint16x8_t d01 =
          vcombine_u16(vqrshrun_n_s32(d0, 7), vqrshrun_n_s32(d1, 7));
      uint16x8_t d23 =
          vcombine_u16(vqrshrun_n_s32(d2, 7), vqrshrun_n_s32(d3, 7));

      d01 = vminq_u16(d01, max);
      d23 = vminq_u16(d23, max);

      store_u16_4x4(d, dst_stride, vget_low_u16(d01), vget_high_u16(d01),
                    vget_low_u16(d23), vget_high_u16(d23));

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  } else {
    do {
      const int16_t *s = (const int16_t *)src;
      uint16_t *d = dst;
      int width = w;

      do {
        int16x8_t s0[8], s1[8], s2[8], s3[8];
        load_s16_8x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                     &s0[4], &s0[5], &s0[6], &s0[7]);
        load_s16_8x8(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                     &s1[4], &s1[5], &s1[6], &s1[7]);
        load_s16_8x8(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                     &s2[4], &s2[5], &s2[6], &s2[7]);
        load_s16_8x8(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                     &s3[4], &s3[5], &s3[6], &s3[7]);

        uint16x8_t d0 = highbd_convolve8_8(s0[0], s0[1], s0[2], s0[3], s0[4],
                                           s0[5], s0[6], s0[7], filters, max);
        uint16x8_t d1 = highbd_convolve8_8(s1[0], s1[1], s1[2], s1[3], s1[4],
                                           s1[5], s1[6], s1[7], filters, max);
        uint16x8_t d2 = highbd_convolve8_8(s2[0], s2[1], s2[2], s2[3], s2[4],
                                           s2[5], s2[6], s2[7], filters, max);
        uint16x8_t d3 = highbd_convolve8_8(s3[0], s3[1], s3[2], s3[3], s3[4],
                                           s3[5], s3[6], s3[7], filters, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  }
}

void vpx_highbd_convolve8_avg_horiz_neon(const uint16_t *src,
                                         ptrdiff_t src_stride, uint16_t *dst,
                                         ptrdiff_t dst_stride,
                                         const InterpKernel *filter, int x0_q4,
                                         int x_step_q4, int y0_q4,
                                         int y_step_q4, int w, int h, int bd) {
  if (x_step_q4 != 16) {
    vpx_highbd_convolve8_avg_horiz_c(src, src_stride, dst, dst_stride, filter,
                                     x0_q4, x_step_q4, y0_q4, y_step_q4, w, h,
                                     bd);
    return;
  }

  assert((intptr_t)dst % 4 == 0);
  assert(dst_stride % 4 == 0);

  const int16x8_t filters = vld1q_s16(filter[x0_q4]);
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

  src -= 3;

  if (w == 4) {
    const int16_t *s = (const int16_t *)src;
    uint16_t *d = dst;

    do {
      int16x4_t s0[8], s1[8], s2[8], s3[8];
      load_s16_4x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                   &s0[4], &s0[5], &s0[6], &s0[7]);
      load_s16_4x8(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                   &s1[4], &s1[5], &s1[6], &s1[7]);
      load_s16_4x8(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                   &s2[4], &s2[5], &s2[6], &s2[7]);
      load_s16_4x8(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                   &s3[4], &s3[5], &s3[6], &s3[7]);

      int32x4_t d0 = highbd_convolve8_4(s0[0], s0[1], s0[2], s0[3], s0[4],
                                        s0[5], s0[6], s0[7], filters);
      int32x4_t d1 = highbd_convolve8_4(s1[0], s1[1], s1[2], s1[3], s1[4],
                                        s1[5], s1[6], s1[7], filters);
      int32x4_t d2 = highbd_convolve8_4(s2[0], s2[1], s2[2], s2[3], s2[4],
                                        s2[5], s2[6], s2[7], filters);
      int32x4_t d3 = highbd_convolve8_4(s3[0], s3[1], s3[2], s3[3], s3[4],
                                        s3[5], s3[6], s3[7], filters);
      uint16x8_t d01 =
          vcombine_u16(vqrshrun_n_s32(d0, 7), vqrshrun_n_s32(d1, 7));
      uint16x8_t d23 =
          vcombine_u16(vqrshrun_n_s32(d2, 7), vqrshrun_n_s32(d3, 7));

      d01 = vminq_u16(d01, max);
      d23 = vminq_u16(d23, max);

      uint16x8_t dd01 = vcombine_u16(vld1_u16(d + 0 * dst_stride),
                                     vld1_u16(d + 1 * dst_stride));
      uint16x8_t dd23 = vcombine_u16(vld1_u16(d + 2 * dst_stride),
                                     vld1_u16(d + 3 * dst_stride));
      d01 = vrhaddq_u16(d01, dd01);
      d23 = vrhaddq_u16(d23, dd23);

      store_u16_4x4(d, dst_stride, vget_low_u16(d01), vget_high_u16(d01),
                    vget_low_u16(d23), vget_high_u16(d23));

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  } else {
    do {
      const int16_t *s = (const int16_t *)src;
      uint16_t *d = dst;
      int width = w;

      do {
        int16x8_t s0[8], s1[8], s2[8], s3[8];
        load_s16_8x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                     &s0[4], &s0[5], &s0[6], &s0[7]);
        load_s16_8x8(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                     &s1[4], &s1[5], &s1[6], &s1[7]);
        load_s16_8x8(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                     &s2[4], &s2[5], &s2[6], &s2[7]);
        load_s16_8x8(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                     &s3[4], &s3[5], &s3[6], &s3[7]);

        uint16x8_t d0 = highbd_convolve8_8(s0[0], s0[1], s0[2], s0[3], s0[4],
                                           s0[5], s0[6], s0[7], filters, max);
        uint16x8_t d1 = highbd_convolve8_8(s1[0], s1[1], s1[2], s1[3], s1[4],
                                           s1[5], s1[6], s1[7], filters, max);
        uint16x8_t d2 = highbd_convolve8_8(s2[0], s2[1], s2[2], s2[3], s2[4],
                                           s2[5], s2[6], s2[7], filters, max);
        uint16x8_t d3 = highbd_convolve8_8(s3[0], s3[1], s3[2], s3[3], s3[4],
                                           s3[5], s3[6], s3[7], filters, max);

        d0 = vrhaddq_u16(d0, vld1q_u16(d + 0 * dst_stride));
        d1 = vrhaddq_u16(d1, vld1q_u16(d + 1 * dst_stride));
        d2 = vrhaddq_u16(d2, vld1q_u16(d + 2 * dst_stride));
        d3 = vrhaddq_u16(d3, vld1q_u16(d + 3 * dst_stride));

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  }
}

void vpx_highbd_convolve8_vert_neon(const uint16_t *src, ptrdiff_t src_stride,
                                    uint16_t *dst, ptrdiff_t dst_stride,
                                    const InterpKernel *filter, int x0_q4,
                                    int x_step_q4, int y0_q4, int y_step_q4,
                                    int w, int h, int bd) {
  if (y_step_q4 != 16) {
    vpx_highbd_convolve8_vert_c(src, src_stride, dst, dst_stride, filter, x0_q4,
                                x_step_q4, y0_q4, y_step_q4, w, h, bd);
    return;
  }

  assert((intptr_t)dst % 4 == 0);
  assert(dst_stride % 4 == 0);

  const int16x8_t filters = vld1q_s16(filter[y0_q4]);
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

  src -= 3 * src_stride;

  if (w == 4) {
    const int16_t *s = (const int16_t *)src;
    uint16_t *d = dst;

    int16x4_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);

    s += 7 * src_stride;

    do {
      int16x4_t s7, s8, s9, s10;
      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &s10);

      int32x4_t d0 =
          highbd_convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7, filters);
      int32x4_t d1 =
          highbd_convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8, filters);
      int32x4_t d2 =
          highbd_convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9, filters);
      int32x4_t d3 =
          highbd_convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10, filters);
      uint16x8_t d01 =
          vcombine_u16(vqrshrun_n_s32(d0, 7), vqrshrun_n_s32(d1, 7));
      uint16x8_t d23 =
          vcombine_u16(vqrshrun_n_s32(d2, 7), vqrshrun_n_s32(d3, 7));

      d01 = vminq_u16(d01, max);
      d23 = vminq_u16(d23, max);

      store_u16_4x4(d, dst_stride, vget_low_u16(d01), vget_high_u16(d01),
                    vget_low_u16(d23), vget_high_u16(d23));

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      const int16_t *s = (const int16_t *)src;
      uint16_t *d = dst;
      int height = h;

      int16x8_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);

      s += 7 * src_stride;

      do {
        int16x8_t s7, s8, s9, s10;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10);

        uint16x8_t d0 =
            highbd_convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters, max);
        uint16x8_t d1 =
            highbd_convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters, max);
        uint16x8_t d2 =
            highbd_convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters, max);
        uint16x8_t d3 =
            highbd_convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);
      src += 8;
      dst += 8;
      w -= 8;
    } while (w != 0);
  }
}

void vpx_highbd_convolve8_avg_vert_neon(const uint16_t *src,
                                        ptrdiff_t src_stride, uint16_t *dst,
                                        ptrdiff_t dst_stride,
                                        const InterpKernel *filter, int x0_q4,
                                        int x_step_q4, int y0_q4, int y_step_q4,
                                        int w, int h, int bd) {
  if (y_step_q4 != 16) {
    vpx_highbd_convolve8_avg_vert_c(src, src_stride, dst, dst_stride, filter,
                                    x0_q4, x_step_q4, y0_q4, y_step_q4, w, h,
                                    bd);
    return;
  }

  assert((intptr_t)dst % 4 == 0);
  assert(dst_stride % 4 == 0);

  const int16x8_t filters = vld1q_s16(filter[y0_q4]);
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

  src -= 3 * src_stride;

  if (w == 4) {
    const int16_t *s = (const int16_t *)src;
    uint16_t *d = dst;

    int16x4_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);

    s += 7 * src_stride;

    do {
      int16x4_t s7, s8, s9, s10;
      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &s10);

      int32x4_t d0 =
          highbd_convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7, filters);
      int32x4_t d1 =
          highbd_convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8, filters);
      int32x4_t d2 =
          highbd_convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9, filters);
      int32x4_t d3 =
          highbd_convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10, filters);
      uint16x8_t d01 =
          vcombine_u16(vqrshrun_n_s32(d0, 7), vqrshrun_n_s32(d1, 7));
      uint16x8_t d23 =
          vcombine_u16(vqrshrun_n_s32(d2, 7), vqrshrun_n_s32(d3, 7));

      d01 = vminq_u16(d01, max);
      d23 = vminq_u16(d23, max);

      uint16x8_t dd01 = vcombine_u16(vld1_u16(d + 0 * dst_stride),
                                     vld1_u16(d + 1 * dst_stride));
      uint16x8_t dd23 = vcombine_u16(vld1_u16(d + 2 * dst_stride),
                                     vld1_u16(d + 3 * dst_stride));
      d01 = vrhaddq_u16(d01, dd01);
      d23 = vrhaddq_u16(d23, dd23);

      store_u16_4x4(d, dst_stride, vget_low_u16(d01), vget_high_u16(d01),
                    vget_low_u16(d23), vget_high_u16(d23));

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      const int16_t *s = (const int16_t *)src;
      uint16_t *d = dst;
      int height = h;

      int16x8_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);

      s += 7 * src_stride;

      do {
        int16x8_t s7, s8, s9, s10;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10);

        uint16x8_t d0 =
            highbd_convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters, max);
        uint16x8_t d1 =
            highbd_convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters, max);
        uint16x8_t d2 =
            highbd_convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters, max);
        uint16x8_t d3 =
            highbd_convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters, max);

        d0 = vrhaddq_u16(d0, vld1q_u16(d + 0 * dst_stride));
        d1 = vrhaddq_u16(d1, vld1q_u16(d + 1 * dst_stride));
        d2 = vrhaddq_u16(d2, vld1q_u16(d + 2 * dst_stride));
        d3 = vrhaddq_u16(d3, vld1q_u16(d + 3 * dst_stride));

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);
      src += 8;
      dst += 8;
      w -= 8;
    } while (w != 0);
  }
}
