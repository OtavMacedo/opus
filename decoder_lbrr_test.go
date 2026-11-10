// SPDX-FileCopyrightText: 2026 The Pion community <https://pion.ly>
// SPDX-License-Identifier: MIT

package opus

import (
	"encoding/hex"
	"encoding/json"
	"fmt"
	"os"
	"testing"

	"github.com/stretchr/testify/require"
)

type lbrrFixture struct {
	Pin   string     `json:"pin"`
	Cases []lbrrCase `json:"cases"`
}

type lbrrCase struct {
	Channels int        `json:"channels"`
	Rate     int        `json:"rate"`
	Mode     string     `json:"mode"`
	Duration int        `json:"duration"`
	Steps    []lbrrStep `json:"steps"`
}

type lbrrStep struct {
	Packet  string  `json:"packet"`
	LBRR    bool    `json:"lbrr"`
	Samples int     `json:"samples"`
	Range   uint32  `json:"range"`
	PCM     []int16 `json:"pcm"`
}

func (c lbrrCase) name() string {
	return fmt.Sprintf("%s/%dch/%dms", c.Mode, c.Channels, c.Duration)
}

func (c lbrrCase) packets(tb testing.TB) [][]byte {
	tb.Helper()
	packets := make([][]byte, len(c.Steps))
	for i, step := range c.Steps {
		var err error
		packets[i], err = hex.DecodeString(step.Packet)
		require.NoError(tb, err)
	}

	return packets
}

func loadLBRRFixture(tb testing.TB) lbrrFixture {
	tb.Helper()
	var fixture lbrrFixture
	data, err := os.ReadFile("testdata/lbrr/libopus.json")
	require.NoError(tb, err)
	require.NoError(tb, json.Unmarshal(data, &fixture))
	require.Equal(tb, "22244de5a79bd1d6d623c32e72bf1954b56235be", fixture.Pin)
	require.Len(tb, fixture.Cases, 7)

	return fixture
}

func TestDecodeLBRRLibopus(t *testing.T) {
	fixture := loadLBRRFixture(t)
	for _, test := range fixture.Cases {
		t.Run(test.name(), func(t *testing.T) {
			decoder, err := NewDecoderWithOutput(test.Rate, test.Channels)
			require.NoError(t, err)
			packets := test.packets(t)
			withLBRR := 0
			for index, step := range test.Steps {
				out := make([]int16, step.Samples*test.Channels)
				n, err := decoder.DecodeToInt16(packets[index], out)
				require.NoError(t, err, "step %d", index)
				require.Equal(t, step.Samples, n, "step %d", index)
				require.Equal(t, step.Range, decoder.rangeFinal, "step %d", index)
				require.Equal(t, step.PCM, out, "step %d", index)
				if step.LBRR {
					withLBRR++
				}
			}
			require.GreaterOrEqual(t, withLBRR, 4)
		})
	}
}

func BenchmarkDecodeLBRR(b *testing.B) {
	fixture := loadLBRRFixture(b)
	for _, test := range fixture.Cases {
		packets := test.packets(b)
		b.Run(test.name(), func(b *testing.B) {
			decoder, err := NewDecoderWithOutput(test.Rate, test.Channels)
			require.NoError(b, err)
			out := make([]int16, test.Steps[0].Samples*test.Channels)
			for _, packet := range packets {
				_, err = decoder.DecodeToInt16(packet, out)
				require.NoError(b, err)
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := range b.N {
				_, err = decoder.DecodeToInt16(packets[i%len(packets)], out)
				if err != nil {
					b.Fatal(err)
				}
			}
		})
	}
}

func TestDecodeLBRRWarmAllocations(t *testing.T) {
	fixture := loadLBRRFixture(t)
	for _, test := range fixture.Cases {
		t.Run(test.name(), func(t *testing.T) {
			decoder, err := NewDecoderWithOutput(test.Rate, test.Channels)
			require.NoError(t, err)
			packets := test.packets(t)
			out := make([]int16, test.Steps[0].Samples*test.Channels)
			decodeAll := func() {
				for _, packet := range packets {
					_, decodeErr := decoder.DecodeToInt16(packet, out)
					require.NoError(t, decodeErr)
				}
			}
			decodeAll()
			require.Zero(t, testing.AllocsPerRun(10, decodeAll))
		})
	}
}
