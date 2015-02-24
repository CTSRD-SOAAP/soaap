import re


class Function:
    """
    Nodes in the callgraph are functions and methods.
    """

    def __init__(self, fn, sandbox, location, cve = None, owner = None):
        assert 'file' in location
        assert 'line' in location

        self.parameters = fn[ fn.find('(') + 1 : fn.find(')') + 1][:-1]
        fn = fn.split('(')[0]

        # Ignore template parameters.
        fn = re.sub(r'<.*>', '', fn)

        components = fn.split('::')
        self.namespace_ = '::'.join(components[:-2])
        self.fn = '::'.join(components[-2:])

        self.sandbox_name = sandbox
        self.location = location
        self.cve = cve
        self.owner = owner

        self.callers = set()
        self.callees = set()

    def filename(self):
        if not self.location:
            return ''

        return self.location['file']

    def fqname(self):
        """ Fully-qualified function name """
        return '::'.join(( self.namespace_, self.fn ))

    def label(self):
        return self.fn

    def name(self):
        return u'%s <<%s>>' % (self.fn, self.sandbox_name)

    def namespace(self):
        return self.namespace_

    def sandbox(self):
        return self.sandbox_name

    def __str__(self):
        return self.name()



def simplify(functions, calls, cluster_key):
    visited = set()
    calls = dict(calls)

    def walk(root):
        if root in visited: return
        visited.add(root)

        if len(root.callees) == 1:
            (callee,) = root.callees
            weight = calls[(root, callee)]

            caller = root

            while (len(callee.callers) == len(callee.callees) == 1
                    and root.cve == callee.cve
                    and cluster_key(root) == cluster_key(callee)):

                calls.pop((caller, callee))
                caller = callee
                (callee,) = callee.callees

            if caller != root:
                calls.pop((caller, callee))
                calls[(root, callee)] = weight

            walk(callee)

        else:
            for callee in root.callees:
                walk(callee)

    # Walk all of the callgraph roots.
    for fn in functions:
        if len(fn.callers) == 0:
            walk(fn)

    dropped = set(functions) - visited

    return (visited, calls)
