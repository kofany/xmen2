CC	=	gcc
NAME	=	xmen
LDFLAGS	=	-s
CFLAGS	=	-O2 -Wall -D_BSD_SOURCE

OBJS	=	main.o clones.o irc.o parse.o action.o command.o

all: $(NAME)

$(NAME): $(OBJS)
	rm -f $(NAME)
	$(CC) -O2 -Xlinker -s -o $(NAME) $(OBJS)

clean:
	rm -f $(OBJS) $(NAME)
