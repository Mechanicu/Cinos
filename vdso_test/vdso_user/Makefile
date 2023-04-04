VDSO_FLAG := -fno-common -shared -fPIC -nostdlib
VDSO_LDFLAG := -Wl,--hash-style=sysv
VDSO_IN := *.c
VDSO_OUT := vdso.so

all:
	@echo "Compiling vdso"
	$(CC) -Wall -Wextra -Wno-unused-parameter $(VDSO_LDFLAG) $(VDSO_FLAG) $(VDSO_IN) -o $(VDSO_OUT)
	cp $(VDSO_OUT) ../

clean:	
	rm -f $(VDSO_OUT) ./*.o