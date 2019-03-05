# CFI-LB: Adaptive Call-site Sensitive Control Flow Integrity
# Authors: Mustakimur Khandaker (Florida State University)
# Abu Naser (Florida State University)
# Wenqing Liu (Florida State University)
# Zhi Wang (Florida State University)
# Yajin Zhou (Zhejiang University)
# Yueqiang Chen (Baidu X-lab)

"""
This is our adaptive CFI-LB filtering algorithm
We assume that our dynamic cfg is a CFI-LB(3) cfg
and this algorithm will apply the filtering on it;
will generate cfilb_depth(0-3).bin
"""

import sys

# python filter.py target_binary_directory
if(__name__ == '__main__'):
    proc_map = dict()
    rfile = open("elf_extract.bin", 'r')
    for line in rfile:
        if(len(line.split('\t')) == 3):
            proc_map[int(line.split('\t')[1], 10)] = int(
                line.split('\t')[2], 10)
    rfile.close()

    com_map = dict()
    rfile = open("cfilb_cfg.bin", 'r')
    for line in rfile:
        if (len(line.split('\t')) == 5):
            call_point = int(line.split('\t')[0], 10)
            call_target = int(line.split('\t')[1], 10)
            call_site1 = int(line.split('\t')[2], 10)
            call_site2 = int(line.split('\t')[3], 10)
            call_site3 = int(line.split('\t')[4].rstrip(), 10)

            # following replaces any external call-site to a 0xffaaee
            flag3 = True, flag2 = True, flag1 = True
            for key, val in proc_map.iteritems():
                if(call_site3 >= key and call_site3 <= (key + val)):
                    flag3 = False
                if(call_site2 >= key and call_site2 <= (key + val)):
                    flag2 = False
                if(call_site1 >= key and call_site1 <= (key + val)):
                    flag1 = False
            if(flag3):
                call_site3 = 0xffaaee
            if(flag2):
                call_site2 = 0xffaaee
            if(flag1):
                call_site1 = 0xffaaee

            if(not(call_point, call_site1, call_site2, call_site3) in com_map):
                com_map[(call_point, call_site1, call_site2, call_site3)] = []
            com_map[(call_point, call_site1, call_site2,
                     call_site3)].append(call_target)
    rfile.close()

    wfile = open("cfilb_cfg.bin", 'w')
    for key, val in com_map.iteritems():
        for item in set(val):
            wfile.write(str(key[0]) + "\t" + str(item) + "\t" + str(key[1]) + "\t" + str(key[2]) +
                        "\t" + str(key[3]) + "\n")
    wfile.close()

    rfile = open("cfilb_cfg.bin", 'r')
    w0file = open("cfilb_depth0.bin", 'w')
    w1file = open("cfilb_depth1.bin", 'w')
    w2file = open("cfilb_depth2.bin", 'w')
    w3file = open("cfilb_depth3.bin", 'w')

    zeroDict = dict()   # CFI-LB(0)
    oneDict = dict()    # CFI-LB(1)
    twoDict = dict()    # CFI-LB(2)
    threeDict = dict()  # CFI-LB(3)

    for line in rfile:
        if (len(line.split('\t')) == 5):
            call_point = int(line.split('\t')[0], 10)
            call_target = int(line.split('\t')[1], 10)
            call_site1 = int(line.split('\t')[2], 10)
            call_site2 = int(line.split('\t')[3], 10)
            call_site3 = int(line.split('\t')[4].rstrip(), 10)

            # following create CFI-LB(0-3) CFG
            if (not call_point in zeroDict):
                zeroDict[call_point] = []
            if (not call_target in zeroDict[call_point]):
                zeroDict[call_point].append(call_target)

            if (not (call_point, call_site1) in oneDict):
                oneDict[(call_point, call_site1)] = []
            if (not call_target in oneDict[(call_point, call_site1)]):
                oneDict[(call_point, call_site1)].append(call_target)

            if (not (call_point, call_site1, call_site2) in twoDict):
                twoDict[(call_point, call_site1, call_site2)] = []
            if (not call_target in twoDict[(call_point, call_site1, call_site2)]):
                twoDict[(call_point, call_site1, call_site2)
                        ].append(call_target)

            if (not (call_point, call_site1, call_site2, call_site3) in threeDict):
                threeDict[(call_point, call_site1,
                           call_site2, call_site3)] = []
            if (not call_target in threeDict[(call_point, call_site1, call_site2, call_site3)]):
                threeDict[(call_point, call_site1, call_site2, call_site3)
                          ].append(call_target)
    rfile.close()

    quality = dict()
    largest_set = dict()
    total_set = dict()

    for call_site_level in range(0, 4):
        choice = dict()

        for point, targets in zeroDict.iteritems():
            # determine the depth for every call-point for max call-site level
            res = []
            res_dict = dict()
            res.append(len(targets))
            res_dict[0] = len(targets)

            total = 0
            p = 0
            for key, target in oneDict.iteritems():
                if(key[0] == point):
                    total += len(target)
                    p += 1
            if(call_site_level > 0):
                res.append(total / (float)(p))
                res_dict[1] = total / (float)(p)

            total = 0
            p = 0
            for key, target in twoDict.iteritems():
                if(key[0] == point):
                    total += len(target)
                    p += 1
            if(call_site_level > 1):
                res.append(total / (float)(p))
                res_dict[2] = total / (float)(p)

            total = 0
            p = 0
            for key, target in threeDict.iteritems():
                if(key[0] == point):
                    total += len(target)
                    p += 1
            if(call_site_level > 2):
                res.append(total / (float)(p))
                res_dict[3] = total / (float)(p)

            m_res = min(res)
            if(m_res == res_dict[0]):
                choice[point] = 0
            elif(m_res == res_dict[1]):
                choice[point] = 1
            elif(m_res == res_dict[2]):
                choice[point] = 2
            elif(m_res == res_dict[3]):
                choice[point] = 3

        # following calculate the quantitive security
        target_sum0 = 0
        ec_sum0 = 0
        target_sum1 = 0
        ec_sum1 = 0
        target_sum2 = 0
        ec_sum2 = 0
        target_sum3 = 0
        ec_sum3 = 0
        largest_size = 0
        for point, level in choice.iteritems():
            if(level == 0):
                target_sum0 += len(zeroDict[point])
                ec_sum0 += 1
                if(len(zeroDict[point]) > largest_size):
                    largest_size = len(zeroDict[point])
            elif(level == 1):
                for key, val in oneDict.iteritems():
                    if(key[0] == point):
                        target_sum1 += len(val)
                        ec_sum1 += 1
                        if(len(val) > largest_size):
                            largest_size = len(val)
            elif(level == 2):
                for key, val in twoDict.iteritems():
                    if(key[0] == point):
                        target_sum2 += len(val)
                        ec_sum2 += 1
                        if(len(val) > largest_size):
                            largest_size = len(val)
            elif(level == 3):
                for key, val in threeDict.iteritems():
                    if(key[0] == point):
                        target_sum3 += len(val)
                        ec_sum3 += 1
                        if(len(val) > largest_size):
                            largest_size = len(val)

        quality[call_site_level] = target_sum0 + \
            target_sum1 + target_sum2 + target_sum3
        quality[call_site_level] /= (float)(ec_sum0 +
                                            ec_sum1 + ec_sum2 + ec_sum3)
        quality[call_site_level] *= largest_size
        largest_set[call_site_level] = largest_size
        total_set[call_site_level] = (ec_sum0 +
                                      ec_sum1 + ec_sum2 + ec_sum3)

    min_qual = 9999
    select_call_site_level = 0
    for key, val in quality.iteritems():
        if(min_qual > val):
            select_call_site_level = key
            min_qual = val

    choice = dict()
    for point, targets in zeroDict.iteritems():
        # determine the depth for every call-point
        res = []
        res_dict = dict()
        res.append(len(targets))
        res_dict[0] = len(targets)

        total = 0
        p = 0
        for key, target in oneDict.iteritems():
            if(key[0] == point):
                total += len(target)
                p += 1
        if (select_call_site_level > 0):
            res.append(total / (float)(p))
            res_dict[1] = total / (float)(p)

        total = 0
        p = 0
        for key, target in twoDict.iteritems():
            if(key[0] == point):
                total += len(target)
                p += 1
        if(select_call_site_level > 1):
            res.append(total / (float)(p))
            res_dict[2] = total / (float)(p)

        total = 0
        p = 0
        for key, target in threeDict.iteritems():
            if(key[0] == point):
                total += len(target)
                p += 1
        if(select_call_site_level > 2):
            res.append(total / (float)(p))
            res_dict[3] = total / (float)(p)

        m_res = min(res)
        if(m_res == res_dict[0]):
            choice[point] = 0
        elif(m_res == res_dict[1]):
            choice[point] = 1
        elif(m_res == res_dict[2]):
            choice[point] = 2
        elif(m_res == res_dict[3]):
            choice[point] = 3

    c_level = dict()
    c_level[0] = 0
    c_level[1] = 0
    c_level[2] = 0
    c_level[3] = 0

    for point, level in choice.iteritems():
        if(level == 0):
            c_level[0] += 1
            for t in zeroDict[point]:
                w0file.write(str(point) +
                             "\t" + str(t) + "\n")
        elif(level == 1):
            c_level[1] += 1
            for key, val in oneDict.iteritems():
                if(key[0] == point):
                    for t in val:
                        w1file.write(str(point) + '\t' + str(t) +
                                     "\t" + str(key[1]) + "\n")
        elif(level == 2):
            c_level[2] += 1
            for key, val in twoDict.iteritems():
                if(key[0] == point):
                    for t in val:
                        w2file.write(str(point) + '\t' + str(t) + '\t' + str(key[1]) +
                                     "\t" + str(key[2]) + "\n")
        elif(level == 3):
            c_level[3] += 1
            for key, val in threeDict.iteritems():
                if(key[0] == point):
                    for t in val:
                        w3file.write(str(point) + '\t' + str(t) + '\t' + str(key[1]) + '\t' + str(key[2]) +
                                     "\t" + str(key[3]) + "\n")

    w0file.close()
    w1file.close()
    w2file.close()
    w3file.close()
