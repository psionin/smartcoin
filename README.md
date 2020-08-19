# SmartCoin 1.1 (August 2020)
***

![Smartcoin](http://smartcoin.cc/images/smartcoin2-196x196.png)

## What is Smartcoin?
Smartcoin is a cryptocurrency like Bitcoin, using X11 as its proof of work (POW) algorithm. Dogecoin Core 1.14.2 code has been used as a base for the most recent Smartcoin update for much wow performance and stability.

http://smartcoin.cc/

## License – Much license
Smartcoin Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.

## Development and contributions
Development is ongoing, and the development team, as well as other volunteers, can freely work in their own trees and submit pull requests when features or bug fixes are ready.

#### Contributions

Developers are strongly encouraged to write [unit tests](src/test/README.md) for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run
(assuming they weren't disabled in configure) with: `make check`. Further details on running
and extending unit tests can be found in [/src/test/README.md](/src/test/README.md).

There are also [regression and integration tests](/qa) of the RPC interface, written
in Python, that are run automatically on the build server.
These tests can be run (if the [test dependencies](/qa) are installed) with: `qa/pull-tester/rpc-tests.py`

Changes should be tested by somebody other than the developer who wrote the
code. This is especially important for large or high-risk changes. It is useful
to add a test plan to the pull request description if testing the changes is
not straightforward.

## Frequently Asked Questions

### How many SMC? – To the moon!
About 30 million SMC are scheduled to be minted, most of them already made.

The block reward schedule:

- Genesis Block           400,000 SMC
- Block 0,000,002 - 0,000,999:  1 SMC
- Block 0,001,000 - 0,199,999: 64 SMC
- Block 0,200,000 - 0,299,999: 32 SMC
- Block 0,300,000 - 0,562,799: 16 SMC
- Block 0,562,800 - 0,825,599:  8 SMC
- Block 0,825,600 - 1,088,399:  4 SMC
- Block 1,088,400 - 1,351,199:  2 SMC
- Block 1,351,199 - 1,613,999:  1 SMC
- ............... - ......... x/2 SMC

### Wow plz make smartcoind/smartcoin-cli/smartcoin-qt

  The following are developer notes on how to build Smartcoin on your native platform. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

  - [OSX Build Notes](doc/build-osx.md)
  - [Unix Build Notes](doc/build-unix.md)
  - [Windows Build Notes](doc/build-msw.md)

### Such ports
RPC 8583
P2P 58585

## Development tips and tricks

**compiling for debugging**

Run configure with the --enable-debug option, then make. Or run configure with
CXXFLAGS="-g -ggdb -O0" or whatever debug flags you need.

**debug.log**

If the code is behaving strangely, take a look in the debug.log file in the data directory;
error and debugging messages are written there.

The -debug=... command-line option controls debugging; running with just -debug will turn
on all categories (and give you a very large debug.log file).

The Qt code routes qDebug() output to debug.log under category "qt": run with -debug=qt
to see it.

**testnet and regtest modes**

Run with the -testnet option to run with "play smartcoins" on the test network, if you
are testing multi-machine code that needs to operate across the internet.

If you are testing something that can run on one machine, run with the -regtest option.
In regression test mode, blocks can be created on-demand; see qa/rpc-tests/ for tests
that run in -regtest mode.

**DEBUG_LOCKORDER**

Smartcoin Core is a multithreaded application, and deadlocks or other multithreading bugs
can be very difficult to track down. Compiling with -DDEBUG_LOCKORDER (configure
CXXFLAGS="-DDEBUG_LOCKORDER -g") inserts run-time checks to keep track of which locks
are held, and adds warnings to the debug.log file if inconsistencies are detected.
