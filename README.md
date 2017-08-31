# Headless GSPlus

This is a branch of [GSPlus](https://github.com/digarok/gsplus) which is a fork of GSport which is a fork of a fork of KEGS.

This particular branch (modemworks) was designed to run a ProLine BBS headless.

## Building

Headless kegs with ModemWorks support has been known to build on Linux and OS X.

    mkdir build
    cd build
    cmake -DHOST_MW=ON -DDRIVER=HEADLESS ../src/
    make

## Using

Set up your disk images and config with a normal version of GSPlus. 

You can send a SIGINFO (OS X - `control-T`) or SIGUSR1 (`killall -SIGUSR1 GSPlus`) to dump the text screen.

See also, [Host ModemWorks](https://github.com/ksherlock/host-modemworks) for the corresponding ProLine Serial/Modem/Time/Hash tools.
