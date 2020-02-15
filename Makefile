CC=gcc
CFLAGS=-I. 
#CFLAGS=

OBJECTS = cli srv


all: $(OBJECTS)

$(OBJECTS):%:%.c
	@echo Compiling $<  to  $@
	$(CC) -o $@ $< $(CFLAGS)

	
clean:
	rm  $(OBJECTS) 
