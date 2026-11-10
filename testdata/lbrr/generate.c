/* SPDX-FileCopyrightText: 2026 The Pion community <https://pion.ly>
 * SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include "opus.h"

#define INPUT_RATE 48000
#define OUTPUT_RATE 16000
#define INPUT_SAMPLES (INPUT_RATE * 48 / 100)

static void check(int result) {
    if (result < 0) {
        fprintf(stderr, "%s\n", opus_strerror(result));
        exit(1);
    }
}

static opus_int16 signal_sample(int i, int channel, unsigned *seed) {
    int phase = i % 19200;
    if (phase < 9600) {
        int period = 160 + phase / 80 + channel * 13;
        return (opus_int16)((i % period) * 12000 / period - 6000);
    }
    if (phase < 14400) {
        *seed = *seed * 1103515245u + 12345u;
        return (opus_int16)((int)((*seed >> 16) & 0x1fff) - 4096);
    }
    return 0;
}

static void run_case(int channels, int milliseconds, int hybrid, int bitrate) {
    int error;
    unsigned seed = 1;
    int input_samples = INPUT_RATE * milliseconds / 1000;
    int output_samples = OUTPUT_RATE * milliseconds / 1000;
    int with_lbrr = 0;
    OpusEncoder *encoder = opus_encoder_create(INPUT_RATE, channels, OPUS_APPLICATION_VOIP, &error);
    check(error);
    OpusDecoder *decoder = opus_decoder_create(OUTPUT_RATE, channels, &error);
    check(error);
    check(opus_encoder_ctl(encoder, OPUS_SET_BITRATE(bitrate)));
    check(opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(10)));
    check(opus_encoder_ctl(encoder, OPUS_SET_INBAND_FEC(1)));
    check(opus_encoder_ctl(encoder, OPUS_SET_PACKET_LOSS_PERC(10)));
    /* Test-only control from the pinned src/opus_private.h. */
    check(opus_encoder_ctl(encoder, 11002, hybrid ? 1001 : 1000));
    check(opus_encoder_ctl(encoder, OPUS_SET_BANDWIDTH(hybrid ? OPUS_BANDWIDTH_FULLBAND : OPUS_BANDWIDTH_WIDEBAND)));
    printf("{\"channels\":%d,\"rate\":%d,\"mode\":\"%s\",\"duration\":%d,\"steps\":[",
        channels, OUTPUT_RATE, hybrid ? "hybrid" : "silk", milliseconds);
    for (int offset = 0; offset < INPUT_SAMPLES; offset += input_samples) {
        opus_int16 input[2880 * 2], output[960 * 2];
        unsigned char packet[1275];
        for (int i = 0; i < input_samples; i++) {
            for (int channel = 0; channel < channels; channel++) {
                input[i * channels + channel] = signal_sample(offset + i, channel, &seed);
            }
        }
        int bytes = opus_encode(encoder, input, input_samples, packet, sizeof(packet));
        check(bytes);
        int config = packet[0] >> 3;
        if (hybrid ? config < 12 || config > 15 : config > 11) exit(2);
        int lbrr = opus_packet_has_lbrr(packet, bytes);
        check(lbrr);
        with_lbrr += lbrr;
        int decoded = opus_decode(decoder, packet, bytes, output, output_samples, 0);
        check(decoded);
        if (decoded != output_samples) exit(3);
        opus_uint32 final_range;
        check(opus_decoder_ctl(decoder, OPUS_GET_FINAL_RANGE(&final_range)));
        printf("%s{\"packet\":\"", offset ? "," : "");
        for (int i = 0; i < bytes; i++) printf("%02x", packet[i]);
        printf("\",\"lbrr\":%s,\"samples\":%d,\"range\":%u,\"pcm\":[", lbrr ? "true" : "false", decoded, final_range);
        for (int i = 0; i < decoded * channels; i++) printf("%s%d", i ? "," : "", output[i]);
        printf("]}");
    }
    printf("]}");
    if (with_lbrr < 4) exit(4);
    opus_encoder_destroy(encoder);
    opus_decoder_destroy(decoder);
}

int main(void) {
    printf("{\"pin\":\"22244de5a79bd1d6d623c32e72bf1954b56235be\",\"cases\":[");
    run_case(1, 20, 0, 32000);
    printf(",");
    run_case(1, 40, 0, 32000);
    printf(",");
    run_case(1, 60, 0, 32000);
    printf(",");
    run_case(2, 20, 0, 48000);
    printf(",");
    run_case(2, 60, 0, 48000);
    printf(",");
    run_case(1, 20, 1, 40000);
    printf(",");
    run_case(2, 20, 1, 64000);
    puts("]}");
    return 0;
}
