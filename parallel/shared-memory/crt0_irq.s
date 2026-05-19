;; crt0_irq.s — SDCC z80 startup for the IM2 interrupt-driven receive demo.
;;
;; Stock SDCC crt0 does not arm interrupts. IM2 needs:
;;   0x0000 : cold-start jump
;;   0x0200 : the 2-byte IM2 vector table entry.  On an accepted IRQ the
;;            Z80 forms the table address (I << 8) | vec.  The host
;;            supplies vec = 0x00 and we set I = 0x02, so the CPU reads
;;            the handler address from 0x0200/0x0201 = the word below.
;;   init   : SP into PRIVATE ram (0xBF00, below the 0xC000 shared
;;            window so stack and shared accumulator never collide),
;;            I register, IM 2, gsinit (zero BSS), EI, then main().
        .module crt0_irq
        .globl  _main
        .globl  _nic_isr
        .globl  _rxbyte
        .globl  _rxflag

        .area   _HEADER (ABS)
        .org    0x0000
        di
        jp      init

        .org    0x0200                  ; IM2 vector table entry (vec=0)
        .dw     _nic_isr                ; CPU jumps to whatever is here

        .org    0x0300
init:
        ld      sp, #0xBF00             ; private stack, below 0xC000
        ld      a, #0x02                ; high byte of the vector table
        ld      i, a
        im      2
        call    gsinit                  ; zero BSS / copy initializers
        ei
        call    _main
1$:
        halt
        jr      1$

        .area   _HOME
        .area   _CODE
        .area   _INITIALIZER
        .area   _GSINIT
        .area   _GSFINAL
        .area   _DATA
        .area   _INITIALIZED
        .area   _BSEG
        .area   _BSS
        .area   _HEAP

        .area   _CODE
__clock::
        ret

;; Hand-written IM2 ISR. The Z80 enters with interrupts already
;; disabled (iff1=iff2=0) and RETI alone does NOT re-enable them, so we
;; must EI explicitly. Order: read the NIC FIRST — that consumes the
;; mailbox and drops the device's /INT line — THEN ei + reti. Re-enabling
;; before the read would re-enter on the same byte (interrupt storm).
;; This is exactly the level-sensitive-ISR discipline.
_nic_isr:
        push    af
        in      a, (0x10)               ; P_NIC: read byte -> clears source
        ld      (_rxbyte), a
        ld      a, #0x01
        ld      (_rxflag), a            ; tell main a byte arrived
        pop     af
        ei                              ; safe: source already cleared
        reti

        .area   _GSINIT
gsinit::
        .area   _GSFINAL
        ret
