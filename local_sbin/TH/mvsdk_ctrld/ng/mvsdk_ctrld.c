/* mvsdk control daemon */
// ipc
#include<sys/ipc.h>
#include<sys/types.h>
#include<sys/shm.h>
// syscall
#include<fcntl.h>
#include<unistd.h>
#include<limits.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/ioctl.h>
#include<pthread.h>
// syscall network
#include<sys/socket.h>
#include<arpa/inet.h>
// c-std
#include<stdio.h>
#include<time.h>
#include<string.h>
#include<errno.h>
#include<string.h>
#include<stdlib.h>
// user
#include<MVSDK/CameraApi.h>
#include"mvsdk_ctrld.h"
#include"jpeg_simp.h"

/* ipc-global variable */
static void *shm_pt;
static int shm_id;
#define shm_size (5300224) // 4096 * 1294, 5.3 million pixels
#define shm_mode (0660)

#define LOG_FILE "/var/log/mvsdk_ctrld.log"
#define LOG_INFO() fprintf(stderr, "[%f] (II) ", (clock()/(float)CLOCKS_PER_SEC))
#define LOG_ERROR() fprintf(stderr, "[%f] (EE) ", (clock()/(float)CLOCKS_PER_SEC))
#define LOG_MODE (0644)
#define PID_FILE "/var/run/mvsdk_ctrld.pid"
#define WORKDIR "/tmp"

/* camera related resources */
// hardware issues, first 20 images have bug
#define MVSDK_BEGIN_GET_TIMES (20)
#define MVSDK_DEFAULT_EXPOUSE_TIME (1000*100) // us
static tSdkFrameHead sFrameInfo;
static int hCamera;
static BYTE *pbyBuffer;
static double retExposureTime;
static unsigned int media_type; // mono or bayer
static unsigned int img_height;
static unsigned int img_width;

/* network comminucation resources */
#define MAXLINE (65000)
#define MAX_TCP_LENGTH (65495)
#define BACKLOG (1)
#define CLIENT_MAXNUM (1) // max-num of active client
#define MAX_ASK_LENGTH (16)
#define MIN_ASK_LENGTH (8)
unsigned int cli_num = 0;
#define MAX_ACK_LENGTH (32)

/* force send n bytes, fork from UNP */
static ssize_t sendn(const int sockfd, const void *buf, size_t len, int flags) {
    size_t nleft;       // not sent # bytes
    ssize_t nsent;      // sent # bytes
    const char *pt;
    pt = (const char *)buf;
    nleft = len;
    while(nleft > 0) {
        nsent = send(sockfd, pt, nleft, flags);
        if(nsent <= 0) {
            if(nsent < 0 && errno == EINTR)
                nsent = 0; // then send() again
            else
                return -1; // unexpected error occurs
        }
        nleft -= nsent;
        pt += nsent;
    }
}
/* recv, re-recv() after Signal Interrupt issue */
static ssize_t recvSI(const int sockfd, void *buf, size_t len, int flags) {
    ssize_t nbytes;
    while(1) {
        nbytes = recv(sockfd, buf, len, flags);
        if(nbytes < 0 && errno == EINTR)
            continue;
        else
            return nbytes;
    }
}

/* stage 1, simply redirect stderr to logfile */
static int log2file(void) {
    int fd, ret;

#ifndef DEBUG
    char pid_str[12];
    fd = open(LOG_FILE, O_WRONLY | O_CREAT, LOG_MODE);
    if(fd < 0)
        return -1;
    dup2(fd, STDERR_FILENO);

    fd = open(PID_FILE, O_WRONLY | O_CREAT, LOG_MODE);
    if(fd < 0)
        return -1;
    memset(pid_str, '\0', 12);
    snprintf(pid_str, 12, "%d\n", getpid());
    write(fd, pid_str, 12);
    close(fd);
#endif

    time_t start_time = time(NULL);
    fprintf(stderr, "\n\n>>>> mvsdk_ctrld: start: %s",
            ctime(&start_time));

    LOG_INFO();
    fprintf(stderr, "stage1: finish redirecting stdout and stderr\n");

    return 0;
}

