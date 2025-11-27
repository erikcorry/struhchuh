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
a fixed distance from each other, at most a word apart).  For example,
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
sequence was misachan's https://mischasan.wordpress.com/2011/07/16/convergence-sse2-and-strstr/ but he does extra work to find a match across a
word boundary, which I avoid.

The GNU C library uses some similar tricks for strlen, but does
bytewise operations until it hits alignment, which I think is
a waste, especially if a the sequence is quickly found. https://sourceware.org/git/?p=glibc.git;a=blob_plain;f=sysdeps/x86_64/strlen.S;hb=HEAD

## Results

For finding a single byte, results from my fast desktop on small inputs (ns/search)
and large inputs (GiB/s).

```
            naive:                  4.0 GiB/s  9.9 ns/search
    pure_mycroft4:                  7.4 GiB/s  5.5 ns/search
         mycroft4:                  7.4 GiB/s  9.3 ns/search
          mycroft:                 14.1 GiB/s  3.7 ns/search
     pure_mycroft:                 14.7 GiB/s  2.6 ns/search
        pure_sse2:                 38.5 GiB/s  4.4 ns/search
             sse2:                 36.4 GiB/s  4.6 ns/search
sse2_and_mycroft4:                 36.4 GiB/s  4.1 ns/search
           memchr:                153.6 GiB/s  2.6 ns/search
```

The "pure" algorithms are the ones that only use word-sized loads.  They
are the clear winners, overall, though the ones that step a byte at a time
until alignment are marginally faster searching very large strings.

memchr is probably using AVX-512 unaligned masked non-faulting loads.

For finding two-byte sequences (not aligned), we get:

```
          twobyte:                 4.0 GiB/s   10.9 ns/search  // Naive implemntation.
         mycroft2:                 9.9 GiB/s    3.8 ns/search
    pure_mycroft2:                10.0 GiB/s    4.9 ns/search
          twosse2:                21.6 GiB/s    6.7 ns/search
    twosse2_early:                28.5 GiB/s   10.3 ns/search
         twobsse2:                14.0 GiB/s    6.1 ns/search
    pure_twobsse2:                13.9 GiB/s    2.8 ns/search
```

Again the pure algorithms look like the clear winners unless you know
search strings are long, and the character sequence is rare.
