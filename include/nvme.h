#ifndef NVME_H
#define NVME_H

#include <stdint.h>

// Ctrl registers

#define NVME_REG_CAP  0x00    // 64 bit. what the controller supports
#define NVME_REG_VS   0x08
#define NVME_REG_CC   0x14    // write this to enable/configure
#define NVME_REG_CSTS 0x1C    // read this to see the state
#define NVME_REG_AQA  0x24    // admin queue sizes
#define NVME_REG_ASQ  0x28    // 64-bit. admin SQ physical address
#define NVME_REG_ACQ  0x30    // 64-bit. admin CQ physical address
#define NVME_REG_DB   0x1000  // doorbell starts


#define NVME_CC_EN         (1u << 0)
#define NVME_CC_IOSQES(n)  ((uint32_t)(n) << 0x10)
#define NVME_CC_IOCQES(n)  ((uint32_t)(n) << 0x14)
#define NVME_CSTS_READY    (1u << 0)
#define NVME_CAP_MQES(c)   ((c) & 0xFFFF)
#define NVME_CAP_TO(c)     (((c) >> 0x18) & 0xFF)
#define NVME_CAP_DSTRD(c)  (((c) >> 0x20) & 0xF);

// DB address
#define NVME_SQ_TAIL_DB(qid, stride)     (NVME_REG_DB + (2 * (qid))      * (4u << (stride)))
#define NVME_CQ_HEAD_DB(qid, stride)     (NVME_REG_DB + (2 * (qid)) + 1) * (4u << (stride)))



#define NVME_ADM_CREATE_SQ   0x01
#define NVME_ADM_CREATE_CQ   0x05
#define NVME_ADM_IDENTIFY    0x06
#define NVME_IO_WRITE        0x01
#define NVME_IO_READ         0x02

typedef struct {
 uint32_t cdw0;
 uint32_t nsid;
 uint64_t rsvd; // reserved
 uint64_t mptr;
 uint64_t prp1, prp2; // where the data goes in RAM
 uint32_t cdw10, cdw11, cdw12, cdw13, cdw14, cdw15; // meaning depends on opcode
} __attribute__((packed)) nvme_sqe_t;
_Static_assert(sizeof(nvme_sqe_t) == 0x40, "SQE MUST BE 64 BYTES");
typedef struct {
 uint32_t result;
 uint32_t rsvd; // reserved
 uint16_t sq_head, sq_id;
 uint16_t cid;
 uint16_t status;
} __attribute((packed)) nvme_cqe_t;

_Static_assert(sizeof(nvme_cqe_t) == 0x10, "CQE MUST BE 16 BYTES");

typedef struct {
 nvme_sqe_t *sq; // virt addresses of rings in RAM
 nvme_cqe_t *cq; // physical addresses, which the device uses
 uint64_t sq_pa, cq_pa;
 volatile uint32_t *sq_db, *cq_db; // doorbell registers
 uint16_t qid, size, sq_tail, cq_head;
 uint8_t phase;
} nvme_queue_t;

int nvme_probe(uint8_t bus, uint8_t dev, uint8_t func);



#endif // NVME_H
