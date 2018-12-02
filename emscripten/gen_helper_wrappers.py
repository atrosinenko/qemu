#!/usr/bin/python
from pyparsing import *

from sys import stdin

ident = Word(alphanums + '_')
helper_name = ident('name')
flags = CharsNotIn("),")('flags')
delim = Suppress(ZeroOrMore(' ') + ',' + ZeroOrMore(' '))
typeDecl = (ZeroOrMore(delim + ident))('types')

def_helper = Suppress('DEF_HELPER_FLAGS_' + Word(nums) + '(') + helper_name + delim + flags + typeDecl + Suppress(')' + ZeroOrMore(' '))

sizes = {
    'int': (1, 'int'),
    'env': (1, 'void *'),
    'i32': (1, 'uint32_t'),
    'i64': (2, 'uint64_t'),
    's32': (1, 'int32_t'),
    's64': (2, 'int64_t'),
    'f32': (1, 'float'),
    'f64': (2, 'double'),
    'tl' : (1, 'target_ulong'),
    'ptr': (1, 'void *'),
    'MMXReg': (1, 'void *'),
    'XMMReg': (1, 'void *'),
    'ZMMReg': (1, 'void *')
}

jumping_helpers = [] #["pause", "raise_interrupt", "raise_exception", "hlt"]

def gen_call(name, types):
    res = "helper_" + name + "("
    cur = 1
    first = True
    for t in types:
        if not first:
            res += ", "
        res += "(" + sizes[t][1] + ")(0"
        sz = sizes[t][0]
        for i in xrange(sz):
            res += " | (((long long)arg" + str(cur + i) + ") << " + str(i * 32) + ")"
        res += ")"
        cur += sz
        first = False
    return res + ")"

print "#ifndef WRAPPERS_H"
print "#define WRAPPERS_H"
for line in stdin:
    try:
        helper = def_helper.parseString(line)
        print "long long " + helper.name + "_wrapper(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11, int arg12) {"
        if helper.types[0] in ["void", "noreturn"]:
            print "\t" + gen_call(helper.name, helper.types[1:]) + ";"
            print "\treturn 0;"
        else:
            print "\tlong long res = " + gen_call(helper.name, helper.types[1:]) + ";"
            print "\treturn res;"
        print "}"
    except ParseException as e:
        pass
print "#endif"
