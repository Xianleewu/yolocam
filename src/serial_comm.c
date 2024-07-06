#include "serial_comm.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// 初始化串口
int serial_init(serialport_t *port, const char *device, int baudrate) {
    port->fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);
    if (port->fd < 0) {
        perror("open");
        return -1;
    }

    memset(&port->tty, 0, sizeof(port->tty));
    if (tcgetattr(port->fd, &port->tty) != 0) {
        perror("tcgetattr");
        close(port->fd);
        return -1;
    }

    cfsetospeed(&port->tty, baudrate);
    cfsetispeed(&port->tty, baudrate);

    port->tty.c_cflag = (port->tty.c_cflag & ~CSIZE) | CS8;
    port->tty.c_iflag &= ~IGNBRK;
    port->tty.c_lflag = 0;
    port->tty.c_oflag = 0;
    port->tty.c_cc[VMIN] = 0;
    port->tty.c_cc[VTIME] = 5;
    port->tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    port->tty.c_cflag |= (CLOCAL | CREAD);
    port->tty.c_cflag &= ~(PARENB | PARODD);
    port->tty.c_cflag &= ~CSTOPB;
    port->tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(port->fd, TCSANOW, &port->tty) != 0) {
        perror("tcsetattr");
        close(port->fd);
        return -1;
    }

    if (pthread_mutex_init(&port->lock, NULL) != 0) {
        perror("pthread_mutex_init");
        close(port->fd);
        return -1;
    }

    return 0;
}

int serial_close(serialport_t *port) {
    if (pthread_mutex_destroy(&port->lock) != 0) {
        perror("pthread_mutex_destroy");
        return -1;
    }
    return close(port->fd);
}

int serial_send(serialport_t *port, const void *data, size_t size) {
    pthread_mutex_lock(&port->lock);
    int result = write(port->fd, data, size);
    pthread_mutex_unlock(&port->lock);
    return result;
}

int serial_receive(serialport_t *port, void *buffer, size_t size) {
    pthread_mutex_lock(&port->lock);
    int result = read(port->fd, buffer, size);
    pthread_mutex_unlock(&port->lock);
    return result;
}

