#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <asm/termbits.h> /* struct termios2 */
#include <time.h>
#include <ctype.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>
// #include <termios.h>

#define CANUSB_INJECT_SLEEP_GAP_DEFAULT 200 /* ms */
#define CANUSB_TTY_BAUD_RATE_DEFAULT 2000000
#define ID_DEFAULT 0x0005
// #define ID_LSB_DEFAULT 0x05
// #define ID_MSB_DEFAULT 0x00
#define SPEED_DEFAULT CANUSB_SPEED_500000
#define PORT_DEFAULT "/dev/ttyUSB0"
#define DATA_DEFAULT (long int)0x0102030405060708

// long int data_default = 0x0102030405060708;

typedef enum {
  CANUSB_SPEED_1000000 = 0x01,
  CANUSB_SPEED_800000  = 0x02,
  CANUSB_SPEED_500000  = 0x03,
  CANUSB_SPEED_400000  = 0x04,
  CANUSB_SPEED_250000  = 0x05,
  CANUSB_SPEED_200000  = 0x06,
  CANUSB_SPEED_125000  = 0x07,
  CANUSB_SPEED_100000  = 0x08,
  CANUSB_SPEED_50000   = 0x09,
  CANUSB_SPEED_20000   = 0x0a,
  CANUSB_SPEED_10000   = 0x0b,
  CANUSB_SPEED_5000    = 0x0c,
} 
CANUSB_SPEED;

typedef enum {
  CANUSB_MODE_NORMAL          = 0x00,
  CANUSB_MODE_LOOPBACK        = 0x01,
  CANUSB_MODE_SILENT          = 0x02,
  CANUSB_MODE_LOOPBACK_SILENT = 0x03,
} CANUSB_MODE;

typedef enum {
  CANUSB_FRAME_STANDARD = 0x01,
  CANUSB_FRAME_EXTENDED = 0x02,
} CANUSB_FRAME;

typedef enum {
    CANUSB_INJECT_PAYLOAD_MODE_RANDOM      = 0,
    CANUSB_INJECT_PAYLOAD_MODE_INCREMENTAL = 1,
    CANUSB_INJECT_PAYLOAD_MODE_FIXED       = 2,
} CANUSB_PAYLOAD_MODE;




static int terminate_after = 0;
static int program_running = 1;
static int inject_payload_mode = CANUSB_INJECT_PAYLOAD_MODE_FIXED;
static float inject_sleep_gap = CANUSB_INJECT_SLEEP_GAP_DEFAULT;
static int print_traffic = 0;


pthread_t thread_send_id, thread_receiv_id, tid;

int flag = 0;
int enter_key_flag = 1;

// char inputChar;



static CANUSB_SPEED canusb_int_to_speed(int speed)
{
  switch (speed) {
  case 1000000:
    return CANUSB_SPEED_1000000;
  case 800000:
    return CANUSB_SPEED_800000;
  case 500000:
    return CANUSB_SPEED_500000;
  case 400000:
    return CANUSB_SPEED_400000;
  case 250000:
    return CANUSB_SPEED_250000;
  case 200000:
    return CANUSB_SPEED_200000;
  case 125000:
    return CANUSB_SPEED_125000;
  case 100000:
    return CANUSB_SPEED_100000;
  case 50000:
    return CANUSB_SPEED_50000;
  case 20000:
    return CANUSB_SPEED_20000;
  case 10000:
    return CANUSB_SPEED_10000;
  case 5000:
    return CANUSB_SPEED_5000;
  default:
    return 0;
  }
}



static int generate_checksum(const unsigned char *data, int data_len)
{
  int i, checksum;

  checksum = 0;
  for (i = 0; i < data_len; i++) {
    checksum += data[i];
  }

  return checksum & 0xff;
}



