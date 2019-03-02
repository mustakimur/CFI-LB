/*
 * Project: Adaptive Control Flow Integrity with Look Back and Multi-scope CFG
 * Generation Author: Mustakimur Rahman Khandaker (mrk15e@my.fsu.edu) Florida
 * State University Supervised: Dr-> Zhi Wang
 */
#include "simglibc.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// signal handling purpose
map<triton::uint512, triton::uint512> sigHandlers;
map<triton::uint512, triton::uint64> jmp_stat_map;

/* following three array lists all relocated handlers information */
string lib_name[] = {"printf",   "puts",         "strlen",
                     "signal",   "atoi",         "time",
                     "getenv",   "free",         "localtime",
                     "abort",    "strncpy",      "remove",
                     "strncmp",  "fprintf",      "vfprintf",
                     "fflush",   "exit",         "strcpy",
                     "strcmp",   "toupper",      "fread",
                     "exp",      "atof",         "clock",
                     "fclose",   "ctime",        "system",
                     "strchr",   "strrchr",      "rewind",
                     "fputs",    "memset",       "pow",
                     "log",      "strncat",      "strspn",
                     "fputc",    "strcspn",      "fgets",
                     "calloc",   "ftell",        "feof",
                     "malloc",   "memcpy",       "tolower",
                     "strcat",   "sqrt",         "floor",
                     "strstr",   "fwrite",       "strtoul",
                     "sprintf",  "strpbrk",      "fseek",
                     "fopen",    "realloc",      "perror",
                     "strtok",   "strftime",     "memmove",
                     "unlink",   "ferror",       "abs",
                     "getcwd",   "close",        "read",
                     "memcmp",   "setjmp",       "longjmp",
                     "ungetc",   "bsearch",      "getpagesize",
                     "vsprintf", "obstack_free", "__xstat",
                     "__fxstat", "_IO_getc",     "_obstack_newchunk",
                     "open"};  // name to
                               // match
int lib_addr[] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};  // address to
                                                       // relocate
FUNC(*lib_rel[]) = {__printf,   __puts,         __strlen,
                    __signal,   __atoi,         __time,
                    __getenv,   __free,         __localtime,
                    __abort,    __strncpy,      __remove,
                    __strncmp,  __fprintf,      __vfprintf,
                    __fflush,   __exit,         __strcpy,
                    __strcmp,   __toupper,      __fread,
                    __exp,      __atof,         __clock,
                    __fclose,   __ctime,        __system,
                    __strchr,   __strrchr,      __rewind,
                    __fputs,    __memset,       __pow,
                    __log,      __strncat,      __strspn,
                    __fputc,    __strcspn,      __fgets,
                    __calloc,   __ftell,        __feof,
                    __malloc,   __memcpy,       __tolower,
                    __strcat,   __sqrt,         __floor,
                    __strstr,   __fwrite,       __strtoul,
                    __sprintf,  __strpbrk,      __fseek,
                    __fopen,    __realloc,      __perror,
                    __strtok,   __strftime,     __memmove,
                    __unlink,   __ferror,       __abs,
                    __getcwd,   __close,        __read,
                    __memcmp,   __setjmp,       __longjmp,
                    __ungetc,   __bsearch,      __getpagesize,
                    __vsprintf, __obstack_free, ____xstat,
                    ____fxstat, ___IO_getc,     ___obstack_newchunk,
                    __open};  // handler to
                              // call

unsigned long long mem_written = 0;

// print debug only information
void debug(string s) {
  if (DEBUG) {
    cout << "[Sym-Emulator] " << s << endl;
  }
  return;
}

void displayRLibC(retLibc *r) {
  cout << "return value " << hex << r->value << dec << endl;
  cout << "is terminating function " << r->isTerminate << endl;
  cout << "is symbolic " << r->isSymbolic << endl;
  if (r->sym_addr != 0x0) {
    cout << "symbolic information ..." << endl;
    cout << "... address " << hex << r->sym_addr << dec << endl;
    cout << "... size " << r->sym_size << endl;
    cout << "... seed " << r->sym_raw << endl;
  }
}

string toHex(unsigned long long x) {
  std::stringstream sstream;
  sstream << std::hex << x;
  std::string result = sstream.str();
  return result;
}

// retrieve the text from starting memory address
string getMemoryString(triton::uint512 addr) {
  string s;
  triton::uint64 index = 0;

  while (api.getConcreteMemoryValue(triton::uint64(addr + index))) {
    char c = (char)api.getConcreteMemoryValue(triton::uint64(addr + index));
    if (isprint(c)) {
      s += c;
    }
    index++;
  }
  return s;
}

