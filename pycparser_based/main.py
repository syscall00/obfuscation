import os
import obfuscator
import argparse

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("-f", help="Choose the file to obfuscate", action="store")
    parser.add_argument("-sobf", help="Active the strings obfuscation", action="store_true")
    parser.add_argument("-fobf", help="Active the function call obfuscation", action="store_true")
    args = parser.parse_args()

    if args.f is None:
        print("obfuscator: error: no input file. Check usage calling with parameter -h")
        exit(-1)
    if os.path.exists(args.f):
        obfuscator.Parameters.STRING_OBF = args.sobf
        obfuscator.Parameters.FUNC_OBF = args.fobf
        obfuscator.Obfuscator(args.f)
    else:
        print("file does not exists")

