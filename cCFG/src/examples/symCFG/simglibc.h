/*
 * CFI-LB: Adaptive Call-site Sensitive Control Flow Integrity
 * Authors: Mustakimur Khandaker (Florida State University)
 */
#ifndef SIMGLIBC_H
#define SIMGLIBC_H
#include <fstream>
#include <triton/api.hpp>
#include <triton/elf.hpp>
#include <triton/x86Specifications.hpp>

// false to stop showing debug information
#define DEBUG false

// default address setup for stack, plt, argv memories
#define BASE_PLT 0x1000000
#define BASE_MEM 0x10000000
#define MAX_MEM_ALLOCATION 500

#define MAX_WAITING_SEED 70
#define MAX_ACCEPTED_FRESH_SEED 15
#define MAX_TRIED_FRESH_SEED 50
#define MAX_USED_SEED 500

#define EMULATOR_REGULAR_EXIT 0
#define DISCOVERED_EXTERNAL_LIBC 1
#define NO_LIBC_MATCHED 2
#define UNEXPECTED_TERMINATOR 3
#define LIBC_MATCHED 4

// Triton code base greatly depends on namespace properties
using namespace std;
using namespace triton;
using namespace triton::arch;
using namespace triton::modes;
using namespace triton::format;
using namespace triton::format::elf;

typedef struct glibcRet {
  triton::uint512 value;
  triton::uint64 sym_addr = 0x0;
  unsigned int sym_size;
  unsigned char *sym_raw;
  bool isSymbolic = false;
  bool isTerminate = false;
  bool isVoid = false;
  int jmpFlag = 0;
} retLibc;

struct seedInfo {
  triton::uint512 data;
  bool isSymbolic;
  bool isWrite;
};

typedef map<triton::uint64, seedInfo>
    seedMap;  // holds memory mapping for seeds

// use for store trace dump seed information in memory
struct blockInfo {
  unsigned long long rbp_address = 0x0;  // function initial rbp value
  map<unsigned long long, unsigned long long>
      ins_mem_map;  // instructions involved with symbolic variable write
  map<unsigned long long, bool>
      mem_sym_map;  // if this memory need to be symbolic, set T, else F
  map<unsigned long long, unsigned int>
      mem_size_map;  // content size for a memory address
  map<unsigned long long, unsigned char *>
      mem_data_map;  // content in byte for a memory address
  map<unsigned long long, bool>
      mem_write_map;            // if memory write ins, then true
  unsigned long long exitAddr;  // block exit address
};

const unsigned char charset[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

// generic type of relocated methods
typedef void FUNC(retLibc *r);
void debug(string s);
string toHex(unsigned long long x);
bool isLib(triton::uint64 key);
void simglibcinit();
int hookingHandler(triton::uint64 pc, triton::uint64 call_addr, seedMap seed,
                   blockInfo *blk);
void makeRelocation(triton::format::elf::Elf *binary);

string getMemoryString(triton::uint512 addr);
string strReplace(string str, string match, string rep);
int collectFormat(string str, bool *c);
string repFormatString(string str);
void __signal(retLibc *r);
void __strlen(retLibc *r);
void __printf(retLibc *r);
void __puts(retLibc *r);
void __atoi(retLibc *r);
void __time(retLibc *r);
void __getenv(retLibc *r);
void __free(retLibc *r);
void __localtime(retLibc *r);
void __abort(retLibc *r);
void __strncpy(retLibc *r);
void __strcpy(retLibc *r);
void __remove(retLibc *r);
void __strncmp(retLibc *r);
void __strcmp(retLibc *r);
void __fprintf(retLibc *r);
void __vfprintf(retLibc *r);
void __fflush(retLibc *r);
void __exit(retLibc *r);
void __toupper(retLibc *r);
void __tolower(retLibc *r);
void __fread(retLibc *r);
void __exp(retLibc *r);
void __atof(retLibc *r);
void __clock(retLibc *r);
void __fclose(retLibc *r);
void __ctime(retLibc *r);
void __system(retLibc *r);
void __strchr(retLibc *r);
void __strrchr(retLibc *r);
void __rewind(retLibc *r);
void __fputs(retLibc *r);
void __memset(retLibc *r);
void __pow(retLibc *r);
void __log(retLibc *r);
void __strncat(retLibc *r);
void __strspn(retLibc *r);
void __fputc(retLibc *r);
void __strcspn(retLibc *r);
void __fgets(retLibc *r);
void __calloc(retLibc *r);
void __ftell(retLibc *r);
void __feof(retLibc *r);
void __malloc(retLibc *r);
void __memcpy(retLibc *r);
void __strcat(retLibc *r);
void __sqrt(retLibc *r);
void __floor(retLibc *r);
void __strstr(retLibc *r);
void __fwrite(retLibc *r);
void __strtoul(retLibc *r);
void __sprintf(retLibc *r);
void __strpbrk(retLibc *r);
void __fseek(retLibc *r);
void __realloc(retLibc *r);
void __fopen(retLibc *r);
void __perror(retLibc *r);
void __strtok(retLibc *r);
void __strftime(retLibc *r);
void __memmove(retLibc *r);
void __abs(retLibc *r);
void __getcwd(retLibc *r);
void __unlink(retLibc *r);
void __ferror(retLibc *r);
void __close(retLibc *r);
void __read(retLibc *r);
void __memcmp(retLibc *r);
void __setjmp(retLibc *r);
void __longjmp(retLibc *r);
void __strtol(retLibc *r);
void __open(retLibc *r);
void __strerror(retLibc *r);
void __ungetc(retLibc *r);
void __bsearch(retLibc *r);
void __getpagesize(retLibc *r);
void __vsprintf(retLibc *r);
void __obstack_free(retLibc *r);
void ____xstat(retLibc *r);
void ____fxstat(retLibc *r);
void ___IO_getc(retLibc *r);
void ___obstack_newchunk(retLibc *r);
/* need to implement for gcc */

#endif
