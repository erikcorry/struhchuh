// Copyright 2018 Erik Corry.  See the LICENSE file, the BSD 2-clause
// license.


// Various routines for searching strings for characters.  Since these
// routines were written with a JIT compiler in mind, they assume the
// string being searched for is a compile-time constant.  The length
// is given, and null bytes are ignored (may occur in the strings).

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <emmintrin.h>

#include "search.h"

// Search for a single asterisk by stepping through the string.
int test_naive(const char* s, int len) {
  for (int i = 0; i < len; i++) {
    if (s[i] == '*') {
      return i;
    }
  }
  return -127;
}

// Search for *# by stepping through the string.
int test_twobyte(const char* s, int len) {
  len--;
  for (int i = 0; i < len; i++) {
    if (s[i] == '*' && s[i + 1] == '#') {
      return i;
    }
  }
  return -127;
}

typedef __m128i uint128_t;

// Search for "*" using only aligned SSE2 128 bit loads. This may load data
// either side of the string, but can never cause a fault because the loads are
// in 128 bit sections also covered by the string.
// Use this algorithm if you have SSE or equivalent and you are searching for a
// single character.
int test_pure_sse2(const char* s, int len) {
  int last_bits = (uintptr_t)s & 15;
  int alignment_mask = 0xffff << last_bits;
  const uint128_t mask = _mm_set1_epi8('*');
  for (int i = -last_bits ; i < len; i += 16) {
    // Load aligned to a 128 bit XMM2 register.
    uint128_t raw = *(uint128_t*)(s + i);
    // Puts 0xff or 0x00 in the corresponding bytes depending on whether the
    // bytes in the input are equal. PCMPEQB.
    uint128_t comparison = _mm_cmpeq_epi8(raw, mask);
    // Takes the top bit of each byte and puts it in the corresponding bit of a
    // normal integer.  PMOVMSKB.
    int bits = _mm_movemask_epi8(comparison) & alignment_mask;
    if (bits) {
      int answer = i + __builtin_ffs(bits) - 1;
      if (answer >= len) return -127;
      return answer;
    }
    alignment_mask = 0xffff;
  }
  return -127;
}

// Search for "*" by starting with at least one byte load and switching to
// aligned SSE2 128 bit loads when an aligned address is reached. This may load
// data after the end of the string, but can never cause a fault because the
// loads are in 128 bit sections also covered by the string.
int test_sse2(const char* s, int len) {
  int i = 0;
  while (i < len) {
    if (s[i] == '*') {
      return i;
    }
    i++;
    if (((uintptr_t)(s + i) & 15) == 0) break;
  }
  if (i >= len) return -127;
  const uint128_t mask = _mm_set1_epi8('*');
  for ( ; i < len; i += 16) {
    // Load aligned to a 128 bit XMM2 register.
    uint128_t raw = *(uint128_t*)(s + i);
    // Puts 0xff or 0x00 in the corresponding bytes depending on whether the
    // bytes in the input are equal. PCMPEQB.
    uint128_t comparison = _mm_cmpeq_epi8(raw, mask);
    // Takes the top bit of each byte and puts it in the corresponding bit of a
    // normal integer.  PMOVMSKB.
    int bits = _mm_movemask_epi8(comparison);
    if (bits) {
      int answer = i + __builtin_ffs(bits) - 1;
      if (answer >= len) return -127;
      return answer;
    }
  }
  return -127;
}

// Search for "*" by starting with at least one byte load and switching to
// aligned 4-byte loads when 4-byte alignment is reached.  Then when 16 byte
// alignment is reached, it switches again to aligned SSE2 128 bit loads. This
// may load data after the end of the string, but can never cause a fault
// because the loads are in 32 bit or 128 bit sections also covered by the
// string.
int test_sse2_and_mycroft4(const char* s, int len) {
  int i = 0;
  while (i < len) {
    if (s[i] == '*') {
      return i;
    }
    i++;
    if (((uintptr_t)(s + i) & 3) == 0) break;
  }
  if (i >= len) return -127;
  const uint32_t mask32 = 0x2a2a2a2aul;
  const uint32_t highs = 0x80808080ul;
  const uint32_t ones = 0x01010101ul;
  while (i < len) {
    uint32_t raw = *(uint32_t*)(s + i) ^ mask32;
    raw = (raw - ones) & (~raw) & highs;
    if (raw) {
      int answer = i + (__builtin_ffs(raw) >> 3) - 1;
      if (answer >= len) return -127;
      return answer;
    }
    i += 4;
    if (((uintptr_t)(s + i) & 15) == 0) break;
  }
  if (i >= len) return -127;
  const uint128_t mask = _mm_set1_epi8('*');
  for ( ; i < len; i += 16) {
    // Load aligned to a 128 bit XMM2 register.
    uint128_t raw = *(uint128_t*)(s + i);
    // Puts 0xff or 0x00 in the corresponding bytes depending on whether the
    // bytes in the input are equal. PCMPEQB.
    uint128_t comparison = _mm_cmpeq_epi8(raw, mask);
    // Takes the top bit of each byte and puts it in the corresponding bit of a
    // normal integer.  PMOVMSKB.
    int bits = _mm_movemask_epi8(comparison);
    if (bits) {
      int answer = i + __builtin_ffs(bits) - 1;
      if (answer >= len) return -127;
      return answer;
    }
  }
  return -127;
}

