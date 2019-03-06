# CFI-LB: Adaptive Call-site Sensitive Control Flow Integrity
# Authors: Mustakimur Khandaker (Florida State University)
# Abu Naser (Florida State University)
# Wenqing Liu (Florida State University)
# Zhi Wang (Florida State University)
# Yajin Zhou (Zhejiang University)
# Yueqiang Chen (Baidu X-lab)

"""
This python code use elftools api to extract elf binary information
1. it record the entry address of main function
2. it record the function information from symbol table
3. it record the global variable information from symbol table
"""

import sys

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from elftools.elf.sections import SymbolTableSection
from elftools.elf.descriptions import describe_symbol_shndx

# we may exclude few known functions and variables from to be tracked
exclude_func = ['__libc_csu_init', '__libc_csu_fini', '_start']
exclude_obj = ['CI_TABLE', 'CI_LENGTH', 'CFILB_D0', 'CFILB_D0_C', 'CFILB_D1',
               'CFILB_D1_C', 'CFILB_D2', 'CFILB_D2_C', 'CFILB_D3', 'CFILB_D3_C',
               'CFILB_HASH_TABLE', 'logFile']


def record_main_function_address(elfBinary):
    file = open("record_main.tmp", 'w')
    with open(elfBinary, 'rb') as f:
        elffile = ELFFile(f)

        symtab_name = '.symtab'
        symtab = elffile.get_section_by_name(symtab_name)

        if not isinstance(symtab, SymbolTableSection):
            print('  The file has no %s section' % symtab_name)

        for sym in symtab.iter_symbols():
            if ('STV_DEFAULT' in (sym.entry['st_other'])['visibility']
                    and 'STT_FUNC' in (sym.entry['st_info'])['type']
                and (sym.entry['st_shndx'] == 14 or sym.entry['st_shndx'] == 13 or sym.entry['st_shndx'] == 12)
                    and str(sym.name) == '_fini'):
                file.write(str(sym.entry['st_value']) + "\n")
    file.close()


def cal_func_info(elfBinary, wfile):
    with open(elfBinary, 'rb') as f:
        elffile = ELFFile(f)

        symtab_name = '.symtab'
        symtab = elffile.get_section_by_name(symtab_name)

        if not isinstance(symtab, SymbolTableSection):
            print('  The file has no %s section' % symtab_name)

        for sym in symtab.iter_symbols():
            if ('STV_DEFAULT' in (sym.entry['st_other'])['visibility']
                    and 'STT_FUNC' in (sym.entry['st_info'])['type']
                and (sym.entry['st_shndx'] == 14 or sym.entry['st_shndx'] == 13 or sym.entry['st_shndx'] == 12)
                    and not sym.name in exclude_func):
                    # function_name, function_entry_address, function_size
                wfile.write(sym.name + "\t" + str(sym.entry['st_value']) + "\t"
                            + str(sym.entry['st_size']) + "\n")


def take_global_snapshot(elfBinary, gfile):
    with open(elfBinary, 'rb') as f:
        elffile = ELFFile(f)

        symtab_name = '.symtab'
        symtab = elffile.get_section_by_name(symtab_name)

        if not isinstance(symtab, SymbolTableSection):
            print('  The file has no %s section' % symtab_name)

        for sym in symtab.iter_symbols():
            if ('STB_GLOBAL' in (sym.entry['st_info'])['bind']
                    and 'STV_DEFAULT' in (sym.entry['st_other'])['visibility']
                    and 'STT_OBJECT' in (sym.entry['st_info'])['type']
                    and not sym.name in exclude_obj):
                    # global_variable_address, global_variable_size
                gfile.write(str(sym.entry['st_value']) + "\t"
                            + str(sym.entry['st_size']) + "\n")


# python extract.py target_binary_directory target_binary
if(__name__ == '__main__'):
    elfBinary = str(sys.argv[1])

    record_main_function_address(elfBinary)

    wfile = open("elf_extract.bin", 'w')
    cal_func_info(elfBinary, wfile)
    wfile.close()

    gfile = open("glb_snapshot.bin", 'w')
    take_global_snapshot(elfBinary, gfile)
    gfile.close()
