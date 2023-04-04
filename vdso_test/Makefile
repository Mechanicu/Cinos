PREFIX := 
CC := $(PREFIX)gcc
MAIN_IN := *.c *.S *.o
MAIN_OUT := vdso_test_main
SUB_DIR1 := ./vdso_user
SUB_DIR2 := ./vdso_init

all:
	make -C $(SUB_DIR1) all
	make -C $(SUB_DIR2) all
	$(CC) -Wall -Wextra -Wno-unused-parameter $(MAIN_IN) -o $(MAIN_OUT)

clean:
	make -C $(SUB_DIR1) clean
	make -C $(SUB_DIR2) clean
	rm -f *.o *.so $(MAIN_OUT)