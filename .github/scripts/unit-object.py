#!/usr/bin/env python3
"""Write one linkable unit for a Robix source file.

Linux units are relocatable ELF objects. Windows units are DLLs.
The unit exports the source bytes so a later link can include that file.
"""
import pathlib
import struct
import sys

ELF_MACHINE = {
    "linux-x64": 62,
    "linux-arm64": 183,
    "linux-arm": 40,
    "linux-riscv64": 243,
    "linux-ppc64le": 21,
    "linux-s390x": 22,
    "linux-loongarch64": 258,
}
PE_MACHINE = {
    "windows-x64": 0x8664,
    "windows-arm64": 0xAA64,
    "windows-x86": 0x14C,
}


def align(value, boundary):
    return (value + boundary - 1) & ~(boundary - 1)


def write_elf(machine, payload, symbol, destination):
    rodata = payload
    shstr = b"\0.text\0.rodata\0.shstrtab\0.symtab\0.strtab\0"
    strtab = b"\0" + symbol.encode("ascii") + b"\0"
    text_size = 0
    rodata_off = 64
    # Layout is filled after we know section offsets. Build a simple file:
    # header, rodata, shstrtab, strtab, symtab, section headers.
    parts = []
    header_size = 64
    rodata_offset = header_size
    shstr_offset = align(rodata_offset + len(rodata), 8)
    str_offset = shstr_offset + len(shstr)
    sym_offset = align(str_offset + len(strtab), 8)
    symbol_entry = struct.pack("<IBBHQQ", strtab.index(symbol.encode("ascii")), 0, 0, 1, 1, 0)
    # Elf64_Sym: st_name, st_info, st_other, st_shndx, st_value, st_size
    null_symbol = struct.pack("<IBBHQQ", 0, 0, 0, 0, 0, 0)
    symtab = null_symbol + struct.pack("<IBBHQQ", 1, (1 << 4) | 1, 0, 2, 0, len(rodata))
    shoff = align(sym_offset + len(symtab), 8)
    sections = [
        (b"", 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (b".text", 1, 6, 0, 0, 0, 0, 0, 1, 0),
        (b".rodata", 1, 2, 0, rodata_offset, len(rodata), 0, 0, 1, 0),
        (b".shstrtab", 3, 0, 0, shstr_offset, len(shstr), 0, 0, 1, 0),
        (b".symtab", 2, 0, 0, sym_offset, len(symtab), 5, 1, 8, 24),
        (b".strtab", 3, 0, 0, str_offset, len(strtab), 0, 0, 1, 0),
    ]
    names = {item[0]: shstr.index(item[0]) if item[0] else 0 for item in sections}
    shents = b""
    for name, kind, flags, addr, offset, size, link, info, addralign, entsize in sections:
        shents += struct.pack(
            "<IIQQQQIIQQ",
            names[name],
            kind,
            flags,
            addr,
            offset,
            size,
            link,
            info,
            addralign,
            entsize,
        )
    file_size = shoff + len(shents)
    blob = bytearray(file_size)
    blob[0:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into(
        "<HHIQQQIHHHHHH",
        blob,
        16,
        1,
        machine,
        1,
        0,
        0,
        shoff,
        0,
        64,
        0,
        0,
        64,
        len(sections),
        3,
    )
    blob[rodata_offset:rodata_offset + len(rodata)] = rodata
    blob[shstr_offset:shstr_offset + len(shstr)] = shstr
    blob[str_offset:str_offset + len(strtab)] = strtab
    blob[sym_offset:sym_offset + len(symtab)] = symtab
    blob[shoff:shoff + len(shents)] = shents
    pathlib.Path(destination).write_bytes(blob)


def write_dll(machine, payload, symbol, destination):
    # A small PE DLL with the source in .rdata and one export.
    pe32plus = machine != 0x14C
    section_align = 0x1000
    file_align = 0x200
    export_name = symbol.encode("ascii") + b"\0"
    dll_name = (symbol + ".dll").encode("ascii") + b"\0"
    # Export directory is built after we know RVAs.
    rdata = payload
    # Place export tables after the payload, 8-byte aligned inside the section.
    cursor = align(len(rdata), 8)
    names_rva_base = 0x2000
    eat = struct.pack("<I", names_rva_base + cursor + 40 + len(export_name) + len(dll_name))
    # Simpler fixed export block:
    # [payload][export directory 40][dll name][function name][address table][name ptr][ordinal]
    address = names_rva_base  # payload starts at section RVA
    export_rva = names_rva_base + align(len(payload), 8)
    name_rva = export_rva + 40
    func_rva = name_rva + len(dll_name)
    address_table = func_rva + len(export_name)
    name_table = address_table + 4
    ordinal_table = name_table + 4
    section = bytearray(ordinal_table + 2 - names_rva_base)
    section[0:len(payload)] = payload
    struct.pack_into(
        "<IIIIIIIIII",
        section,
        export_rva - names_rva_base,
        0,
        0,
        0,
        name_rva,
        1,
        1,
        1,
        address_table,
        name_table,
        ordinal_table,
    )
    section[name_rva - names_rva_base:name_rva - names_rva_base + len(dll_name)] = dll_name
    section[func_rva - names_rva_base:func_rva - names_rva_base + len(export_name)] = export_name
    struct.pack_into("<I", section, address_table - names_rva_base, address)
    struct.pack_into("<I", section, name_table - names_rva_base, func_rva)
    struct.pack_into("<H", section, ordinal_table - names_rva_base, 0)
    raw_size = align(len(section), file_align)
    section.extend(b"\0" * (raw_size - len(section)))

    dos = bytearray(128)
    dos[0:2] = b"MZ"
    struct.pack_into("<I", dos, 0x3C, 128)
    pe = bytearray()
    pe += b"PE\0\0"
    section_count = 1
    optional_size = 112 if not pe32plus else 240 - 112
    # COFF
    pe += struct.pack("<HHIIIHH", machine, section_count, 0, 0, 0, 240 if pe32plus else 224, 0x2022)
    image_size = 0x2000 + align(len(section), section_align)
    entry = 0
    if pe32plus:
        optional = bytearray(240)
        struct.pack_into("<H", optional, 0, 0x20B)
        struct.pack_into("<I", optional, 16, entry)
        struct.pack_into("<Q", optional, 24, 0x100000000)
        struct.pack_into("<I", optional, 32, 0x1000)
        struct.pack_into("<I", optional, 36, 0x200)
        struct.pack_into("<HH", optional, 40, 6, 0)
        struct.pack_into("<HH", optional, 44, 0, 0)
        struct.pack_into("<HH", optional, 48, 6, 0)
        struct.pack_into("<I", optional, 56, image_size)
        struct.pack_into("<I", optional, 60, 0x200)
        struct.pack_into("<H", optional, 68, 3)
        struct.pack_into("<Q", optional, 72, 0x100000)
        struct.pack_into("<Q", optional, 80, 0x1000)
        struct.pack_into("<Q", optional, 88, 0x100000)
        struct.pack_into("<Q", optional, 96, 0x1000)
        struct.pack_into("<I", optional, 108, 16)
        # data directory 0 is export
        struct.pack_into("<II", optional, 112, export_rva, 40)
    else:
        optional = bytearray(224)
        struct.pack_into("<H", optional, 0, 0x10B)
        struct.pack_into("<I", optional, 16, entry)
        struct.pack_into("<I", optional, 28, 0x10000000)
        struct.pack_into("<I", optional, 32, 0x1000)
        struct.pack_into("<I", optional, 36, 0x200)
        struct.pack_into("<HH", optional, 40, 6, 0)
        struct.pack_into("<HH", optional, 44, 0, 0)
        struct.pack_into("<HH", optional, 48, 6, 0)
        struct.pack_into("<I", optional, 56, image_size)
        struct.pack_into("<I", optional, 60, 0x200)
        struct.pack_into("<H", optional, 68, 3)
        struct.pack_into("<I", optional, 72, 0x100000)
        struct.pack_into("<I", optional, 76, 0x1000)
        struct.pack_into("<I", optional, 80, 0x100000)
        struct.pack_into("<I", optional, 84, 0x1000)
        struct.pack_into("<I", optional, 92, 16)
        struct.pack_into("<II", optional, 96, export_rva, 40)
    section_header = bytearray(40)
    section_header[0:6] = b".rdata"
    struct.pack_into("<I", section_header, 8, len(payload))
    struct.pack_into("<I", section_header, 12, 0x2000)
    struct.pack_into("<I", section_header, 16, raw_size)
    struct.pack_into("<I", section_header, 20, 0x200)
    struct.pack_into("<I", section_header, 36, 0x40000040)
    headers = dos + pe + optional + section_header
    headers.extend(b"\0" * (0x200 - len(headers)))
    pathlib.Path(destination).write_bytes(bytes(headers) + bytes(section))


def main():
    kind, target, source, destination, symbol = sys.argv[1:6]
    payload = pathlib.Path(source).read_bytes()
    if kind == "elf":
        write_elf(ELF_MACHINE[target], payload, symbol, destination)
    elif kind == "dll":
        write_dll(PE_MACHINE[target], payload, symbol, destination)
    else:
        raise SystemExit("unit kind must be elf or dll")


if __name__ == "__main__":
    main()
