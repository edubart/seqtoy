# Seqtoy

This is a tool for creating small music sequence loops to be used
in small games.
It's written in [Nelua](https://nelua.io/) for [RIV](https://docs.rives.io) fantasy console.

You can try it in your browser
[here](https://emulator.rives.io/#cartridge=https://raw.githubusercontent.com/edubart/cartridges/main/seqtoy.sqfs).

![Screenshot](https://raw.githubusercontent.com/edubart/seqtoy/master/seqtoy.png)

## Sound loop examples

Here is a list of 10 sound loops created with this tool as examples, click on them to play in your browser, you can also mix them right-away.

- [01.seqt.rivcard](https://emulator.rives.io/#cartridge=https://raw.githubusercontent.com/edubart/cartridges/main/seqtoy.sqfs&incard=https://raw.githubusercontent.com/edubart/cartridges/main/cards/01.seqt.rivcard)
- [02.seqt.rivcard](https://emulator.rives.io/#cartridge=https://raw.githubusercontent.com/edubart/cartridges/main/seqtoy.sqfs&incard=https://raw.githubusercontent.com/edubart/cartridges/main/cards/02.seqt.rivcard)
- [03.seqt.rivcard](https://emulator.rives.io/#cartridge=https://raw.githubusercontent.com/edubart/cartridges/main/seqtoy.sqfs&incard=https://raw.githubusercontent.com/edubart/cartridges/main/cards/03.seqt.rivcard)
- [04.seqt.rivcard](https://emulator.rives.io/#cartridge=https://raw.githubusercontent.com/edubart/cartridges/main/seqtoy.sqfs&incard=https://raw.githubusercontent.com/edubart/cartridges/main/cards/04.seqt.rivcard)
- [05.seqt.rivcard](https://emulator.rives.io/#cartridge=https://raw.githubusercontent.com/edubart/cartridges/main/seqtoy.sqfs&incard=https://raw.githubusercontent.com/edubart/cartridges/main/cards/05.seqt.rivcard)
- [06.seqt.rivcard](https://emulator.rives.io/#cartridge=https://raw.githubusercontent.com/edubart/cartridges/main/seqtoy.sqfs&incard=https://raw.githubusercontent.com/edubart/cartridges/main/cards/06.seqt.rivcard)
- [07.seqt.rivcard](https://emulator.rives.io/#cartridge=https://raw.githubusercontent.com/edubart/cartridges/main/seqtoy.sqfs&incard=https://raw.githubusercontent.com/edubart/cartridges/main/cards/07.seqt.rivcard)
- [08.seqt.rivcard](https://emulator.rives.io/#cartridge=https://raw.githubusercontent.com/edubart/cartridges/main/seqtoy.sqfs&incard=https://raw.githubusercontent.com/edubart/cartridges/main/cards/08.seqt.rivcard)
- [09.seqt.rivcard](https://emulator.rives.io/#cartridge=https://raw.githubusercontent.com/edubart/cartridges/main/seqtoy.sqfs&incard=https://raw.githubusercontent.com/edubart/cartridges/main/cards/09.seqt.rivcard)
- [10.seqt.rivcard](https://emulator.rives.io/#cartridge=https://raw.githubusercontent.com/edubart/cartridges/main/seqtoy.sqfs&incard=https://raw.githubusercontent.com/edubart/cartridges/main/cards/10.seqt.rivcard)

## Notes

Have in mind this is a sequencer toy, with emphasis on *toy*,
it's NOT a goal to create a DAW (Digital Audio Workstation).
However this tools is enough to create simple and interesting sound loops
to be used in simple games.

As a toy, it has some intentional constraints to not make its users
overwhelmed by so many options professional music sequencers have,
these are some of them:

- Sequence time span can have at most 112 notes
- Only the notes in the D# Major Pentatonic are available to be played, so each note always sounds nice relative to another note
- Can only have up to 4 pre-defined tracks with predefined instrument synthesizer (Strings, Lead, Bass and Drums)

## Controls

You can only use the keyboard to use this tool.
This is intentionally so it can be used with game pads and also on mobile in the future where there is no mouse.

The keyboard shortcuts are mapped in a way to be compatible with RIV gamepad mappings,
so it can be a bit strange at first but it's a matter of getting used to it.
You can view the controls by holding E or SELECT button gamepad button.

## How it works

It plays simple audio waveforms (Sine, Square, Sawtooth, etc) for the instruments
every frame, check the RIV documentation for more details about waveforms.

## Compiling

First make sure you have the RIV SDK installed in your environment, then just type `make` to compile.
You can also play it by typing `make run`.