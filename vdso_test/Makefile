PREFIX := 
CC := $(PREFIX)gcc
MAIN_IN := *.c *.S *.o
MAIN_OUT := vdso_test_main
MAIN_LDFLAGS := -lpthread

SUB_DIR = vdso_user vdso_init vdso_update
SUB_DIR_CLEAN = $(SUB_DIR:%=%_clean)

.PHONY: all subdirs test
all: subdirs test

test:
	$(CC) -Wall -Wextra -Wno-unused-parameter $(MAIN_IN) $(MAIN_LDFLAGS) -o $(MAIN_OUT)

.PHONY: subdirs $(SUB_DIR)

subdirs: $(SUB_DIR)

$(SUB_DIR):
	make -C $@ all


.PHONY: clean $(SUB_DIR_CLEAN)
clean:$(SUB_DIR_CLEAN)
	rm -rf $(MAIN_OUT) ./*.o ./*.so

$(SUB_DIR_CLEAN):
	make  -C $(@:%_clean=%) clean