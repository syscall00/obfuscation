import copy
import os
import random
import re

from pycparser import c_generator, parse_file
from constants import BaseStrings
from parser import FuncDeclarationVisitor, FuncCallVisitor, FuncDefVisitor


class Parameters:
    FUNC_OBF = 0
    STRING_OBF = 0


class Obfuscator(object):

    def __init__(self, file):
        if not Parameters.FUNC_OBF and not Parameters.STRING_OBF:
            print("Nothing to do.")
            exit(0)

        self._base_filename = file
        self.__ast = ""
        self._content = ""
        self._include = []
        self.__func_hashtable = []
        self.__max_length = 0

        self.start_obfuscation()

    def start_obfuscation(self):
        # Preprocess the file removing the #include directives
        self.__preprocess()

        # parse the file
        self.__ast = parse_file(BaseStrings.OUTPUT_FOLDER + "/" + BaseStrings.OUTPUT_FILE + ".c", use_cpp=True)

        # Apply transformation
        self._transform()

        # regenerate the source code starting from modified AST
        g = c_generator.CGenerator()
        self._content = g.visit(self.__ast)

        # Postprocess adding the #include directives
        self._postprocess()

    def __preprocess(self):
        """
        Preprocess the file. If not exist, create the output folder,
        read the file to be obfuscated and create a copy without
        #include directives
        """
        if not os.path.isdir(BaseStrings.OUTPUT_FOLDER):
            os.mkdir(BaseStrings.OUTPUT_FOLDER)
        f = open(self._base_filename, "r")
        self._content = f.read()
        f.close()

        def remove_define():
            # Save define
            self._include = re.findall(BaseStrings.INCLUDE_PATTERN, self._content)

            # Remove define
            self._content = re.sub(BaseStrings.INCLUDE_PATTERN, '', self._content)

        remove_define()

        f = open(BaseStrings.OUTPUT_FOLDER + "/" + BaseStrings.OUTPUT_FILE + ".c", "w+")
        f.write(self._content)
        f.close()

    def _transform(self):
        """
        Apply the obfuscation according the input parameters
        """

        def transform_functions():
            v = FuncDeclarationVisitor()
            v.visit(copy.deepcopy(self.__ast))

            self.__func_hashtable = v.func_table
            if len(self.__func_hashtable) > 0:
                c = FuncCallVisitor(v.func_table.keys())
                c.visit(self.__ast)

        def transform_strings():
            v = FuncDefVisitor()
            v.visit(copy.deepcopy(self.__ast))
            self.__max_length = v.max_length

        # if func_obfuscation is required...
        if Parameters.FUNC_OBF:
            transform_functions()

        # if string obfuscation is required...
        if Parameters.STRING_OBF:
            transform_strings()

    def _postprocess(self):
        """
        Postprocess the file
        """

        header_content = ""

        # If string obfuscation is required and there are strings
        # in the file
        if Parameters.STRING_OBF and self.__max_length > 0:
            # add the header for strcpy
            if "#include <string.h>" not in self._include:
                self._include.append("#include <string.h>")

            # add decrypt function in main file
            self._content = self._content + "\n" + BaseStrings.DECRYPT

            # FIX THIS SHIT!!!
            # add the decrypt function to the obfuscated hashmap
            self.__func_hashtable["decrypt"] = "char* (*decrypt)(char str[], int length)"

            # initialize random array
            random_array = BaseStrings.RANDOM_VALUES_INIT % (
                self.__max_length, ','.join([repr(random.randrange(0, 255)) for x in range(5)]))
            header_content += random_array + "\n"

            var = '"' + "\\0" * self.__max_length + '"'

            # add the CRYPT macro definition
            header_content += BaseStrings.CRYPT_STR % var + "\n"

            # add the CRYPT_ macro definition
            var = ""
            for i in range(0, self.__max_length):
                # Add a newline every 4 character ciphered
                if i > 0 and i % 4 == 0:
                    var += "\\\n"
                var += " str[" + str(i) + "]^" + BaseStrings.RANDOM_VALUES + "[" + str(i) + "],"
            header_content += BaseStrings.CRYPT__ % var

            # add the decrypt() function prototype
            header_content += "\n" + BaseStrings.DECRYPT_PROTOTYPE + ";\n"

        # If function obfuscation is required and there are function definite
        # in the file
        if Parameters.FUNC_OBF and len(self.__func_hashtable) > 0:
            temp_str = ""

            # Add all func prototypes in the struct declaration
            for elem in self.__func_hashtable.values():
                temp_str += "\t" + elem + ";\n"
            header_content += BaseStrings.FUNCTION_STRUCT % temp_str + "\n"

            # Add the initialization of the struct in the main file
            if len(self.__func_hashtable) > 0:
                temp_str = ""
                for func_name in self.__func_hashtable:
                    temp_str += "\t" + func_name + ",\n"
                funcListDefinition = BaseStrings.FUNCTION_STRUCT_DECLARATION % temp_str
                self._content = self._content + funcListDefinition

        self._include.append('#include "%s.h"' % BaseStrings.OUTPUT_FILE)

        # add includes
        includes = '\n'.join(self._include)
        self._content = includes + "\n" + self._content

        # write the header file
        f = open(BaseStrings.OUTPUT_FOLDER + "/" + BaseStrings.OUTPUT_FILE + ".h", "w")
        f.write(header_content)
        f.close()

        # write the main file
        f = open(BaseStrings.OUTPUT_FOLDER + "/" + BaseStrings.OUTPUT_FILE + ".c", "w")
        f.write(self._content)
        f.close()
