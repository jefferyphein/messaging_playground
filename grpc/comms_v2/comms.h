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

typedef struct comms_submit_header_t {
    uint32_t size;
    uint32_t dst;
    uint64_t tag;
} comms_submit_header_t;

typedef struct comms_reap_header_t {
    uint32_t size;
    uint32_t rc;
    uint64_t tag;
} comms_reap_header_t;

typedef struct comms_catch_header_t {
    uint32_t size;
    uint32_t src;
    uint64_t opaque;
} comms_catch_header_t;

typedef struct comms_packet_t {
    union {
        comms_submit_header_t submit;
        comms_reap_header_t reap;
        comms_catch_header_t caught;
    };
    uint8_t *payload;
} comms_packet_t;

typedef struct comms_t comms_t;
int comms_create(comms_t **C, comms_end_point_t *this_end_point, comms_end_point_t *end_point_list, size_t end_point_count, int lane_count, char **error);
int comms_configure(comms_t *C, const char *key, const char *value, char **error);
int comms_start(comms_t *C, char **error);
int comms_wait_for_start(comms_t *C, double timeout, char **error);
int comms_wait_for_shutdown(comms_t *C, double timeout, char **error);
int comms_shutdown(comms_t *C, char **error);
int comms_destroy(comms_t *C, char **error);

typedef struct comms_accessor_t comms_accessor_t;
int comms_accessor_create(comms_accessor_t **A, comms_t *C, int lane, char **error);
int comms_accessor_destroy(comms_accessor_t *A, char **error);

int comms_submit (comms_accessor_t *A, comms_packet_t packet_list[], size_t packet_count, char **error);
int comms_reap   (comms_accessor_t *A, comms_packet_t packet_list[], size_t packet_count, char **error);
int comms_catch  (comms_accessor_t *A, comms_packet_t packet_list[], size_t packet_count, char **error);
int comms_release(comms_accessor_t *A, comms_packet_t packet_list[], size_t packet_count, char **error);

typedef struct comms_reader_t comms_reader_t;
typedef struct comms_writer_t comms_writer_t;

#endif // __COMMS_H_