// char * strtok ( char * str, const char * delimiters );
void __strtok(retLibc *r) {
  debug("strtok hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);

  string src1 = getMemoryString(arg1);
  string src2 = getMemoryString(arg2);

  char *temp = (char *)src1.c_str();
  char *token = strtok(temp, src2.c_str());

  if (token == NULL) {
    r->value = 0x0;
  } else {
    r->value = arg1 + (token - temp);
  }

  return;
}

// char * strpbrk (char * str1, const char * str2 );
void __strpbrk(retLibc *r) {
  debug("strpbrk hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  string src1 = getMemoryString(arg1);
  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);
  string src2 = getMemoryString(arg2);

  char *temp = (char *)src1.c_str();
  char *token = strpbrk(temp, src2.c_str());

  if (token == NULL) {
    r->value = 0x0;
  } else {
    r->value = arg1 + (token - temp);
  }

  return;
}

// char * strstr (char * str1, const char * str2 );
void __strstr(retLibc *r) {
  debug("strstr hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  string src1 = getMemoryString(arg1);
  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);
  string src2 = getMemoryString(arg2);

  char *temp = (char *)src1.c_str();
  char *token = strstr(temp, src2.c_str());

  if (token == NULL) {
    r->value = 0x0;
  } else {
    r->value = arg1 + (token - temp);
  }

  return;
}

// char* strcpy(char* destination, const char* source);
void __strcpy(retLibc *r) {
  debug("strcpy hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);
  string src2 = getMemoryString(arg2);
  int size = strlen(src2.c_str());

  r->sym_raw = (unsigned char *)malloc(size + 1);
  int i = 0;
  while (i < size) {
    r->sym_raw[i] = src2[i];
    i++;
  }

  // set concrete value to memory (like memory point to another memory)
  api.setConcreteMemoryAreaValue(arg1, r->sym_raw, size);
  r->value = arg1;

  return;
}

// char * strncpy ( char * destination, const char * source, size_t num );
void __strncpy(retLibc *r) {
  debug("strncpy hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);
  triton::uint512 arg3 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDX);
  string src2 = getMemoryString(arg2);
  int size =
      strlen(src2.c_str()) <= (int)arg3 ? strlen(src2.c_str()) : (int)arg3;

  r->sym_raw = (unsigned char *)malloc(size + 1);
  int i = 0;
  while (i < size) {
    r->sym_raw[i] = src2[i];
    i++;
  }

  // set concrete value to memory (like memory point to another memory)
  api.setConcreteMemoryAreaValue(arg1, r->sym_raw, size);
  r->value = arg1;

  return;
}

// char * strcat ( char * destination, const char * source);
void __strcat(retLibc *r) {
  debug("strcat hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  string src1 = getMemoryString(arg1);
  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);
  string src2 = getMemoryString(arg2);
  int size1 = strlen(src1.c_str());
  int size2 = strlen(src2.c_str());
  int size = size1 + size2;

  r->sym_raw = (unsigned char *)malloc(size + 1);
  int i = 0;
  while (i < size1) {
    r->sym_raw[i] = src1[i];
    i++;
  }
  int j = 0;
  while (j < size2) {
    r->sym_raw[i] = src2[j];
    i++;
    j++;
  }

  // set concrete value to memory (like memory point to another memory)
  api.setConcreteMemoryAreaValue(arg1, r->sym_raw, size);
  r->value = arg1;

  return;
}

// char * strncat ( char * destination, const char * source, size_t num );
void __strncat(retLibc *r) {
  debug("strncat hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  string src1 = getMemoryString(arg1);
  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);
  string src2 = getMemoryString(arg2);
  triton::uint512 arg3 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDX);
  int size1 = strlen(src1.c_str());
  int size2 =
      strlen(src2.c_str()) <= (int)arg3 ? strlen(src2.c_str()) : (int)arg3;
  int size = size1 + size2;

  r->sym_raw = (unsigned char *)malloc(size + 1);
  int i = 0;
  while (i < size1) {
    r->sym_raw[i] = src1[i];
    i++;
  }
  int j = 0;
  while (j < size2) {
    r->sym_raw[i] = src2[j];
    i++;
    j++;
  }

  // set concrete value to memory (like memory point to another memory)
  api.setConcreteMemoryAreaValue(arg1, r->sym_raw, size);
  r->value = arg1;

  return;
}

// char * strchr (char * str, int character );
void __strchr(retLibc *r) {
  debug("strchr hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  string src1 = getMemoryString(arg1);
  triton::uint512 arg2 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RSI);

  char *temp = (char *)src1.c_str();
  char *token = strchr(temp, (int)arg2);

  if (token == NULL) {
    r->value = 0x0;
  } else {
    r->value = arg1 + (token - temp);
  }

  return;
}
// const char *strrchr(const char *str, int character);
void __strrchr(retLibc *r) {
  debug("strrchr hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  string src1 = getMemoryString(arg1);
  triton::uint512 arg2 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RSI);

  char *temp = (char *)src1.c_str();
  char *token = strrchr(temp, (int)arg2);

  if (token == NULL) {
    r->value = 0x0;
  } else {
    r->value = arg1 + (token - temp);
  }

  return;
}

// size_t strlen ( const char * str );
void __strlen(retLibc *r) {
  debug("strlen hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  string src1 = getMemoryString(
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI));

  r->value = strlen(src1.c_str());

  if (api.isMemorySymbolized(MemoryAccess(arg1, 8))) {
    r->isSymbolic = true;
  }
  return;
}

// int strncmp ( const char * str1, const char * str2, size_t num );
void __strncmp(retLibc *r) {
  debug("strncmp hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  string src1 = getMemoryString(
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI));
  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);
  string src2 = getMemoryString(
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RSI));

  triton::uint512 arg3 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDX);

  int temp = strncmp(src1.c_str(), src2.c_str(), (unsigned int)arg3);

  r->value = (triton::uint512)(temp < 0 ? 0xffffffffffffffff : temp);

  if (api.isMemorySymbolized(MemoryAccess(arg1, 8)) ||
      api.isMemorySymbolized(MemoryAccess(arg2, 8))) {
    r->isSymbolic = true;
  }

  return;
}

