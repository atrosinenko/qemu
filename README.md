This is a port of [QEMU](https://www.qemu.org/) machine emulator to JavaScript using [Emscripten](http://kripken.github.io/emscripten-site/).

It is still a work in progress. Now it is mostly capable of running x86 operating systems with graphics support. It should be quite easy to support other 32-bit guest architectures as well (now it can run OpenWRT on MIPS). It has a working proof-of-concept implementation of just-in-time translation of machine code to JavaScript. :) It is still quite slow, though.

## How to test it

You do not need to build it yourself if you just want to test it. You even don't have to install anything special. Just navigate your browser to this [on-line demo](https://atrosinenko.github.io/qemujs-demo/), but beware of traffic usage (Qemu.js is quite large, about several tens of megabytes) and accidental browser freezes or crashes (Qemu.js is a huge JS app).

## How to experiment with it

If you want to build it yourself, see build instruction in [this repo](https://github.com/atrosinenko/qemujs-builder). Please note that you don't have to clone **this** repo yourself, because it will be downloaded as a submodule of `qemujs-builder`.

## Links

Here is my [article](https://habrahabr.ru/post/315770/) (in Russian) about Qemu.js.

Some similar pre-existing projects:
* [JSLinux](https://bellard.org/jslinux/) -- machine emulator written by Fabrice Bellard, the original author of QEMU
* [Virtual x86](https://copy.sh/v86/) -- another JavaScript machine emulator
* [another](https://github.com/yingted/qemu) attempt (now frozen?) to port QEMU to JavaScript
* [Unicorn.js](https://alexaltea.github.io/unicorn.js/) -- JavaScript port of a QEMU fork
