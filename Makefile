
CC ?= gcc
 
APP_NAME = Sistema_seguridad
OBJS = Sistema_seguridad.o
 
all: $(APP_NAME)
 
$(APP_NAME): $(OBJS)
	$(CC) -o $@ $^
 
%.o: %.c
	$(CC) -c $^ -o $@
 
 
clean:
	rm *.o $(APP_NAME)
