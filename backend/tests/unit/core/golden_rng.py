# core test data: the reference values in test_rng.cpp and test_xoshiro.cpp come from
# python3 -I golden_rng.py, an implementation independent of core's C++.
# Independent reference values for core's generators, written from the published algorithms
# (Matsumoto and Nishimura's mt19937-64.c, Vigna's splitmix64.c, Lemire 2019, rand 0.9.2's
# xoshiro256plusplus.rs, uniform_int.rs, increasing_uniform.rs and slice.rs).
M64 = (1 << 64) - 1
M32 = (1 << 32) - 1
GAMMA = 0x9E3779B97F4A7C15


class MT64:
    NN, MM = 312, 156
    A = 0xB5026F5AA96619E9
    UM, LM = 0xFFFFFFFF80000000, 0x7FFFFFFF

    def __init__(self, seed=5489):
        self.mt = [0] * self.NN
        self.mt[0] = seed & M64
        for i in range(1, self.NN):
            p = self.mt[i - 1]
            self.mt[i] = (6364136223846793005 * (p ^ (p >> 62)) + i) & M64
        self.i = self.NN

    def next(self):
        NN, MM, mt = self.NN, self.MM, self.mt
        if self.i >= NN:
            for k in range(NN):
                x = (mt[k] & self.UM) | (mt[(k + 1) % NN] & self.LM)
                mt[k] = mt[(k + MM) % NN] ^ (x >> 1) ^ (self.A if x & 1 else 0)
            self.i = 0
        x = mt[self.i]
        self.i += 1
        x ^= (x >> 29) & 0x5555555555555555
        x ^= (x << 17) & 0x71D67FFFEDA60000 & M64
        x ^= (x << 37) & 0xFFF7EEE000000000 & M64
        x ^= x >> 43
        return x & M64


def splitmix_next(state):
    state = (state + GAMMA) & M64
    z = state
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
    return state, z ^ (z >> 31)


class Rng:
    def __init__(self, seed, stream):
        state = (seed + stream * GAMMA) & M64
        _, key = splitmix_next(state)
        self.key = key
        self.e = MT64(key)

    def next(self):
        return self.e.next()

    def below(self, n):
        x = self.next()
        m = x * n
        lo = m & M64
        if lo < n:
            t = ((1 << 64) - n) % n
            while lo < t:
                x = self.next()
                m = x * n
                lo = m & M64
        return m >> 64

    def unit(self):
        return (self.next() >> 11) * 2.0 ** -53

    def shuffle(self, a):
        for i in range(len(a), 1, -1):
            j = self.below(i)
            a[i - 1], a[j] = a[j], a[i - 1]


class Xoshiro:
    def __init__(self, seed):
        s = []
        state = seed
        for _ in range(4):
            state, z = splitmix_next(state)
            s.append(z)
        self.s = s
        self.u32 = 0

    def next_u64(self):
        s = self.s
        rotl = lambda x, k: ((x << k) | (x >> (64 - k))) & M64
        res = (rotl((s[0] + s[3]) & M64, 23) + s[0]) & M64
        t = (s[1] << 17) & M64
        s[2] ^= s[0]
        s[3] ^= s[1]
        s[1] ^= s[2]
        s[0] ^= s[3]
        s[2] ^= t
        s[3] = rotl(s[3], 45)
        return res

    def next_u32(self):
        self.u32 += 1
        return self.next_u64() >> 32

    def below(self, bound):
        rng = bound & M32
        prod = self.next_u32() * rng
        result, lo = prod >> 32, prod & M32
        if lo > ((-rng) & M32):
            new_hi = (self.next_u32() * rng) >> 32
            if lo + new_hi > M32:
                result += 1
        return result & M32


def calc_bound(m):
    product, current = m, m + 1
    while product * current <= M32:
        product *= current
        current += 1
    return product, current - m


def shuffle09(a, rng):
    draws = 0
    if len(a) <= 1:
        return 0
    n, chunk, rem = 0, 0, 1
    for i in range(len(a)):
        nn = n + 1
        if rem == 0:
            bound, r = calc_bound(nn)
            chunk = rng.below(bound)
            draws += 1
            nrem = r - 1
        else:
            nrem = rem - 1
        if nrem == 0:
            idx = chunk
        else:
            idx = chunk % nn
            chunk //= nn
        rem, n = nrem, nn
        a[i], a[idx] = a[idx], a[i]
    return draws


if __name__ == "__main__":
    m = MT64()
    for _ in range(9999):
        m.next()
    print("mt19937_64 10000th:", m.next())

    r = Rng(42, 0)
    print("stream(42,0) key:", r.key)
    print("stream(42,0) first:", [r.next() for _ in range(3)])
    r = Rng(42, 1)
    print("stream(42,1) first:", [r.next() for _ in range(3)])
    r = Rng(42, 0)
    print("stream(42,0) below(3054) x8:", [r.below(3054) for _ in range(8)])
    r = Rng(42, 0)
    print("stream(42,0) unit x3:", [repr(r.unit()) for _ in range(3)])
    r = Rng(42, 0)
    a = list(range(10))
    r.shuffle(a)
    print("stream(42,0) shuffle 0..9:", a)
    r = Rng(7, 3)
    print("stream(7,3) below(2**63+1) x2:", [r.below(2**63 + 1) for _ in range(2)])

    x = Xoshiro(0)
    print("xoshiro(0):", [x.next_u64() for _ in range(3)])
    x = Xoshiro(42)
    p = list(range(3054))
    d = shuffle09(p, x)
    print("shuffle 3054 seed 42 first10:", p[:10], "bounded draws:", d, "u32 draws:", x.u32)
    print("second perm first10:", end=" ")
    d2 = shuffle09(p, x)
    print(p[:10], d2)
    x = Xoshiro(42)
    print("xoshiro(42) below:", [x.below(b) for b in (1, 2, 3, 10, 479001600, 4294967295, 4294967295)])
    x = Xoshiro(5)
    q = list(range(13))
    shuffle09(q, x)
    print("shuffle 13 seed 5:", q)
