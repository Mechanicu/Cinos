VDSO_INIT_IN := *.c
VDSO_INIT_OUT := vdso_init.o

all:
	$(CC) -c -Wall -Wextra -Wno-unused-parameter $(VDSO_INIT_IN) -o $(VDSO_INIT_OUT)
	cp $(VDSO_INIT_OUT) ../

clean:
	rm -f $(VDSO_INIT_OUT)