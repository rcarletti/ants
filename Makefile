#---------------------------------------------------
# Target file to be compiled by default
#------ ---------------------------------------------
MAIN = ants
#---------------------------------------------------
# CC will be the compiler to use
#---------------------------------------------------
CC = gcc
#---------------------------------------------------
# CC options
#---------------------------------------------------
CFLAGS = -Wall -g
CLIBS = -lpthread -lrt `allegro-config --libs`
#---------------------------------------------------
# Additional programs
#---------------------------------------------------
RM = rm
#---------------------------------------------------
# Dependencies
#---------------------------------------------------
$(MAIN) : $(MAIN).o ptask.o
	$(CC) $(CFLAGS) -o $(MAIN) $(MAIN).o ptask.o $(CLIBS)

$(MAIN).o: $(MAIN).c
	$(CC) $(CFLAGS) -c $(MAIN).c

ptask.o: ptask.c
	$(CC) $(CFLAGS) -c ptask.c

clean:
	$(RM) $(MAIN) *.o