static int frame_is_complete(const unsigned char *frame, int frame_len)
{
  if (frame_len > 0) {
    if (frame[0] != 0xaa) {
      /* Need to sync on 0xaa at start of frames, so just skip. */
      return 1;
    }
  }

  if (frame_len < 2) {
    return 0;
  }

  if (frame[1] == 0x55) { /* Command frame... */
    if (frame_len >= 20) { /* ...always 20 bytes. */
      return 1;
    } else {
      return 0;
    }
  } else if ((frame[1] >> 4) == 0xc) { /* Data frame... */
    if (frame_len >= (frame[1] & 0xf) + 5) { /* ...payload and 5 bytes. */
      return 1;
    } else {
      return 0;
    }
  }

  /* Unhandled frame type. */
  return 1;
}



static int frame_send(int tty_fd, const unsigned char *frame, int frame_len)
{
  int result, i;

  if (print_traffic) {
    printf(">>> ");
    for (i = 0; i < frame_len; i++) {
      printf("%02x ", frame[i]);
    }
    if (print_traffic > 1) {
      printf("    '");
      for (i = 4; i < frame_len - 1; i++) {
        printf("%c", isalnum(frame[i]) ? frame[i] : '.');
      }
      printf("'");
    }
    printf("\n");
  }

  result = write(tty_fd, frame, frame_len);
  if (result == -1) {
    fprintf(stderr, "write() failed: %s\n", strerror(errno));
    return -1;
  }

  return frame_len;
}



static int frame_recv(int tty_fd, unsigned char *frame, int frame_len_max)
{
  int result, frame_len, checksum;
  unsigned char byte;
  int first_byte = 1;

  if (print_traffic)
  {
    //fprintf(stderr, "<<< ");
  }

  frame_len = 0;
  //printf("\nReceiver thread: \n\n");
  while (program_running) {
    result = read(tty_fd, &byte, 1);
    if (result == -1) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        fprintf(stderr, "read() failed: %s\n", strerror(errno));
        return -1;
      }

    } else if (result > 0) {
      if (print_traffic)
      {
        // if(first_byte == 1) printf("\n\nReceiver thread:\n");
        // fprintf(stderr, "%02x ", byte);
        first_byte = 0;
      }

      if (frame_len == frame_len_max) {
        fprintf(stderr, "frame_recv() failed: Overflow\n");
        return -1;
      }

      frame[frame_len++] = byte;

      if (frame_is_complete(frame, frame_len)) {
        break;
      }
    }

    usleep(10);
  }

  if (print_traffic)
    //fprintf(stderr, "\n");

  /* Compare checksum for command frames only. */
  if ((frame_len == 20) && (frame[0] == 0xaa) && (frame[1] == 0x55)) {
    checksum = generate_checksum(&frame[2], 17);
    if (checksum != frame[frame_len - 1]) {
      fprintf(stderr, "frame_recv() failed: Checksum incorrect\n");
      return -1;
    }
  }

  return frame_len;
}



static int command_settings(int tty_fd, CANUSB_SPEED speed, CANUSB_MODE mode, CANUSB_FRAME frame)
{
  int cmd_frame_len;
  unsigned char cmd_frame[20];

  cmd_frame_len = 0;
  cmd_frame[cmd_frame_len++] = 0xaa;
  cmd_frame[cmd_frame_len++] = 0x55;
  cmd_frame[cmd_frame_len++] = 0x12;
  cmd_frame[cmd_frame_len++] = speed;
  cmd_frame[cmd_frame_len++] = frame;
  cmd_frame[cmd_frame_len++] = 0; /* Filter ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Filter ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Filter ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Filter ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Mask ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Mask ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Mask ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Mask ID not handled. */
  cmd_frame[cmd_frame_len++] = mode;
  cmd_frame[cmd_frame_len++] = 0x01;
  cmd_frame[cmd_frame_len++] = 0;
  cmd_frame[cmd_frame_len++] = 0;
  cmd_frame[cmd_frame_len++] = 0;
  cmd_frame[cmd_frame_len++] = 0;
  cmd_frame[cmd_frame_len++] = generate_checksum(&cmd_frame[2], 17);

  if (frame_send(tty_fd, cmd_frame, cmd_frame_len) < 0) {
    return -1;
  }

  return 0;
}


//-----------------------------------begin my data-------------------------------------

