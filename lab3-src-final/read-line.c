/*
 * CS252: Systems Programming
 * Purdue University
 * Example that shows how to read one line with simple editing
 * using raw terminal.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAX_BUFFER_LINE 2048
#define HISTORY_SIZE 16

extern void tty_raw_mode(void);

// Buffer where line is stored
int line_length;
char line_buffer[MAX_BUFFER_LINE];
char right_side_buffer[MAX_BUFFER_LINE];
int right_side_length;

// Simple history array
// This history does not change. 
// Yours have to be updated.
int history_index = 0;
int history_index_rev;
int history_full = 0;
char * history[HISTORY_SIZE];
/*char * history [] = {
  "ls -al | grep x", 
  "ps -e",
  "cat read-line-example.c",
  "vi hello.c",
  "make",
  "ls -al | grep xxx | grep yyy"
};*/
int history_length = HISTORY_SIZE;

void read_line_print_usage()
{
  char * usage = "\n"
    " ctr-a        Move to the beiginning of line\n"
    " ctr-e        Move to the beiginning of line\n"
    " ctr-h        Removes the character at the position before the cursor.\n"
    " ctr-h        Removes the character at the cursor\n"
    " ctrl-?       Print usage\n"
    " Backspace    Deletes last character\n"
    " up arrow     See last command in the history\n";

  write(1, usage, strlen(usage));
}

/* 
 * Input a line with some basic editing.
 */
char * read_line() {

  // Set terminal in raw mode
  tty_raw_mode();

  line_length = 0;
  right_side_length = 0;

  // Read one line until enter is typed
  while (1) {

    // Read one character in raw mode.
    char ch;
    read(0, &ch, 1);

    if (ch>=32 && ch != 127) {
      // It is a printable character. 

      // Do echo
      write(1,&ch,1);

      // If max number of character reached return.
      if (line_length==MAX_BUFFER_LINE-2) break; 
    
      // add char to buffer.
      line_buffer[line_length]=ch;
      line_length++;

      // Check right_side_buffer
      if (right_side_length) {
        for (int i=right_side_length-1; i>=0; i--) {
          char c = right_side_buffer[i];
          write(1,&c,1);
        }
      }
      for (int i=0; i<right_side_length; i++) {
        char c = 8;
        write(1,&c,1);
      }
    }
    else if (ch==10) {
      // <Enter> was typed. Return line
      if (right_side_length) {
        for (int i=right_side_length-1; i>=0; i--) {
          char c = right_side_buffer[i];

          line_buffer[line_length]=c;
          line_length++;
        }
      }

      if (line_length != 0) {
        if (history[history_index]==NULL) 
          history[history_index] = (char *)malloc(MAX_BUFFER_LINE);
  
        strcpy(history[history_index], line_buffer);
        history_index_rev = history_index;
        history_index++;
        if (history_index>=history_length) {
          history_index = 0;
          history_full = 1;
        }
      }

      right_side_length=0;
      // Print newline
      write(1,&ch,1);

      break;
    }
    else if (ch == 31) {
      // ctrl-?
      read_line_print_usage();
      line_buffer[0]=0;
      break;
    }
    else if (ch == 1) {
      // ctrl-A was typed. The cursor moves to the beginning of the line
      int tmp = line_length;
      for (int i=0; i<tmp; i++) {
        char c = 8;
        write(1,&c,1);
        right_side_buffer[right_side_length] = line_buffer[line_length-1];
        right_side_length++;
        line_length--;
      }
    }
    else if (ch == 5) {
      // ctrl-E was typed. The cursor moves to the end of the line
      for (int i=right_side_length-1; i>=0; i--) {
        write(1,"\033[1C",5);
        line_buffer[line_length]=right_side_buffer[right_side_length-1];
        right_side_length--;
        line_length++;
      }
    }
    else if (ch == 4) {
      // ctrl-D was typed

      // Go back one character
      if (line_length == 0) continue;

      for(int i=right_side_length-2; i>=0; i--) {
        char c = right_side_buffer[i];
        write(1,&c,1);
      }
      // Write a space to erase the last character read
      ch = ' ';
      write(1,&ch,1);

      // Go back one character
      for (int i=0; i<right_side_length; i++) {
        char c = 8;
        write(1,&c,1);
      }

      // Remove one character from buffer
      right_side_length--;
    }
    else if (ch == 8 || ch == 127) {
      // <backspace> was typed. Remove previous character read.

      // Removes the character at the cursor
      if (line_length == 0) continue;

      ch = 8;
      write(1,&ch,1);

      for(int i=right_side_length-1; i>=0; i--) {
        char c = right_side_buffer[i];
        write(1,&c,1);
      }
      // Write a space to erase the last character read
      ch = ' ';
      write(1,&ch,1);
      
      // Go back one character
      for (int i=0; i<right_side_length+1; i++) {
        char c = 8;
        write(1,&c,1);
      }

      // Remove one character from buffer
      line_length--;
    }
    else if (ch==27) {
      // Escape sequence. Read two chars more
      //
      // HINT: Use the program "keyboard-example" to
      // see the ascii code for the different chars typed.
      //
      char ch1; 
      char ch2;
      read(0, &ch1, 1);
      read(0, &ch2, 1);
      if (ch1==91 && (ch2==65 || ch2==66)) {
	      // Up arrow. Print next line in history.
       
	      // Erase old line
	      // Print backspaces
	      int i = 0;
	      for (i =0; i < line_length; i++) {
	        ch = 8;
	        write(1,&ch,1);
	      }
       
	      // Print spaces on top
	      for (i =0; i < line_length+right_side_length; i++) {
	        ch = ' ';
	        write(1,&ch,1);
	      }
       
	      // Print backspaces
	      for (i =0; i < line_length+right_side_length; i++) {
	        ch = 8;
	        write(1,&ch,1);
	      }	
        right_side_length = 0;
	      // Copy line from history
	      strcpy(line_buffer, history[history_index_rev]);
	      line_length = strlen(line_buffer);
        int tmp = history_full?history_length:history_index;
        int up_down = ch2 == 65? -1 : 1;
	      history_index_rev=(history_index_rev+up_down)%tmp;
        if (history_index_rev == -1) history_index_rev = tmp-1;
       
	      // echo line
	      write(1, line_buffer, line_length);
      } 
      else if (ch1==91 && ch2==68) {
        // Left arrow. 

        // Move the cursor to the left
        if (line_length == 0) continue;
        ch = 8;
        write(1,&ch,1);
        // Allow insertion 
        right_side_buffer[right_side_length] = line_buffer[line_length-1];
        right_side_length++;
        line_length--;
      }
      else if (ch1==91 && ch2==67) {
        // right arrow. 

        // Move the cursor to the arrow
        if (right_side_length == 0) continue;
        write(1,"\033[1C",5);
        // Allow insertion 
        line_buffer[line_length]=right_side_buffer[right_side_length-1];
        line_length++;
        right_side_length--;
      }
      
    }

  }

  // Add eol and null char at the end of string
  line_buffer[line_length]=10;
  line_length++;
  line_buffer[line_length]=0;

  return line_buffer;
}

