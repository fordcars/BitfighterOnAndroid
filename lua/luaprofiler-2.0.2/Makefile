include config.linux

OBJS= src/clocks.o src/core_profiler.o src/function_meter.o src/stack.o src/lua50_profiler.o


profiler: $(OBJS)
	mkdir -p bin && $(LD) -Bshareable -o $(PROFILER_OUTPUT) $(OBJS)

clean:
	rm -f $(PROFILER_OUTPUT) src/*.o
	
cleano:
	rm -f src/*.o

install:
	install -m 0755 $(PROFILER_OUTPUT) /usr/local/lib/lua/5.1

