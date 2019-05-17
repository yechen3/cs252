#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <unistd.h>

void tty_raw_mode(void)
{
	struct termios tty_attr;
     
	tcgetattr(0,&tty_attr);

	/* Set raw mode. */
	tty_attr.c_lflag &= (~(ICANON|ECHO));
	tty_attr.c_cc[VTIME] = 0;
	tty_attr.c_cc[VMIN] = 1;
     
	tcsetattr(0,TCSANOW,&tty_attr);
}

int main(int argc, char ** argv) 
{
  tty_raw_mode();

  while (1) {
    char ch;
    read(0, &ch, 1);
    printf("%d\n", ch);
  }
}

