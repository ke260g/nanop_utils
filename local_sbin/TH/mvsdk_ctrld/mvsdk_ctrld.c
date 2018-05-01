/* mvsdk control daemon */
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
#include<string.h>

#include<MVSDK/CameraApi.h>
#include"mvsdk_ctrld.h"

/* ipc-global variable */
static void *shm_pt;
static int shm_id;
#define shm_size (5300224) // 4096 * 1294, 5.3 million pixels
#define shm_mode (0666)
#define msq_mode_MISO (0666)
#define msq_mode_MOSI (0666)
static int msq_id_MISO;
static int msq_id_MOSI;

#define LOG_FILE "/var/log/mvsdk_ctrld.log"
#define LOG_INFO " (II) "
#define LOG_ERROR " (EE) "
#define UPTIME() (printf("[%f]", (clock()/(float)CLOCKS_PER_SEC)))
#define LOG_MODE (0644)
#define PID_FILE "/var/run/mvsdk_ctrld.pid"

#define MVSDK_BEGIN_GET_TIMES (20)

/* stage 1 */
static int log_file(void) {
	int fd, ret;

#ifndef DEBUG
	char pid_str[12];
	fd = open(LOG_FILE, O_WRONLY | O_CREAT, LOG_MODE);
	if(fd < 0)
		return -1;
	dup2(fd, STDOUT_FILENO);

	fd = open(PID_FILE, O_WRONLY | O_CREAT, LOG_MODE);
	if(fd < 0)
		return -1;
	memset(pid_str, '\0', 12);
	snprintf(pid_str, 12, "%d\n", getpid());
	write(fd, pid_str, 12);
	close(fd);
#endif

	time_t start_time = time(NULL);
	printf("\n\n>>>> mvsdk_ctrld: start: %s", ctime(&start_time));
	UPTIME();
	printf(LOG_INFO"stage1: finish redirecting stdout and stderr\n");

	return 0;
}

/* stage 2 */
static tSdkFrameHead sFrameInfo;
static BYTE *pbyBuffer;
static int hCamera;
static int media_type; // mono or bayer
static unsigned short img_height;
static unsigned short img_width;
static float exposure_time;
static int mvsdk_init(void) {
	unsigned char *g_pRgbBuffer;
	int i, num, ret;
	int iCameraCounts = 4,
		iStatus = -1;
	tSdkCameraDevInfo tCameraEnumList[4];
	tSdkCameraCapbility tCapability;
	tSdkImageResolution sImageSize;

	CameraSdkInit(1);
	CameraEnumerateDevice(tCameraEnumList, &iCameraCounts);
	if(iCameraCounts == 0) {
		UPTIME(); printf(LOG_ERROR"stage2: device not found.\n");
		return -1;
	}

	for(i = 0; i < iCameraCounts; i++) {
		UPTIME(); printf(LOG_INFO"stage2: device[%d]: %s\n",
				i, tCameraEnumList[i].acProductName);
	}

	FILE *fs = fopen(PRODUCT_NAME_FILE, "w");
	if(fs != NULL) {
		fprintf(fs, "%s", tCameraEnumList[0].acProductName);
		fclose(fs);
	}
	/* static set to 0, first device */
	num = 0;

	iStatus = CameraInit(&tCameraEnumList[num], -1, -1, &hCamera);

	if(iStatus != CAMERA_STATUS_SUCCESS) {
		UPTIME(); printf(LOG_ERROR"stage2: CameraInit failed.\n");
		return -1;
	}
	CameraGetCapability(hCamera, &tCapability);

	CameraPause(hCamera);
	CameraSetTriggerMode(hCamera, 1);
	CameraSetAeState(hCamera, FALSE);
	CameraSetExposureTime(hCamera, 1000);
	double retExpouseTime;
	CameraGetExposureTime(hCamera, &retExpouseTime);
	exposure_time = (float)retExpouseTime;

	if(tCapability.sIspCapacity.bMonoSensor) {
		CameraSetIspOutFormat(hCamera, CAMERA_MEDIA_TYPE_MONO8);
		media_type = CAMERA_MEDIA_TYPE_MONO8;
	}
	else {
		CameraSetIspOutFormat(hCamera, CAMERA_MEDIA_TYPE_RGB8);
		media_type = CAMERA_MEDIA_TYPE_RGB8;
	}

	CameraSetImageResolution(hCamera, &tCapability.pImageSizeDesc[0]);
	CameraSoftTrigger(hCamera);

	img_height = tCapability.sResolutionRange.iHeightMax;
	img_width = tCapability.sResolutionRange.iWidthMax;
	/* set resolution to the maximum */
	for(i = 0; i < tCapability.iImageSizeDesc; i++) {
		if(tCapability.pImageSizeDesc[i].iHeight == img_height &&
				tCapability.pImageSizeDesc[i].iWidth == img_width) {
			CameraSetImageResolution(hCamera, &tCapability.pImageSizeDesc[i]);
			ret = 0;
		} else
			ret = -1;
	}

	CameraPlay(hCamera);
	for(i = 0; i < MVSDK_BEGIN_GET_TIMES; i++) {
		CameraSoftTrigger(hCamera);
		ret = CameraGetImageBuffer(hCamera, &sFrameInfo, &pbyBuffer, 2000);
		if(ret == CAMERA_STATUS_SUCCESS)
			CameraReleaseImageBuffer(hCamera, pbyBuffer);
		else {
			UPTIME(); printf(LOG_ERROR"CameraGetImageBuffer: timeout\n");
			break;
		}
		CameraReleaseImageBuffer(hCamera, pbyBuffer);
	}


	UPTIME(); printf(LOG_INFO"stage2: mvsdk_init() success\n");
	return 0;
}

