// grump -- a very simple serial port interface
//
// TODO:
// 1. atexit() handler to put the terminal back to normal
// 2. signal handler to ditto
// 3. consider mimicking mimicking miniterm.py's "exit" keybinding?
// 4. ditto for it's "obey a file"? do I ever want to use that?
//
//      (miniterm.py says "Quit: Ctrl+]  |  Upload: Ctrl+U")

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#ifndef __linux__
#include <sys/ttycom.h>
#endif
#include <stdbool.h>

typedef unsigned char byte;

static void print_usage()
{
  fprintf(stderr,
          "Usage: grump [-stop <1|2>] [-parity <odd|even|none>] [-speed <speed>] \n"
          "             [-shake <none|ctsrts|xon>] [<serial device>]\n"
          "\n"
          "A very simple program for interacting over the serial port.\n"
          "\n"
          "       <stop>  defaults to 1\n"
          "       <parity> defaults to none\n"
          "       <speed> defaults to 115200\n"
          "       <shake> defaults to off\n"
          "       <serial device> defaults to /dev/ttyUSB0\n"
          "\n"
          "So, for instance:\n"
          "\n"
          "    grump\n"
          "    grump /dev/ttyUSB0\n"
          "    grump /dev/ttyS0\n"
          "\n"
          "Beware that CTRL-C both sends CTRL-C and also breaks out of grump.\n"
          "Future versions of this software may try to do something more sensible.\n"
          );
}

