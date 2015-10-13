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

import math
import sys


def attribute_list(attrs):
    """
    Convert a Python dictionary into a dot-style attribute list:
    [ foo = "x", bar = "y" ]
    """

    return (
        '[ '
        + ', '.join([
                '%s = "%s"' % i for i in attrs.items()
            ])
        + ' ]'
    )



def write_directed_graph(nodes, edges, out = sys.stdout):
    """
    Write a directed graph (in GraphViz Dot format) to an output stream.
    """

    out.write('digraph {\n  rankdir = BT;\n\n')

    for (name,label) in nodes:
        out.write('  "%s" [ label = "%s", shape = "rectangle" ];\n' % (
            name, label))

    out.write('\n')

    for ((orig,dest), weight) in edges.items():
        attributes = {
            'penwidth': math.log(weight) + 1,
            'weight': weight
        }

        attributes = ', '.join([ '%s = "%s"' % a for a in attributes.items() ])
        out.write('  "%s" -> "%s" [ %s ];\n' % (orig[0], dest[0], attributes))

    out.write('\n}\n')