static int send_my_data_frame(int tty_fd, 
                              CANUSB_FRAME frame,
                              int can_id,
                              long int can_data, 
                              int data_length_code)
{
  #define MAX_FRAME_SIZE 13
  int data_frame_len = 0;
  unsigned char data_frame[MAX_FRAME_SIZE] = {0x00};


  if (data_length_code < 0 || data_length_code > 8)
  {
    fprintf(stderr, "Data length code (DLC) must be between 0 and 8!\n");
    return -1;
  }

  /* Byte 0: Packet Start */
  data_frame[data_frame_len++] = 0xaa;

  /* Byte 1: CAN Bus Data Frame Information */
  data_frame[data_frame_len] = 0x00;
  data_frame[data_frame_len] |= 0xC0; /* Bit 7 Always 1, Bit 6 Always 1 */
  if (frame == CANUSB_FRAME_STANDARD)
    data_frame[data_frame_len] &= 0xDF; /* STD frame */
  else /* CANUSB_FRAME_EXTENDED */
    data_frame[data_frame_len] |= 0x20; /* EXT frame */
  data_frame[data_frame_len] &= 0xEF; /* 0=Data */
  data_frame[data_frame_len] |= data_length_code; /* DLC=data_len */
  data_frame_len++;

  /* Byte 2 to 3: ID */
  data_frame[data_frame_len++] = can_id & 0xFF; /* lsb */
  data_frame[data_frame_len++] = can_id >> 8; /* msb */

  /* Byte 4 to (4+data_len): Data */
  // for (int i = 0; i < data_length_code; i++)
  //   data_frame[data_frame_len++] = data[i];


  // Data frame default
  data_frame[data_frame_len++] = (unsigned char) (can_data >> (7*8));
  data_frame[data_frame_len++] = (unsigned char) ((can_data >> (6*8) ) & 0xFF);
  data_frame[data_frame_len++] = (unsigned char) ((can_data >> (5*8) ) & 0xFF);
  data_frame[data_frame_len++] = (unsigned char) ((can_data >> (4*8) ) & 0xFF);
  data_frame[data_frame_len++] = (unsigned char) ((can_data >> (3*8) ) & 0xFF);
  data_frame[data_frame_len++] = (unsigned char) ((can_data >> (2*8) ) & 0xFF);
  data_frame[data_frame_len++] = (unsigned char) ((can_data >> (1*8) ) & 0xFF);
  data_frame[data_frame_len++] = (unsigned char) (can_data & 0xFF);
  /* Last byte: End of frame */


  data_frame[data_frame_len++] = 0x55;

  if (frame_send(tty_fd, data_frame, data_frame_len) < 0)
  {
    fprintf(stderr, "Unable to send frame!\n");
    return -1;
  }

  return 0;

}

//-------------------------------------------end my data frame-----------------------



static int hex_value(int c)
{
  if (c >= 0x30 && c <= 0x39) /* '0' - '9' */
    return c - 0x30;
  else if (c >= 0x41 && c <= 0x46) /* 'A' - 'F' */
    return (c - 0x41) + 10;
  else if (c >= 0x61 && c <= 0x66) /* 'a' - 'f' */
    return (c - 0x61) + 10;
  else
    return -1;
}




static int convert_from_hex(const char *hex_string, unsigned char *bin_string, int bin_string_len)
{
  int n1, n2, high;

  high = -1;
  n1 = n2 = 0;
  while (hex_string[n1] != '\0') {
    if (hex_value(hex_string[n1]) >= 0) {
      if (high == -1) {
        high = hex_string[n1];
      } else {
        bin_string[n2] = hex_value(high) * 16 + hex_value(hex_string[n1]);
        n2++;
        if (n2 >= bin_string_len) {
          printf("hex string truncated to %d bytes\n", n2);
          break;
        }
        high = -1;
      }
    }
    n1++;
  }

  return n2;
}


// static int inject_data_frame(int tty_fd)
// {
//   int data_len;
//   // unsigned char binary_data[8];
//   struct timespec gap_ts;
//   struct timeval now;
//   int error = 0;

