#ifndef __SERIAL_COMM_H__
#define __SERIAL_COMM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <termios.h>

typedef struct {
  int fd;
  struct termios tty;
  pthread_mutex_t lock;
} serialport_t;

int serial_init(serialport_t *port, const char *device, int baudrate);

int serial_close(serialport_t *port);

int serial_send(serialport_t *port, const void *data, size_t size);

int serial_receive(serialport_t *port, void *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /*__SERIAL_COMM_H__*/
