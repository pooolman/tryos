#ifndef _INCLUDE_8259A_H_
#define _INCLUDE_8259A_H_

#include <stdint.h>

/* IRQ 所使用的中断向量 */
#define IRQ0_INT_VECTOR	32	//PIT
#define IRQ1_INT_VECTOR	33	//keyboard
#define IRQ2_INT_VECTOR	34
#define IRQ3_INT_VECTOR	35
#define IRQ4_INT_VECTOR	36
#define IRQ5_INT_VECTOR	37
#define IRQ6_INT_VECTOR	38
#define IRQ7_INT_VECTOR	39
#define IRQ8_INT_VECTOR	40
#define IRQ9_INT_VECTOR	41
#define IRQ10_INT_VECTOR	42
#define IRQ11_INT_VECTOR	43
#define IRQ12_INT_VECTOR	44
#define IRQ13_INT_VECTOR	45
#define IRQ14_INT_VECTOR	46 //master IDE
#define IRQ15_INT_VECTOR	47

void init_8259A(void);

void inhibit_IRline(uint8_t IRline);

void enable_IRline(uint8_t IRline);

void send_EOI(uint8_t int_vector);

#endif
