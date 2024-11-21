all: target debugger

# Build statically, as a hack to simplify the example. That ensures
# that the code always appears at the same address, freeing
# the "debugger" from having to infer addresses in the target. 
target: target.c
	gcc -static -o $@ $<

debugger: debugger.c
	gcc -Wall -o $@ $<