//   gap_ts.tv_sec = inject_sleep_gap / 1000;
//   gap_ts.tv_nsec = (long)(((long long)(inject_sleep_gap * 1000000)) % 1000000000LL);

//   /* Set seed value for pseudo random numbers. */
//   gettimeofday(&now, NULL);
//   srandom(now.tv_usec);


//   data_len = 8;



//   if (gap_ts.tv_sec || gap_ts.tv_nsec)
//     nanosleep(&gap_ts, NULL);

//   if (terminate_after && (--terminate_after == 0))
//     program_running = 0;

//   error = send_my_data_frame(tty_fd, CANUSB_FRAME_STANDARD, ID_DEFAULT, DATA_DEFAULT, data_len);

//   return error;
// }



static void dump_data_frames(int tty_fd)
{
  int i, frame_len;
  unsigned char frame[32];
  struct timespec ts;
  while (program_running) {
    if(flag) break;
    frame_len = frame_recv(tty_fd, frame, sizeof(frame));

    if (! program_running)
      break;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    // printf("\nReceiver thread: \n\n");

    //-------------printf time receiv----------------
    //printf("%lu.%06lu ", ts.tv_sec, ts.tv_nsec / 1000);

    if (frame_len == -1) {
      printf("Frame recieve error!\n");

    } else {

      if ((frame_len >= 6) &&
          (frame[0] == 0xaa) &&
          ((frame[1] >> 4) == 0xc)) 
      {
        if((frame[3] == 0x05 && frame[2] == 0x00) || (frame[3] == 0x00 && frame[2] == 0x01))
        {}
        else
        {
          printf("Frame ID: %02x%02x, Data: ", frame[3], frame[2]);
          for (i = frame_len - 2; i > 3; i--) {
            printf("%02x ", frame[i]);
          }
          printf("\n\n\n");
        }

      } 
      
      else 
      {
        /* printf("Unknown: ");
        for (i = 0; i <= frame_len; i++) {
          printf("%02x ", frame[i]);
        }
        printf("\n\n\n"); */
      }
    }

    if (terminate_after && (--terminate_after == 0))
      program_running = 0;
  }
}



