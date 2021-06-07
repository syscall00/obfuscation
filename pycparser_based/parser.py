from pycparser import c_ast, c_generator


class FuncDeclarationVisitor(c_ast.NodeVisitor):
    """
    This class saves all the user defined function and their signature in an hashtable.
    """
    def __init__(self):
        self.func_table = {}

    def visit_FuncDecl(self, node):
        """
        Visit a function declaration
        """

        t = node.type
        # Skip eventually PtrDecl in case of function with return type one or more pointers
        # eg. int *func(int *x, int *y);
        while type(t) is not c_ast.TypeDecl:
            t = t.type

        match = t.declname
        t.declname = "(*" + t.declname + ")"

        # Generate function signature from AST Node
        a = c_generator.CGenerator().visit(node)

        # Add pair <func_name, func_signature> in the hashtable
        if match != "main" and t.declname not in self.func_table:
            self.func_table[match] = a


class FuncCallVisitor(c_ast.NodeVisitor):
    """
    This class visit all the function call and create an indirection when find
    a function present in the function list.
    """
    def __init__(self, func_list):
        self.func_list = func_list

    def visit_FuncCall(self, node):
        """
        Visit a function declaration
        """
        for elem in self.func_list:
            if node.name.name == elem:
                node.name.name = "myFuncList." + node.name.name
            if node.args:
                self.visit(node.args)


class FuncDefVisitor(c_ast.NodeVisitor):
    """
    This class visit all the strings and cipher them.
    """
    def __init__(self):
        self.max_length = 0

    def visit_Constant(self, node):
        """
        Visit a Constant. If it is a string, transform it adding the CRYPT macro
        """
        if node.type == 'string':
            var = node.value.replace('"', '')
            self.max_length = len(var) if self.max_length < len(var) else self.max_length
            node.value = "CRYPT(" + node.value + ", " + str(len(var)) + ")"
