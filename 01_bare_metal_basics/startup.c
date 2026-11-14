/*
 * Reset_Handler pseudocode:
 *
 * 1. Get source address of .data in Flash
 * 2. Get destination start and end addresses of .data in RAM
 * 3. Copy .data from Flash to RAM
 *
 * 4. Get start and end addresses of .bss in RAM
 * 5. Clear .bss by writing zero to each location
 *
 * 6. Call main()
 *
 * 7. Enter an infinite loop if main() returns
 */

#include<stdint.h>

typedef union {
    uint32_t stack_pointer;
    void (*handler)(void);
} VectorEntry;

extern uint8_t _sidata;
extern uint8_t _sdata;
extern uint8_t _edata;
extern uint8_t _ebss;
extern uint8_t _sbss;

extern uint8_t _estack;

extern int main(void);


void initialize_data_section(void){
    uint8_t* src = &_sidata;
    uint8_t* dest = &_sdata;


    while(dest < &_edata){
        *dest = *src;
        dest++;
        src++;
    }
}

void clear_bss_section(void){
    uint8_t* dest = &_sbss;

    while(dest < &_ebss){
        *dest = 0;
        dest++;
    }
}


void Reset_Handler(void)
{
    // 1. Initialize .data
    initialize_data_section();

    // 2. Clear .bss
    clear_bss_section();

    // 3. Call main()
    main();

    // 4. Infinite loop if main returns
    while(1){

    }
}

void Default_Handler(void)
{
    while (1)
    {
        // Stay here if an unexpected exception occurs
    }
}

__attribute__((section(".isr_vector"), used, aligned(256)))
const VectorEntry vector_table[] = {
    // Vector entries
    {.stack_pointer = (uint32_t)(uintptr_t)&_estack},
    {.handler = &Reset_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},//3
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.stack_pointer  = 0},
    {.stack_pointer  = 0},
    {.stack_pointer  = 0},
    {.stack_pointer  = 0},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.stack_pointer  = 0},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
// External IRQ 0: vector index 16
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
// External IRQ 31: vector index 47
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler},
    {.handler = &Default_Handler}
};