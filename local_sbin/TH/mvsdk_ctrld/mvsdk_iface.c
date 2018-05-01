/* mvsdk control client */
#include<sys/ipc.h>
#include<sys/types.h>
#include<sys/shm.h>
#include<sys/msg.h>

#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/types.h>
#include<unistd.h>
#include<time.h>
#include<string.h>
#include<errno.h>

#include"mvsdk_ctrld.h"

// #define PID_FILE "/var/run/mvsdk_iface.pid"
#define PID_FILE "/tmp/mvsdk_iface.pid"
#define PID_STR_LENGTH (12)

#define LOG_INFO " mvsdk_iface (II) "
#define LOG_ERROR " mvsdk_iface  (EE) "
#define PRODUCT_NAME_LENGTH (12)

static int shm_id;
static void *shm_pt;
static int msq_id_MISO;
static int msq_id_MOSI;
static int init_success_flag = -1;
/* as device is not thread-safe,
 * needs to record and tell last shm_attach pid */

static int init_success_handler(void) {
	int fd_pid;
	char pid_str[PID_STR_LENGTH];

	/* save the successful pid */
	memset(pid_str, '\0', PID_STR_LENGTH); 	// clear
	snprintf(pid_str, PID_STR_LENGTH, "%d", getpid()); // set
	pid_str[PID_STR_LENGTH - 1] = '\n'; // stop byte for a line
	fd_pid = open(PID_FILE, O_WRONLY | O_CREAT, 0644);
	if(fd_pid < 0) {
        printf(LOG_ERROR"open() %s to write pid error: %s\n", PID_FILE, strerror(errno));
		return -1;
    }
	write(fd_pid, pid_str, PID_STR_LENGTH);
	close(fd_pid);

	printf(LOG_INFO"ipc_init: ipc_init() successfully pid %d\n", getpid());
	init_success_flag = 0;
	return 0;
}

static int init_failed_handler(void) {
	int fd_pid;
	char pid_str[PID_STR_LENGTH];
	int last_pid = -1; 

	/* get the last running pid */
	fd_pid = open(PID_FILE, O_RDONLY | O_NONBLOCK);
	if(fd_pid < 0)
		return -1;
	memset(pid_str, '\0', PID_STR_LENGTH); // clear
	read(fd_pid, pid_str, PID_STR_LENGTH); // set
	close(fd_pid);
	pid_str[PID_STR_LENGTH - 1] = '\0';    // string stop byte
	last_pid = atoi(pid_str);

	printf(LOG_ERROR"init failed, last running process is %d.\n", last_pid);
	init_success_flag = -1;
	return -1;
}

