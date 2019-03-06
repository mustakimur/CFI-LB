# CFI-LB: Adaptive Call-site Sensitive Control Flow Integrity
# Authors: Mustakimur Khandaker (Florida State University)
# Abu Naser (Florida State University)
# Wenqing Liu (Florida State University)
# Zhi Wang (Florida State University)
# Yajin Zhou (Zhejiang University)
# Yueqiang Chen (Baidu X-lab)

"""
This is a simple elftools extension to track if binary translate because of
cfg table instrumentation
"""

import sys

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from elftools.elf.sections import SymbolTableSection
from elftools.elf.descriptions import describe_symbol_shndx

exclude_func = ['__libc_csu_init', '__libc_csu_fini', '_start']

cfg_dict = dict()
ncfg_dict = dict()
com_map = dict()
n_com_map = dict()
diff_map = dict()


def bin_cfg_info(elfBinary):
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
                cfg_dict[sym.name] = sym.entry['st_value']


def bin_ncfg_info():
    rfile = open("elf_extract.bin", 'r')
    for line in rfile:
        if(len(line.split('\t')) == 3):
            ncfg_dict[line.split('\t')[0]] = (int(
                line.split('\t')[1], 10), int(
                line.split('\t')[2], 10))
    rfile.close()


def cfg_data():
    rfile = open("cfilb_cfg.bin", 'r')
    for line in rfile:
        if (len(line.split('\t')) == 5):
            call_point = int(line.split('\t')[0], 10)
            call_target = int(line.split('\t')[1], 10)
            call_site1 = int(line.split('\t')[2], 10)
            call_site2 = int(line.split('\t')[3], 10)
            call_site3 = int(line.split('\t')[4].rstrip(), 10)

            if(not(call_point, call_site1, call_site2, call_site3) in com_map):
                com_map[(call_point, call_site1, call_site2, call_site3)] = []
            com_map[(call_point, call_site1, call_site2,
                     call_site3)].append(call_target)
    rfile.close()


def diff_translation():
    diff_32 = 0
    diff_0 = 0
    diff_other = 0
    for key, val in ncfg_dict.iteritems():
        cfg_addr = cfg_dict[key]
        ncfg_addr = val[0]
        ncfg_size = val[1]
        diff = 0
        if(cfg_addr > ncfg_addr):
            diff = cfg_addr - ncfg_addr
            diff_map[key] = (ncfg_addr, ncfg_size, diff)
        else:
            diff_map[key] = (ncfg_addr, ncfg_size, 0)

    for key, val in com_map.iteritems():
        site1_old = key[1]
        site2_old = key[2]
        site3_old = key[3]
        site1_new = site1_old
        site2_new = site2_old
        site3_new = site3_old

        for name, info in diff_map.iteritems():
            if(site1_old >= info[0] and site1_old <= info[1]):
                site1_new += info[2]
            if(site2_old >= info[0] and site2_old <= info[1]):
                site2_new += info[2]
            if(site3_old >= info[0] and site3_old <= info[1]):
                site3_new += info[2]
        for target_old in val:
            target_new = target_old
            for name, info in diff_map.iteritems():
                if(target_old >= info[0] and target_old <= info[1]):
                    target_new += info[2]

            if(not(key[0], site1_new, site2_new, site3_new) in n_com_map):
                n_com_map[(key[0], site1_new, site2_new, site3_new)] = []
            n_com_map[(key[0], site1_new, site2_new, site3_new)
                      ].append(target_new)

    wfile = open("cfilb_cfg.bin", 'w')
    for key, val in n_com_map.iteritems():
        for item in set(val):
            wfile.write(str(key[0]) + "\t" + str(item) + "\t" + str(key[1]) + "\t" + str(key[2]) +
                        "\t" + str(key[3]) + "\n")
    wfile.close()


# python calculate_diff.py target_binary_directory target_binary
if (__name__ == '__main__'):
    elfBinary = str(sys.argv[1])

    bin_cfg_info(elfBinary)
    bin_ncfg_info()
    cfg_data()

    diff_translation()
