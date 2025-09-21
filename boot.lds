OUTPUT_ARCH( "riscv" )

ENTRY( _start )


SECTIONS
{
/*   . = 0x80200000;*/ 
    . = 0xFFFFFFFF80000000;
    _start = .;

    .text : { *(.text .text.*)
        . = ALIGN(0x1000);
        _trampoline = .;
        *(trampsec)
        . = ALIGN(0x1000);
        ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page");   
    }
    
    .rodata :  ALIGN(32) {
        *(.rodata .rodata.*)
    }

    _data_loc = ALIGN(0x1000);
    . = ALIGN(0x1000);

    .data :  ALIGN(32)  { 
        *(.data)  
    }

    .sdata : ALIGN(0x1000) {
	    __global_pointer$ = . + 0x800;
        *(.sdata .sdata.* .sdata2.*)
    } 

    .bss : ALIGN(8) { 
        _bss_start = .;
        *(.bss) 
        _bss_end = .;
    }
    
    .got : { *(.got*) }

     /* Page tables here, align to 4K boundary */

    .pgtables (NOLOAD) : ALIGN(0x1000) {
        _pgtable_start = .;
        *(.pgtables)
        . = ALIGN(4);
        _pgtable_end = .;
    }

    .payload : ALIGN(0x1000) {
        _servers_img = .;
		*(.payload)
		. = ALIGN(0x1000);
        _servers_img_end = .;
	}

    /* Stack top */

    .stack (NOLOAD): ALIGN(0x1000) {
        _stack_top = .;
        . = . + 0x1000;
        _estack = .;
    }

    _end = .;

    /DISCARD/ : { *(.comment .note .eh_frame) }

}