static int ipc_init(void) {
	int ret, errno_r;
	int shm_id; key_t shm_key;
	struct shmid_ds shm_state;
	key_t msq_key_MISO;
	key_t msq_key_MOSI;
	struct msqid_ds buf;
	// 1: create shared-memory
	shm_key = ftok(IPCKEY_PATHNAME, shm_key_proj_id);
	if(shm_key == -1) {
		errno_r = errno;
		printf(LOG_ERROR"ipc_init: ftok() failed to create shm_key: %s\n",
				strerror(errno_r));
		goto err1; //return -1;
	}

	shm_id = shmget(shm_key, 0, 0400);
	if(shm_id == -1) {
		errno_r = errno;
		printf(LOG_ERROR"ipc_init: shmget() failed to get shm_id: %s\n",
				strerror(errno_r));
		goto err1; //return -1;
	}

	/* as device is not thread-safe,
	 * it needs to detect multi-threads/process */
	ret = shmctl(shm_id, IPC_STAT, &shm_state);
	if(ret < 0) {
		errno_r = errno;
		printf(LOG_ERROR"ipc_init: shmctl() failed to get shm state: %s\n",
				strerror(errno_r));
		goto err2;
	}
	if(shm_state.shm_nattch > 1) {// another client process using
	    printf(LOG_ERROR"init failed, another client process using\n");
		goto err2;
    } else if(shm_state.shm_nattch < 1) { // daemon crashed
	    printf(LOG_ERROR"init failed, daemon may crashed\n");
        goto err2;
    }

	shm_pt = shmat(shm_id, NULL, 0);
	if(shm_pt == (void *)-1) {
		errno_r = errno;
		printf(LOG_ERROR"ipc_init: shmat() failed to attach shm: %s\n",
				strerror(errno_r));
		goto err2; // return -1;
	}
	// for debug
	// *(short *)shm_pt = 0xfa;
	// shmctl(shm_id, IPC_RMID, NULL);

	// 2: create message queue MISO
	msq_key_MISO = ftok(IPCKEY_PATHNAME, msq_key_MISO_proj_id);
	if(msq_key_MISO == -1) {
		errno_r = errno;
		printf(LOG_ERROR"ipc_init: ftok() failed to create msq_key_MISO: %s\n",
				strerror(errno_r));
		goto err2; // return -1;
	}

	msq_id_MISO = msgget(msq_key_MISO, 0200);
	if(msq_id_MISO == -1) {
		errno_r = errno;
		printf(LOG_ERROR"ipc_init: msgget() failed to get msq_id_MISO: %s\n",
				strerror(errno_r));
		goto err2; // return -1;
	}

	// 2: create message queue MOSI
	msq_key_MOSI = ftok(IPCKEY_PATHNAME, msq_key_MOSI_proj_id);
	if(msq_key_MOSI == -1) {
		errno_r = errno;
		printf(LOG_ERROR"ipc_init: ftok() failed to create msq_key_MOSI: %s\n",
				strerror(errno_r));
		goto err3; // return -1;
	}

	msq_id_MOSI = msgget(msq_key_MOSI, IPC_CREAT | 0400);
	if(msq_id_MOSI == -1) {
		errno_r = errno;
		printf(LOG_ERROR"ipc_init: msgget() failed to get msq_id_MOSI: %s\n",
				strerror(errno_r));
		goto err3; // return -1;
	}

	return init_success_handler();
err4:
	// msgctl(msq_id_MOSI, IPC_RMID, NULL);
err3:
	// msgctl(msq_id_MISO, IPC_RMID, NULL);
err2:
	// shmctl(shm_id, IPC_RMID, NULL);
err1:
	return init_failed_handler();
}

/* public interface */
int mvsdk_init(void) {
	int ret;
	ret = ipc_init();
	return ret; // -1 or 0
}

/* get product name */
int mvsdk_get_product_name(char *str, const int size) {
	FILE *fs;
	char str_r[PRODUCT_NAME_LENGTH];

	memset(str, '\0', size);
	if(init_success_flag)
		return init_failed_handler();

	fs = fopen(PRODUCT_NAME_FILE, "r");
	if(fs == NULL)
		return -1;

	memset(str_r, '\0', PRODUCT_NAME_LENGTH);
	fread(str_r, 1, PRODUCT_NAME_LENGTH, fs);

	if(size > PRODUCT_NAME_LENGTH)
		memcpy(str, str_r, PRODUCT_NAME_LENGTH);
	else
		memcpy(str, str_r, size);

	str[size - 1] = '\0';
	return 0;
}

/* get the resolvation */
int mvsdk_get_resolution(int *height, int *width) {
	int ret;
	msgbuf_t cmd, ack;
	unsigned int img_resolv, height_r, width_r;

    *height = -1;
    *width = -1;

	if(init_success_flag)
		return init_failed_handler();

	cmd.mtype = ROWS_ROLS_GET;

	// before send cmd, needs to clear the unreceived ack
	msgrcv(msq_id_MOSI, &ack, sizeof(ack), 0, IPC_NOWAIT);

	ret = msgsnd(msq_id_MISO, &cmd, sizeof(cmd), IPC_NOWAIT);
	if(ret < 0) return -1;
	ret = msgrcv(msq_id_MOSI, &ack, sizeof(ack), 0, 0);
	if(ret < 0) return -1;
	memcpy(&img_resolv, &ack.mtext, 4);
	height_r = img_resolv >> 16;
	width_r = (img_resolv << 16) >> 16 ;

	*height = height_r;
	*width = width_r;
	return 0;
}

