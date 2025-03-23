all: server client

server:
	$(MAKE) -C server

client:
	$(MAKE) -C client

clean:
	$(MAKE) -C server clean
	$(MAKE) -C client clean

.PHONY: all server client clean