// int strcmp ( const char * str1, const char * str2 );
void __strcmp(retLibc *r) {
  debug("strcmp hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  string src1 = getMemoryString(
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI));
  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);
  string src2 = getMemoryString(
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RSI));

  int temp = strcmp(src1.c_str(), src2.c_str());
  r->value = (triton::uint512)(temp < 0 ? 0xffffffffffffffff : temp);

  if (api.isMemorySymbolized(MemoryAccess((triton::uint64)arg1, 8)) ||
      api.isMemorySymbolized(MemoryAccess((triton::uint64)arg2, 8))) {
    r->isSymbolic = true;
  }
  return;
}

// size_t strspn ( const char * str1, const char * str2 );
void __strspn(retLibc *r) {
  debug("strspn hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);

  string src1 = getMemoryString(arg1);
  string src2 = getMemoryString(arg2);

  r->value = strspn(src1.c_str(), src2.c_str());

  if (api.isMemorySymbolized(MemoryAccess(arg1, 8))) {
    r->isSymbolic = true;
  }

  return;
}

// size_t strcspn ( const char * str1, const char * str2 );
void __strcspn(retLibc *r) {
  debug("strcspn hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);

  string src1 = getMemoryString(arg1);
  string src2 = getMemoryString(arg2);

  r->value = strcspn(src1.c_str(), src2.c_str());

  if (api.isMemorySymbolized(MemoryAccess(arg1, 8))) {
    r->isSymbolic = true;
  }
  return;
}

// size_t strftime(char *ptr, size_t maxsize, const char *format,
//                const struct tm *timeptr);
void __strftime(retLibc *r) {
  debug("strftime hooked ...");

  time_t rawtime;
  struct tm *info;

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  triton::uint512 arg2 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RSI);

  char temp[80];
  time(&rawtime);
  info = localtime(&rawtime);
  strftime(temp, 80, "%x - %I:%M%p", info);
  int size = strlen(temp);
  r->sym_raw = (unsigned char *)malloc(size + 1);

  for (int i = 0; i <= size; i++) {
    r->sym_raw[i] = temp[i];
  }

  // set concrete value to memory (like memory point to another memory)
  api.setConcreteMemoryAreaValue(arg1, r->sym_raw, size);
  r->value = size;
  r->sym_addr = arg1;
  r->sym_size = size;
  r->isSymbolic = true;

  return;
}

// clock_t clock(void)
void __clock(retLibc *r) {
  debug("clock hooked ...");
  r->value = clock();
  r->isSymbolic = true;
  return;
}

// struct tm * localtime (const time_t * timer);
void __localtime(retLibc *r) {
  debug("localtime hooked ...");

  time_t rawtime;
  struct tm *timeinfo;

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  r->sym_raw = (unsigned char *)malloc(sizeof(timeinfo));
  memcpy(r->sym_raw, timeinfo, sizeof(timeinfo));

  // set concrete value to memory (like memory point to another memory)
  api.setConcreteMemoryAreaValue(BASE_MEM + mem_written, r->sym_raw,
                                 sizeof(timeinfo));
  r->value = BASE_MEM + mem_written;
  mem_written += sizeof(timeinfo);

  return;
}

// char *ctime(const time_t *timer)
void __ctime(retLibc *r) {
  debug("ctime hooked ...");

  time_t curtime;
  std::vector<triton::uint8> temp;
  time(&curtime);
  char *res = ctime(&curtime);

  for (int j = 0; j < strlen(res); j++) {
    temp.push_back(res[j]);
  }
  api.setConcreteMemoryAreaValue(BASE_MEM + mem_written, temp);
  r->value = BASE_MEM + mem_written;
  mem_written += strlen(res);

  return;
}

// time_t time(time_t *t)
void __time(retLibc *r) {
  debug("time hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);

  time_t t = time(NULL);

  unsigned char *temp = (unsigned char *)&t;
  int size = sizeof(t);

  api.setConcreteMemoryAreaValue(BASE_MEM + mem_written, temp, size);
  r->value = BASE_MEM + mem_written;
  mem_written += size;

  if (arg1 != 0x0) {
    if (sizeof(t) < MAX_MEM_ALLOCATION) {
      r->sym_raw = (unsigned char *)malloc(size + 1);
    } else {
      r->sym_raw = (unsigned char *)malloc(MAX_MEM_ALLOCATION);
    }
    memcpy(r->sym_raw, temp, size);
    r->sym_addr = arg1;
    api.setConcreteMemoryAreaValue(arg1, r->sym_raw, size);
    r->sym_size = size;
  }

  return;
}

// int memcmp(const void *str1, const void *str2, size_t n)
void __memcmp(retLibc *r) {
  debug("memcmp hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);
  triton::uint512 arg3 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDX);

  int temp = memcmp((const void *)arg1, (const void *)arg2, (size_t)arg3);

  r->value = (triton::uint512)(temp < 0 ? 0xffffffffffffffff : temp);

  if (api.isMemorySymbolized(MemoryAccess(arg1, 8)) ||
      api.isMemorySymbolized(MemoryAccess(arg2, 8))) {
    r->isSymbolic = true;
  }

  return;
}

// void * memcpy ( void * destination, const void * source, size_t num );
void __memcpy(retLibc *r) {
  debug("memcpy hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);
  string src2 = getMemoryString(arg2);
  int size = strlen(src2.c_str());

  r->sym_raw = (unsigned char *)malloc(size + 1);
  int i = 0;
  while (i < size) {
    r->sym_raw[i] = src2[i];
    i++;
  }

  // set concrete value to memory (like memory point to another memory)
  api.setConcreteMemoryAreaValue(arg1, r->sym_raw, size);
  r->value = arg1;

  return;
}

// void * memset ( void * ptr, int value, size_t num );
void __memset(retLibc *r) {
  debug("memset hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  triton::uint512 arg2 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RSI);
  triton::uint512 arg3 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDX);
  string src1 = getMemoryString(arg1);
  int size = strlen(src1.c_str());

  char *temp = (char *)src1.c_str();
  if (arg3 > 100) arg3 = 100;
  memset(temp, (int)arg2, (int)arg3);

  r->sym_raw = (unsigned char *)malloc(size + 1);
  int i = 0;
  while (i < size) {
    r->sym_raw[i] = temp[i];
    i++;
  }

  // set concrete value to memory (like memory point to another memory)
  api.setConcreteMemoryAreaValue(arg1, r->sym_raw, size);
  r->value = arg1;

  return;
}

// void * memmove ( void * destination, const void * source, size_t num );
void __memmove(retLibc *r) {
  debug("memmove hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);
  string src1 = getMemoryString(arg1);
  string src2 = getMemoryString(arg2);
  triton::uint512 arg3 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDX);

  char *temp = (char *)src1.c_str();

  memmove(temp, src2.c_str(), (size_t)arg3);

  int size = strlen(temp);

  r->sym_raw = (unsigned char *)malloc(size + 1);
  int i = 0;
  while (i <= size) {
    r->sym_raw[i] = temp[i];
    i++;
  }

  // set concrete value to memory (like memory point to another memory)
  api.setConcreteMemoryAreaValue(arg1, r->sym_raw, size);
  r->value = arg1;

  return;
}
// char *strerror(int errnum)
void __strerror(retLibc *r) {
  debug("strerror hooked ...");
  std::vector<triton::uint8> temp;
  char res[30] = "arbitrary string";
  int size = strlen(res);
  for (int j = 0; j < size; j++) {
    temp.push_back(res[j]);
  }
  // set concrete value to memory (like memory point to another memory)
  api.setConcreteMemoryAreaValue(BASE_MEM + mem_written, temp);
  r->value = BASE_MEM + mem_written;
  mem_written += size;
  return;
}

// void perror ( const char * str )
void __perror(retLibc *r) {
  debug("perror hooked ...");
  r->isVoid = true;
  return;
}

// FILE *fopen(const char *filename, const char *mode);
void __fopen(retLibc *r) {
  debug("fopen hooked ...");
  r->value = 0xabcdef;
  return;
}

// int fclose(FILE *stream)
void __fclose(retLibc *r) {
  debug("fclose hooked ...");
  r->value = 0x0;
  return;
}

// int close(int fd)
void __close(retLibc *r) {
  debug("close hooked ...");
  r->value = 0x0;
  return;
}

// int remove(const char *filename)
void __remove(retLibc *r) {
  debug("remove hooked ...");
  r->value = 0x0;
  return;
}

// int fseek(FILE *stream, long int offset, int origin);
void __fseek(retLibc *r) {
  debug("fseek hooked ...");
  r->value = 0x0;
  return;
}

// void rewind(FILE *stream)
void __rewind(retLibc *r) {
  debug("rewind hooked ...");
  r->isVoid = true;
  return;
}

// int open(const char *pathname, int flags, mode_t mode)
void __open(retLibc *r) {
  debug("open hooked ...");
  r->value = 0xabcdef;
  return;
}

// ssize_t read(int fildes, void *buf, size_t nbyte)
void __read(retLibc *r) {
  debug("read hooked ...");

  triton::uint512 arg1 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI);
  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);
  triton::uint512 arg3 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDX);

  int size =
      ((arg3) < MAX_MEM_ALLOCATION) ? (int)(arg3) : MAX_MEM_ALLOCATION - 1;

  r->sym_raw = (unsigned char *)malloc(size + 1);

  int i = 0;
  while (i < (size + 1)) {
    int key = rand() % (int)(sizeof charset - 1);
    r->sym_raw[i] = charset[key];
    i++;
  }
  r->sym_raw[i] = '\0';

  r->value = i;
  // set concrete value to memory (like memory point to another memory)
  r->sym_addr = arg2;
  api.setConcreteMemoryAreaValue(arg2, r->sym_raw, i);
  r->sym_size = i;

  return;
}

