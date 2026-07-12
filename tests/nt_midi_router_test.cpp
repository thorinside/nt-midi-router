#include <cassert>
#include <cstdint>

#include "../nt_midi_router.cpp"

namespace {

struct SentMessage {
    uint32_t destinations;
    uint8_t byte0;
    uint8_t byte1;
    uint8_t byte2;
};

SentMessage gSent[8];
int gSentCount = 0;

void resetSent() {
    gSentCount = 0;
}

void setDefaults(_midiRouterAlgorithm& alg, int16_t (&values)[kNumParams]) {
    for (int i = 0; i < kNumParams; ++i)
        values[i] = kParameters[i].def;
    alg.v = values;
}

void testDefaultStillRoutesOnlyChannelOne() {
    _midiRouterAlgorithm alg;
    int16_t values[kNumParams];
    setDefaults(alg, values);

    resetSent();
    midiMessage(&alg, 0xb0, 10, 64);
    assert(gSentCount == 2);

    midiMessage(&alg, 0xb1, 10, 64);
    assert(gSentCount == 2);
}

void testRangeRoutesOneThroughEightOnly() {
    _midiRouterAlgorithm alg;
    int16_t values[kNumParams];
    setDefaults(alg, values);
    values[kParamInputChannel] = 1;
    values[kParamInputChannelEnd] = 8;
    values[kParamOutputChannel1] = 0;
    values[kParamOutputChannel2] = 0;
    values[kParamThruChannel] = 1;

    resetSent();
    midiMessage(&alg, 0xb0, 10, 64);
    midiMessage(&alg, 0xb7, 11, 65);
    assert(gSentCount == 2);
    assert(gSent[0].byte0 == 0xb0);
    assert(gSent[0].destinations == kNT_destinationBreakout);
    assert(gSent[0].byte1 == 10);
    assert(gSent[0].byte2 == 64);
    assert(gSent[1].byte0 == 0xb7);
    assert(gSent[1].destinations == kNT_destinationBreakout);
    assert(gSent[1].byte1 == 11);
    assert(gSent[1].byte2 == 65);

    midiMessage(&alg, 0xb8, 12, 66);
    midiMessage(&alg, 0xbf, 13, 67);
    assert(gSentCount == 2);
}

void testAllStillIgnoresRangeEnd() {
    _midiRouterAlgorithm alg;
    int16_t values[kNumParams];
    setDefaults(alg, values);
    values[kParamInputChannel] = 0;
    values[kParamInputChannelEnd] = 8;

    assert(inputChannelAllowed(&alg, 0));
    assert(inputChannelAllowed(&alg, 15));
}

void testReversedRangeIsNormalized() {
    _midiRouterAlgorithm alg;
    int16_t values[kNumParams];
    setDefaults(alg, values);
    values[kParamInputChannel] = 8;
    values[kParamInputChannelEnd] = 1;

    assert(inputChannelAllowed(&alg, 0));
    assert(inputChannelAllowed(&alg, 7));
    assert(!inputChannelAllowed(&alg, 8));
}

} // namespace

extern "C" void NT_sendMidi2ByteMessage(uint32_t destinations, uint8_t byte0, uint8_t byte1) {
    gSent[gSentCount++] = { destinations, byte0, byte1, 0 };
}

extern "C" void NT_sendMidi3ByteMessage(uint32_t destinations, uint8_t byte0, uint8_t byte1, uint8_t byte2) {
    gSent[gSentCount++] = { destinations, byte0, byte1, byte2 };
}

int main() {
    testDefaultStillRoutesOnlyChannelOne();
    testRangeRoutesOneThroughEightOnly();
    testAllStillIgnoresRangeEnd();
    testReversedRangeIsNormalized();
    return 0;
}
