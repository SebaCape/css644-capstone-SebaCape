CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
LDFLAGS = -lpthread

SERVER = db_server
CLIENT = db_client
BENCH = db_benchmark

all: $(SERVER) $(CLIENT) $(BENCH)

$(SERVER): kvstore_server.o kvstore.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(CLIENT): kvstore_client.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BENCH): benchmark.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SERVER) $(CLIENT) $(BENCH) *.o data.db data.tmp server.pid /tmp/db_socket

# Benchmarking targets
bench: $(SERVER) $(BENCH)
	@echo "Starting server..."
	@./$(SERVER) > /dev/null 2>&1 &
	@SERVER_PID=$$!; \
	sleep 1; \
	echo "Running benchmarks..."; \
	./$(BENCH) all; \
	kill $$SERVER_PID 2>/dev/null || true; \
	rm -f /tmp/db_socket data.db data.tmp server.pid

bench-seq-set: $(SERVER) $(BENCH)
	@./$(SERVER) > /dev/null 2>&1 &
	@SERVER_PID=$$!; \
	sleep 1; \
	./$(BENCH) seq_set 1000; \
	kill $$SERVER_PID 2>/dev/null || true; \
	rm -f /tmp/db_socket data.db data.tmp server.pid

bench-seq-get: $(SERVER) $(BENCH)
	@./$(SERVER) > /dev/null 2>&1 &
	@SERVER_PID=$$!; \
	sleep 1; \
	./$(BENCH) seq_get 500; \
	kill $$SERVER_PID 2>/dev/null || true; \
	rm -f /tmp/db_socket data.db data.tmp server.pid

bench-concurrent: $(SERVER) $(BENCH)
	@./$(SERVER) > /dev/null 2>&1 &
	@SERVER_PID=$$!; \
	sleep 1; \
	./$(BENCH) concurrent 8 250; \
	kill $$SERVER_PID 2>/dev/null || true; \
	rm -f /tmp/db_socket data.db data.tmp server.pid

bench-mixed: $(SERVER) $(BENCH)
	@./$(SERVER) > /dev/null 2>&1 &
	@SERVER_PID=$$!; \
	sleep 1; \
	./$(BENCH) mixed 1000; \
	kill $$SERVER_PID 2>/dev/null || true; \
	rm -f /tmp/db_socket data.db data.tmp server.pid

bench-size: $(SERVER) $(BENCH)
	@./$(SERVER) > /dev/null 2>&1 &
	@SERVER_PID=$$!; \
	sleep 1; \
	./$(BENCH) size 100; \
	kill $$SERVER_PID 2>/dev/null || true; \
	rm -f /tmp/db_socket data.db data.tmp server.pid

test: $(SERVER) $(CLIENT)
	@echo "Starting server..."
	@./$(SERVER) > /dev/null 2>&1 &
	@SERVER_PID=$$!; \
	sleep 1; \
	echo "Running quick functionality test..."; \
	(echo "set test_key test_value"; sleep 0.5; echo "get test_key"; sleep 0.5; echo "size"; sleep 0.5; echo "quit") | ./$(CLIENT); \
	kill $$SERVER_PID 2>/dev/null || true; \
	rm -f /tmp/db_socket data.db data.tmp server.pid

.PHONY: all clean bench bench-seq-set bench-seq-get bench-concurrent bench-mixed bench-size test