// Search for "*#" by starting with at least one byte load and switching to
// aligned SSE2 128 bit loads when an aligned address is reached. This may load
// data after the end of the string, but can never cause a fault because the
// loads are in 128 bit sections also covered by the string.
int test_twosse2(const char* s, int len) {
  int i = 0;
  while (i < len - 1) {
    if (s[i] == '*' && s[i + 1] == '#') {
      return i;
    }
    i++;
    if (((uintptr_t)(s + i) & 15) == 0) break;
  }
  if (i >= len - 1) return -127;
  const uint128_t star_pattern = _mm_set1_epi8('*');
  const uint128_t hash_pattern = _mm_set1_epi8('#');
  int prev = (s[i - 1] == '*') << 15;
  for ( ; i < len; i += 16) {
    // Load aligned to a 128 bit XMM2 register.
    uint128_t raw = *(uint128_t*)(s + i);
    int stars = _mm_movemask_epi8(_mm_cmpeq_epi8(raw, star_pattern));
    int hashes = _mm_movemask_epi8(_mm_cmpeq_epi8(raw, hash_pattern));
    if ((prev & 0x8000) && (hashes & 1)) return i - 1;
    int combined = (stars << 1) & hashes;
    if (combined) {
      int result = i + __builtin_ffs(combined) - 2;
      if (result >= len - 1) return -127;
      return result;
    }
    prev = stars;
  }
  return -127;
}

// Search for "*#" by starting with at least one byte load and switching to
// aligned SSE2 128 bit loads when an aligned address is reached. This may load
// data after the end of the string, but can never cause a fault because the
// loads are in 128 bit sections also covered by the string.  This one is
// slightly simpler, but also slightly slower than test_twosse2.
int test_twobsse2(const char* s, int len) {
  int i = 0;
  while (i < len - 1) {
    if (s[i] == '*' && s[i + 1] == '#') {
      return i;
    }
    i++;
    if (((uintptr_t)(s + i) & 15) == 0) break;
  }
  if (i >= len - 1) return -127;
  const uint128_t star_pattern = _mm_set1_epi8('*');
  const uint128_t hash_pattern = _mm_set1_epi8('#');
  int stars = s[i - 1] == '*';
  for ( ; i < len; i += 16) {
    // Load aligned to a 128 bit XMM2 register.
    uint128_t raw = *(uint128_t*)(s + i);
    stars += _mm_movemask_epi8(_mm_cmpeq_epi8(raw, star_pattern)) << 1;
    int hashes = _mm_movemask_epi8(_mm_cmpeq_epi8(raw, hash_pattern));
    // We need to find out if the nth bit of hashes is set and also
    // the n-1th bit of stars.
    if (hashes & stars) {
      int result = i + __builtin_ffs(hashes & stars) - 2;
      if (result >= len - 1) return -127;
      return result;
    }
    stars >>= 16;
  }
  return -127;
}

// Search for "*#" using only aligned SSE2 128 bit loads. This may load data
// either side of the string, but can never cause a fault because the loads are
// in 128 bit sections also covered by the string.
// Use this algorithm if you have SSE or equivalent and you are searching for a
// two-character sequence.
int test_pure_twobsse2(const char* s, int len) {
  int last_bits = (uintptr_t)s & 15;
  int alignment_mask = 0xffff << last_bits;
  const uint128_t star_pattern = _mm_set1_epi8('*');
  const uint128_t hash_pattern = _mm_set1_epi8('#');
  int stars = 0;
  for (int i = -last_bits ; i < len; i += 16) {
    // Load aligned to a 128 bit XMM2 register.
    uint128_t raw = *(uint128_t*)(s + i);
    stars += (_mm_movemask_epi8(_mm_cmpeq_epi8(raw, star_pattern)) & alignment_mask) << 1;
    int hashes = _mm_movemask_epi8(_mm_cmpeq_epi8(raw, hash_pattern));
    // We need to find out if the nth bit of hashes is set and also
    // the n-1th bit of stars.
    int combined = hashes & stars;
    if (combined) {
      int result = i + __builtin_ffs(combined) - 2;
      if (result >= len - 1) return -127;
      return result;
    }
    stars >>= 16;
    alignment_mask = 0xffff;
  }
  return -127;
}

