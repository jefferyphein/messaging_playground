#ifndef __COMMS_H_
#define __COMMS_H_

#include <stdint.h>
#include <stdlib.h>

#define COMMS_DELIVERED         0   // reap: packet was sent
#define COMMS_NOT_DELIVERED     1   // reap: destination queue was backed up, not sent
#define COMMS_LOCAL             2   // catch: skipped the network
#define COMMS_REMOTE            3   // catch: remote packet received

typedef struct comms_end_point_t {
    char *name;
    char *address;
} comms_end_point_t;

typedef struct comms_packet_t {
    uint32_t    src;
    uint32_t    dst;
    int32_t     lane;
    uint32_t    size;
    uint32_t    capacity;
    int32_t     rc;
    uint8_t    *payload;
} comms_packet_t;

typedef struct comms_t comms_t;

int comms_create(comms_t **C, comms_end_point_t *end_point_list, size_t end_point_count, int local_index, int lane_count, char **error);
int comms_configure(comms_t *C, const char *key, const char *value, char **error);

int comms_start(comms_t *C, char **error);
//int comms_stop_and_destroy(comms_t *C, comms_packet_t **packet_list, size_t packet_count, char **error);
int comms_wait(comms_t *C, char **error);
int comms_shutdown(comms_t *C, char **error);
int comms_destroy(comms_t *C, char **error);

int comms_submit(comms_t *C, comms_packet_t **packet_list, size_t packet_count, char **error);
int comms_reap(comms_t *C, comms_packet_t **packet_list, size_t packet_count, char **error);

int comms_catch(comms_t *C, comms_packet_t **packet_list, size_t packet_count, int lane, char **error);
int comms_release(comms_t *C, comms_packet_t **packet_list, size_t packet_count, char **error);

#endif // __COMMS_H_
