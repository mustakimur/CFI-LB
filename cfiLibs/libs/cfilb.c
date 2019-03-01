/*
 * CFI-LB: Adaptive Call-site Sensitive Control Flow Integrity
 * Authors: Mustakimur Khandaker (Florida State University)
 * Abu Naser (Florida State University)
 * Wenqing Liu (Florida State University)
 * Zhi Wang (Florida State University)
 * Yajin Zhou (Zhejiang University)
 * Yueqiang Chen (Baidu X-lab)
 */
#include <stdio.h>
#include <stdlib.h>
#include "cfilb.h"
// CI CFG Table from LLVM Pass
__attribute__((__used__)) __attribute__((section("cfg_label_data")))
const int *CI_TABLE[] = {};
__attribute__((__used__)) unsigned int CI_LENGTH = 0;

// CFI-LB CFG Table from dCFG and cCFG
__attribute__((__used__)) __attribute__((section("cfg_label_data")))
const int *CFILB_D0[] = {};
__attribute__((__used__)) unsigned int CFILB_D0_C = 0;

__attribute__((__used__)) __attribute__((section("cfg_label_data")))
const int *CFILB_D1[] = {};
__attribute__((__used__)) unsigned int CFILB_D1_C = 0;

__attribute__((__used__)) __attribute__((section("cfg_label_data")))
const int *CFILB_D2[] = {};
__attribute__((__used__)) unsigned int CFILB_D2_C = 0;

__attribute__((__used__)) __attribute__((section("cfg_label_data")))
const int *CFILB_D3[] = {};
__attribute__((__used__)) unsigned int CFILB_D3_C = 0;

// hash based test table
struct HashItem {
  int depth;
  unsigned long long call_site[3];
  unsigned long long point;
  unsigned long long target;
  struct HashItem *next;
};
struct HashItem *CFILB_HASH_TABLE[HASH_KEY_RANGE] = {NULL};

FILE *logFile;

void cfi_hash_insert(unsigned long point, unsigned long target) {
  unsigned long hashIndex = (point ^ target) % HASH_KEY_RANGE;
  struct HashItem *item = (struct HashItem *)malloc(sizeof(struct HashItem));
  item->depth = -1;
  item->point = point;
  item->target = target;
  item->next = NULL;

  if (CFILB_HASH_TABLE[hashIndex] == NULL) {
    CFILB_HASH_TABLE[hashIndex] = item;
  } else {
    struct HashItem *temp = CFILB_HASH_TABLE[hashIndex];
    while (temp->next != NULL) {
      temp = temp->next;
    }
    temp->next = item;
  }
}

void cfilb_D0_hash_insert(unsigned long point, unsigned long target) {
  unsigned long hashIndex = (point ^ target) % HASH_KEY_RANGE;
  struct HashItem *item = (struct HashItem *)malloc(sizeof(struct HashItem));
  item->depth = 0;
  item->point = point;
  item->target = target;
  item->next = NULL;

  if (CFILB_HASH_TABLE[hashIndex] == NULL) {
    CFILB_HASH_TABLE[hashIndex] = item;
  } else {
    struct HashItem *temp = CFILB_HASH_TABLE[hashIndex];
    while (temp->next != NULL) {
      temp = temp->next;
    }
    temp->next = item;
  }
}

void cfilb_D1_hash_insert(unsigned long point, unsigned long target,
                          unsigned long site1) {
  unsigned long hashIndex = (point ^ target ^ site1) % HASH_KEY_RANGE;
  struct HashItem *item = (struct HashItem *)malloc(sizeof(struct HashItem));
  item->depth = 1;
  item->call_site[0] = site1;
  item->point = point;
  item->target = target;
  item->next = NULL;

  if (CFILB_HASH_TABLE[hashIndex] == NULL) {
    CFILB_HASH_TABLE[hashIndex] = item;
  } else {
    struct HashItem *temp = CFILB_HASH_TABLE[hashIndex];
    while (temp->next != NULL) {
      temp = temp->next;
    }
    temp->next = item;
  }
}

void cfilb_D2_hash_insert(unsigned long point, unsigned long target,
                          unsigned long site1, unsigned long site2) {
  unsigned long hashIndex = (point ^ target ^ site1 ^ site2) % HASH_KEY_RANGE;
  struct HashItem *item = (struct HashItem *)malloc(sizeof(struct HashItem));
  item->depth = 2;
  item->call_site[0] = site1;
  item->call_site[1] = site2;
  item->point = point;
  item->target = target;
  item->next = NULL;

  if (CFILB_HASH_TABLE[hashIndex] == NULL) {
    CFILB_HASH_TABLE[hashIndex] = item;
  } else {
    struct HashItem *temp = CFILB_HASH_TABLE[hashIndex];
    while (temp->next != NULL) {
      temp = temp->next;
    }
    temp->next = item;
  }
}