/* stage 3 */
static int ipc_init(void) {
	int ret, errno_r;
	key_t shm_key;
	key_t msq_key_MISO;
	key_t msq_key_MOSI;
	struct msqid_ds buf;
	// 1: create shared-memory
	shm_key = ftok(IPCKEY_PATHNAME, shm_key_proj_id);
	if(shm_key == -1) {
		errno_r = errno; UPTIME();
		printf(LOG_ERROR"stage3: ftok() failed to create shm_key: %s\n",
				strerror(errno_r));
		goto err1; //return -1;
	}

	shm_id = shmget(shm_key, shm_size, IPC_CREAT | shm_mode);
	if(shm_id == -1) {
		errno_r = errno; UPTIME();
		printf(LOG_ERROR"stage3: shmget() failed to get shm_id: %s\n",
				strerror(errno_r));
		goto err1; //return -1;
	}

	shm_pt = shmat(shm_id, NULL, 0);
	if(shm_pt == (void *)-1) {
		errno_r = errno; UPTIME();
		printf(LOG_ERROR"stage3: shmat() failed to attach shm: %s\n",
				strerror(errno_r));
		goto err2; // return -1;
	}
	memset(shm_pt, 0, shm_size);

	// 2: create message queue MISO
	msq_key_MISO = ftok(IPCKEY_PATHNAME, msq_key_MISO_proj_id);
	if(msq_key_MISO == -1) {
		errno_r = errno; UPTIME();
		printf(LOG_ERROR"stage3: ftok() failed to create msq_key_MISO: %s\n",
				strerror(errno_r));
		goto err2; // return -1;
	}

	msq_id_MISO = msgget(msq_key_MISO, IPC_CREAT | msq_mode_MISO);
	if(msq_id_MISO == -1) {
		errno_r = errno; UPTIME();
		printf(LOG_ERROR"stage3: msgget() failed to get msq_id_MISO: %s\n",
				strerror(errno_r));
		goto err2; // return -1;
	}

	ret = msgctl(msq_id_MISO, IPC_STAT, &buf);
	if(ret == -1) {
		errno_r = errno; UPTIME();
		printf(LOG_ERROR"stage3: msgctl() failed to get msqid_ds: %s\n",
				strerror(errno_r));
		goto err3;
	}
	buf.msg_qbytes = sizeof(msgbuf_t);
	ret = msgctl(msq_id_MISO, IPC_SET, &buf);
	if(ret == -1) {
		errno_r = errno; UPTIME();
		printf(LOG_ERROR"stage3: msgctl() failed to set msqid_ds: %s\n",
				strerror(errno_r));
		goto err3;
	}

	// 2: create message queue MOSI
	msq_key_MOSI = ftok(IPCKEY_PATHNAME, msq_key_MOSI_proj_id);
	if(msq_key_MOSI == -1) {
		errno_r = errno; UPTIME();
		printf(LOG_ERROR"stage3: ftok() failed to create msq_key_MOSI: %s\n",
				strerror(errno_r));
		goto err3; // return -1;
	}

	msq_id_MOSI = msgget(msq_key_MOSI, IPC_CREAT | msq_mode_MOSI );
	if(msq_id_MOSI == -1) {
		errno_r = errno; UPTIME();
		printf(LOG_ERROR"stage3: msgget() failed to get msq_id_MOSI: %s\n",
				strerror(errno_r));
		goto err3; // return -1;
	}

	ret = msgctl(msq_id_MOSI, IPC_STAT, &buf);
	if(ret == -1) {
		errno_r = errno; UPTIME();
		printf(LOG_ERROR"stage3: msgctl() failed to get msqid_ds: %s\n",
				strerror(errno_r));
		goto err4;
	}

	buf.msg_qbytes = sizeof(msgbuf_t);
	ret = msgctl(msq_id_MOSI, IPC_SET, &buf);
	if(ret == -1) {
		errno_r = errno; // UPTIME();
		printf(LOG_ERROR"stage3: msgctl() failed to set msqid_ds: %s\n",
				strerror(errno_r));
		goto err4;
	}

	UPTIME(); printf(LOG_INFO"stage3: ipc_init() successfully.\n");
	return 0;

err4:
	msgctl(msq_id_MOSI, IPC_RMID, NULL);
err3:
	msgctl(msq_id_MISO, IPC_RMID, NULL);
err2:
	shmctl(shm_id, IPC_RMID, NULL);
err1:
	return -1;
}

