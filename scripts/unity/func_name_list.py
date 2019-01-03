#!/usr/bin/python
import sys
import re
import argparse

def func_names_from_header(in_file, out_file):
    f_in = open(in_file)
    content = f_in.read()
    f_in.close()

    f_out = open(out_file,'w')

    x= re.findall(r"(struct\s|\s?)(\w+\s)(\*?)(\w+?)(\(.*?\))(;)", content, re.M | re.S)
    for item in x:
        print item[3]
        f_out.write(item[3] + "\n")


    f_out.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--input", type=str, help="input header file", required=True)
    parser.add_argument("-o", "--output", type=str, help="output function list file")
    parser.add_argument("-p", "--postfix", type=str, help="output file as input file with postfix")
    args = parser.parse_args()
    if args.output:
        out_name = args.output
    elif args.postfix:
        out_name = args.input + args.postfix
    else:
        out_name = args.input + ".flist"
       
    func_names_from_header(args.input, out_name)