void cfilb_D3_hash_insert(unsigned long point, unsigned long target,
                          unsigned long site1, unsigned long site2,
                          unsigned long site3) {
  unsigned long hashIndex =
      (point ^ target ^ site1 ^ site2 ^ site3) % HASH_KEY_RANGE;
  struct HashItem *item = (struct HashItem *)malloc(sizeof(struct HashItem));
  item->depth = 3;
  item->call_site[0] = site1;
  item->call_site[1] = site2;
  item->call_site[2] = site3;
  item->point = point;
  item->target = target;
  item->next = NULL;

  if (CFILB_HASH_TABLE[hashIndex] == NULL) {
    CFILB_HASH_TABLE[hashIndex] = item;
  } else {
    struct HashItem *temp = CFILB_HASH_TABLE[hashIndex];
    while (temp->next != NULL) {
      temp = temp->next;
    }
    temp->next = item;
  }
}

__attribute__((__used__)) void cd_cfg_monitor(unsigned long long point,
                                              unsigned long long target,
                                              unsigned long long site1,
                                              unsigned long long site2,
                                              unsigned long long site3) {}

__attribute__((__used__))
void cfilb_reference_monitor(unsigned long long point,
                             unsigned long long target) {
  unsigned long long site1 = (unsigned long long)__builtin_return_address(1);
  unsigned long long site2 = (unsigned long long)__builtin_return_address(2);
  unsigned long long site3 = (unsigned long long)__builtin_return_address(3);
  if (cfilb_static_check(point, target)) {
    cd_cfg_monitor(point, target, site1, site2, site3);
  }
}

// check the static point-to analysis table
__attribute__((__used__))
int cfilb_static_check(unsigned long long point, unsigned long long target) {
  unsigned long hashIndex = (point ^ target) % HASH_KEY_RANGE;
  if (CFILB_HASH_TABLE[hashIndex] != NULL) {
    struct HashItem *temp = CFILB_HASH_TABLE[hashIndex];
    while (temp != NULL) {
      if (temp->depth == -1 && temp->point == point && temp->target == target) {
        return 1;
      }
      temp = temp->next;
    }
  }
  return 0;
}

// reference monitor at level 0
__attribute__((__used__))
void cfilb_monitor_d0(unsigned long long point, unsigned long long target) {
  unsigned long long hashIndex = ((point ^ target) % HASH_KEY_RANGE);

  if (CFILB_HASH_TABLE[hashIndex] != NULL) {
    struct HashItem *temp = CFILB_HASH_TABLE[hashIndex];
    while (temp != NULL) {
      if (temp->depth == 0 && temp->point == point && temp->target == target) {
        return;
      }
      temp = temp->next;
    }
  }

  if (cfilb_static_check(point, target)) {
    fprintf(logFile,
            "[sCFG] %llx -> %llx -> %llx -> %llu -> "
            "%llx\n",
            (unsigned long long)__builtin_return_address(1),
            (unsigned long long)__builtin_return_address(2),
            (unsigned long long)__builtin_return_address(3), point, target);
    return;
  }

  fprintf(stderr,
          "[CFI-LB]Terminate program from %llu because target to %llu\n", point,
          target);

  exit(0);
}
// reference monitor at level 1
__attribute__((__used__))
void cfilb_monitor_d1(unsigned long long point, unsigned long long target) {
  unsigned long long site1 = (unsigned long long)__builtin_return_address(1);

  unsigned long long hashIndex = ((site1 ^ point ^ target) % HASH_KEY_RANGE);

  if (CFILB_HASH_TABLE[hashIndex] != NULL) {
    struct HashItem *temp = CFILB_HASH_TABLE[hashIndex];
    while (temp != NULL) {
      if (temp->depth == 1 && temp->point == point && temp->target == target &&
          temp->call_site[0] == site1) {
        return;
      }
      temp = temp->next;
    }
  }

  if (cfilb_static_check(point, target)) {
    fprintf(logFile,
            "[sCFG] %llx -> %llx -> %llx -> %llu -> "
            "%llu\n",
            (unsigned long long)__builtin_return_address(1),
            (unsigned long long)__builtin_return_address(2),
            (unsigned long long)__builtin_return_address(3), point, target);
    return;
  }

  fprintf(stderr,
          "[CFI-LB]Terminate program from %llu because target to %llx\n", point,
          target);

  exit(0);
}

