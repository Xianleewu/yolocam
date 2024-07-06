#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

int input_width = 1920;
int input_height = 1080;
int output_width = 480;
int output_height = 480;

static char RKNN_YOLO_MODEL[256] = "/oem/model/yolov5s-640-640.rknn";
static char output_tty[256] = "/dev/ttyS2";
static char ini_config_file[256] = "/usr/share/yolocam/default.ini";
static char remote_host[256] = "192.168.100.11";
static int remote_port = 9900;

void usage(const char *progname) {
    printf("Usage: %s [options]\n", progname);
    printf("Options:\n");
    printf("  -w, --input-width WIDTH          Set input width (default: 1920)\n");
    printf("  -h, --input-height HEIGHT        Set input height (default: 1080)\n");
    printf("  -W, --output-width WIDTH         Set output width (default: 480)\n");
    printf("  -H, --output-height HEIGHT       Set output height (default: 480)\n");
    printf("  -m, --model FILE                 Set RKNN YOLO model file path (default: /oem/model/yolov5s-640-640.rknn)\n");
    printf("  -t, --tty DEVICE                 Set output tty device (default: /dev/ttyS2)\n");
    printf("  -c, --config FILE                Set ini config file path (default: /usr/share/yolocam/default.ini)\n");
    printf("  -r, --remote-host HOST           Set remote host (default: 192.168.100.11)\n");
    printf("  -p, --remote-port PORT           Set remote port (default: 9900)\n");
    printf("  -?, --help                       Show this help message\n");
}

void parse_args(int argc, char **argv) {
    static struct option long_options[] = {
        {"input-width", required_argument, 0, 'w'},
        {"input-height", required_argument, 0, 'h'},
        {"output-width", required_argument, 0, 'W'},
        {"output-height", required_argument, 0, 'H'},
        {"model", required_argument, 0, 'm'},
        {"tty", required_argument, 0, 't'},
        {"config", required_argument, 0, 'c'},
        {"remote-host", required_argument, 0, 'r'},
        {"remote-port", required_argument, 0, 'p'},
        {"help", no_argument, 0, '?'},
        {0, 0, 0, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, "w:h:W:H:m:t:c:r:p:?", long_options,
                              NULL)) != -1) {
        switch (opt) {
        case 'w':
            input_width = atoi(optarg);
            break;
        case 'h':
            input_height = atoi(optarg);
            break;
        case 'W':
            output_width = atoi(optarg);
            break;
        case 'H':
            output_height = atoi(optarg);
            break;
        case 'm':
            strncpy(RKNN_YOLO_MODEL, optarg, sizeof(RKNN_YOLO_MODEL) - 1);
            RKNN_YOLO_MODEL[sizeof(RKNN_YOLO_MODEL) - 1] = '\0';
            break;
        case 't':
            strncpy(output_tty, optarg, sizeof(output_tty) - 1);
            output_tty[sizeof(output_tty) - 1] = '\0';
            break;
        case 'c':
            strncpy(ini_config_file, optarg, sizeof(ini_config_file) - 1);
            ini_config_file[sizeof(ini_config_file) - 1] = '\0';
            break;
        case 'r':
            strncpy(remote_host, optarg, sizeof(remote_host) - 1);
            remote_host[sizeof(remote_host) - 1] = '\0';
            break;
        case 'p':
            remote_port = atoi(optarg);
            break;
        case '?':
        default:
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}