// Search for "*" using a variant of Alan Mycroft's trick for finding null
// bytes in a string: Mycroft observed that you can load a 4 byte word and then
// do:
// if ((word - 0x01010101) & (~word) & 0x80808080) ...
// to detect null bytes.  This variant observes that you can check for '*'
// (0x2a) by xoring with 0x2a2a2a2a, which will convert asterisks to nulls.
// This version starts with at least one bytewise comparison until we are
// 4-byte aligned. The 4-byte loads are always aligned.  This may load data
// after the string, but can never cause a fault because the loads are in 4
// byte sections also covered by the string.
int test_mycroft4(const char* s, int len) {
  int i = 0;
  while (i < len) {
    if (s[i] == '*') {
      return i;
    }
    i++;
    if (((uintptr_t)(s + i) & 3) == 0) break;
  }
  if (i >= len) return -127;
  const uint32_t mask = 0x2a2a2a2aul;
  const uint32_t highs = 0x80808080ul;
  const uint32_t ones = 0x01010101ul;
  for ( ; i < len; i += 4) {
    uint32_t raw = *(uint32_t*)(s + i) ^ mask;
    raw = (raw - ones) & (~raw) & highs;
    if (raw) {
      int answer = i + (__builtin_ffs(raw) >> 3) - 1;
      if (answer >= len) return -127;
      return answer;
    }
  }
  return -127;
}

// Search for "*" using a variant of Alan Mycroft's trick for finding null
// bytes in a string (see above).
// This version only does aligned 4-byte loads.
// This may load data either side of the string, but can never cause a fault
// because the loads are in 4 byte sections also covered by the string.
int test_pure_mycroft4(const char* s, int len) {
  int last_bits = (uintptr_t)s & 3;
  int i = -last_bits;
  const uint32_t mask = 0x2a2a2a2aul;
  uint32_t highs = 0x80808080ul << (last_bits << 3);
  const uint32_t ones = 0x01010101ul;
  for ( ; i < len; i += 4) {
    uint32_t raw = *(uint32_t*)(s + i) ^ mask;
    raw = (raw - ones) & (~raw) & highs;
    if (raw) {
      int answer = i + (__builtin_ffs(raw) >> 3) - 1;
      if (answer >= len) return -127;
      return answer;
    }
    highs = 0x80808080ul;
  }
  return -127;
}

// Search for "*" using a variant of Alan Mycroft's trick for finding null
// bytes in a string (see above).
// This version starts with at least one bytewise comparison until we are
// 8-byte aligned. The 8-byte loads are always aligned.  This may load data
// after the string, but can never cause a fault because the loads are in 8
// byte sections also covered by the string.
int test_mycroft(const char* s, int len) {
  int i = 0;
  while (i < len) {
    if (s[i] == '*') {
      return i;
    }
    i++;
    if (((uintptr_t)(s + i) & 7) == 0) break;
  }
  if (i >= len) return -127;
  const uint64_t mask = 0x2a2a2a2a2a2a2a2aul;
  const uint64_t highs = 0x8080808080808080ul;
  const uint64_t ones = 0x0101010101010101ul;
  for ( ; i < len; i += 8) {
    uint64_t raw = *(uint64_t*)(s + i) ^ mask;
    raw = (raw - ones) & (~raw) & highs;
    if (raw) {
      int answer = i + (__builtin_ffsll(raw) >> 3) - 1;
      if (answer >= len) return -127;
      return answer;
    }
  }
  return -127;
}

// Search for "*" using a variant of Alan Mycroft's trick for finding null
// bytes in a string (see above).
// This version only does aligned 8-byte loads.
// This may load data either side of the string, but can never cause a fault
// because the loads are in 8 byte sections also covered by the string.
// Use this algorithm if you are searching for a single character and you
// don't have SSE2 and you can't load unaligned.
int test_pure_mycroft(const char* s, int len) {
  int last_bits = (uintptr_t)s & 7;
  int i = -last_bits;
  const uint64_t mask = 0x2a2a2a2a2a2a2a2aul;
  uint64_t highs = 0x8080808080808080ul << (last_bits << 3);
  const uint64_t ones = 0x0101010101010101ul;
  for ( ; i < len; i += 8) {
    uint64_t raw = *(uint64_t*)(s + i) ^ mask;
    raw = (raw - ones) & (~raw) & highs;
    if (raw) {
      int answer = i + (__builtin_ffsll(raw) >> 3) - 1;
      if (answer >= len) return -127;
      return answer;
    }
    highs = 0x8080808080808080ul;
  }
  return -127;
}

