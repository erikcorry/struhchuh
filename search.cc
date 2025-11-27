// Copyright 2018 Erik Corry.  See the LICENSE file, the BSD 2-clause
// license.

// Test harness for the searching routines.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>
#include <sys/mman.h>

#include "search.h"

void set_up();

const char* small = 0;
const char* large = 0;

int small_length;
int large_length;

static int random_offsets[4096];

void set_up() {
  static const char* sm = "Now is the time *# for all good men";
  small = sm;
  small_length = strlen(sm);

  srandom(314159);
  for (int i = 0; i < 4096; i++) {
    random_offsets[i] = random();
  }

  int LONG = 10000;
  char* l = (char*)malloc(LONG);
  large_length = LONG;
  for (int i = 0; i < LONG; i += 4) {
    l[i] = 'F';
    l[i + 1] = 'o';
    l[i + 2] = 'o';
    l[i + 3] = ' ';
  }

  l[(LONG * 3) / 4] = '*';
  l[(LONG * 3) / 4 + 1] = '#';

  large = l;
}

typedef int searcher(const char* s, int len);

void time(searcher* fn, const char* name) {
  for (int size = 0; size < 2; size++) {
    struct timeval start, end;
    int sum = 0;
    gettimeofday(&start, 0);
    int limit = size ? 1000000 : 100000000;
    for (int i = 0; i < limit; i++) {
      if (size) {
        sum += fn(large + (i & 127), large_length - (i & 127));
      } else {
        int off = random_offsets[i & 4095] & 15;
        sum += fn(small + off, small_length - off);
      }
    }
    gettimeofday(&end, 0);
    int ms = (end.tv_sec - start.tv_sec) * 1000;
    ms += (end.tv_usec - start.tv_usec) / 1000;
    printf("(%5s) %17s: %5dms %d\n", size ? "big" : "small", name, ms, sum);
  }
}

void test(const char* name, searcher* testee, int bytes) {
  static const int PAGE = 4096;
  char* three_pages = (char*)mmap(NULL, PAGE * 3, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  mprotect(three_pages, PAGE, PROT_NONE);
  mprotect(three_pages + 2 * PAGE, PAGE, PROT_NONE);
  char* start = three_pages + PAGE;
  char* end = three_pages + PAGE * 2;

  for (int len = 0; len < 40; len++) {
    memset(start, 'a', len);
    memset(start + len, '*', 30);
    if (bytes == 2) {
      for (int k = 1; k < 30; k += 2) start[len + k] = '#';
    }
    int f;
    if ((f = testee(start, len)) != -127) {
      printf("%s: Expected not found, but found at %d\n", name, f);
    }
    for (int pos = 0; pos < len + 1 - bytes; pos++) {
      memset(start, 'a', len);
      start[pos] = '*';
      if (bytes == 2) start[pos + 1] = '#';
      if ((f = testee(start, len)) != pos) {
        printf("%s: Expected at %d, but found at %d\n", name, pos, f);
        printf("len = %d, pos = %d, start=%p\n", len, pos, start);
      }
      if (bytes == 2) {
        for (int k = 0; k < pos; k++) {
          start[k] = '*';
          if ((f = testee(start, len)) != pos) {
            printf("%s: Expected at %d, but found at %d\n", name, pos, f);
            printf("len = %d, pos = %d, start=%p\n", len, pos, start);
          }
          start[k] = 'a';
        }
      }
    }
  }
  for (int len = 0; len < 40; len++) {
    memset(end - len, 'a', len);
    memset(end - len - 30, '*', 30);
    int f;
    if ((f = testee(end - len, len)) != -127) {
      printf("%s: Expected not found, but found at %d\n", name, f);
    }
    for (int pos = 0; pos < len + 1 - bytes; pos++) {
      memset(end - len, 'a', len);
      end[-len + pos] = '*';
      if (bytes == 2) end[-len + pos + 1] = '#';
      if ((f = testee(end - len, len)) != pos) {
        printf("%s: Expected at %d, but found at %d\n", name, pos, f);
      }
    }
  }

  munmap(three_pages, PAGE * 3);

  char* buffer = (char*)malloc(129);
  buffer[128] = '\0';
  srandom(314159);
  for (int iterations = 0; iterations < 10000; iterations++) {
    for (int i = 0; i < 128; i++) {
      int r = random() & 7;
      switch (r) {
        case 0:
          buffer[i] = '*';
          break;
        case 1:
          buffer[i] = '#';
          break;
        case 2:
          buffer[i] = '*' - 128;
          break;
        case 3:
          buffer[i] = '#' - 128;
          break;
        case 4:
          buffer[i] = random();
        default:
          buffer[i] = '#' - 3 + r;
          break;
      }
    }
    char* start = buffer + (random() & 127);
    int len = random() % (buffer + 128 - start);
    int index = bytes == 2 ? test_twobyte(start, len) : test_naive(start, len);
    int guess = testee(start, len);
    if (index != guess) {
      printf("%s: Randomly expected %d, got %d for search length %d in '%s'\n",
          name, index, guess, len, start);
    }
  }
}

int main() {
  set_up();
  test("naive", test_naive, 1);
  test("memchr", test_memchr, 1);
  test("pure_mycroft4", test_pure_mycroft4, 1);
  test("mycroft4", test_mycroft4, 1);
  test("mycroft", test_mycroft, 1);
  test("pure_mycroft", test_pure_mycroft, 1);
  test("pure_sse2", test_pure_sse2, 1);
  test("sse2", test_sse2, 1);
  test("sse2_and_mycroft4", test_sse2_and_mycroft4, 1);
  test("twobyte", test_twobyte, 2);
  test("mycroft2", test_mycroft2, 2);
  test("pure_mycroft2", test_pure_mycroft2, 2);
  test("twosse2", test_twosse2, 2);
  test("twobsse2", test_twobsse2, 2);
  test("pure_twobsse2", test_pure_twobsse2, 2);
  time(test_naive, "naive");
  time(test_memchr, "memchr");
  time(test_pure_mycroft4, "pure_mycroft4");
  time(test_mycroft4, "mycroft4");
  time(test_mycroft, "mycroft");
  time(test_pure_mycroft, "pure_mycroft");
  time(test_pure_sse2, "pure_sse2");
  time(test_sse2, "sse2");
  time(test_sse2_and_mycroft4, "sse2_and_mycroft4");
  time(test_twobyte, "twobyte");
  time(test_mycroft2, "mycroft2");
  time(test_pure_mycroft2, "pure_mycroft2");
  time(test_twosse2, "twosse2");
  time(test_twobsse2, "twobsse2");
  time(test_pure_twobsse2, "pure_twobsse2");
}
