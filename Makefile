CLI_INCLUDES := $(shell pkg-config --cflags opencv) 
CLI_LIBS := $(shell pkg-config --libs opencv) 
SRV_INCLUDES :=
SRV_LIBS :=
CFLAGS += $(CLI_INCLUDES) $(SRV_INCLUDES)

SRV_OBJ := \
	server.o

CLI_OBJ := \
       client.o	\
       if.o	\
       net.o

all: camview camserv 

camview: $(CLI_OBJ)
	$(CC) -o $@ $(CLI_OBJ) $(CLI_LIBS)

camserv: $(SRV_OBJ)
	$(CC) -o $@ $(SRV_OBJ) $(SRV_LIBS)
	
clean:
	$(RM) $(CLI_OBJ) $(SRV_OBJ) $(MTN_OBJ) camview camserv
