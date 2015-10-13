TARGET = libnative.so
#
SRCS =\
					./native.c \
					./cache.c \
					./ccstmnative.c \
#
OBJS =\
					./native.o \
					./cache.o \
					./ccstmnative.o \
#

$(TARGET) : $(OBJS)
						gcc -Wall -shared -o $(TARGET) $(OBJS)
						@rm -f $(OBJS)
						@echo "libnative.so make successfull"

$(OBJS)   : $(SRCS)
						gcc -fPIC -I /usr/local/jamvm/include -c $(SRCS)
						@echo "*.o make successfull"
install : $(TARGET)
						@echo "Installing $(TARGET) in ~/ldlib/"
						@export LD_LIBRARY_PATH=~/ldlib/
						@cp $(TARGET) ~/ldlib/

clean   :; @rm -f $(OBJS)
					 @rm -f $(TARGET)
