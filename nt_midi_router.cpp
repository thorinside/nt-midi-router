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
    kNumParams,
};

static const char* kOffOnStrings[] = { "Off", "On", NULL };

static const char* kInputChannelStrings[] = {
    "All", "1", "2", "3", "4", "5", "6", "7", "8",
    "9", "10", "11", "12", "13", "14", "15", "16", NULL
};

static const char* kOutputChannelStrings[] = {
    "Off", "Same", "1", "2", "3", "4", "5", "6", "7", "8",
    "9", "10", "11", "12", "13", "14", "15", "16", NULL
};

static const char* kMessageTypeStrings[] = {
    "CC only", "CC+notes", "All channel", NULL
};

static const _NT_parameter kParameters[] = {
    { .name = "Enabled", .min = 0, .max = 1, .def = 1, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = kOffOnStrings },
    { .name = "In ch", .min = 0, .max = 16, .def = 1, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = kInputChannelStrings },
    { .name = "Messages", .min = 0, .max = 2, .def = kMessageCcOnly, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = kMessageTypeStrings },
    { .name = "CC low", .min = 0, .max = 127, .def = 0, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL },
    { .name = "CC high", .min = 0, .max = 127, .def = 127, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL },
    { .name = "Out ch 1", .min = 0, .max = 17, .def = 3, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = kOutputChannelStrings },
    { .name = "Out ch 2", .min = 0, .max = 17, .def = 4, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = kOutputChannelStrings },
    { .name = "Thru ch", .min = 0, .max = 17, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = kOutputChannelStrings },
    { .name = "Breakout", .min = 0, .max = 1, .def = 1, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = kOffOnStrings },
    { .name = "USB", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = kOffOnStrings },
    { .name = "Select Bus", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = kOffOnStrings },
    { .name = "Internal", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = kOffOnStrings },
};

static const uint8_t kFilterPage[] = {
    kParamEnabled, kParamInputChannel, kParamMessageType, kParamCcLow, kParamCcHigh
};

static const uint8_t kOutputsPage[] = {
    kParamOutputChannel1, kParamOutputChannel2, kParamThruChannel
};

static const uint8_t kDestinationsPage[] = {
    kParamToBreakout, kParamToUsb, kParamToSelectBus, kParamToInternal
};

static const _NT_parameterPage kPages[] = {
    { .name = "Filter", .numParams = ARRAY_SIZE(kFilterPage), .group = 0, .unused = { 0, 0 }, .params = kFilterPage },
    { .name = "Outputs", .numParams = ARRAY_SIZE(kOutputsPage), .group = 0, .unused = { 0, 0 }, .params = kOutputsPage },
    { .name = "Destinations", .numParams = ARRAY_SIZE(kDestinationsPage), .group = 0, .unused = { 0, 0 }, .params = kDestinationsPage },
};

static const _NT_parameterPages kParameterPages = {
    .numPages = ARRAY_SIZE(kPages),
    .pages = kPages,
};

struct _midiRouterAlgorithm : public _NT_algorithm {
    _midiRouterAlgorithm() : routedCount(0) {}
    uint32_t routedCount;
};

static void calculateRequirements(_NT_algorithmRequirements& req, const int32_t*) {
    req.numParameters = ARRAY_SIZE(kParameters);
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

static void sendChannelMessage(uint32_t destinations, uint8_t status, int channel, uint8_t data1, uint8_t data2) {
    uint8_t byte0 = (uint8_t)(status | (channel & 0x0f));

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
    _midiRouterAlgorithm* alg = (_midiRouterAlgorithm*)self;

    if (!alg->v[kParamEnabled])
        return;

    uint8_t status = byte0 & 0xf0;
    int inputChannel = byte0 & 0x0f;
    int selectedInputChannel = alg->v[kParamInputChannel];

    if (selectedInputChannel > 0 && inputChannel != selectedInputChannel - 1)
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
    .guid = NT_MULTICHAR('T', 'h', 'M', 'r'),
    .name = "NT MIDI Router",
    .description = "Duplicates and remaps MIDI channel messages",
    .numSpecifications = 0,
    .specifications = NULL,
    .calculateStaticRequirements = NULL,
    .initialise = NULL,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .parameterChanged = NULL,
    .step = step,
    .draw = NULL,
    .midiRealtime = NULL,
    .midiMessage = midiMessage,
    .tags = kNT_tagUtility,
    .hasCustomUi = NULL,
    .customUi = NULL,
    .setupUi = NULL,
    .serialise = NULL,
    .deserialise = NULL,
    .midiSysEx = NULL,
    .parameterUiPrefix = NULL,
    .parameterString = NULL,
};

} // namespace

extern "C" uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
    switch (selector) {
    case kNT_selector_version:
        return kNT_apiVersionCurrent;
    case kNT_selector_numFactories:
        return 1;
    case kNT_selector_factoryInfo:
        return (uintptr_t)((data == 0) ? &kFactory : NULL);
    }

    return 0;
}