// reference monitor at level 2
__attribute__((__used__))
void cfilb_monitor_d2(unsigned long long point, unsigned long long target) {
  unsigned long long site1 = (unsigned long long)__builtin_return_address(1);
  unsigned long long site2 = (unsigned long long)__builtin_return_address(2);

  unsigned long long ret = site1 ^ site2;

  unsigned long long hashIndex = ((ret ^ point ^ target) % HASH_KEY_RANGE);

  if (CFILB_HASH_TABLE[hashIndex] != NULL) {
    struct HashItem *temp = CFILB_HASH_TABLE[hashIndex];
    while (temp != NULL) {
      if (temp->depth == 2 && temp->point == point && temp->target == target &&
          temp->call_site[0] == site1 && temp->call_site[1] == site2) {
        return;
      }
      temp = temp->next;
    }
  }

  if (cfilb_static_check(point, target)) {
    fprintf(logFile,
            "[sCFG] %llx -> %llx -> %llx -> %llu -> "
            "%llx\n",
            (unsigned long long)__builtin_return_address(1),
            (unsigned long long)__builtin_return_address(2),
            (unsigned long long)__builtin_return_address(3), point, target);
    return;
  }

  fprintf(stderr,
          "[CFI-LB]Terminate program from %llu because target to %llx\n", point,
          target);

  exit(0);
}

// reference monitor at level 3
__attribute__((__used__))
void cfilb_monitor_d3(unsigned long long point, unsigned long long target) {
  unsigned long long site1 = (unsigned long long)__builtin_return_address(1);
  unsigned long long site2 = (unsigned long long)__builtin_return_address(2);
  unsigned long long site3 = (unsigned long long)__builtin_return_address(3);

  unsigned long long ret = site1 ^ site2 ^ site3;

  unsigned long long hashIndex = ((ret ^ point ^ target) % HASH_KEY_RANGE);

  if (CFILB_HASH_TABLE[hashIndex] != NULL) {
    struct HashItem *temp = CFILB_HASH_TABLE[hashIndex];
    while (temp != NULL) {
      if (temp->depth == 3 && temp->point == point && temp->target == target &&
          temp->call_site[0] == site1 && temp->call_site[1] == site2 &&
          temp->call_site[2] == site3) {
        return;
      }
      temp = temp->next;
    }
  }

  if (cfilb_static_check(point, target)) {
    fprintf(logFile,
            "[sCFG] %llx -> %llx -> %llx -> %llu -> "
            "%llx\n",
            (unsigned long long)__builtin_return_address(1),
            (unsigned long long)__builtin_return_address(2),
            (unsigned long long)__builtin_return_address(3), point, target);
    return;
  }

  fprintf(stderr,
          "[CFI-LB]Terminate program from %llu because target to %llx\n", point,
          target);

  exit(0);
}

// initialize the hash table at the beginning of the program execution
void cfilb_init() {
  int i;

  for (i = 0; i < CI_LENGTH; i += 2) {
    cfi_hash_insert(CI_TABLE[i], CI_TABLE[i + 1]);
  }

  for (i = 0; i < CFILB_D0_C; i += 2) {
    cfilb_D0_hash_insert(CFILB_D0[i], CFILB_D0[i + 1]);
  }
  for (i = 0; i < CFILB_D1_C; i += 3) {
    cfilb_D1_hash_insert(CFILB_D1[i], CFILB_D1[i + 1], CFILB_D1[i + 2]);
  }
  for (i = 0; i < CFILB_D2_C; i += 4) {
    cfilb_D2_hash_insert(CFILB_D2[i], CFILB_D2[i + 1], CFILB_D2[i + 2],
                         CFILB_D2[i + 3]);
  }
  for (i = 0; i < CFILB_D3_C; i += 5) {
    cfilb_D3_hash_insert(CFILB_D3[i], CFILB_D3[i + 1], CFILB_D3[i + 2],
                         CFILB_D3[i + 3], CFILB_D3[i + 4]);
  }

  logFile = fopen("cfilb_log", "a");
}

void cfilb_end() { fclose(logFile); }

__attribute__((section(".preinit_array"),
               used)) void (*_cfilb_preinit)(void) = cfilb_init;

__attribute__((section(".fini_array"),
               used)) void (*_cfilb_fini)(void) = cfilb_end;
