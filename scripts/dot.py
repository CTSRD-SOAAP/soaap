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
