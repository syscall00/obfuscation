class BaseStrings:

    # general purpose constants
    INCLUDE_PATTERN = '#include .*'
    OUTPUT_FOLDER = "out"
    OUTPUT_FILE = "obfuscated"

    # string obfuscation constants
    CRYPT_STR = "#define CRYPT(str, n) decrypt((char[]){ CRYPT_(str %s) }, n)"
    CRYPT__ = "#define CRYPT_(str)%s '\\0'"
    RANDOM_VALUES = "r_keys"
    RANDOM_VALUES_INIT = "int " + RANDOM_VALUES + "[%s] = {%s};"

    DECRYPT_PROTOTYPE = "char* decrypt(char str[], int length)"
    DECRYPT = DECRYPT_PROTOTYPE + " { \n" \
                                  "\tchar ret[length]; \n" \
                                  "\tfor(int i = 0; i < length; i++) \n" \
                                  "\t\tret[i] = str[i]^" + RANDOM_VALUES + "[i]; \n" \
                                                                           "\tstrcpy(str,ret); \n" \
                                                                           "\treturn str; \n" \
                                                                           "}"
    # struct obfuscation constants
    FUNCTION_STRUCT = "#pragma once\n" \
                      "typedef struct {\n" \
                      "%s" \
                      "} funcList;\n" \
                      "extern funcList myFuncList;"
    FUNCTION_STRUCT_DECLARATION = "funcList myFuncList = {\n" \
                                  "%s" \
                                  "};"
