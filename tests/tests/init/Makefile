BIN =  proc_init.bin proc_init_timeout.bin cmd_line_args.bin \
	kill_procs.bin hello_world.bin print_to.bin \
	null_ptrs.bin numa_check.bin strong_sym.bin hello_world_build.bin \
	local_rank.bin initialized.bin

CFLAGS+=-I../

build: $(BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c $^

%.bin: %.o ../test_utils.o
	$(CC) $(CFLAGS) $(LIB_PATH) -o $@ $^ $(LIBS)

clean:
	rm -rf *~ \#_* *.o *.bin

.PHONY: clean