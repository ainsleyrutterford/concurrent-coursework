/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of
 * which can be found via http://creativecommons.org (and should be included as
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"

pcb_t pcb[ 2 ]; int executing = 0;

void scheduler( ctx_t* ctx ) {
  if     ( 0 == executing ) {
    memcpy( &pcb[ 0 ].ctx, ctx, sizeof( ctx_t ) ); // preserve P_1
    pcb[ 0 ].status = STATUS_READY;                // update   P_1 status
    memcpy( ctx, &pcb[ 1 ].ctx, sizeof( ctx_t ) ); // restore  P_2
    pcb[ 1 ].status = STATUS_EXECUTING;            // update   P_2 status
    executing = 1;                                 // update   index => P_2
  }
  else if( 1 == executing ) {
    memcpy( &pcb[ 1 ].ctx, ctx, sizeof( ctx_t ) ); // preserve P_2
    pcb[ 1 ].status = STATUS_READY;                // update   P_2 status
    memcpy( ctx, &pcb[ 0 ].ctx, sizeof( ctx_t ) ); // restore  P_1
    pcb[ 0 ].status = STATUS_EXECUTING;            // update   P_1 status
    executing = 0;                                 // update   index => P_1
  }
  return;
}

extern void     main_P3();
extern uint32_t tos_P3;
extern void     main_P4();
extern uint32_t tos_P4;

void hilevel_handler_rst( ctx_t* ctx ) {

  memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );
  pcb[ 0 ].pid      = 1;
  pcb[ 0 ].status   = STATUS_READY;
  pcb[ 0 ].ctx.cpsr = 0x50;
  pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_P3 );
  pcb[ 0 ].ctx.sp   = ( uint32_t )( &tos_P3  );

  memset( &pcb[ 1 ], 0, sizeof( pcb_t ) );
  pcb[ 1 ].pid      = 2;
  pcb[ 1 ].status   = STATUS_READY;
  pcb[ 1 ].ctx.cpsr = 0x50;
  pcb[ 1 ].ctx.pc   = ( uint32_t )( &main_P4 );
  pcb[ 1 ].ctx.sp   = ( uint32_t )( &tos_P4  );

  memcpy( ctx, &pcb[ 0 ].ctx, sizeof( ctx_t ) );
  pcb[ 0 ].status = STATUS_EXECUTING;
  executing = 0;

  TIMER0->Timer1Load  = 0x00100000; // select period = 2^20 ticks ~= 1 sec
  TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
  TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
  TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
  TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer

  GICC0->PMR          = 0x000000F0; // unmask all            interrupts
  GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt
  GICC0->CTLR         = 0x00000001; // enable GIC interface
  GICD0->CTLR         = 0x00000001; // enable GIC distributor

  int_enable_irq();

  return;
}

void hilevel_handler_irq(ctx_t* ctx) {
  // Read  the interrupt identifier so we know the source.
  uint32_t id = GICC0->IAR;

  // Handle the interrupt, then clear (or reset) the source.
  if( id == GIC_SOURCE_TIMER0 ) {
    scheduler(ctx);
    TIMER0->Timer1IntClr = 0x01;
  }

  // Write the interrupt identifier to signal we're done.
  GICC0->EOIR = id;

  return;
}


void hilevel_handler_svc(ctx_t* ctx, uint32_t id) {
  switch(id) {

    case 0x01 : { // 0x01 => write( fd, x, n )
      int   fd = ( int   )( ctx->gpr[ 0 ] );
      char*  x = ( char* )( ctx->gpr[ 1 ] );
      int    n = ( int   )( ctx->gpr[ 2 ] );

      for( int i = 0; i < n; i++ ) {
        PL011_putc( UART0, *x++, true );
      }

      ctx->gpr[ 0 ] = n;
      break;
    }

    default : {
      break ;
    }

  }
  return;
}
