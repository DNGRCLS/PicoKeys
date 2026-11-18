# Third-party notices

PicoKeys' own source code is licensed under the MIT License (see [`LICENSE`](LICENSE)).

The PicoKeys firmware file (`.uf2`) also contains the third-party software listed below. Their licenses require their copyright notices and license terms to accompany binary copies, so they are reproduced here. This file applies to every PicoKeys firmware release built with **Raspberry Pi Pico SDK 2.2.0** and **xPack GNU Arm Embedded GCC 14.2.1** (newlib 4.4.0).

| Component | Version in the firmware | License |
|---|---|---|
| [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) | 2.2.0 | BSD-3-Clause |
| [cyw43-driver](https://github.com/georgerobotics/cyw43-driver), including the CYW43439 Wi-Fi chip firmware | commit `dd75682` (bundled with Pico SDK 2.2.0) | Raspberry Pi licence (`LICENSE.RP`), see below |
| [lwIP](https://savannah.nongnu.org/projects/lwip/) | 2.2.1 (bundled with Pico SDK 2.2.0) | BSD-3-Clause |
| [TinyUSB](https://github.com/hathach/tinyusb) | 0.18.0 (bundled with Pico SDK 2.2.0) | MIT |
| printf implementation by Marco Paland (Pico SDK `pico_printf`) | as in Pico SDK 2.2.0 | MIT |
| DHCP server (`src/dhcpserver.c`) from [MicroPython](https://micropython.org) via [pico-examples](https://github.com/raspberrypi/pico-examples) | 2018–2019 | MIT |
| [newlib](https://sourceware.org/newlib/) C library (parts of `libc`) | 4.4.0 | Various permissive licenses, see [`licenses/newlib-COPYING.NEWLIB.txt`](licenses/newlib-COPYING.NEWLIB.txt) |
| GCC runtime (`libgcc`, start-up files) | 14.2.1 | GPL-3.0 with the GCC Runtime Library Exception (no notice required) |

Not included: the computer-OS detection in PicoKeys follows the approach of [QMK](https://github.com/qmk/qmk_firmware)'s `os_detection` feature, but no QMK code is used.

---

## Raspberry Pi Pico SDK

Includes the boot stage 2 loader, hardware and runtime libraries, and the CYW43 and lwIP integration code.

```text
Copyright 2020 (c) 2020 Raspberry Pi (Trading) Ltd.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
   disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
   disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---

## cyw43-driver and CYW43439 firmware

The cyw43-driver version in Pico SDK 2.2.0 is offered under two licenses. **PicoKeys uses it under the Raspberry Pi licence (`LICENSE.RP`)**, which permits use and redistribution only together with Raspberry Pi's RP2040 or other Raspberry Pi semiconductor devices. PicoKeys firmware runs only on the RP2040. Both licence texts are reproduced below.

### LICENSE.RP (the licence PicoKeys relies on)

```text
Copyright (C) 2019-2022 George Robotics Pty Ltd

Raspberry Pi Ltd (Licensor) hereby grants to you a non-exclusive license to
use this software solely with the Licensor's microcontroller chip (RP2040) or
any other semiconductor device produced by the Licensor. No other use is
permitted under the terms of this licence.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. The software can only be used and redistributed in conjunction with RP2040
   or any other semiconductor device produced by the Licensor.
2. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
3. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE LICENSOR AND COPYRIGHT OWNER "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE LICENSOR OR COPYRIGHT OWNER BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This software is also available from the copyright owner under different
terms (see ./LICENSE)
```

### LICENSE (alternative terms from the copyright owner)

```text
Copyright (C) 2019-2022 George Robotics Pty Ltd

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Any redistribution, use, or modification in source or binary form is done
   solely for personal benefit and not for any commercial purpose or for
   monetary gain.

THIS SOFTWARE IS PROVIDED BY THE LICENSOR AND COPYRIGHT OWNER "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE LICENSOR OR COPYRIGHT OWNER BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This software is also available for use with Raspberry Pi Ltd semiconductor
devices under different terms (see ./LICENSE.RP).

For commercial licensing options please email contact@georgerobotics.com.au
```

---

## lwIP

```text
Copyright (c) 2001, 2002 Swedish Institute of Computer Science.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
```

---

## TinyUSB

```text
The MIT License (MIT)

Copyright (c) 2018, hathach (tinyusb.org)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

---

## printf implementation (Pico SDK `pico_printf`)

```text
(c) Marco Paland (info@paland.com)
            2014-2019, PALANDesign Hannover, Germany

The MIT License (MIT)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

---

## DHCP server (`src/dhcpserver.c`, from MicroPython)

```text
The MIT License (MIT)

Copyright (c) 2018-2019 Damien P. George

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

---

## newlib C library

The firmware links parts of newlib 4.4.0 (string, memory, number-parsing and `sscanf` routines, and `malloc`). newlib is made up of many files under different permissive licenses. The complete set of newlib license texts is in [`licenses/newlib-COPYING.NEWLIB.txt`](licenses/newlib-COPYING.NEWLIB.txt).

---

## GCC runtime library

`libgcc` and the compiler's start-up files are covered by the GNU General Public License v3 with the GCC Runtime Library Exception, which allows them to be combined into firmware under any license, without a notice requirement. They are listed here for completeness.
