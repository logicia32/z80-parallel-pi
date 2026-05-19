/* chips z80.h pin-protocol glue. One clock tick, then
 * service the pin mask: private/shared memory via the arbiter, I/O
 * ports, and IM2 interrupt-acknowledge. /INT is asserted in the pin
 * mask BEFORE the tick while a request is pending and not yet acked
 * (level-triggered, like real hardware). Completion = Z80_HALT pin OR a
 * P_DONE OUT (io_out sets c->halted). */
#include "machine.h"

void core_tick(CORE *c)
{
    uint64_t p = c->pins;

    if (c->int_req && !c->int_acked)
        p |= Z80_INT;
    else
        p &= ~Z80_INT;

    p = z80_tick(&c->cpu, p);
    c->cycles++;

    if ((p & (Z80_MREQ | Z80_RD)) == (Z80_MREQ | Z80_RD)) {
        Z80_SET_DATA(p, mc_rb(c, Z80_GET_ADDR(p)));
    }
    else if ((p & (Z80_MREQ | Z80_WR)) == (Z80_MREQ | Z80_WR)) {
        mc_wb(c, Z80_GET_ADDR(p), Z80_GET_DATA(p));
    }
    else if ((p & (Z80_IORQ | Z80_M1)) == (Z80_IORQ | Z80_M1)) {
        Z80_SET_DATA(p, (unsigned char) c->int_vec);   /* IM2 vector */
        c->int_acked = 1;
        c->int_taken++;                  /* an IRQ was really accepted */
    }
    else if ((p & (Z80_IORQ | Z80_RD)) == (Z80_IORQ | Z80_RD)) {
        Z80_SET_DATA(p, io_in(c, Z80_GET_ADDR(p) & 0xff));
    }
    else if ((p & (Z80_IORQ | Z80_WR)) == (Z80_IORQ | Z80_WR)) {
        io_out(c, Z80_GET_ADDR(p) & 0xff, Z80_GET_DATA(p));
    }

    c->pins = p;

    if (p & Z80_HALT)
        c->halted = 1;
}
