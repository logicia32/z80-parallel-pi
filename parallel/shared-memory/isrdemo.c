/* isrdemo.c — interrupt-driven receive on the self-built machine.
 *
 * This is the thing the message-passing part (Part 2) could not do: a
 * REAL interrupt-driven receive. main() never polls the NIC. It only
 * waits on a flag that the IM2 ISR (crt0_irq.s :: _nic_isr) sets. The
 * device raises /INT, the Z80 vectors through the table at 0x0200 to
 * the hand-written ISR, the ISR reads the byte and RETIs. The host
 * (irq_host.c) delivers K bytes purely by interrupt and checks both the
 * checksum AND that exactly K interrupts were actually taken.
 *
 * Build: --no-std-crt0 with crt0_irq.s (sets I, IM 2, EI).
 */

/* set by the ISR, cleared by main. BSS -> zeroed by crt0 gsinit. */
volatile unsigned char rxbyte;
volatile unsigned char rxflag;

__sfr __at (0x0A) P_READY;    /* OUT: "IM2 armed, you may deliver now" */
__sfr __at (0x0B) P_RESULT;   /* OUT: checksum of received bytes       */
__sfr __at (0x08) P_DONE;     /* OUT: host stops scheduling this core  */

#define K 4                   /* bytes the host will deliver by IRQ    */

void main(void)
{
    unsigned char n, sum = 0;

    P_READY = 0;              /* crt0 already did EI: tell host to send */

    for (n = 0; n < K; n++) {
        while (!rxflag)       /* NOT polling the NIC — only this flag,  */
            ;                 /* which only the ISR ever sets.          */
        rxflag = 0;
        sum += rxbyte;
    }

    P_RESULT = sum;           /* host verifies this == expected sum     */
    P_DONE = 0;
    for (;;)
        ;
}
