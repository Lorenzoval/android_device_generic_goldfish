/************************************************************************************
 *
 *  Copyright (C) 2009-2011 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ************************************************************************************/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <sys/errno.h>

#include <utils/Log.h>

#include "liba2dp.h"

#define INC_DATA_SOCK_PATH  "/data/inc_data_path"
#define FD_NOT_CONNECTED -1

typedef struct {
   int connect_fd; 
} tBRCM_A2DP_DATA_CB;

static tBRCM_A2DP_DATA_CB brcm_a2dp_data_cb;

static int a2dp_start()
{
    int ret;
    char* param = "no param";

    struct sockaddr_un remote;
    int s, len;

    LOGD("liba2dp_brcm: a2dp_start");

    /* set up connection to the bt */
    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        LOGD("liba2dp_brcm: a2dp_start: failed to create socket");
        return -1;
    }

    LOGD("Trying to connect to BTLD (%d)\n", s);

    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, INC_DATA_SOCK_PATH);
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    if (connect(s, (struct sockaddr *)&remote, len) == -1) {
        perror("connect");
        LOGD("liba2dp_brcm: a2dp_start: failed to connect ");
        close( s );
        return -1;
    }

    len = 10240 * 1;
    setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char*)&len, (int)sizeof(len));

    LOGD("Connected to BTLD dFd = %d", s);

    brcm_a2dp_data_cb.connect_fd = s;

    return 0;
}

int a2dp_init(int rate, int channels, a2dpData* dataPtr)
{
    memset(&brcm_a2dp_data_cb, 0, sizeof(tBRCM_A2DP_DATA_CB));
    brcm_a2dp_data_cb.connect_fd = FD_NOT_CONNECTED;
    
    *dataPtr = (a2dpData*)&brcm_a2dp_data_cb;
    
    return 0;
}

void a2dp_set_sink(a2dpData data, const char* address)
{
    /* This function has no effect on BRCM BTLD stack */
    return;
}

void a2dp_cleanup(a2dpData data)
{
    
    LOGD("liba2dp_brcm: a2dp_cleanup\n");
    if( brcm_a2dp_data_cb.connect_fd != FD_NOT_CONNECTED)
    {
        close(brcm_a2dp_data_cb.connect_fd);
    }

    brcm_a2dp_data_cb.connect_fd = FD_NOT_CONNECTED;
    return;
}

int a2dp_stop(a2dpData data)
{
    LOGD("liba2dp_brcm: a2dp_stop\n");
    a2dp_cleanup(data);

    return 0;
}

int a2dp_write(a2dpData data, const void* buffer, int count)
{
    int sent;
    struct pollfd pfd;

    //LOGD("liba2dp_brcm: a2dp_write %d bytes", count);

    if (brcm_a2dp_data_cb.connect_fd == FD_NOT_CONNECTED) {
        LOGE("a2dp_write failed: socket not ready. Attempt to start a2dp\n");
        // This could happen, if the remote side send a AV suspend instead of a pause
        // to suspend streaming. We will attempt to connect to the socket again
        // BTLD would reject this if we are still suspended. And accept the socket
        // only when the remote headset has sent a start.
        
        if (a2dp_start() < 0) {
             LOGW("a2dp_start failed. Possibly coz remote side has suspended A2dp");
             return -1;
        }
    }

    pfd.fd = brcm_a2dp_data_cb.connect_fd;
    pfd.events = POLLOUT;

    /* poll for 500ms */

    /* send time out */
    if (poll(&pfd, 1, 500) == 0)
        return 0;

    if ((sent = send(brcm_a2dp_data_cb.connect_fd, buffer, count, MSG_NOSIGNAL)) == -1) {
        LOGE("liba2dp_brcm: a2dp_write failed with errno=%d\n", errno);
        a2dp_cleanup(data);
        return -1;
    }

    return sent;
}
