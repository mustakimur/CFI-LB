/*
 * CFI-LB: Adaptive Call-site Sensitive Control Flow Integrity
 * Authors: Mustakimur Khandaker (Florida State University)
 * Abu Naser (Florida State University)
 * Wenqing Liu (Florida State University)
 * Zhi Wang (Florida State University)
 * Yajin Zhou (Zhejiang University)
 * Yueqiang Chen (Baidu X-lab)
 */

#define HASH_KEY_RANGE 1000000

void cfilb_reference_monitor(unsigned long long, unsigned long long);
void cd_cfg_monitor(unsigned long long, unsigned long long, unsigned long long,
                    unsigned long long, unsigned long long);
extern void cfilb_monitor_d0(unsigned long long, unsigned long long);
extern void cfilb_monitor_d1(unsigned long long, unsigned long long);
extern void cfilb_monitor_d2(unsigned long long, unsigned long long);
extern void cfilb_monitor_d3(unsigned long long, unsigned long long);
int cfilb_static_check(unsigned long long, unsigned long long);

void cfilb_init();
void cfilb_end();