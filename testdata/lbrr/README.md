<!-- SPDX-FileCopyrightText: 2026 The Pion community <https://pion.ly> -->
<!-- SPDX-License-Identifier: MIT -->

# LBRR reference fixtures

`libopus.json` is generated from synthetic integer PCM by `generate.c`, using
Xiph libopus commit `22244de5a79bd1d6d623c32e72bf1954b56235be` (1.6.1), the
same pin as `testdata/short-plc`.

The encoder runs at 48 kHz with `OPUS_APPLICATION_VOIP`, in-band FEC on,
10% expected packet loss, complexity 10 and VBR, so SILK adds LBRR frames to
most packets. The signal alternates voiced, unvoiced and silent stretches, so
some SILK frames have LBRR and some do not. There are seven cases of 480 ms:

| Mode | Channels | Packet duration |
| --- | ---: | --- |
| SILK wideband | 1 | 20, 40 and 60 ms |
| SILK wideband | 2 | 20 and 60 ms |
| Hybrid fullband | 1 and 2 | 20 ms |

The mode is set with the pinned test-only force-mode control
(`src/opus_private.h`, request 11002). Every packet is decoded by libopus at
16 kHz with FEC off, which only reads past the LBRR frames, and the fixture
records the packet, whether it carries LBRR, the final range and the PCM.
`generate.c` exits with an error if a packet comes out in the wrong mode or a
case has fewer than four packets with LBRR.

Regenerate on Linux from a clean checkout of the pin, built like the PLC
fixtures:

```sh
git checkout --detach 22244de5a79bd1d6d623c32e72bf1954b56235be
./autogen.sh
./configure --disable-shared --disable-intrinsics --disable-extra-programs --disable-doc
make -j4
cc -O2 -Iinclude /path/to/pion/opus/testdata/lbrr/generate.c .libs/libopus.a -lm -o generate
./generate > /path/to/pion/opus/testdata/lbrr/libopus.json
```

Recorded with GCC 16.2.1, Linux/amd64; floating-point build, intrinsics
disabled.