// size_t fread ( void * ptr, size_t size, size_t count, FILE * stream );
void __fread(retLibc *r) {
  debug("fread hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  triton::uint512 arg2 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RSI);
  triton::uint512 arg3 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDX);

  int size = ((arg2 * arg3) < MAX_MEM_ALLOCATION) ? (int)(arg2 * arg3)
                                                  : MAX_MEM_ALLOCATION - 1;

  r->sym_raw = (unsigned char *)malloc(size + 1);

  int i = 0;
  while (i < (size + 1)) {
    int key = rand() % (int)(sizeof charset - 1);
    r->sym_raw[i] = charset[key];
    i++;
  }
  r->sym_raw[i] = '\0';

  r->value = i;
  // set concrete value to memory (like memory point to another memory)
  r->sym_addr = arg1;
  api.setConcreteMemoryAreaValue(arg1, r->sym_raw, i);
  r->sym_size = i;

  return;
}

// char * fgets ( char * str, int num, FILE * stream );
void __fgets(retLibc *r) {
  debug("fgets hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  triton::uint512 arg2 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RSI);

  if (arg2 < MAX_MEM_ALLOCATION - 1) {
    r->sym_raw = (unsigned char *)malloc((size_t)arg2 + 1);
  } else {
    r->sym_raw = (unsigned char *)malloc(MAX_MEM_ALLOCATION);
  }

  int size = arg2 < MAX_MEM_ALLOCATION ? int(arg2 + 1) : MAX_MEM_ALLOCATION - 1;

  int i = 0;
  while (i < size) {
    int key = rand() % (int)(sizeof charset - 1);
    r->sym_raw[i] = charset[key];
    i++;
  }
  r->sym_raw[i] = '\0';

  // set concrete value to memory (like memory point to another memory)
  api.setConcreteMemoryAreaValue(arg1, r->sym_raw, i);
  r->value = arg1;
  r->sym_addr = arg1;
  r->sym_size = i;

  return;
}

// int ungetc(int char, FILE *stream)
void __ungetc(retLibc *r) {
  debug("ungetc hooked ...");

  triton::uint512 arg1 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI);
  r->value = arg1;

  return;
}

