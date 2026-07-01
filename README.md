# NT MIDI Router

Custom Expert Sleepers disting NT utility plugin for duplicating and remapping incoming MIDI channel messages.

The default parameters implement the requested workflow:

- listen to incoming MIDI CC messages on channel 1
- pass through every CC number from 0 to 127
- retransmit each CC to channels 2 and 3
- send the result to the MIDI breakout

## Parameters

Filter:

- `Enabled`: turns routing on or off
- `In ch`: `All` or a specific input MIDI channel
- `Messages`: `CC only`, `CC+notes`, or `All channel`
- `CC low` / `CC high`: inclusive CC range for CC messages

Outputs:

- `Out ch 1`: first remapped output channel
- `Out ch 2`: second remapped output channel
- `Thru ch`: optional original/same-channel pass-through, or another channel

Destinations:

- `Breakout`
- `USB`
- `Select Bus`
- `Internal`

Duplicate output channels are suppressed, so choosing the same channel twice only sends one copy.

## Important Input-Port Note

The disting NT plugin MIDI callback receives MIDI channel messages delivered to algorithms by the host. The API exposes output destinations, but it does not pass source-port metadata into `midiMessage()`. Use the NT's system/global MIDI settings to decide which physical inputs feed the algorithm MIDI stream, and avoid enabling a physical forwarding loop when this plugin is also sending back to the same port.

## Build

```sh
git submodule update --init --recursive
make verify
```

The hardware object is written to:

```text
plugins/nt_midi_router.o
```

Copy that `.o` file to the disting NT SD card to install the plugin.