/* stage 4 */
static int mvsdk_ctrld(void) {
	struct msqid_ds buf;
	msgbuf_t cmd_buf;
	msgbuf_t ack_buf;
	int ret;

	unsigned int img_resolv;
	double retExpouseTime;	
	unsigned int ack_val = 1; // non-zero means error
	int mono_bayer;
blocked_waitcmd:
	msgrcv(msq_id_MISO, &cmd_buf, sizeof(cmd_buf), 0, 0);
	ack_buf.mtype = cmd_buf.mtype;
	switch(cmd_buf.mtype) {
		case ROWS_ROLS_GET:
			img_resolv = (img_height << 16) | img_width;
			memcpy(&ack_buf.mtext, &img_resolv, 4);
			ack_val = 0;
			break;
		case EXPOUSE_TIME_GET:
			CameraGetExposureTime(hCamera, &retExpouseTime);
			exposure_time = (float)retExpouseTime;
			memcpy(&ack_buf.mtext, &exposure_time, 4);
			ack_val = 0;
			break;
		case EXPOUSE_TIME_SET:
			memcpy(&exposure_time, &cmd_buf.mtext, 4);
			CameraSetExposureTime(hCamera, exposure_time);
			ack_val = 0;
			break;
		case MONO_OR_BAYER:
			if(media_type == CAMERA_MEDIA_TYPE_MONO8)
				memset(&ack_buf.mtext, 0, 4);
			else
				memset(&ack_buf.mtext, 1, 4);
			memcpy(&ack_buf.mtext, &ack_val, 4);
			break;
		case TRI_AND_GET_IMG:
			memset(&ack_buf.mtext, 0, sizeof(ack_buf.mtype));
			CameraSoftTrigger(hCamera);
			ret = CameraGetImageBuffer(hCamera, &sFrameInfo, &pbyBuffer, 2000);
			if(ret == CAMERA_STATUS_SUCCESS) {
				// CameraImageProcess(hCamera, pbyBuffer, shm_pt, &sFrameInfo);
				memcpy(shm_pt, pbyBuffer, img_height * img_width);
				CameraReleaseImageBuffer(hCamera, pbyBuffer);
				ack_val = 0;
			} else {
				UPTIME(); printf(LOG_ERROR"CameraGetImageBuffer: timeout\n");
				ack_val = 1;
			}
			CameraReleaseImageBuffer(hCamera, pbyBuffer);
			memcpy(&ack_buf.mtext, &ack_val, 4);
			break;
		default:
			ack_buf.mtype = 99;
	}
	msgsnd(msq_id_MOSI, &ack_buf, sizeof(ack_buf), IPC_NOWAIT);

	goto blocked_waitcmd;
	return 0;
}

int main(int argc, char **argv) {
	int ret;
#ifndef DEBUG
	chdir("/tmp");
#endif
	daemon(1, 1);

	ret = log_file(); // stage 1
	if(ret == -1)
		return -1;

	ret = mvsdk_init(); // stage 2
	if(ret == -1)
		return -1;

	ret = ipc_init(); // stage 3
	if(ret == -1)
		return -1;

	UPTIME(); printf(LOG_INFO"msqid %d\n", msq_id_MOSI);
	UPTIME(); printf(LOG_INFO"msqid %d\n", msq_id_MISO);
	UPTIME(); printf(LOG_INFO"shm_id %d\n", shm_id);

#ifndef DEBUG
	fclose(stdout);
#endif
	mvsdk_ctrld(); // stage 4

	return 0;
}