// int _IO_getc(FILE *fp)
void ___IO_getc(retLibc *r) {
  debug("__IO_getc hooked ...");
  r->value = rand() % (int)(sizeof charset - 1);
  return;
}

// long int ftell(FILE *stream);
void __ftell(retLibc *r) {
  debug("ftell hooked ...");

  r->value = rand() % (int)(sizeof charset - 1);
  r->isSymbolic = true;

  return;
}

// int fprintf(FILE *stream, const char *format, ...)
void __fprintf(retLibc *r) {
  debug("fprintf hooked ...");

  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);

  string src2 = getMemoryString(arg2);

  r->value = strlen(src2.c_str());
  r->isSymbolic = true;

  return;
}

// int vfprintf(FILE* stream, const char* format, va_list arg);
void __vfprintf(retLibc *r) {
  debug("vfprintf hooked ...");

  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);

  string src2 = getMemoryString(arg2);

  r->value = strlen(src2.c_str());
  r->isSymbolic = true;

  return;
}

// int fflush ( FILE * stream );
void __fflush(retLibc *r) {
  debug("fflush hooked ...");
  r->value = 0x0;
  return;
}

// int feof(FILE *stream);
void __feof(retLibc *r) {
  debug("feof hooked ...");
  r->value = rand() % 2;
  return;
}

// int ferror(FILE *stream)
void __ferror(retLibc *r) {
  debug("ferror hooked ...");
  r->value = rand() % 2;
  return;
}

// size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream);
void __fwrite(retLibc *r) {
  debug("fwrite hooked ...");

  triton::uint512 arg2 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RSI);
  triton::uint512 arg3 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDX);

  r->value = arg2 * arg3;

  return;
}

// int fputs(const char *str, FILE *stream);
void __fputs(retLibc *r) {
  debug("fputs hooked ...");
  r->value = 0x1;
  return;
}

// int fputc ( int character, FILE * stream );
void __fputc(retLibc *r) {
  debug("fputc hooked ...");

  triton::uint512 arg1 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI);
  r->value = arg1;

  return;
}

// void* realloc (void* ptr, size_t size);
void __realloc(retLibc *r) {
  debug("realloc hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  triton::uint512 arg2 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RSI);

  if (arg1 == 0x0) {
    arg1 = BASE_MEM + mem_written;
    mem_written += (unsigned long long)(arg2) < MAX_MEM_ALLOCATION
                       ? (unsigned long long)arg2
                       : MAX_MEM_ALLOCATION;
  }

  r->value = arg1;

  return;
}

// void *malloc(size_t size);
void __malloc(retLibc *r) {
  debug("malloc hooked ...");

  triton::uint512 arg1 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI);

  r->value = BASE_MEM + mem_written;
  mem_written += (unsigned long long)arg1 < MAX_MEM_ALLOCATION
                     ? (unsigned long long)(arg1)
                     : MAX_MEM_ALLOCATION;

  return;
}

// void* calloc (size_t num, size_t size);
void __calloc(retLibc *r) {
  debug("calloc hooked ...");

  triton::uint512 arg1 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI);
  triton::uint512 arg2 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RSI);

  r->value = BASE_MEM + mem_written;
  mem_written += (unsigned long long)(arg1 * arg2) < MAX_MEM_ALLOCATION
                     ? (unsigned long long)(arg1 * arg2)
                     : MAX_MEM_ALLOCATION;

  return;
}

// void free(void *ptr)
void __free(retLibc *r) {
  debug("free hooked ...");
  r->isVoid = true;
  return;
}

//
void ___obstack_newchunk(retLibc *r) {
  debug("_obstack_newchunk hooked ...");
  r->isVoid = true;
  return;
}

// obstack_free (obstack_ptr, first_object_allocated_ptr)
void __obstack_free(retLibc *r) {
  debug("obstack_free hooked ...");
  r->isVoid = true;
  return;
}

