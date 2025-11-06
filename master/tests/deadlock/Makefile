include ../Makefile.common

TARGETS = test_deadlock test_deadlock_real
SRC_MUTEX = test_deadlock_mutex_order.c
SRC_REAL = test_deadlock_real.c

all: $(TARGETS)

# Fuentes reales que el test necesita
PROJECT_SRC = \
    ../../src/aging.c \
	../../src/init_master.c \
    ../../src/query_control_manager.c \
	../../src/worker_manager.c \
    ../../src/scheduler.c \
    ../../../utils/src/connection/serialization.c

test_deadlock: $(SRC_MUTEX)
	$(CC) $(CFLAGS) $(SRC_MUTEX) $(PROJECT_SRC) ../unity/src/unity.c \
	    -lcommons -lpthread -lcrypto -o test_deadlock

test_deadlock_real: $(SRC_REAL)
	$(CC) $(CFLAGS) $(SRC_REAL) $(PROJECT_SRC) ../unity/src/unity.c \
	    -lcommons -lpthread -lcrypto -o test_deadlock_real

clean:
	rm -f $(TARGETS) *.o
