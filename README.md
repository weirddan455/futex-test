# futex-test
Simple C program for Linux that tests if the kernel supports additional futex functions used by Wine

The FUTEX_MULTIPLE test was adapted from the example in the futex man page. Use --verbose to print the demonstration to console.

If your kernel does not support a futex extension, you should see a "Function not implemented" error.
If you get another error, it's possible there's a bug. Open an Issue and post the output as well as your system information.

Two versions of futex2 are tested.  One has been merged into the mainline vanilla kernel in 5.16.
The other is an old variant that is only available by out of tree patches.

Patches can be found at https://github.com/Frogging-Family/linux-tkg

## AUR

If you are an Arch Linux user, you can find this program on the AUR here:  https://aur.archlinux.org/packages/futex-test-git

Note that this is not maintained by me so please report AUR specific issues in the AUR comments.

## Prerequisites
You will need to have gcc installed as well as the standard C library header files usually located in /usr/include
On Arch Linux, the headers are part of the glibc package so simply installing gcc should be sufficient:

```
# pacman -S gcc
```

If you run into issues, try installing the base-devel package group:

```
# pacman -S base-devel
```

TODO: Write instructions for other popular distros (Ubuntu, Debian, Fedora, etc.)

## Get the code
If you have git installed, clone the repo with:

```
git clone https://github.com/weirddan455/futex-test.git
```

You can then pull down any future updates with

```
git pull
```

If you don't wish to install git, you can just copy-paste futex-test.c from the web interface into a new file.

## Compiling
To compile, simply run:

```
gcc futex-test.c -o futex-test -O2
```

## Running
To run the program:

```
./futex-test
```

GCC should make the binary executable by default but if not:

```
chmod +x futex-test
```

Optional arguments are --verbose (or -v) and a number of loops to run FUTEX_MULTIPLE.  Ex:

```
./futex-test --verbose 50
```