/* stage 2, initialize the camera */
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
        LOG_ERROR();
        fprintf(stderr, "stage2: device not found.\n");
        return -1;
    }

    for(i = 0; i < iCameraCounts; i++) {
        LOG_INFO();
        fprintf(stderr, "stage2: device[%d]: %s\n",
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
        LOG_ERROR();
        fprintf(stderr, "stage2: CameraInit failed.\n");
        return -1;
    }
    CameraGetCapability(hCamera, &tCapability);

    CameraPause(hCamera);
    CameraSetTriggerMode(hCamera, 1);
    CameraSetAeState(hCamera, FALSE);
    CameraSetExposureTime(hCamera, MVSDK_DEFAULT_EXPOUSE_TIME);
    CameraGetExposureTime(hCamera, &retExposureTime);

    if(tCapability.sIspCapacity.bMonoSensor) {
        CameraSetIspOutFormat(hCamera, CAMERA_MEDIA_TYPE_MONO8);
        // media_type = CAMERA_MEDIA_TYPE_MONO8;
        media_type = 0x0;
    } else {
        CameraSetIspOutFormat(hCamera, CAMERA_MEDIA_TYPE_RGB8);
        // media_type = CAMERA_MEDIA_TYPE_RGB8;
        media_type = 0x1;
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
            LOG_ERROR();
            fprintf(stderr, "CameraGetImageBuffer: timeout\n");
            return -1;
        }
        CameraReleaseImageBuffer(hCamera, pbyBuffer);
    }
    LOG_INFO();
    fprintf(stderr, "stage2: mvsdk_init() success\n");
    return 0;
}

/* stage 3, ipc-shared-mem for local-client get image */
static int ipc_init(void) {
    int ret, errno_r;
    key_t shm_key;
    // 1: create shared-memory
    shm_key = ftok(IPCKEY_PATHNAME, shm_key_proj_id);
    if(shm_key == -1) {
        errno_r = errno;
        LOG_ERROR();
        fprintf(stderr,
                "stage3: ftok() failed to create shm_key: %s\n",
                strerror(errno_r));
        goto err1; //return -1;
    }

    shm_id = shmget(shm_key, shm_size, IPC_CREAT | shm_mode);
    if(shm_id == -1) {
        errno_r = errno; 
        LOG_ERROR();
        fprintf(stderr, "stage3: shmget() failed to get shm_id: %s\n",
                strerror(errno_r));
        goto err1; //return -1;
    }

    shm_pt = shmat(shm_id, NULL, 0);
    if(shm_pt == (void *)-1) {
        errno_r = errno;
        LOG_ERROR();
        fprintf(stderr, "stage3: shmat() failed to attach shm: %s\n",
                strerror(errno_r));
        goto err2; // return -1;
    }
    memset(shm_pt, 0, shm_size);

    LOG_INFO();
    fprintf(stderr, "stage3: ipc_init() successfully.\n");
    return 0;

err2:
    shmctl(shm_id, IPC_RMID, NULL);
err1:
    return -1;
}

/* stage 4, tcp listening */
//static int service_handler(int sockfd);
static void *service_handler(void *args);
// release resources after client terminated
static void sig_child(int signum) {
    pid_t child_pid;
    int wstatus;
    while((child_pid = waitpid(-1, &wstatus, WNOHANG)) > 0) {
        LOG_INFO();
        fprintf(stderr, "client:%ld has terminated.\n",
                (unsigned long)child_pid);
        cli_num--;
    }
    return;
}

// mainloop,
static int connfd; /* connet fd, allocated by accpet() */
static pthread_t thrid;
static int mvsdk_ctrld(void) {
    int listenfd;
    struct sockaddr_in servaddr, cliaddr;
    char buff[MAXLINE];
    time_t ticks;
    int user_port;
    int ret;
    socklen_t socklen;
    pid_t pid;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(ECHO_PORT);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    errno = 0;
    ret = listen(listenfd, BACKLOG);
    if(ret == -1) {
        int errno_r = errno;
        fprintf(stderr, "port: %d is invalid, %s", ECHO_PORT, strerror(errno_r));
        return -1;
    }
    fprintf(stderr, "tcp port: %d\n", ECHO_PORT);
    signal(SIGCHLD, sig_child);

    while(1) {
        socklen = sizeof(cliaddr);
        connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &socklen);
        if(connfd < 0) {
            if (errno == EINTR)
                continue;   /* back to while(1) */
            else {
                LOG_ERROR();
                fprintf(stderr, "unexperted error occurs, terminates the daemon");
                exit(EXIT_FAILURE);
            }
        }
        if(cli_num < CLIENT_MAXNUM)
            cli_num++;
        else {
            // resources busy packet back
            LOG_ERROR();
            fprintf(stderr, "Refuse a new coming client\n");
            close(connfd);
            continue;
        }
#if 0
        pid = fork();
        if(pid == 0) { // child process
            /* cleanup the master server end resources */
            close(listenfd);

            service_handler(connfd);
            close(connfd);
            exit(0);
        }
        // parent, listening-process
        close(connfd);
#endif
        ret = pthread_create(&thrid, NULL, service_handler, NULL);
        if(ret < 0)
            cli_num--;
    }
}
/* stage 4.1, fork() and handle client ask */

