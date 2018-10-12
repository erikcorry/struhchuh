# StruhChur

This is a little experiment to find out the fastest way to write a
strchr-like function on modern hardware.  Some of the fastest versions
make use of SSE2 instructions on Intel CPUs.

## The tricks

Some of the plain-C++
versions are inspired by Alan Mycrofts hack for finding null bytes in
memory.  See https://www.cl.cam.ac.uk/~am21/progtricks.html  

The basic trick is to load data a multi-byte word at a time, then
do bit manipulation to find the leftmost null byte.  Although
Mycroft formulated it in terms of finding nulls, you can xor the
word with a pattern of repeated constant bytes first, in order to
find any byte.  For example xor with 0x20202020 to find spaces.

SSE2 versions are not as tricky and make use of the built-in instructions
designed for this purpose.

## The rules

The function being tested is not exactly strchr, since the length is
known and null bytes are ignored.  A second variant finds two consecutive
bytes (for most of the algorithms they don't have to be consecutive, just
a fixed distance from each other, at most a word apart.  For example,
some compilers spend an appreciable time looking for the ``*/`` that ends as comment.  The two bytes do not have to be two-byte aligned.

Functions are tested on a small input (one where the characters are
almost immediately found) and long inputs, where any setup time is
amortized.

Most of the algorithms read outside the actual string, but they only
read within an aligned power-of-two region around actual string
characters.  This means they won't fault on modern hardware, but they
may elicit squeals from frameworks like valgrind that want to detect
use of uninitialized or out-of-bounds memory.  The tests use unmapped
pages to ensure that nothing is read that could cause a trap.

Obviously most of this is undefined behaviour in C and C++ which is
why you shouldn't use C and C++ for performance sensitive code.  I
will not be responsible for any nasal demons that haunt your
sinuses.

## Running

```
make
./search
```

That's it.  Look at the source code to see how the winning function
works.

## Related work

Skipping comments in Clang
https://clang.llvm.org/doxygen/Lexer_8cpp_source.html (search for
SSE2).

Comments on this from DannyBee
https://news.ycombinator.com/item?id=7488994

Documentation for SSE2 instructions
https://software.intel.com/en-us/node/524239

A big inspiration for the SSE2 version of searching for a two-byte
sequence was misachan's https://mischasan.wordpress.com/2011/07/16/convergence-sse2-and-strstr/ but he does extra work to find a match accross a
word boundary, which I avoid.

The GNU C library uses some similar tricks for strlen, but does
bytewise operations until it hits alignment, which I think is
a waste, especially if a the sequence is quickly found. https://sourceware.org/git/?p=glibc.git;a=blob_plain;f=sysdeps/x86_64/strlen.S;hb=HEAD

## Results

For finding a single byte, results from my rather lame laptop on small and large inputs:

```
(small)             naive:  2550ms
(  big)             naive:  5747ms
(small)     pure_mycroft4:  2040ms
(  big)     pure_mycroft4:  2648ms
(small)          mycroft4:  2756ms
(  big)          mycroft4:  2451ms
(small)           mycroft:  2819ms
(  big)           mycroft:  1275ms
(small)      pure_mycroft:  1741ms
(  big)      pure_mycroft:  1348ms
(small)         pure_sse2:   690ms
(  big)         pure_sse2:   463ms
(small)              sse2:  3019ms
(  big)              sse2:   423ms
(small) sse2_and_mycroft4:  3016ms
(  big) sse2_and_mycroft4:   434ms
```

The "pure" algorithms are the ones that only use word-sized loads.  They
are the clear winners, overall, though the ones that step a byte at a time
until alignment are marginally faster searching very large strings.

For finding two-byte sequences, we get

```
(small)           twobyte:  3076ms  // The naive algorithm.
(  big)           twobyte:  7907ms
(small)          mycroft2:  3320ms
(  big)          mycroft2:  2301ms
(small)     pure_mycroft2:  2347ms
(  big)     pure_mycroft2:  3010ms
(small)           twosse2:  3194ms
(  big)           twosse2:   800ms
(small)          twobsse2:  3222ms
(  big)          twobsse2:   837ms
(small)     pure_twobsse2:   978ms
(  big)     pure_twobsse2:   939ms
```

Again the pure algorithms look like the clear winners unless you know
search strings are long, and the character sequence is rare.
