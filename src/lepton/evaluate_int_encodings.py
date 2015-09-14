import sys
from collections import defaultdict
def load_histogram(fn):
    ret = defaultdict(dict)
    with open(fn) as f:
        for line in f:
            try:
                cat,a,b = line.split()
            except:
                cat,b = line.split()
                a = 1
            if not int(b) in ret[cat]:
                ret[cat][int(b)] = int(a)
            else:
                ret[cat][int(b)] += int(a)
    return ret

def make_unary_sign_cost():
    ret = {}
    for i in range(-1025, 1025):
        if not i:
            ret[i] = 1
        elif i < 0:
            ret[i] = -i * 2
        else:
            ret[i] = i * 2 + 1
    return ret

def make_unary_cost():
    ret = {}
    for i in range(-1025, 1025):
        if not i:
            ret[i] = 1
        elif i < 0:
            ret[i] = -i + 2
        else:
            ret[i] = i + 2
    return ret

def log2(i):
    assert i > 0
    ret = 0
    while i:
        i = i // 2
        if i:
            ret += 1
    return ret
def log2_length(i):
    if i == 0:
        return 0
    if i < 0:
        i = -i
    return log2(i if i > 0 else -i) + 1

def make_unary_trunc_cost(n):
    base_cutoff_cost = n
    ret = defaultdict(lambda:0)
    for i in range(-1025, 1025):
        absi = i if i > 0 else -i
        l2len = log2_length(absi - n)
        ret[i] = l2len + 2 + (l2len - 1 if l2len else 0) + base_cutoff_cost
    for i in range(-n, n + 1):
        if not i:
            ret[i] = 1
        elif i < 0:
            ret[i] = -i + 2
        else:
            ret[i] = i + 2
    return ret

unary_sign_cost = make_unary_sign_cost()
unary_cost = make_unary_cost()
unary_trunc_cost = []
for i in range(20):
    unary_trunc_cost.append(make_unary_trunc_cost(i))
unary_exponent_cost = {0 : 1, # 1 exp
              1 : 3, # 2 exp, 1 sign
              2 : 5, # 3 exp, 1 sign, 1 rem
              3 : 7, # 4 exp, 1 sign, 2 rem
              4 : 9, # 5 exp, 1 sign
              5 : 11,# ...
              6 : 13,
              7 : 15,
              8 : 17,
              9 : 19,
              10 : 20}

unary_one_case_exponent_cost = {0 : 1, # 1 exp
              1 : 2, # 2 exp, 1 sign
              2 : 6, # 3 exp, 1 sign, 1 rem
              3 : 8, # 4 exp, 1 sign, 2 rem
              4 : 10, # 5 exp, 1 sign
              5 : 12,# ...
              6 : 14,
              7 : 16,
              8 : 18,
              9 : 20,
              10 : 21,
              -1 : 3, # 2 exp, 1 sign
              -2 : 6, # 3 exp, 1 sign, 1 rem
              -3 : 8, # 4 exp, 1 sign, 2 rem
              -4 : 10, # 5 exp, 1 sign
              -5 : 12,# ...
              -6 : 14,
              -7 : 16,
              -8 : 18,
              -9 : 20,
              -10 : 21}

binary_cost = {0 : 2, # 2 exp
               1 : 5, # 4 exp, 1 sign
               2 : 6, # 4 exp, 1 sign, 1 rem
               3 : 7, # 4 exp, 1 sign, 2 rem
               4 : 8, # ...
              5 : 9,
              6 : 10,
              7 : 11,
              8 : 12,
              9 : 13,
              10 : 14}
def eval_binary_cost(h):
    count = 0
    max = 0
    for i in h:
        if i > max:
            max = i
        count += h[i]
    bin_cost = log2_length(max)
    #print "max was ", max, "count was ", count, "cost was ",bin_cost,"per item"
    return count * bin_cost

def eval_cost(h, c, dolog=False):
    ret = 0
    for i in h:
        cost_index = i;
        if dolog:
            cost_index = log2_length(i)
        if i < 0 and cost_index > 0 and (-cost_index) in c:
            cost = c[-cost_index] #lets us special case -log2_length
        else:
            cost = c[cost_index]
        ret += h[i] * cost
    return ret
for arg in sys.argv[1:]:
    hists = load_histogram(arg)
    total_count = 0
    for name, count in hists.iteritems():
        print arg + '.' + name, 'pure_bin_cost', eval_binary_cost(count)
        print arg + '.' + name, 'unary_exp_cost', eval_cost(count, unary_exponent_cost, dolog=True)
        print arg + '.' + name, 'unary0exp_cost', eval_cost(count, unary_one_case_exponent_cost, dolog=True)
        print arg + '.' + name, 'binaryexp_cost', eval_cost(count, binary_cost, dolog=True)
        print arg + '.' + name, 'unary_cost', eval_cost(count, unary_cost)
        for i in range(len(unary_trunc_cost)):
            #print i, unary_trunc_cost
            print arg + '.' + name, 'untrunc' + str(i//10) + str(i%10), \
              eval_cost(count, unary_trunc_cost[i])

