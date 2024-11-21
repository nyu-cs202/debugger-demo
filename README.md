# debugger-demo
Demo of ptrace and simple debugging functionality

This demo is intended to be run on the CS202 Docker image, specifically
the x86-64 version. If you wish to port the demo to ARM, please let me
(Mike) know, and we will consider extra credit.

Go into your CS202 setup, clone this repository, and then enter Docker: 
```
$ cd ~/cs202
$ git clone <this repo>
$ ./cs202-run-docker
```

Then:
```
$ cd debugger-demo
$ make
```

You should get two binaries: `debugger` and `target`.

Now, run the target by itself:
```
$ ./target
./target: x = 10
./target: x = 10
```

Then you can run the target under the control of the "debugger" and
observe the different output:

```
$ ./debugger
instruction at addr 0x401787 is now: 0xffffffffffffffcc
target: x = 10
Checking: *(%rbp-4): 10
Checking: *(%rbp-4): 202
%rip in target is 0x401788 but we want it to be 0x401787
target: x = 202
target exited
```

The lines to look at are the ones prefixed with `target:`. Those show you
that `x` has a different value for the second `printf`.

