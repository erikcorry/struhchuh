// Copyright 2018 Erik Corry.  See the LICENSE file, the BSD 2-clause
// license.

int test_naive(const char* s, int len);
int test_memchr(const char* s, int len);
int test_pure_mycroft4(const char* s, int len);
int test_mycroft4(const char* s, int len);
int test_mycroft(const char* s, int len);
int test_pure_mycroft(const char* s, int len);
int test_twobyte(const char* s, int len);
int test_mycroft2(const char* s, int len);
int test_pure_mycroft2(const char* s, int len);
int test_pure_sse2(const char* s, int len);
int test_split_sse2(const char* s, int len);
int test_sse2(const char* s, int len);
int test_sse2_and_mycroft4(const char* s, int len);
int test_twosse2(const char* s, int len);
int test_twosse2_early(const char* s, int len);
int test_twobsse2(const char* s, int len);
int test_pure_twobsse2(const char* s, int len);
