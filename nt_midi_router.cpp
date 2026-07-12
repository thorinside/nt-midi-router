/*
 * NT MIDI Router for Expert Sleepers disting NT.
 *
 * Routes incoming MIDI channel messages to up to three output channels and
 * selectable MIDI destinations. Defaults solve the common case:
 * channel 1 CCs -> channels 2 and 3 on the breakout.
 */

#include <distingnt/api.h>
#include <cstddef>
#include <new>
#include <stdint.h>

namespace {

enum {
    kMessageCcOnly,
    kMessageCcNotes,
    kMessageAllChannel,
    kNumMessageTypes,
};

enum {
    kParamEnabled,
    kParamInputChannel,
    kParamMessageType,
    kParamCcLow,
    kParamCcHigh,
    kParamOutputChannel1,
    kParamOutputChannel2,
    kParamThruChannel,
    kParamToBreakout,
    kParamToUsb,
    kParamToSelectBus,
    kParamToInternal,
    // Keep new parameters at the end to preserve released preset indices.
    kParamInputChannelEnd,
    kNumParams,
};

enum {
    kNumOffOnValues = 2,
    kNumInputChannelValues = 17,
    kNumOutputChannelValues = 18,
};

template <typename T, size_t N>
constexpr size_t arrayCount(const T (&)[N]) {
    return N;
}

constexpr uint32_t fourCc(char a, char b, char c, char d) {
    return (static_cast<uint32_t>(a) << 0) |
           (static_cast<uint32_t>(b) << 8) |
           (static_cast<uint32_t>(c) << 16) |
           (static_cast<uint32_t>(d) << 24);
}

static char const * const kOffOnStrings[] = { "Off", "On" };

static char const * const kInputChannelStrings[] = {
    "All", "1", "2", "3", "4", "5", "6", "7", "8",
    "9", "10", "11", "12", "13", "14", "15", "16"
};

static char const * const kOutputChannelStrings[] = {
    "Off", "Same", "1", "2", "3", "4", "5", "6", "7", "8",
    "9", "10", "11", "12", "13", "14", "15", "16"
};

static char const * const kInputChannelEndStrings[] = {
    "Same", "1", "2", "3", "4", "5", "6", "7", "8",
    "9", "10", "11", "12", "13", "14", "15", "16"
};

static char const * const kMessageTypeStrings[] = {
    "CC only", "CC+notes", "All channel"
};

static_assert(arrayCount(kOffOnStrings) == kNumOffOnValues, "Off/On string table mismatch");
static_assert(arrayCount(kInputChannelStrings) == kNumInputChannelValues, "Input channel string table mismatch");
static_assert(arrayCount(kOutputChannelStrings) == kNumOutputChannelValues, "Output channel string table mismatch");
static_assert(arrayCount(kInputChannelEndStrings) == kNumInputChannelValues, "Input channel end string table mismatch");
static_assert(arrayCount(kMessageTypeStrings) == kNumMessageTypes, "Message type string table mismatch");

constexpr _NT_parameter makeParameter(const char* name, int16_t min, int16_t max, int16_t def, uint8_t unit, char const * const * enumStrings) {
    return { name, min, max, def, unit, 0, enumStrings };
}

static const _NT_parameter kParameters[] = {
    makeParameter("Enabled", 0, 1, 1, kNT_unitEnum, kOffOnStrings),
    makeParameter("In ch", 0, 16, 1, kNT_unitEnum, kInputChannelStrings),
    makeParameter("Messages", 0, 2, kMessageCcOnly, kNT_unitEnum, kMessageTypeStrings),
    makeParameter("CC low", 0, 127, 0, kNT_unitNone, nullptr),
    makeParameter("CC high", 0, 127, 127, kNT_unitNone, nullptr),
    makeParameter("Out ch 1", 0, 17, 3, kNT_unitEnum, kOutputChannelStrings),
    makeParameter("Out ch 2", 0, 17, 4, kNT_unitEnum, kOutputChannelStrings),
    makeParameter("Thru ch", 0, 17, 0, kNT_unitEnum, kOutputChannelStrings),
    makeParameter("Breakout", 0, 1, 1, kNT_unitEnum, kOffOnStrings),
    makeParameter("USB", 0, 1, 0, kNT_unitEnum, kOffOnStrings),
    makeParameter("Select Bus", 0, 1, 0, kNT_unitEnum, kOffOnStrings),
    makeParameter("Internal", 0, 1, 0, kNT_unitEnum, kOffOnStrings),
    makeParameter("In ch end", 0, 16, 0, kNT_unitEnum, kInputChannelEndStrings),
};

static_assert(arrayCount(kParameters) == kNumParams, "Parameter table mismatch");

static const uint8_t kFilterPage[] = {
    kParamEnabled, kParamInputChannel, kParamInputChannelEnd,
    kParamMessageType, kParamCcLow, kParamCcHigh
};

static const uint8_t kOutputsPage[] = {
    kParamOutputChannel1, kParamOutputChannel2, kParamThruChannel
};

static const uint8_t kDestinationsPage[] = {
    kParamToBreakout, kParamToUsb, kParamToSelectBus, kParamToInternal
};

constexpr _NT_parameterPage makePage(const char* name, uint8_t numParams, const uint8_t* params) {
    return { name, numParams, 0, { 0, 0 }, params };
}

static const _NT_parameterPage kPages[] = {
    makePage("Filter", static_cast<uint8_t>(arrayCount(kFilterPage)), kFilterPage),
    makePage("Outputs", static_cast<uint8_t>(arrayCount(kOutputsPage)), kOutputsPage),
    makePage("Destinations", static_cast<uint8_t>(arrayCount(kDestinationsPage)), kDestinationsPage),
};

static const _NT_parameterPages kParameterPages = {
    arrayCount(kPages),
    kPages,
};

struct _midiRouterAlgorithm : public _NT_algorithm {
    _midiRouterAlgorithm() : routedCount(0) {
        parameters = nullptr;
        parameterPages = nullptr;
        vIncludingCommon = nullptr;
        v = nullptr;
    }

