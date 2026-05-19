/* memory-mapped simif backend (bidirectional). Absolute volatile __at
 * variable (NOT a (volatile*)cast macro: SDCC z80 dead-store-eliminates
 * repeated stores to a constant address; the __at variable forces every
 * access to be a real side effect, like __sfr __at for I/O ports). */
#include "console.h"
#include "mpi.h"

static volatile unsigned char __at (0x4000) SIF;

#define SIF_PRINT 0x70
#define SIF_STOP  0x73
#define SIF_FIN   0x66   /* write cmd, then read -> 0/1   */
#define SIF_READ  0x72   /* write cmd, then read -> byte  */
#define SIF_OUT   0x77   /* write cmd, then write -> .out */

int  putchar(int c)       { SIF = SIF_PRINT; SIF = (unsigned char)c; return c; }
void sim_exit(void)       { SIF = SIF_STOP; }

unsigned char sif_fin_check(void) { SIF = SIF_FIN;  return SIF; }
unsigned char sif_read(void)      { SIF = SIF_READ; return SIF; }

unsigned char mpi_rx(void)
{
    while (!sif_fin_check())
        ;
    return sif_read();
}

void sif_out(unsigned char b) { SIF = SIF_OUT; SIF = b; }

void mpi_send_frame(unsigned char dst, const unsigned char *buf,
                    unsigned char len)
{
    unsigned char i;
    sif_out(dst);
    sif_out(len);
    for (i = 0; i < len; i++)
        sif_out(buf[i]);
}

unsigned char mpi_recv_frame(unsigned char *buf)
{
    unsigned char dst = mpi_rx();
    unsigned char len = mpi_rx();
    unsigned char i;
    for (i = 0; i < len; i++)
        buf[i] = mpi_rx();
    return dst;
}
