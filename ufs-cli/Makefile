# Common objects shared by both executables
COMMON_OBJS := uic.o query.o ufs_bsg.o common.o query_trans.o

# Unique objects for each executable
LSUFS_UNIQUE_OBJS := shell.o lsufs.o
EOM_UNIQUE_OBJS := ufs_eom.o

# Combined object lists
LSUFS_OBJS := $(LSUFS_UNIQUE_OBJS) $(COMMON_OBJS)
EOM_OBJS := $(EOM_UNIQUE_OBJS) $(COMMON_OBJS)

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