static int header_parser(
        const int sockfd,
        const unsigned char *headpt,
        const unsigned int size) {
    uint32_t seg_len = ntohl(*(uint32_t *)(headpt + 4));
    int ret;
    int iter, nleft; // for send a big picture through ethernet
    double exposure_time_r;
    int nbytes;
    char ack_buf[MAX_ACK_LENGTH];
    uint32_t ack_val;

    fprintf(stderr, "size is %d\n", size);
    // check header_id
    uint8_t header_id = *(uint8_t *)(headpt);
    uint8_t header_cmd = *(uint8_t *)(headpt + 1);
    uint8_t header_len = *(uint8_t *)(headpt + 2);
    // uint8_t header_ = *(uint8_t *)(headpt + 3); // reserve

    // fprintf(stderr, "header_id %d\n", header_id);
    fprintf(stderr, "header_cmd %d\n", header_cmd);
    fprintf(stderr, "header_len %d\n", header_len);
    fprintf(stderr, "seg_len is %d\n", seg_len);

    memset(ack_buf, 0, MAX_ACK_LENGTH);
    memcpy(ack_buf, headpt, 4); // ack the same header
    int j = 0;
    unsigned char *jpg_pic;
    unsigned long jpg_size;
    switch(header_cmd) {
        case SET_EXPOUSE_TIME:
            *(uint32_t *)(ack_buf + 4) = htonl(8 + 4); // segment length
            if(size != 12) { // error
                *(uint32_t *)(ack_buf + 8) = 0xffffffff;
            } else {
                exposure_time_r = (double)ntohl(*(uint32_t *)(headpt + 8));
                CameraSetExposureTime(hCamera, exposure_time_r);
                *(uint32_t *)(ack_buf + 8) = 0;
            }
            sendn(sockfd, ack_buf, 12, 0);
            break;
        case GET_EXPOUSE_TIME:
            *(uint32_t *)(ack_buf + 4) = htonl(8 + 4); // segment length
            CameraGetExposureTime(hCamera, &retExposureTime);
            *(uint32_t *)(ack_buf + 8) = htonl((uint32_t)(retExposureTime));
            sendn(sockfd, ack_buf, 12, 0);
            break;
        case GET_MONO_OR_BAYER:
            *(uint32_t *)(ack_buf + 4) = htonl(8 + 4); // segment length
            // mono or bayer is a constant
            *(uint32_t *)(ack_buf + 8) = htonl((uint32_t)(media_type));
            sendn(sockfd, ack_buf, 12, 0);
            break;
        case GET_ROWS_COLS:
            // complete segment length
            *(uint32_t *)(ack_buf + 4) = htonl(8 + 8);
            // rows and rols are constants
            *(uint32_t *)(ack_buf + 8) = htonl((uint32_t)(img_height));  // rows
            *(uint32_t *)(ack_buf + 12) = htonl((uint32_t)(img_width)); // cols
            sendn(sockfd, ack_buf, 16, 0);
            break;
        case GET_IMAGES:
            // camera trigger and get buffer
            CameraSoftTrigger(hCamera);
            ret = CameraGetImageBuffer(hCamera, &sFrameInfo, &pbyBuffer, 2000);
            if(ret != CAMERA_STATUS_SUCCESS) {
                LOG_ERROR();
                fprintf(stderr, "CameraGetImageBuffer: timeout\n");
                *(uint32_t *)(ack_buf + 8) = 0xffffffff;
            } else {
                // CameraImageProcess(hCamera, pbyBuffer, shm_pt, &sFrameInfo);
                memcpy(shm_pt, pbyBuffer, img_height * img_width);
                CameraReleaseImageBuffer(hCamera, pbyBuffer);
                *(uint32_t *)(ack_buf + 8) = 0;
            }
            CameraReleaseImageBuffer(hCamera, pbyBuffer);
            *(uint32_t *)(ack_buf + 4) = htonl(8 + 4);
            // success or not
            sendn(sockfd, ack_buf, 12, 0);
            break;
        case GET_IMAGES_ETH:
            // camera trigger and get buffer
            CameraSoftTrigger(hCamera);
            ret = CameraGetImageBuffer(hCamera, &sFrameInfo, &pbyBuffer, 2000);
            if(ret != CAMERA_STATUS_SUCCESS) {
                LOG_ERROR();
                fprintf(stderr, "CameraGetImageBuffer: timeout\n");
                *(uint32_t *)(ack_buf + 4) = htonl(8 + 4);
                *(uint32_t *)(ack_buf + 8) = 0xffffffff;
                sendn(sockfd, ack_buf, 12, 0);
            } else {
                // CameraImageProcess(hCamera, pbyBuffer, shm_pt, &sFrameInfo);
                memcpy(shm_pt, pbyBuffer, img_height * img_width);
                encompress_JPEG_m2m(&jpg_pic,
                        (unsigned char *)shm_pt,
                        img_height, img_width, 1,
                        70, &jpg_size);
                // nleft = img_height * img_width;
                nleft = jpg_size;
                *(uint32_t *)(ack_buf + 4) = htonl(8 + 4 + 4);
                *(uint32_t *)(ack_buf + 8) = 0;
                *(uint32_t *)(ack_buf + 12) = htonl(nleft);
                sendn(sockfd, ack_buf, 16, 0);
                fprintf(stderr, "image size %d\n", nleft);

                for(iter = 0, j = 0; iter < nleft - 4000; iter += 4000, j++) {
                    nbytes = recvSI(sockfd, ack_buf, MAX_ACK_LENGTH, 0);
                    // basic check
                    if(nbytes == 0) {
                        LOG_ERROR();
                        fprintf(stderr,
                                "%s: client terminates net image transmition\n",
                                __func__);
                        break;
                    } else if(nbytes < 0) {
                        LOG_ERROR();
                        fprintf(stderr, "%s: unexpected error occus\n", __func__);
                        break;
                    }
                    ret = sendn(sockfd, jpg_pic + iter, 4000, 0);
                    if(ret < 0)
                        break; // error
                    // fprintf(stderr, "iter %d j %d\n", iter, j);
                }
                nbytes = recvSI(sockfd, ack_buf, MAX_ACK_LENGTH, 0);
                // remaining bytes
                if(nleft > iter) {
                    ret = sendn(sockfd, jpg_pic + iter, nleft - iter, 0);
                    if(ret < 0)
                        break; // error
                    // fprintf(stderr, ">>>> iter %d j %d\n", iter, j);
                    // fprintf(stderr, ">>>> %d sent\n", ret);
                }
                fprintf(stderr, "ethernet image transmit end\n");
            }
            CameraReleaseImageBuffer(hCamera, pbyBuffer);
            break;
        default: // error handler
            fprintf(stderr, "%s: error", __func__);
    }
    return 0;
}

