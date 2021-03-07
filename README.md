# futex-test
Simple C program for Linux that tests if the kernel supports additional futex functions used by Wine

The FUTEX_MULTIPLE test was adapted from the example in the futex man page. Use --verbose to print the demonstration to console.
The futex2 test simply makes a syscall for the new futex implementation and checks if the kernel throws an error.

If your kernel does not support a futex extension, you should see a "Function not implemented" error.
If you get another error, it's possible there's a bug. Open an Issue and post the output as well as your system information.

Neither of these are currently supported by a vanilla kernel.  Patches can be found at https://github.com/Frogging-Family/linux-tkg

## Prerequisites
You will need to have gcc installed as well as the standard C library header files usually located in /usr/include
On Arch Linux, the headers are part of the glibc package so simply installing gcc should be sufficient:

```
# pacman -S gcc
```

If you run into issues, try installing the base-devel package group:

```
# pacman -S base-devel
~~~

TODO: Write instructions for other popular distros (Ubuntu, Debian, Fedora, etc.)

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
