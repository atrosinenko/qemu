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
    'int': 1,
    'env': 1,
    'i32': 1,
    'i64': 2,
    's32': 1,
    's64': 2,
    'f32': 1,
    'f64': 2,
    'tl': 1,
    'ptr': 1,
    'MMXReg': 1,
    'XMMReg': 1,
    'ZMMReg': 1
}

jumping_helpers = [] #["pause", "raise_interrupt", "raise_exception", "hlt"]

def gen_call(name, types):
    res = "helper_" + name + "("
    cur = 1
    first = True
    for t in types:
        if not first:
            res += ", "
        res += "0"
        sz = sizes[t]
        for i in xrange(sz):
            res += " | (((long long)arg" + str(cur + i) + ") << " + str(i * 32) + ")"
        cur += sz
        first = False
    return res + ")"

print "#ifndef WRAPPERS_H"
print "#define WRAPPERS_H"
for line in stdin:
    try:
        helper = def_helper.parseString(line)
        print "long long " + helper.name + "_wrapper(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, arg11, arg12) {"
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