    uint32_t routedCount;
};

static void calculateRequirements(_NT_algorithmRequirements& req, const int32_t*) {
    req.numParameters = arrayCount(kParameters);
    req.sram = sizeof(_midiRouterAlgorithm);
    req.dram = 0;
    req.dtc = 0;
    req.itc = 0;
}

static _NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, const _NT_algorithmRequirements&, const int32_t*) {
    _midiRouterAlgorithm* alg = new (ptrs.sram) _midiRouterAlgorithm();
    alg->parameters = kParameters;
    alg->parameterPages = &kParameterPages;
    return alg;
}

static uint32_t outputDestinations(const _midiRouterAlgorithm* alg) {
    uint32_t destinations = 0;

    if (alg->v[kParamToBreakout])
        destinations |= kNT_destinationBreakout;
    if (alg->v[kParamToUsb])
        destinations |= kNT_destinationUSB;
    if (alg->v[kParamToSelectBus])
        destinations |= kNT_destinationSelectBus;
    if (alg->v[kParamToInternal])
        destinations |= kNT_destinationInternal;

    return destinations;
}

static int outputChannelFromParam(int value, int inputChannel) {
    if (value == 0)
        return -1;
    if (value == 1)
        return inputChannel;
    return value - 2;
}

static bool messageTypeAllowed(int messageType, uint8_t status) {
    if (status == 0xb0)
        return true;

    if (messageType == kMessageCcOnly)
        return false;

    if (messageType == kMessageCcNotes)
        return status == 0x80 || status == 0x90;

    return status >= 0x80 && status <= 0xe0;
}

static bool ccAllowed(const _midiRouterAlgorithm* alg, uint8_t cc) {
    int low = alg->v[kParamCcLow];
    int high = alg->v[kParamCcHigh];

    if (low > high) {
        int tmp = low;
        low = high;
        high = tmp;
    }

    return cc >= low && cc <= high;
}

static bool inputChannelAllowed(const _midiRouterAlgorithm* alg, int inputChannel) {
    int first = alg->v[kParamInputChannel];
    if (first == 0)
        return true;

    int last = alg->v[kParamInputChannelEnd];
    if (last == 0)
        last = first;

    if (first > last) {
        int tmp = first;
        first = last;
        last = tmp;
    }

    // Parameter channels are 1-based; MIDI status-byte channels are 0-based.
    return inputChannel >= first - 1 && inputChannel <= last - 1;
}

static void sendChannelMessage(uint32_t destinations, uint8_t status, int channel, uint8_t data1, uint8_t data2) {
    uint8_t byte0 = static_cast<uint8_t>(status | (channel & 0x0f));

    if (status == 0xc0 || status == 0xd0)
        NT_sendMidi2ByteMessage(destinations, byte0, data1);
    else
        NT_sendMidi3ByteMessage(destinations, byte0, data1, data2);
}

static void sendIfUnique(uint32_t destinations, uint8_t status, int channel, uint8_t data1, uint8_t data2, int first, int second) {
    if (channel < 0)
        return;
    if (channel == first || channel == second)
        return;

    sendChannelMessage(destinations, status, channel, data1, data2);
}

static void midiMessage(_NT_algorithm* self, uint8_t byte0, uint8_t byte1, uint8_t byte2) {
    _midiRouterAlgorithm* alg = static_cast<_midiRouterAlgorithm*>(self);

    if (!alg->v[kParamEnabled])
        return;

    uint8_t status = byte0 & 0xf0;
    int inputChannel = byte0 & 0x0f;
    if (!inputChannelAllowed(alg, inputChannel))
        return;

    if (!messageTypeAllowed(alg->v[kParamMessageType], status))
        return;

    if (status == 0xb0 && !ccAllowed(alg, byte1))
        return;

    uint32_t destinations = outputDestinations(alg);
    if (!destinations)
        return;

    int out1 = outputChannelFromParam(alg->v[kParamOutputChannel1], inputChannel);
    int out2 = outputChannelFromParam(alg->v[kParamOutputChannel2], inputChannel);
    int thru = outputChannelFromParam(alg->v[kParamThruChannel], inputChannel);

    sendIfUnique(destinations, status, out1, byte1, byte2, -1, -1);
    sendIfUnique(destinations, status, out2, byte1, byte2, out1, -1);
    sendIfUnique(destinations, status, thru, byte1, byte2, out1, out2);

    alg->routedCount++;
}

static void step(_NT_algorithm*, float*, int) {
}

static const _NT_factory kFactory = {
    fourCc('T', 'h', 'M', 'r'),
    "NT MIDI Router",
    "Duplicates and remaps MIDI channel messages",
    0,
    nullptr,
    nullptr,
    nullptr,
    calculateRequirements,
    construct,
    nullptr,
    step,
    nullptr,
    nullptr,
    midiMessage,
    kNT_tagUtility,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

} // namespace

extern "C" uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
    switch (selector) {
    case kNT_selector_version:
        return kNT_apiVersionCurrent;
    case kNT_selector_numFactories:
        return 1;
    case kNT_selector_factoryInfo:
        return data == 0 ? reinterpret_cast<uintptr_t>(&kFactory) : 0;
    }

    return 0;
}
