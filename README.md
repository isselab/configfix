# ConfigFix for Linux 7.3

ConfigFix sources prepared for a **Linux 7.3** source tree. See the
[main branch](https://github.com/isselab/configfix) for what ConfigFix is.

The files here replace their upstream counterparts, so this branch only fits
Linux 7.3. Use the `copy_patch_*` branch that matches your kernel.

## Prerequisites

Qt (for `xconfig`):

    apt install pkg-config g++ qtbase5-dev

PicoSAT, built as a shared library **with trace support**. ConfigFix loads
`libpicosat-trace.so` at runtime; the `picosat` distribution package only
ships the binary, so build it from source:

    wget http://fmv.jku.at/picosat/picosat-965.tar.gz
    tar xzf picosat-965.tar.gz && cd picosat-965
    ./configure.sh --trace --shared
    make libpicosat.so
    gcc -shared -o libpicosat-trace.so picosat.o version.o \
        -Xlinker -soname -Xlinker libpicosat-trace.so
    sudo install -m 0755 libpicosat-trace.so /usr/local/lib/
    sudo ldconfig

Without it every target still builds, but exits at startup with
"You need to install PicoSAT first".

## Usage

1. Download and extract the Linux 7.3 source tree from
   [kernel.org](https://www.kernel.org/).
2. Run `./copy_files.sh /path/to/linux-7.3` from this branch.
3. Make sure a `.config` exists (e.g. `make defconfig`).
4. Run one of:
   * `make xconfig` — the GUI, with conflict resolution
   * `make cfixconfig` — conflict resolution on the command line
   * `make cfoutconfig` — export constraints and the SAT problem in DIMACS

## Notes

Includes fixes for three defects in the encoding of non-Boolean symbols that
made the extracted formula unsatisfiable from Linux 6.16 onwards
([#1](https://github.com/isselab/configfix/issues/1)):

* an unset `int`/`hex`/`string` symbol is compared as kconfig's implicit
  value (`0`, `0x0`, `""`) rather than as a value distinct from zero;
* a range bound that is itself a symbol is encoded relationally instead of
  being frozen to that symbol's current value;
* the known-value set now registers the constants of every comparison and a
  representative value per interval between them, so a value strictly inside a
  range can be proposed.
