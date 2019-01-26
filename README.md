This is a port of [QEMU](https://www.qemu.org/) machine emulator to JavaScript using [Emscripten](http://kripken.github.io/emscripten-site/).

The port targets single-threaded WebAssembly and contains a proof-of-concept WebAssembly JIT. For now, only 32-bit guest are supported.

This time, it is branched from the upstream `master` branch and now this repo does not require separate `qemujs-build`. This rewrite is still even more work-in-progress than the original one and still does not support block devices.

## Links

Here is my [article](https://habrahabr.ru/post/315770/) (in Russian) about Qemu.js.

Some similar pre-existing projects:
* [JSLinux](https://bellard.org/jslinux/) -- machine emulator written by Fabrice Bellard, the original author of QEMU
* [Virtual x86](https://copy.sh/v86/) -- another JavaScript machine emulator
* [another](https://github.com/yingted/qemu) attempt (now frozen?) to port QEMU to JavaScript
* [Unicorn.js](https://alexaltea.github.io/unicorn.js/) -- JavaScript port of a QEMU fork
