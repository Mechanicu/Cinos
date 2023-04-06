VDSO_UPDATE_IN := *.c
VDSO_UPDATE_OUT := vdso_update.o
CC := gcc

.PHONY: all
all:
	$(CC) -c -Wall -Wextra -Wno-unused-parameter $(VDSO_UPDATE_IN) -o $(VDSO_UPDATE_OUT)
	cp $(VDSO_UPDATE_OUT) ../

.PHONY: clean
clean:
	rm -f $(VDSO_UPDATE_OUT)