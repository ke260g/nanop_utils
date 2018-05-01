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
// syscall network
#include<sys/socket.h>
#include<arpa/inet.h>
// user
#include"mvsdk_ctrld.h"
#include"mvsdk_iface.h"

#define PID_FILE "/tmp/mvsdk_iface.pid"
#define PID_STR_LENGTH (12)

#define LOG_INFO() \
    fprintf(stderr, "[%f] mvsdk_iface (II) ", (clock()/(float)CLOCKS_PER_SEC))
#define LOG_ERROR() \
    fprintf(stderr, "[%f] mvsdk_iface (EE) ", (clock()/(float)CLOCKS_PER_SEC))
#define PRODUCT_NAME_LENGTH (12)

static int sockfd;
static int shm_id;
static void *shm_pt;
static int init_success_flag = -1;

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
        LOG_ERROR();
        fprintf(stderr, "%s: failed to open pid file\n", __func__);
        return -1;
    }
    write(fd_pid, pid_str, PID_STR_LENGTH);
    close(fd_pid);

    LOG_INFO();
    fprintf(stderr, "init successfully pid: %d\n", getpid());
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
    LOG_ERROR();
    fprintf(stderr, "init failed, last running process is %d.\n", last_pid);
    init_success_flag = -1;
    return -1;
}

// NO.1 init step, ipc-shared-mem
static int ipc_init(void) {
    int ret, errno_r;
    int shm_id; key_t shm_key;
    struct shmid_ds shm_state;
    // 1: create shared-memory
    shm_key = ftok(IPCKEY_PATHNAME, shm_key_proj_id);
    if(shm_key == -1) {
        errno_r = errno;
        LOG_ERROR();
        fprintf(stderr, "ipc_init: ftok() failed to create shm_key: %s\n",
                strerror(errno_r));
        goto err1; //return -1;
    }

    shm_id = shmget(shm_key, 0, 0400);
    if(shm_id == -1) {
        errno_r = errno;
        LOG_ERROR();
        fprintf(stderr, "ipc_init: shmget() failed to get shm_id: %s\n",
                strerror(errno_r));
        goto err1; //return -1;
    }

    /* as device is not thread-safe,
     * it needs to detect multi-threads/process */
    ret = shmctl(shm_id, IPC_STAT, &shm_state);
    if(ret < 0) {
        errno_r = errno;
        LOG_ERROR();
        fprintf(stderr, "ipc_init: shmctl() failed to get shm state: %s\n",
                strerror(errno_r));
        goto err2;
    }
    if(shm_state.shm_nattch < 1) { // daemon crashed
        LOG_ERROR();
        fprintf(stderr, "init failed, daemon may crashed\n");
        goto err2;
    }

    shm_pt = shmat(shm_id, NULL, 0);
    if(shm_pt == (void *)-1) {
        errno_r = errno;
        LOG_ERROR();
        fprintf(stderr, "ipc_init: shmat() failed to attach shm: %s\n",
                strerror(errno_r));
        goto err2; // return -1;
    }

    return init_success_handler();

err2:
    //shmctl(shm_id, IPC_RMID, NULL);
err1:
    return init_failed_handler();
}
// NO.2 init step, network
static int net_init(void) {
    int n, errno_r;
    struct sockaddr_in servaddr;
    const char *ip_addr_pt = "127.0.0.1";
    int user_port = 443;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        errno_r = errno;
        LOG_ERROR();
        fprintf(stderr, "socket() error: %s\n", strerror(errno_r));
        return init_failed_handler();
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    //servaddr.sin_port = htons(13); // daytime, well-known port
    servaddr.sin_port = htons(ECHO_PORT);
    if(inet_pton(AF_INET, ip_addr_pt, &servaddr.sin_addr) <= 0) {
        errno_r = errno;
        LOG_ERROR();
        fprintf(stderr, "inet_pton() error: %s\n", strerror(errno_r));
        return init_failed_handler();
    }

    if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        errno_r = errno;
        LOG_ERROR();
        fprintf(stderr, "connect() error: %s\n", strerror(errno_r));
        return init_failed_handler();
    }
}
/**************************/

