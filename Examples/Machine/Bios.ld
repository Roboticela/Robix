ENTRY(bios_entry)
SECTIONS {
  . = 0xF0000;
  .text : {
    *(.text)
    . = 0xFFFF0;
    *(.reset)
    . = 0xFFFFF;
    BYTE(0)
  }
}
