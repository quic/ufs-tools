LSUFS_OBJS := shell.o uic.o query.o ufs_bsg.o common.o lsufs.o
EOM_OBJS := uic.o query.o ufs_bsg.o common.o ufs_eom.o
CC := gcc
CFLAGS := -o0 -g -I. -D_GNU_SOURCE
LDFLAGS :=
RM := rm -f

.PHONY: clean all

all: lsufs ufseom

lsufs: $(LSUFS_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

ufseom: $(EOM_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	@echo -n Cleaning...
	@$(RM) ./*.o
	@$(RM) lsufs
	@$(RM) ufseom
	@echo Done

