/*
 * Copyright (c) 2014-2015 Jonathan Anderson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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
