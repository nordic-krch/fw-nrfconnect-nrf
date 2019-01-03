#!/usr/bin/python
import sys
import re
import argparse

def func_names_from_header(in_file, out_file, out_wrap_file):
    f_in = open(in_file)
    content = f_in.read()
    f_in.close()

    f_wrap = open(out_wrap_file,'w')
    f_out = open(out_file,'w')

    # change static inline functions to normal function declaration
    static_inline_pattern = re.compile(r"((__deprecated )?)(static inline )((struct )?)(\w+ )(\*?)((_impl_)?)(\w+\([^\)]+\))(\n\{[\s\S]+?\n\})", re.M | re.S)
    (content, static_inline_cnt) = static_inline_pattern.subn(r"\4\6\7\10;", content)

    print static_inline_cnt

    # remove syscall include
    syscall_pattern = re.compile(r"#include <syscalls/\w+?.h>", re.M | re.S)
    content = syscall_pattern.sub(r"", content)

    
    syscall_decl_pattern  = re.compile(r"(__syscall )((struct )?)(\w+ )(\*?)([\s\S]+?;)", re.M | re.S)
    content = syscall_decl_pattern.sub("", content) 

    f_out.write(content)
    f_out.close()
    
    # Prepare file with functions prefixed with __wrap_ that will be used for mock generation
    func_pattern = re.compile(r"((struct\s)|\n)([^\s#]\w*\s)(\*?)(\w+?\([\s\S]+?\);)", re.M | re.S)
    (content2, m2) = func_pattern.subn(r"\n\1\3\4__wrap_\5", content)

    f_wrap.write(content2)
    f_wrap.close() 

    print "matches: " + str(m2)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--input", type=str, help="input header file", required=True)
    parser.add_argument("-o", "--output", type=str, help="stripped header file to be included in the test", required=True)
    parser.add_argument("-w", "--wrap", type=str, help="header with __wrap_-prefixed functions for mock generation", required=True)
    args = parser.parse_args()
       
    func_names_from_header(args.input, args.output, args.wrap)
