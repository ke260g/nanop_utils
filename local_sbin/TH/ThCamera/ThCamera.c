#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include<stdio.h>
#include<string.h>
#include<errno.h>

#include<json.h>

//#define CONF_JSON "/etc/TH/ThCamera.conf.json"
#define CONF_JSON "./ThCamera.conf.json"

#define STORAGE_OBTAIN_CMD "df -h | grep \"/$\" | awk '{print $3\"/\"$2}'"
// cmd-output longest string: "1023.xxG/1023.yyT", 15
// do not forget the ending '\n' in cmd-output, round size to 20
#define STORAGE_STRING_SIZE (20)
#define MESG_HEAD "ThCamera"
#define MESG_SIZE (128)
#define MESG_REMAIN (MESG_SIZE - strlen(mesg_data) - 1)
#define MESG_STRNCAT_ROUND ((MESG_REMAIN > 0) ? MESG_REMAIN : 0)
char mesg_data[MESG_SIZE];

#define MESG_PORT (3220)

// message append pair,  "|key:value"
static int mesg_appd_pair(const char *key, const char *value) {
    strncat(mesg_data, "|", MESG_STRNCAT_ROUND);
    strncat(mesg_data, key, MESG_STRNCAT_ROUND);
    strncat(mesg_data, ":" , MESG_STRNCAT_ROUND);
    strncat(mesg_data, value, MESG_STRNCAT_ROUND);
    return 0;
}

// foreach and parse the json config to echo_mesg[]
static int json_object_foreach(json_object *jobj) {
    enum json_type type;
    json_object_object_foreach(jobj, key, val) {
        type = json_object_get_type(val);
        if(type != json_type_string)
            return -1;
        mesg_appd_pair(key, json_object_get_string(val));
    }
}

// mesg resolv, parse conf.json and obtain storage of disk space
static int mesg_resolv(void) {
    // read the whole of conf.json file
    int errno_r;
    FILE *fs;
    fs = fopen(CONF_JSON, "r");
    if(fs == NULL) {
        errno_r = errno;
        memset(mesg_data, '\0', MESG_SIZE);
        snprintf(mesg_data, MESG_SIZE - 1,
                "open ThCamera.conf.json error: %s\n",
                strerror(errno_r));
        return -1;
    }
    fseek(fs, 0, SEEK_END);
    int file_size = ftell(fs);
    if(file_size <= 0) {
        errno_r = errno;
        memset(mesg_data, '\0', MESG_SIZE);
        snprintf(mesg_data, MESG_SIZE - 1,
                "obtain ThCamera.conf.json file size error: %s\n",
                strerror(errno_r));
        fclose(fs);
        return -1;
    }

    fseek(fs, 0, SEEK_SET);
    char *json_string = (char *)malloc(file_size);
    memset(json_string, '\0', file_size);
    fread(json_string, 1, file_size - 1, fs);
    fclose(fs);

    // parse the json config to echo_mesg[]
    json_object *jobj = json_tokener_parse(json_string);
    free(json_string);
    if(jobj == NULL) {
        memset(mesg_data, '\0', MESG_SIZE);
        snprintf(mesg_data, MESG_SIZE - 1,
                "ThCamera.conf.json syntax error\n");
        return -1;
    }
    memset(mesg_data, '\0', MESG_SIZE); // clear and initiail
    // append the header
    strncat(mesg_data, MESG_HEAD, MESG_STRNCAT_ROUND);
    // foreach
    if(json_object_foreach(jobj) < 0) {
        memset(mesg_data, '\0', MESG_SIZE);
        snprintf(mesg_data, MESG_SIZE - 1,
                "ThCamera.conf.json syntax error\n");
        return -1;
    }

    // obtain storage of disk space string via cmd
    fs = popen(STORAGE_OBTAIN_CMD, "r");
    if(fs == NULL) {
        memset(mesg_data, '\0', MESG_SIZE);
        snprintf(mesg_data, MESG_SIZE - 1,
                "ThCamera.conf.json syntax error\n");
        return -1;
    }

    char storage_string[STORAGE_STRING_SIZE];
    memset(storage_string, '\0', STORAGE_STRING_SIZE);
    fread(storage_string, 1, STORAGE_STRING_SIZE - 1, fs);
    mesg_appd_pair("Storage", storage_string);

    return 0;
}

// udp echo-back-message main loop
static int udp_echo_mesg_loop(void) {
    int ret,
        sockfd;
    struct sockaddr_in cliaddr;
    socklen_t addrlen;

    // prepare to listen udp-port
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    bzero(&cliaddr, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(MESG_PORT);
    cliaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    addrlen = sizeof(cliaddr);
    bind(sockfd, (struct sockaddr *)&cliaddr, sizeof(cliaddr));

    // main loop
    char ping_start;
    while(1) {
        ret = recvfrom(sockfd, &ping_start, 1, 0,
                (struct sockaddr *)&cliaddr, &addrlen);
        if(ret < 0) {
            // receive datagram error
            sleep(5);
            continue;
        } else if (addrlen != sizeof(cliaddr)) {
            // receive datagram error, may be not a legal ipv4 packet
            sleep(5);
            continue;
        } else {
            // perform echo-back-message
            mesg_resolv();
            sendto(sockfd, mesg_data, MESG_SIZE, 0,
                    (struct sockaddr *)&cliaddr, sizeof(cliaddr));
            sleep(5);
        }
    }
}

int main( int argc, char **argv ) {
    daemon(1, 1);
    udp_echo_mesg_loop();
    return 0;
}