//static int service_handler(int sockfd) {
static void *service_handler(void *args) {
    // str_echo(sockfd);
    //
    int ret;
    unsigned int nbytes;
    int sockfd = connfd;
    while(1) {
        char recv_buf[MAX_ASK_LENGTH];

        nbytes = recvSI(sockfd, recv_buf, MAX_ASK_LENGTH, 0);
        if(nbytes == 0)
            break; // client terminates
        else if(nbytes < 0) {
            int errno_r = errno;
            LOG_ERROR();
            fprintf(stderr, "%s: unexpected error occurs: %s\n",
                    __func__, strerror(errno_r));
            break;
        }

        if(nbytes < MIN_ASK_LENGTH) {
            LOG_ERROR();
            fprintf(stderr, "%s: recv() too small ask\n", __func__);
            break;
        }
        fprintf(stderr, "receive %d bytes\n", nbytes);
        int pending_bytes = -1;
        ioctl(sockfd, FIONREAD, &pending_bytes);
        fprintf(stderr, ">>>>> pending %d\n", pending_bytes);
        if(pending_bytes > 0) {
            // never support multi-ask
            fprintf(stderr, "receive error ask\n");
            break;
        }
        ret = header_parser(sockfd, recv_buf, nbytes);
    }
    close(sockfd);
    cli_num--;
    LOG_INFO();
    fprintf(stderr, "service_handler finished and exited\n");
}
/******************************************/

int main(int argc, char **argv) {
    int ret = 0;
    chdir(WORKDIR);
    // daemon(1, 1);

    //ret = log2file(); // stage 1
    if(ret == -1)
        return -1;

    ret = mvsdk_init(); // stage 2
    if(ret == -1)
        return -1;

    ret = ipc_init(); // stage 3
    if(ret == -1)
        return -1;

    LOG_INFO(); fprintf(stderr, "shm_id %d\n", shm_id);

    mvsdk_ctrld(); // stage 4

    return 0;
}
