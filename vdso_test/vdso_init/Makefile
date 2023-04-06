VDSO_INIT_IN := *.c
VDSO_INIT_OUT := vdso_init.o

.PHONY: all
all:
	$(CC) -c -Wall -Wextra -Wno-unused-parameter $(VDSO_INIT_IN) -o $(VDSO_INIT_OUT)
	cp $(VDSO_INIT_OUT) ../

.PHONY: clean
clean:
	rm -f $(VDSO_INIT_OUT)