# core test data: python3 -I python_repr.py > python_repr.tsv
# Writes <16 hex digits of the IEEE-754 bits><TAB><Python repr> per line, for core's format_number test.
import math
import random
import struct
import sys


def bits(x):
    return struct.unpack("<Q", struct.pack("<d", x))[0]


def from_bits(b):
    return struct.unpack("<d", struct.pack("<Q", b))[0]


cases = [0.0, -0.0, 1.0, -1.0, 0.1, 0.2, 0.30000000000000004, 1 / 3, 2 / 3, 1e-05, 1.5e-05, 0.0001,
         0.00012345, 0.001, 13632.95, 100.0, 1e15, 1e16, 1.234e16, 9999999999999998.0,
         12345678901234567.0, 1e22, 1e23, 5e-324, 2.2250738585072014e-308, 2.225073858507201e-308,
         1.7976931348623157e308, -1.7976931348623157e308, 123456.789, 0.5, 0.25, 0.125, 2.5, 1e100,
         1e-100, 1.0000000000000002, 0.9999999999999999, 4.35, 0.07, -0.06, 0.67, 1.01, 1.45,
         -1.56, 5.19, 94.57, 2.0 ** 53, 2.0 ** 53 + 2, 2.0 ** 63, 2.0 ** 64, 1e-4, 9.999999999999999e-05,
         0.00010000000000000002, 1e16 - 2, 1e16 + 2, 123e-7, 7e-10, 6.02214076e23, 299792458.0,
         math.pi, math.e, -math.pi * 1e-20, 0.009990009990009990, 0.0009990009990009, 1.0 / 1001]
rng = random.Random(20261118)
for _ in range(150):                                  # any double: random bit patterns
    b = rng.getrandbits(64)
    x = from_bits(b)
    if math.isfinite(x):
        cases.append(x)
for _ in range(150):                                  # numbers like FuSION's: decimals and ratios
    cases.append(round(rng.uniform(-10, 10), rng.randint(0, 6)))
    cases.append(rng.randint(1, 1000) / rng.randint(1, 1000))
for e in range(-8, 20):                               # around the fixed/exponent boundaries
    cases.append(10.0 ** e)
    cases.append(1.5 * 10.0 ** e)
    cases.append(-(10.0 ** e) * 9.87654321)

seen = set()
out = sys.stdout
out.write("# <IEEE-754 bits in hex>\t<Python 3.12 repr>; written by python_repr.py, never edited by hand\n")
for x in cases:
    x = float(x)
    b = bits(x)
    if b in seen:
        continue
    seen.add(b)
    out.write(f"{b:016x}\t{x!r}\n")
