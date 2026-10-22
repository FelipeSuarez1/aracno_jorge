#
TARGET = spider

# add 'export SOCEDS_DEST_ROOT=~/altera/14.0/embedded' in /etc/profile
# 
CROSS_COMPILE = arm-linux-gnueabihf-
#CFLAGS = -g -Wall -static -std=gnu++11 -I ${SOCEDS_DEST_ROOT}/ip/altera/hps/altera_hps/hwlib/include -I./bt/inc
CFLAGS = -g -Wall -std=gnu++11 -I ${SOCEDS_DEST_ROOT}/ip/altera/hps/altera_hps/hwlib/include -I./bt/inc
LDFLAGS =  -g -Wall  -lstdc++ -L./bt/lib -lbluetooth -lrt -lpthread
#CC = $(CROSS_COMPILE)gcc
CC = $(CROSS_COMPILE)g++
ARCH= arm

all: $(TARGET)

$(TARGET): Main.o CSpider.o CSpiderLeg.o CMotor.o terasic_os.o mmap.o BtSppCommand.o BtSpp.o  Queue.o QueueCommand.o PIO_LED.o PIO_BUTTON.o
	$(CC) $(LDFLAGS)   $^ -o $@ 

%.o : %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(TARGET) *.a *.o *~