/* get the mono or bayer */
int mvsdk_get_mono_bayer(int *val) {
	int ret;
	msgbuf_t cmd, ack;
	int mono_bayer;

	cmd.mtype = MONO_OR_BAYER;

	if(init_success_flag)
		return init_failed_handler();

	// before send cmd, needs to clear the unreceived ack
	msgrcv(msq_id_MOSI, &ack, sizeof(ack), 0, IPC_NOWAIT);

	ret = msgsnd(msq_id_MISO, &cmd, sizeof(cmd), IPC_NOWAIT);
	if(ret < 0) return -1;
	ret = msgrcv(msq_id_MOSI, &ack, sizeof(ack), 0, 0);
	if(ret < 0) return -1;
	memcpy(&ret, &ack.mtext, 4);
	if(ret) // bayer
		*val = 1;
	else // mono
		*val = 0;
	return 0;
}

/* get the exposure time */
int mvsdk_get_exposure_time(float *exposure_time) {
	int ret;
	msgbuf_t cmd, ack;
	float exposure_time_r;

    *exposure_time = -1;
	if(init_success_flag)
		return init_failed_handler();

	cmd.mtype = EXPOUSE_TIME_GET;

	// before send cmd, needs to clear the unreceived ack
	msgrcv(msq_id_MOSI, &ack, sizeof(ack), 0, IPC_NOWAIT);

	ret = msgsnd(msq_id_MISO, &cmd, sizeof(cmd), IPC_NOWAIT);
	if(ret < 0) return -1;
	ret = msgrcv(msq_id_MOSI, &ack, sizeof(ack), 0, 0);
	if(ret < 0) return -1;

	memcpy(&exposure_time_r, &ack.mtext, 4);
	*exposure_time = exposure_time_r;
	return 0;
}

/* set exposure time */
int mvsdk_set_exposure_time(const float exposure_time) {
	int ret;
	msgbuf_t cmd, ack;

	if(init_success_flag)
		return init_failed_handler();

	cmd.mtype = EXPOUSE_TIME_SET;
	memcpy(&cmd.mtext, &exposure_time, 4);

	// before send cmd, needs to clear the unreceived ack
	msgrcv(msq_id_MOSI, &ack, sizeof(ack), 0, IPC_NOWAIT);

	ret = msgsnd(msq_id_MISO, &cmd, sizeof(cmd), 0);
	if(ret < 0) return -1;
	ret = msgrcv(msq_id_MOSI, &ack, sizeof(ack), 0, 0);
	if(ret < 0) return -1;

	return 0;
}

/* get image */
int mvsdk_get_image(void **bufpt) {
	int ret;
	msgbuf_t cmd, ack;
	unsigned int ack_val;

    *bufpt = NULL;

	if(init_success_flag)
		return init_failed_handler();

	cmd.mtype = EXPOUSE_TIME_SET;
	cmd.mtype = TRI_AND_GET_IMG;

	// before send cmd, needs to clear the unreceived ack
	msgrcv(msq_id_MOSI, &ack, sizeof(ack), 0, IPC_NOWAIT);

	ret = msgsnd(msq_id_MISO, &cmd, sizeof(cmd), IPC_NOWAIT);
	if(ret < 0) return -1;
	ret = msgrcv(msq_id_MOSI, &ack, sizeof(ack), 0, 0);
	if(ret < 0) return -1;

	memcpy(&ack_val, &ack.mtext, 4);
	if(ack_val)
		return -1;

	*bufpt = shm_pt;
	return 0;
}
