#
# echo server Build File
#

EXEC	= echo 

all: $(EXEC)

# Which compiler
CC = gcc

STRIP=strip
BASEDIR=

LIBDIR=$(BASEDIR)/lib/
INCDIR=$(BASEDIR)/inc/

# Where are include files kept
INCLUDE = 


# Use this for Linux client software
CDEFINE = -DLINUX 

# Options for development
CFLAGS		= $(INCLUDE) $(CDEFINE) -g -Wall -Os 
LDFLAGS		=
LINKFLAGS	= -o

# add libs for lib crypto TBD
#LIBS		=  -lcrypto -lssl

# Options for release
# CFLAGS = -O -Wall -ansi

#Dependancies for all
DEPENDALL	=  

#Objects put all your .c files here but with .o extentions
OBJ	= echo_server.o yselect.o daemonize.o arch.o

$(EXEC): $(OBJ) $(DEPENDALL) 
	$(CC) $(LINKFLAGS) $(EXEC) $(OBJ) $(LIBS)
	$(STRIP) $(EXEC)

clean:
	-rm *.o $(MYLIB)
	 rm -f *.map
	 rm -f $(EXEC)