int main(int argc, char **argv)
{
    int    serial_fd;
    // file_fd = -1;
  char  *serial_portname = NULL;
  struct termios term, saved_term;
  fd_set rd_map;
  int    speed = 115200;
  int    bspeed;
  int parity = 0; // -1 for odd, +1 for even.
  int stop = 1;
  int shake = 0; // 1 - crtscts, 2 - xonoff (not yet supported)

  int ii = 1;

  while (ii < argc)
  {
    if (argv[ii][0] == '-')
    {
      if (!strcmp("--help",argv[ii]) || !strcmp("-help",argv[ii]) ||
          !strcmp("-h",argv[ii]))
      {
        print_usage();
        return 0;
      }
      else if (!strcmp("-shake", argv[ii]))
      {
          ++ii;
          // Account for user confusion .., 
          if (!strcmp("rtscts", argv[ii]) || !strcmp("ctsrts", argv[ii]))
          {
              shake = 1;
          }
          else if (!strcmp("xon", argv[ii]))
          {
              shake = 2;
          }
          else if (!strcmp("none", argv[ii]) || !strcmp("off", argv[ii]))
          {
              shake = 0;
          }
          else
          {
              fprintf(stderr, "### grump: Invalid handshake type - '%s'\n",
                      argv[ii]);
              return 1;
          }
      }
      else if (!strcmp("-parity", argv[ii]))
      {
          ++ii;
          if (!strcmp(argv[ii],"odd")) 
          {
              parity = -1; 
          }
          else if (!strcmp(argv[ii], "none"))
          {
              parity = 0;
          }
          else if (!strcmp(argv[ii], "even"))
          {
              parity = 1;
          }
      }
      else if (!strcmp("-stop", argv[ii]))
      {
          ++ii;
          stop = atoi(argv[ii+1]);
          if (stop != 1 && stop != 2)
          {
              fprintf(stderr, "### grump: Can only cope with 1 or 2 stop bits - '%s'\n",
                      argv[ii]);
              return 1;
          }
      }
      else if (!strcmp("-speed",argv[ii]))
      {
        speed = atoi(argv[ii+1]);
        ii++;
      }
      else
      {
        fprintf(stderr,"### grump: "
                "Unrecognised command line switch '%s'\n",argv[ii]);
        return 1;
      }
    }
    else
    {
      serial_portname = argv[ii];
    }
    ii++;
  }

  if (serial_portname == NULL)
    serial_portname = "/dev/ttyUSB0";

  switch (speed)
  {
    case 50: bspeed = B50; break;
    case 75: bspeed = B75; break;
    case 110: bspeed = B110; break;
    case 134: bspeed = B134; break;
    case 150: bspeed = B150; break;
    case 200: bspeed = B200; break;
    case 300: bspeed = B300; break;
    case 600: bspeed = B600; break;
    case 1200: bspeed = B1200; break;
    case 1800: bspeed = B1800; break;
    case 2400: bspeed = B2400; break;
    case 4800: bspeed = B4800; break;
    case 9600: bspeed = B9600; break;
    case 19200: bspeed = B19200; break;
    case 38400: bspeed = B38400; break;
    case 57600: bspeed = B57600; break;
    case 115200: bspeed = B115200; break;
    case 230400: bspeed = B230400; break;
    default:
      printf("Speed %d not supported\n",speed);
      return 1; 
  }

  /* Open the port.  NB. without O_NONBLOCK, this would wait until the	*/
  /* modem raises carrier detect.					*/
  serial_fd = open(serial_portname, O_RDWR | O_NONBLOCK);
  if (serial_fd < 0)
  {
    fprintf(stderr,"Cannot open serial port '%s': %s\n",
            serial_portname,strerror(errno));
    return 1;
  }

  // Adjust the port settings.  This is done by reading the current setings
  // into memory, making any desired change to that in-memory copy, then
  // writing them back into the terminal device so that all the changes come
  // into effect. See termios(4) man page for detail

  // Save the current terminal settings, so we can reset to them later on
  tcgetattr(serial_fd, &term);
  // Set the port speed as requested
  cfsetispeed(&term,bspeed);
  cfsetospeed(&term,bspeed);
  // Turn off canonical processing (line buffer, backspace etc)
  cfmakeraw(&term);
  // Turn of carrier detect (ie. make /dev/ttyd0 work like cuaa0
  term.c_cflag |= CLOCAL | CREAD;
  term.c_cflag &= ~(PARENB | PARODD);
  if (parity < 0)
  { 
      // Odd parity
      term.c_cflag |= PARENB | PARODD;
  }
  else if (parity > 0)
  {
      // Even parity
      term.c_cflag |= PARENB;
  }
  if (stop == 2)
  {
      term.c_cflag |= CSTOPB;
  }
  else 
  {
      term.c_cflag &= ~CSTOPB;
  }

  term.c_cflag &= ~(CRTSCTS);
  term.c_iflag &= ~(IXON);
  switch (shake)
  {
  case 0: break;
  case 1: term.c_cflag |= CRTSCTS; break;
  case 2: term.c_iflag |= IXON; break;
  }
      
#ifndef __linux__
  // Turn off RTS/CTS flow control (probably off anyhow, but make sure)
  term.c_cflag &= ~(CCTS_OFLOW | CRTS_IFLOW);
#endif
  // And write the changes out so they take effect.
  tcsetattr(serial_fd, TCSANOW, &term);

  // Similarly, disable line buffering of the keyboard, but remember the old
  // settings to restore on exit
  tcgetattr(STDIN_FILENO, &term);
  saved_term = term;
  cfmakeraw(&term);
  tcsetattr(STDIN_FILENO, TCSANOW, &term);


  printf(">> grump ready (speed %d[%d])\r\n",speed,bspeed);
  FD_ZERO(&rd_map);
  for (;;)
  {
    int ret;

    FD_SET(serial_fd, &rd_map);
    FD_SET(STDIN_FILENO, &rd_map);
    if ((ret = select(FD_SETSIZE, &rd_map, NULL, NULL, NULL)) < 0)
    {
      fprintf(stderr,"Error waiting for input: %s",strerror(errno));
      return 1;
    }
    // If we have input from the user, send it down the serial port
    if (FD_ISSET(STDIN_FILENO, &rd_map))
    {
      ssize_t len;
      unsigned char ch;
      len = read(STDIN_FILENO, &ch, sizeof(ch));
      if (len != 1)
      {
        // really should check what happened more carefully
          printf("!!! read %d chars instead of 1\n", (int)len);
        continue;
      }
      else if (ch == 3)
      {
          ssize_t rv;
          // The user typed CTRL-C. Send it down the serial port, but also take
          // it as an instruction to set the keyboard back to normal and exit.
          rv = write(serial_fd, &ch, 1);
          tcsetattr(STDIN_FILENO, TCSANOW, &saved_term);
          (void)rv;
          return 0;
      }
      else
      {
          ssize_t rv;
          rv = write(serial_fd, &ch, 1);
          (void)rv;
      }
    }
    // If we have output from the serial port, reflect it to the user
    if (FD_ISSET(serial_fd, &rd_map))
    {
      byte bb;
      ssize_t len = read(serial_fd, &bb, 1);
      if (len != 1)
      {
        fprintf(stderr,"Error reading from serial port (read %d): %s",
                (int)len,strerror(errno));
        return 1;
      }
      {
          ssize_t rv;
          rv = write(STDOUT_FILENO, &bb, 1);
          (void)rv;
      }
    }
  }
}

/* End file */