/* public interface */
int mvsdk_init(void) {
    int ret;
    ret = ipc_init();

    if(ret)
        return ret; // -1 or 0

    ret = net_init();
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
int mvsdk_get_resolution(unsigned int *height, unsigned int *width) {
    int ret;

    *height = 0;
    *width = 0;
    if(init_success_flag)
        return init_failed_handler();

    int nbytes;
    uint8_t buf[MAX_ASK_LENGTH]; // header
    uint8_t recv_buf[MAX_ACK_LENGTH];

    buf[0] = HEADER_ID;
    buf[1] = GET_ROWS_COLS; // cmd
    buf[2] = 8; // length
    buf[3] = 0;

    *(uint32_t *)(buf + 4) = htonl(8); // segment length
    nbytes = sendn(sockfd, buf, 8, 0);
    bzero(recv_buf, 0);

    nbytes= recvSI(sockfd, recv_buf, MAX_ACK_LENGTH, 0);
    // basic check
    if(nbytes == 0) {
        LOG_ERROR();
        fprintf(stderr, "%s: another process locked, server terminated prematurely\n", __func__);
        return -1;
    } else if(nbytes < 0) {
        LOG_ERROR();
        fprintf(stderr, "%s: unexpected error occus\n", __func__);
        return -1;
    }
    // contents check
    if(nbytes != 16) {
        LOG_ERROR();
        fprintf(stderr, "%s: segment error occus\n", __func__);
        return -1;
    }
    uint32_t rows = ntohl(*(uint32_t *)(recv_buf + 8));
    uint32_t cols = ntohl(*(uint32_t *)(recv_buf + 12));
    *height = (unsigned int)rows;
    *width = (unsigned int)cols;
    return 0;
}

/* get the mono or bayer */
int mvsdk_get_mono_bayer(int *val) {
    int ret;
    int mono_bayer;
    *val = -1;
    if(init_success_flag)
        return init_failed_handler();

    int nbytes;
    uint8_t recv_buf[MAX_ACK_LENGTH];
    uint8_t buf[MAX_ASK_LENGTH]; // header

    buf[0] = HEADER_ID;
    buf[1] = GET_MONO_OR_BAYER; // cmd
    buf[2] = 8; // length
    buf[3] = 0;

    *(uint32_t *)(buf + 4) = htonl(8); // segment length
    nbytes = sendn(sockfd, buf, 8, 0);
    bzero(recv_buf, 0);

    nbytes= recvSI(sockfd, recv_buf, MAX_ACK_LENGTH, 0);
    // basic check
    if(nbytes == 0) {
        LOG_ERROR();
        fprintf(stderr, "%s: another process locked, server terminated prematurely\n", __func__);
        return -1;
    } else if(nbytes < 0) {
        LOG_ERROR();
        fprintf(stderr, "%s: unexpected error occus\n", __func__);
        return -1;
    }
    // contents check
    if(nbytes != 12) {
        LOG_ERROR();
        fprintf(stderr, "%s: segment error occus\n", __func__);
        return -1;
    }
    uint32_t val_r = ntohl(*(uint32_t *)(recv_buf + 8));
    *val = (int)val_r;
    return 0;
}

/* get the exposure time */
int mvsdk_get_exposure_time(unsigned long *exposure_time) {
    int ret;
    uint32_t exposure_time_r;
    int nbytes;
    uint8_t recv_buf[MAX_ACK_LENGTH];
    uint8_t buf[MAX_ASK_LENGTH]; // header

    *exposure_time = 0;

    if(init_success_flag)
        return init_failed_handler();

    buf[0] = HEADER_ID;
    buf[1] = GET_EXPOUSE_TIME; // cmd
    buf[2] = 8; buf[3] = 0;
    *(uint32_t *)(buf + 4) = htonl(8); // segment length
    nbytes = sendn(sockfd, buf, 8, 0);
    bzero(recv_buf, 0);

    nbytes= recvSI(sockfd, recv_buf, MAX_ACK_LENGTH, 0);
    // basic check
    if(nbytes == 0) {
        LOG_ERROR();
        fprintf(stderr, "%s: another process locked, server terminated prematurely\n", __func__);
        return -1;
    } else if(nbytes < 0) {
        LOG_ERROR();
        fprintf(stderr, "%s: unexpected error occus\n", __func__);
        return -1;
    }
    // contents check
    if(nbytes != 12) {
        LOG_ERROR();
        fprintf(stderr, "%s: segment error occus\n", __func__);
        return -1;
    }

    exposure_time_r = ntohl(*(uint32_t *)(recv_buf + 8));
    *exposure_time = exposure_time_r;
    return 0;
}

/* set the exposure time */
int mvsdk_set_exposure_time(const unsigned long exposure_time) {
    int ret;
    int nbytes;
    uint8_t recv_buf[MAX_ACK_LENGTH];
    uint8_t buf[MAX_ASK_LENGTH]; // header

    if(init_success_flag)
        return init_failed_handler();

    if(exposure_time < 0) {
        LOG_ERROR();
        fprintf(stderr, "%s: error first-param %ld \n", __func__, exposure_time);
        return -1;
    }

    buf[0] = HEADER_ID;
    buf[1] = SET_EXPOUSE_TIME; // cmd
    buf[2] = 8; buf[3] = 0;
    *(uint32_t *)(buf + 4) = htonl(12); // segment length
    *(uint32_t *)(buf + 8) = htonl(exposure_time);
    nbytes = sendn(sockfd, buf, 12, 0);
    bzero(recv_buf, 0);

    nbytes= recvSI(sockfd, recv_buf, MAX_ACK_LENGTH, 0);
    // basic check
    if(nbytes == 0) {
        LOG_ERROR();
        fprintf(stderr, "%s: another process locked, server terminated prematurely\n", __func__);
        return -1;
    } else if(nbytes < 0) {
        LOG_ERROR();
        fprintf(stderr, "%s: unexpected error occus\n", __func__);
        return -1;
    }
    // contents check
    if(nbytes != 12) {
        LOG_ERROR();
        fprintf(stderr, "%s: segment error occus\n", __func__);
        return -1;
    }
    if(*(uint32_t *)(recv_buf + 8) == 0xffffffff) {
        LOG_ERROR();
        fprintf(stderr, "%s: server return error\n", __func__);
        return -1;
    }

    return 0;
}

/* get image */
int mvsdk_get_image(unsigned char **bufpt) {
    int ret;
    int nbytes;
    uint8_t recv_buf[MAX_ACK_LENGTH];
    uint8_t buf[MAX_ASK_LENGTH]; // header

    *bufpt = NULL;
    if(init_success_flag || bufpt == NULL)
        return init_failed_handler();

    buf[0] = HEADER_ID;
    buf[1] = GET_IMAGES; // cmd
    buf[2] = 8; buf[3] = 0;
    *(uint32_t *)(buf + 4) = htonl(8); // segment length
    nbytes = sendn(sockfd, buf, 8, 0);
    bzero(recv_buf, 0);

    nbytes= recvSI(sockfd, recv_buf, MAX_ACK_LENGTH, 0);
    // basic check
    if(nbytes == 0) {
        LOG_ERROR();
        fprintf(stderr, "%s: another process locked, server terminated prematurely\n", __func__);
        return -1;
    } else if(nbytes < 0) {
        LOG_ERROR();
        fprintf(stderr, "%s: unexpected error occus\n", __func__);
        return -1;
    }
    // contents check
    if(nbytes != 12) {
        LOG_ERROR();
        fprintf(stderr, "%s: segment error occus\n", __func__);
        return -1;
    }
    if(*(uint32_t *)(recv_buf + 8) == 0xffffffff) {
        LOG_ERROR();
        fprintf(stderr, "%s: server return error, failed to get image\n", __func__);
        return -1;
    }

    *bufpt = (unsigned char *)shm_pt;
    return 0;
}

