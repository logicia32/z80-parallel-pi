#ifndef MPI_H
#define MPI_H
/* minimal MPI-ish transport over memory-mapped simif (rom[0x4000]).
 * router<->node framing: [DST(1) | LEN(1) | PAYLOAD(LEN)]. */
unsigned char sif_fin_check(void);
unsigned char sif_read(void);
unsigned char mpi_rx(void);                 /* blocking 1-byte recv */
void          sif_out(unsigned char b);     /* 1 byte -> .out FIFO  */
void          mpi_send_frame(unsigned char dst,
                             const unsigned char *buf, unsigned char len);
unsigned char mpi_recv_frame(unsigned char *buf); /* -> dst; buf=payload */
#endif
