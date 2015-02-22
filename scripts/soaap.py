import callgraph
import collections


def parse_vulnerabilities(soaap):
    """
    Parse information about calls to previously-vulnerable code.
    """

    functions = {}
    calls = collections.defaultdict(int)

    for vuln in soaap['vulnerability_warning']:
        fn = vuln['function']
        sandbox = vuln['sandbox']
        if sandbox == '"none"': sandbox = None

        key = (fn, sandbox)
        if key in functions:
            callee = functions[key]

        else:
            callee = functions[key] = callgraph.Function(
                fn, sandbox, location = vuln['location'], cve = vuln['cve'])


        for t in vuln['trace']:
            key = (t['function'], sandbox)

            if key in functions:
                caller = functions[key]

            else:
                caller = functions[key] = callgraph.Function(
                    fn = t['function'],
                    sandbox = sandbox,
                    location = t['location'] if 'location' in t else t
                )

            callee.callers.add(caller)
            caller.callees.add(callee)
            calls[(caller, callee)] += 1

            callee = caller

    return (functions.values(), calls)