static int adapter_init(const char *tty_device, int baudrate)
{
  int tty_fd, result;
  struct termios2 tio;

  tty_fd = open(tty_device, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (tty_fd == -1) {
    fprintf(stderr, "open(%s) failed: %s\n", tty_device, strerror(errno));
    return -1;
  }

  result = ioctl(tty_fd, TCGETS2, &tio);
  if (result == -1) {
    fprintf(stderr, "ioctl() failed: %s\n", strerror(errno));
    close(tty_fd);
    return -1;
  }

  tio.c_cflag &= ~CBAUD;
  tio.c_cflag = BOTHER | CS8 | CSTOPB;
  tio.c_iflag = IGNPAR;
  tio.c_oflag = 0;
  tio.c_lflag = 0;
  tio.c_ispeed = baudrate;
  tio.c_ospeed = baudrate;

  result = ioctl(tty_fd, TCSETS2, &tio);
  if (result == -1) {
    fprintf(stderr, "ioctl() failed: %s\n", strerror(errno));
    close(tty_fd);
    return -1;
  }

  return tty_fd;
}



static void signal_handle(int signo)
{
  program_running = 0;
  flag = 1;
}


//------------------------------------reveive thread----------------------------------------------
static void *thr_receiv_handle(void *args)
{
  int *cache_ttyid = (int *)args;
  dump_data_frames(*cache_ttyid);
}
//-------------------------------------send thread------------------------------------------------
// static void *thr_send_handle(void *args)
// {
//   while(1)
//   {
//     if(flag) break;
//     int *data_tty_fd = (int *)args;
//     if (inject_data_frame(*data_tty_fd) == -1)
//     {
//       fprintf(stderr, "Inject fail!\n\n\n");
//       sleep(5);
//     } 
//     else
//     {
//       fprintf(stderr, "Inject ok!\n\n\n");
//       sleep(5);
//     }
//   }
// }

//----------------------------------check presskey--------------------------------------------------
static void *readChar(void *args) 
{
  // int print_flag = 0;
  // // char char_buff;
  char inputChar;
  int *data_tty_fd = (int *)args;
  fprintf(stderr, "************* Select diagnostic message *************\n"
                  " 1) Press 'a' to request ReadDataByID service\n"
                  " 2) Press 'b' to request WriteDataByID service\n"
                  " 3) Press 'c' to request DefaultSession service\n"
                  " 4) Press 'd' to requested ExtendSession service ...\n"
                  "\n");
  while(1)
  {
    if(enter_key_flag == 1)
    {
      printf("Enter your choices: ");
      enter_key_flag = 0;
    }
    inputChar = getchar();
    if (inputChar == '\n')
    {
      continue;
    }
    switch (inputChar)
    {
      case 'a':
      {
        fprintf(stderr, "ReadDataByID service does not support now...\n"
                        "please input again: ");
        // your 'a' code goes here
        // example:
        //send_my_data_frame(*data_tty_fd, CANUSB_FRAME_STANDARD, ID_DEFAULT,0xaabbaabbaabbaabb, 8);
        break;
      }
      
      case 'b':
      {
        fprintf(stderr, "Write DataBy ID service does not support now...\n"
                        "please input again: ");
        // your 'b' code goes here
        // example:
        //send_my_data_frame(*data_tty_fd, CANUSB_FRAME_STANDARD, ID_DEFAULT, 0xaa, 8);
        break;
      }

      case 'c':
      {
        send_my_data_frame(*data_tty_fd, CANUSB_FRAME_STANDARD, 0x601, 0x0210010000000000, 8);
        printf("Requested DefaultSession service...\n");
        break;
      } 

      case 'd':
      {
        printf("Requested ExtendSession service ...\n");
        send_my_data_frame(*data_tty_fd, CANUSB_FRAME_STANDARD, 0x601, 0x0210030000000000, 8);

        break;
      } 

      default:
      {
        printf("Invalid input, please input again: ");
        break;
      }
    }
}
return NULL;
}


int main(int argc, char *argv[])
{
  int c, tty_fd, ret;
  char press;
  char *tty_device = NULL; 
  CANUSB_SPEED speed = 0;
  int baudrate = CANUSB_TTY_BAUD_RATE_DEFAULT;
  print_traffic = 1;


//hard fix 
  tty_device = PORT_DEFAULT;
  speed = SPEED_DEFAULT;


  signal(SIGTERM, signal_handle);
  signal(SIGHUP, signal_handle);
  signal(SIGINT, signal_handle);

  if (tty_device == NULL) {
    fprintf(stderr, "Please specify a TTY!\n");
    return EXIT_FAILURE;
  }

  if (speed == 0) {
    fprintf(stderr, "Please specify a valid speed!\n");
    return EXIT_FAILURE;
  }

  tty_fd = adapter_init(tty_device, baudrate);
  if (tty_fd == -1) {
    fprintf(stderr, "open(/dev/ttyUSB0) failed: No such file or directory\n");
    return EXIT_FAILURE;
  }

  //command_settings(tty_fd, speed, CANUSB_MODE_LOOPBACK, CANUSB_FRAME_STANDARD);
  command_settings(tty_fd, speed, CANUSB_MODE_NORMAL, CANUSB_FRAME_STANDARD);



  
  if (ret = pthread_create(&thread_receiv_id, NULL, &thr_receiv_handle, (void *)&tty_fd)) {
      printf("pthread_create() error number=%d\n", ret);
      return -1;
  }

  // if (ret = pthread_create(&thread_send_id, NULL, &thr_send_handle, (void *)&tty_fd)) {
  //     printf("pthread_create() error number=%d\n", ret);
  //     return -1;
  // }

  if (ret = pthread_create(&tid, NULL, &readChar, (void *)&tty_fd)) {
      printf("pthread_create() error number=%d\n", ret);
      return -1;
  }


  pthread_join(thread_receiv_id, NULL);
  // pthread_join(thread_send_id, NULL);
  // pthread_join(tid, NULL);


  return EXIT_SUCCESS;
}
