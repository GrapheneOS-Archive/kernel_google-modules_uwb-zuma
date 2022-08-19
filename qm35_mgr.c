#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>

#include "uci_ioctls.h"

#define UCI_DEV_FILE "/dev/" UCI_DEV_NAME

static const char * const state[] = {
	[QM35_CTRL_STATE_UNKNOWN] = "unknown",
	[QM35_CTRL_STATE_OFF] = "off",
	[QM35_CTRL_STATE_RESET] = "reset",
	[QM35_CTRL_STATE_COREDUMP] = "coredump",
	[QM35_CTRL_STATE_READY] = "ready",
	[QM35_CTRL_STATE_FW_DOWNLOADING] = "firmware downloading",
	[QM35_CTRL_STATE_UCI_APP] = "UCI application"
};

struct qm35_ctx {
	int fd;
	unsigned int state;
	unsigned long int ioctl_cmd;
	char *read_buf;
	char *write_buf;
	unsigned long len;
};

/* Prints usage information for this program to STREAM (typically
 * stdout or stderr), and exit the program with EXIT_CODE.  Does not
 * return.
 */
static void print_usage(FILE *stream, int exit_code, const char *path)
{
	/* take only the last portion of the path */
    const char *basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;

	fprintf(stream, "Usage:  %s [options]\n", basename);
	fprintf(stream, "Options are:\n"
		"  -h  --help             Display this usage information.\n"
		"  -s  --state            Get device state.\n"
		"  -r  --reset            Reset the device.\n"
		"  -u  --upload           Enter firmware upload.\n");
	fprintf(stream, "Usage:  %s -f buffer\n", basename);
	fprintf(stream,
		"  -f  --feed             Feed data to device.\n");
	fprintf(stream, "Usage:  %s -d length\n", basename);
	fprintf(stream,
		"  -d  --dump             Dump data from device.\n");
	exit(exit_code);
}

static int get_qm35_dev_fd(struct qm35_ctx * const qm35_hdl)
{
	int err = 0;

	if (qm35_hdl->fd != -1)
		return -EBUSY;

	qm35_hdl->fd = open(UCI_DEV_FILE, O_RDWR);
	if (qm35_hdl->fd == -1) {
		perror("UCI: open() failed\n");
		return -1;
	}

	return 0;
}


static int send_ioctl(struct qm35_ctx * const qm35_hdl)
{
	int err = 0;

	err = get_qm35_dev_fd(qm35_hdl);
	if (err)
		return -1;

	err = ioctl(qm35_hdl->fd, qm35_hdl->ioctl_cmd, &qm35_hdl->state);
	if (err) {
		perror("UCI: ioctl() command failed!\n");
		return -1;
	}

	fprintf(stdout, "QM35 controller state: %s\n", state[qm35_hdl->state]);

	err = close(qm35_hdl->fd);
	if (err) {
		perror("Error during close() of main FD");
		return -1;
	}

	qm35_hdl->fd = -1;

	return 0;
}

static int feed(struct qm35_ctx * const qm35_hdl)
{
	int err = 0;

	err = get_qm35_dev_fd(qm35_hdl);
	if (err)
		return -1;

	qm35_hdl->len = strlen(qm35_hdl->write_buf);
	write(qm35_hdl->fd, (void *) qm35_hdl->write_buf, qm35_hdl->len);

	err = close(qm35_hdl->fd);
	if (err) {
		perror("Error during close() of main FD");
		return -1;
	}

	qm35_hdl->fd = -1;

	return err;
}

static int dump(struct qm35_ctx * const qm35_hdl)
{
	int err = 0;

	err = get_qm35_dev_fd(qm35_hdl);
	if (err)
		return -1;

	qm35_hdl->read_buf = calloc(1, qm35_hdl->len);
	if (!qm35_hdl->read_buf) {
		err = -ENOMEM;
		goto err_close_device;
	}

	read(qm35_hdl->fd, (void *) qm35_hdl->read_buf, qm35_hdl->len);
	fprintf(stdout, "QM35 value read: ");
	for(int i = 0; i < qm35_hdl->len; i++)
		fprintf(stdout, "0x%02X ", qm35_hdl->read_buf[i]);
	fprintf(stdout, "\n");

err_close_device:
	err = close(qm35_hdl->fd);
	if (err) {
		perror("Error during close() of main FD");
		return -1;
	}

	qm35_hdl->fd = -1;

	return err;
}

int main(int argc, char *argv[])
{
	struct qm35_ctx *qm35_hdl;
	int err = 0;
	int c;
	int help_flag = 0;

	static struct option long_options[] = {
		{"help", no_argument, 0, 'h' },
		{"state", no_argument, 0, 's' },
		{"reset", no_argument, 0, 'r' },
		{"upload", no_argument, 0, 'u'},
		{"feed", required_argument, 0, 'f' },
		{"dump", required_argument, 0, 'd' },
		{0, 0, 0, 0 } /* Required at end of array. */
	};

	const char *short_options = "hsrf:d:";

	qm35_hdl = calloc(1, sizeof(*qm35_hdl));
	if (!qm35_hdl)
		return -ENOMEM;

	qm35_hdl->fd = -1;

	do {
		c = getopt_long_only(argc, argv, short_options, long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			help_flag = 1;
			break;
		case 's':
			qm35_hdl->ioctl_cmd = QM35_CTRL_GET_STATE;
			break;
		case 'r':
			qm35_hdl->ioctl_cmd = QM35_CTRL_RESET;
			break;
		case 'u':
			qm35_hdl->ioctl_cmd = QM35_CTRL_FW_UPLOAD;
			break;
		case 'f':
			qm35_hdl->write_buf = optarg;
			break;
		case 'd':
			qm35_hdl->len = atoi(optarg);
			break;
		case '?':
			print_usage(stderr, 1, argv[0]);
		default:
			fprintf(stdout, "getopt returned %c code\n", c);
		}
	} while (1);

	if (help_flag) {
	        print_usage(stdout, 0, argv[0]);
		goto free_qm35;
	}

	if (qm35_hdl->ioctl_cmd) {
		err = send_ioctl(qm35_hdl);
		goto free_qm35;
	}

	if (qm35_hdl->write_buf) {
		err = feed(qm35_hdl);
		goto free_qm35;
	}

	err = dump(qm35_hdl);

free_qm35:
	free(qm35_hdl);
	return err;
}