// int sprintf(char *str, const char *format, ...);
void __sprintf(retLibc *r) {
  debug("sprintf hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  triton::uint512 arg2 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RSI);

  string src2 = getMemoryString(arg2);

  r->value = sizeof(src2.c_str());

  if (r->value < MAX_MEM_ALLOCATION) {
    r->sym_raw = (unsigned char *)malloc(size_t(r->value + 1));
    memcpy(r->sym_raw, src2.c_str(), (size_t)r->value);
  } else {
    r->sym_raw = (unsigned char *)malloc(MAX_MEM_ALLOCATION);
    memcpy(r->sym_raw, src2.c_str(), (size_t)MAX_MEM_ALLOCATION - 1);
    r->value = MAX_MEM_ALLOCATION - 1;
  }

  // set concrete value to memory (like memory point to another memory)
  api.setConcreteMemoryAreaValue(arg1, r->sym_raw, (triton::usize)r->value);

  r->isSymbolic = true;

  return;
}

// int vsprintf(char *str, const char *format, va_list arg)
void __vsprintf(retLibc *r) {
  debug("vsprintf hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  triton::uint512 arg2 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RSI);

  string src2 = getMemoryString(arg2);

  r->value = sizeof(src2.c_str());

  if (r->value < MAX_MEM_ALLOCATION) {
    r->sym_raw = (unsigned char *)malloc(size_t(r->value + 1));
    memcpy(r->sym_raw, src2.c_str(), (size_t)r->value);
  } else {
    r->sym_raw = (unsigned char *)malloc(MAX_MEM_ALLOCATION);
    memcpy(r->sym_raw, src2.c_str(), (size_t)MAX_MEM_ALLOCATION - 1);
    r->value = MAX_MEM_ALLOCATION - 1;
  }

  // set concrete value to memory (like memory point to another memory)
  api.setConcreteMemoryAreaValue(arg1, r->sym_raw, (triton::usize)r->value);

  r->isSymbolic = true;
}

// int printf(const char *format, ...)
void __printf(retLibc *r) {
  debug("printf hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);

  string src1 = getMemoryString(arg1);

  r->value = strlen(src1.c_str());
  return;
}

// int puts ( const char * str );
void __puts(retLibc *r) {
  debug("puts hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  string src1 = getMemoryString(arg1);

  r->value = strlen(src1.c_str());

  return;
}

// unsigned long int strtoul (const char* str, char** endptr, int base);
void __strtoul(retLibc *r) {
  debug("strtoul hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  string src1 = getMemoryString(arg1);
  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);
  triton::uint512 arg3 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDX);

  r->value = strtoul(src1.c_str(), NULL, (int)arg3);

  if (api.isMemorySymbolized(MemoryAccess(arg1, 8))) {
    r->isSymbolic = true;
  }
  return;
}

// long int strtol(const char *str, char **endptr, int base)
void __strtol(retLibc *r) {
  debug("strtol hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  string src1 = getMemoryString(arg1);
  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);
  triton::uint512 arg3 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDX);

  r->value = strtol(src1.c_str(), NULL, (int)arg3);

  if (api.isMemorySymbolized(MemoryAccess(arg1, 8))) {
    r->isSymbolic = true;
  }
  return;
}

// double atof (const char* str);
void __atof(retLibc *r) {
  debug("atof hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  string src1 = getMemoryString(arg1);
  r->value = (triton::uint512)atof(src1.c_str());
  if (api.isMemorySymbolized(MemoryAccess(arg1, 8))) {
    r->isSymbolic = true;
  }
  return;
}

// int atoi (const char * str);
void __atoi(retLibc *r) {
  debug("atoi hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  string src1 = getMemoryString(arg1);
  r->value = (triton::uint512)atoi(src1.c_str());
  if (api.isMemorySymbolized(MemoryAccess(arg1, 8))) {
    r->isSymbolic = true;
  }
  return;
}

// double sqrt (double x);
void __sqrt(retLibc *r) {
  debug("sqrt hooked ...");

  triton::uint512 arg1 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI);

  r->value = (triton::uint512)sqrt(double(arg1));

  if (api.isRegisterSymbolized(triton::arch::x86::ID_REG_RDI)) {
    r->isSymbolic = true;
  }
  return;
}

// double floor (double x);
void __floor(retLibc *r) {
  debug("floor hooked ...");

  triton::uint512 arg1 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI);

  r->value = (triton::uint512)floor(double(arg1));

  if (api.isRegisterSymbolized(triton::arch::x86::ID_REG_RDI)) {
    r->isSymbolic = true;
  }
  return;
}

// double pow(double x, double y)
void __pow(retLibc *r) {
  debug("pow hooked ...");

  triton::uint512 arg1 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI);
  triton::uint512 arg2 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RSI);

  r->value = (triton::uint512)pow(double(arg1), double(arg2));

  if (api.isRegisterSymbolized(triton::arch::x86::ID_REG_RDI) ||
      api.isRegisterSymbolized(triton::arch::x86::ID_REG_RSI)) {
    r->isSymbolic = true;
  }
  return;
}

// double log(double x)
void __log(retLibc *r) {
  debug("log hooked");

  triton::uint512 arg1 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI);

  if (arg1 != 0x0)
    r->value = (triton::uint512)log(double(arg1));
  else
    r->value = 0x0;

  if (api.isRegisterSymbolized(triton::arch::x86::ID_REG_RDI)) {
    r->isSymbolic = true;
  }
  return;
}

// int abs(int x)
void __abs(retLibc *r) {
  debug("abs hooked");

  triton::uint512 arg1 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI);

  r->value = (triton::uint512)exp(int(arg1));

  if (api.isRegisterSymbolized(triton::arch::x86::ID_REG_RDI)) {
    r->isSymbolic = true;
  }
  return;
}