// Search for "*#" using a variant of Alan Mycroft's trick for finding null
// bytes in a string (see above).
// This version starts with at least one bytewise comparison until we are
// 8-byte aligned, but uses non-aligned 8-byte loads after that, because it
// is looking for a 2-byte sequence.  This may load data
// after the string, but can never cause a fault because the loads do not
// cross an 8-byte boundary on the right.
int test_mycroft2(const char* s, int len) {
  int i = 0;
  while (i < len - 1) {
    if (s[i] == '*' && s[i + 1] == '#') {
      return i;
    }
    i++;
    if (((uintptr_t)(s + i) & 7) == 0) break;
  }
  if (i >= len - 1) return -127;
  const uint64_t mask_star = 0x2a2a2a2a2a2a2a2aul;
  const uint64_t mask_hash = 0x2323232323232323ul;
  const uint64_t highs = 0x8080808080808080ul;
  const uint64_t ones = 0x0101010101010101ul;
  for ( ; i < len; i += 8) {
    uint64_t raw = *(uint64_t*)(s + i - 1) ^ mask_star;
    uint64_t raw2 = *(uint64_t*)(s + i) ^ mask_hash;
    raw = ((raw - ones) & (~raw));
    raw2 = ((raw2 - ones) & (~raw2));
    uint64_t combined = raw & raw2 & highs;
    if (combined) {
      int result = (i - 1) + (__builtin_ffsll(combined) >> 3) - 1;
      if (result >= len - 1) return -127;
      return result;
    }
  }
  return -127;
}

// Search for "*#" using a variant of Alan Mycroft's trick for finding null
// bytes in a string (see above).
// This version only does aligned 8-byte loads.
// This may load data either side of the string, but can never cause a fault
// because the loads are in 8 byte sections also covered by the string.
// Searching for two-byte sequences without SSE instructions is challenging
// and this is not much faster than the naive approach.
int test_pure_mycroft2(const char* s, int len) {
  int last_bits = (uintptr_t)s & 7;
  int i = -last_bits;
  const uint64_t mask_star = 0x2a2a2a2a2a2a2a2aul;
  const uint64_t mask_hash = 0x2323232323232323ul;
  uint64_t highs = 0x8080808080808080ul << (last_bits << 3);
  const uint64_t ones = 0x0101010101010101ul;
  uint64_t stars_low = 0;
  for ( ; i < len; i += 8) {
    uint64_t raw = *(uint64_t*)(s + i);
    uint64_t new_stars = raw ^ mask_star;
    uint64_t hashes = raw ^ mask_hash;
    new_stars = ((new_stars - ones) & (~new_stars)) & highs;
    hashes = ((hashes - ones) & (~hashes));
    stars_low += new_stars << 8;
    uint64_t combined = stars_low & hashes;
    if (combined) {
      int result = (i - 1) + (__builtin_ffsll(combined) >> 3) - 1;
      if (result >= len - 1) return -127;
      return result;
    }
    stars_low = new_stars >> 56;
    highs = 0x8080808080808080ul;
  }
  return -127;
}

// If we don't have SSE2 then we are missing the instruction that takes the
// high bit of each byte and bunches them in the bottom of the word.  In fact
// we can simulate that, but it's not very fast:
// We have ('x's are zeros and the bits we want are numbered 76543210.
// 7xxx xxxx 6xxx xxxx 5xxx xxxx 4xxx xxxx 3xxx xxxx 2xxx xxxx 1xxx xxxx 0xxx xxxx
// val |= val >> 28, which moves them to:
// 7xxx xxxx 6xxx xxxx 5xxx xxxx 4xxx 7xxx 3xxx 6xxx 2xxx 5xxx 1xxx 4xxx 0xxx xxxx
// val |= val >> 14, which moves them to:
// 7xxx xxxx 6xxx xxxx 5xxx xxxx 4xxx 7xxx 3xxx 6x4x 2x7x 5x3x 1x6x 4x2x 0x5x xx1x
// val |= val >> 7, which moves them to:
// 7xxx xxx7 6xxx xxx6 5xxx xxx5 4xxx 7xx4 3xx7 6x43 2x76 5432 1765 4321 0654 x210
// (unsigned char)(val >> 7)
// Doing this last step earlier makes it more parallel if there are multiple independent shifters.
// But it's still too slow.
