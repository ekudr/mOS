OUTPUT_ARCH( "riscv" )

ENTRY( _start )

PHDRS {
    rodata PT_LOAD FLAGS(4);    /* Read only */
    text PT_LOAD FLAGS(5);      /* Read and Execute */
    data PT_LOAD FLAGS(6);      /* Read and Write */
}

SECTIONS
{
    . = 0x1000;
    
    .text : 
    { 
        *(.text)
        *(.text.*)
        . = ALIGN(16);
     } : text
    
    .rodata :  ALIGN(8) {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    } : rodata

    .data :  ALIGN(8)  { 

        *(.data .data.*)           
    } : data

    .got : {
        *(.got)
        *(.got.plt)
    } : data
    
    .sdata : ALIGN(0x1000) {
        __global_pointer$ = . + 0x800;
        *(.sdata .sdata.* .sdata2.*)
    } : data



    .bss : ALIGN(8) { 
        _bss_start = .;
        *(.bss .bss.*) 
        *(.sbss .sbss.*) 
        *(COMMON)
        _bss_end = .;
    } : data
    
   

    /DISCARD/ : { *(.comment .note) }

}