// double exp(double x)
void __exp(retLibc *r) {
  debug("exp hooked");

  triton::uint512 arg1 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI);

  r->value = (triton::uint512)exp(double(arg1));

  if (api.isRegisterSymbolized(triton::arch::x86::ID_REG_RDI)) {
    r->isSymbolic = true;
  }
  return;
}

// void *bsearch(const void *key, const void *base, size_t nitems, size_t size,
// int (*compar)(const void *, const void *))
void __bsearch(retLibc *r) {
  debug("bsearch hooked");

  triton::uint512 arg1 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI);
  triton::uint64 arg2 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RSI);
  triton::uint512 arg3 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDX);
  triton::uint512 arg4 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RCX);

  if (arg3 == 0) arg3 = 5;

  r->value = arg2 + ((rand() % arg3) * arg2);

  return;
}

// int tolower(int c);
void __tolower(retLibc *r) {
  debug("tolower hooked ...");

  triton::uint512 c =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI);
  r->value = tolower((int)c);
  if (api.isRegisterSymbolized(triton::arch::x86::ID_REG_RDI)) {
    r->isSymbolic = true;
  }
  return;
}

// int toupper(int c);
void __toupper(retLibc *r) {
  debug("toupper hooked ...");

  triton::uint512 c =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI);
  r->value = tolower((int)c);
  if (api.isRegisterSymbolized(triton::arch::x86::ID_REG_RDI)) {
    r->isSymbolic = true;
  }
  return;
}

// int __xstat(int ver, const char * path, struct stat * stat_buf);
void ____xstat(retLibc *r) {
  debug("xstat hooked ...");
  r->value = 0x0;
  return;
}

// int __fxstat(int ver, int fildes, struct stat * stat_buf);
void ____fxstat(retLibc *r) {
  debug("fxstat hooked ...");
  r->value = 0x0;
  return;
}

// int system(const char *command);
void __system(retLibc *r) {
  debug("system hooked ...");

  string src1 = getMemoryString(
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI));
  r->value = system(src1.c_str());
  r->isSymbolic = true;

  return;
}

// char *getenv(const char *name)
void __getenv(retLibc *r) {
  debug("getenv hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  string src1 = getMemoryString(arg1);

  std::vector<triton::uint8> temp;
  char *res = getenv(src1.c_str());
  int size = strlen(res);
  for (int j = 0; j < size; j++) {
    temp.push_back(res[j]);
  }
  // set concrete value to memory (like memory point to another memory)
  api.setConcreteMemoryAreaValue(BASE_MEM + mem_written, temp);
  r->value = BASE_MEM + mem_written;
  mem_written += size;

  return;
}

// char *getcwd(char *buf, size_t size)
void __getcwd(retLibc *r) {
  debug("getcwd hooked ...");

  triton::uint64 arg1 = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RDI);
  string src1 = getMemoryString(arg1);

  std::vector<triton::uint8> temp;
  char res[40] = "/home/abcd/helloworld/";
  int size = strlen(res);
  for (int j = 0; j < size; j++) {
    temp.push_back(res[j]);
  }
  // set concrete value to memory (like memory point to another memory)
  api.setConcreteMemoryAreaValue(arg1, temp);
  r->value = arg1;

  return;
}

// int getpagesize(void);
void __getpagesize(retLibc *r) {
  debug("getpagesize hooked ...");
  r->value = rand() % 10000;
  r->isSymbolic = true;
  return;
}

// void (*signal(int sig, void (*func)(int)))(int)
void __signal(retLibc *r) {
  debug("signal hooked ...");

  triton::uint512 signal =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI);
  triton::uint512 handler =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RSI);

  sigHandlers[signal] = handler;
  r->value = (triton::uint512)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RAX);

  return;
}

// int unlink(const char *path)
void __unlink(retLibc *r) {
  debug("unlink hooked ...");
  r->value = 0x0;
  return;
}

// int setjmp(jmp_buf environment)
void __setjmp(retLibc *r) {
  debug("setjmp hooked ...");
  triton::uint512 arg1 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI);
  r->jmpFlag = 1;
  r->value = arg1;
  return;
}

// void longjmp (jmp_buf env, int val)
void __longjmp(retLibc *r) {
  debug("longjmp hooked ...");
  triton::uint512 arg1 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RDI);
  triton::uint512 arg2 =
      api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RSI);
  r->jmpFlag = 2;
  r->value = arg2;
  return;
}

// void exit(int status)
void __exit(retLibc *r) {
  debug("exit hooked ...");
  r->value = 0x0;
  r->isTerminate = true;
  return;
}

// void abort (void);
void __abort(retLibc *r) {
  debug("abort hooked ...");

  r->isVoid = true;
  r->isTerminate = true;

  return;
}

bool isLib(triton::uint64 key) {
  for (int i = 0; i < sizeof(lib_addr) / sizeof(int); i++) {
    if (lib_addr[i] == key) {
      return true;
    }
  }
  return false;
}

