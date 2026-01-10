# Makefile for AgSys Protocol Tests

CC = gcc
CFLAGS = -Wall -Wextra -I../include -std=c11

# Test binaries
TEST_BIN = protocol_test
VECTOR_GEN = generate_test_vectors

.PHONY: all test vectors clean

all: $(TEST_BIN) $(VECTOR_GEN)

$(TEST_BIN): protocol_test.c
	$(CC) $(CFLAGS) -o $@ $<

$(VECTOR_GEN): generate_test_vectors.c
	$(CC) $(CFLAGS) -o $@ $<

test: $(TEST_BIN)
	./$(TEST_BIN)

vectors: $(VECTOR_GEN)
	./$(VECTOR_GEN) > test_vectors.json
	@echo "Generated test_vectors.json"

clean:
	rm -f $(TEST_BIN) $(VECTOR_GEN) test_vectors.json