// hook the handler with custom relocate address
int hookingHandler(triton::uint64 pc, triton::uint64 call_addr, seedMap seed,
                   blockInfo *blk) {
  int ret_flag = NO_LIBC_MATCHED;
  unsigned long long mem_addr;
  // next instruction need to be hooked?
  pc = (triton::uint64)api.getConcreteRegisterValue(
      triton::arch::x86::ID_REG_RIP);

  for (int i = 0; i < sizeof(lib_addr) / sizeof(int); i++) {
    if (lib_addr[i] == pc) {
      retLibc *r = new retLibc();
      ret_flag = LIBC_MATCHED;
      // for this instruction we have a handler to be hooked
      lib_rel[i](r);
      if (DEBUG) displayRLibC(r);  // just for logging
      // set rax register to target the handler
      if (!r->isVoid && !r->isTerminate) {
        api.concretizeRegister(triton::arch::x86::ID_REG_RAX);

        if (r->jmpFlag == 1) {
          jmp_stat_map[r->value] = call_addr;
          api.setConcreteRegisterValue(
              Register(triton::arch::x86::ID_REG_RAX, 0x0));
        } else {
          api.setConcreteRegisterValue(
              Register(triton::arch::x86::ID_REG_RAX, r->value));
        }

        if (r->isSymbolic)
          api.convertRegisterToSymbolicVariable(triton::arch::x86::ID_REG_RAX);
      } else if (r->isTerminate) {
        ret_flag = UNEXPECTED_TERMINATOR;
      }
      // store where to return [collect it from call stack]
      auto ret_addr = api.getConcreteMemoryValue(
          MemoryAccess((triton::uint64)api.getConcreteRegisterValue(
                           triton::arch::x86::ID_REG_RSP),
                       8));
      if (r->sym_addr != 0x0) {
        if (blk->ins_mem_map.find(call_addr) == blk->ins_mem_map.end()) {
          blk->ins_mem_map[call_addr] = (unsigned long long)r->sym_addr;
          blk->mem_sym_map[r->sym_addr] = true;
          blk->mem_size_map[r->sym_addr] = r->sym_size;
          blk->mem_data_map[r->sym_addr] = (unsigned char *)malloc(r->sym_size);
          memcpy(blk->mem_data_map[r->sym_addr], r->sym_raw, r->sym_size);
          blk->mem_write_map[r->sym_addr] = true;
          ret_flag = DISCOVERED_EXTERNAL_LIBC;
        } else {
          mem_addr = blk->ins_mem_map[call_addr];
          if (seed[mem_addr].isWrite) {
            if (seed[mem_addr].isSymbolic) {
              api.concretizeAllRegister();
              api.concretizeAllMemory();
              int t = 0;
              while (seed[mem_addr + t].isSymbolic &&
                     seed[mem_addr + t].isWrite) {
                api.convertMemoryToSymbolicVariable(
                    MemoryAccess(mem_addr + t, 8, seed[mem_addr + t].data));
                api.convertMemoryToSymbolicVariable(
                    MemoryAccess(mem_addr + t + 1, 8));
                t++;
              }
            } else {
              int t = 0;
              while (!seed[mem_addr + t].isSymbolic &&
                     seed[mem_addr + t].isWrite) {
                api.setConcreteMemoryValue(
                    MemoryAccess(mem_addr + t, 8, seed[mem_addr + t].data));
                t++;
              }
            }
          }
        }
      }

      // next instruction should be where to return
      api.concretizeRegister(triton::arch::x86::ID_REG_RIP);
      if (r->jmpFlag == 2 &&
          jmp_stat_map.find(r->value) != jmp_stat_map.end()) {
        api.setConcreteRegisterValue(
            Register(triton::arch::x86::ID_REG_RIP, jmp_stat_map[r->value]));
      } else {
        api.setConcreteRegisterValue(
            Register(triton::arch::x86::ID_REG_RIP, ret_addr));
      }

      // assume already return, stack pointer should be reset back one
      // step
      api.concretizeRegister(triton::arch::x86::ID_REG_RSP);
      api.setConcreteRegisterValue(Register(
          triton::arch::x86::ID_REG_RSP,
          api.getConcreteRegisterValue(triton::arch::x86::ID_REG_RSP) + 8));
      free(r->sym_raw);
      delete r;
      return ret_flag;
    }
  }
  return ret_flag;
}

// relocate the library calls to hooked up
void makeRelocation(triton::format::elf::Elf *binary) {
  debug("relocating library functions");

  // setup custom plt table from inside
  for (int i = 0; i < sizeof(lib_addr) / sizeof(int); i++) {
    lib_addr[i] = BASE_PLT + i;
  }

  // retrieve information of symbol table and relocation table from elf
  // binary
  auto symbols = binary->getSymbolsTable();
  auto relocations = binary->getRelocationTable();

  // relocate dynamic symbols to custom memory
  for (auto rel = relocations.begin(); rel != relocations.end(); ++rel) {
    auto symbolName = symbols[rel->getSymidx()].getName();
    auto symbolRelo = rel->getOffset();
    bool flag;

    // iterate over custom list
    for (int i = 0; i < sizeof(lib_addr) / sizeof(int); i++) {
      flag = false;
      if (lib_name[i] == symbolName) {
        flag = true;
        std::stringstream sstream;
        sstream << std::hex << symbolRelo;
        std::string result = sstream.str();
        debug("Relocate:\t" + symbolName + " => 0x" + result);

        api.setConcreteMemoryValue(MemoryAccess(symbolRelo, 8, lib_addr[i]));
        break;
      }
    }
    if (!flag) {
      debug("relocation missed for " + symbolName);
    }
  }

  srand(time(NULL));  // just initialize it; will later use by various function
                      // simulation
  return;
}

void simglibcinit() { mem_written = 0; }