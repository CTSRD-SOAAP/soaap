import callgraph
import collections



def get(key, containers, default = None, exclude = ('"none"',)):
    """
    Retrieve a value from one of a number of containers.
    """

    for container in containers:
        if key in container:
            value = container[key]

            if value not in exclude:
                return value

    return default


analyses = {
    'private_access': (
        lambda fn, details: (fn, get('sandbox', [ details ])),
        lambda x, prev = {}: {
            'owner': tuple(
                [ s['name'] for s in get('sandbox_private', [ x, prev ], []) ]
            ),
            'sandbox': (
                [ s['name'] for s in get('sandbox_access', [ x ], []) ]
                or [ get('sandbox', [ prev ]) ]
            )[0],
        },
    ),

    'vulnerability_warning': (
        lambda fn, details: (fn, details['sandbox']),
        lambda x, prev = {}: {
            'cve': x['cve'] if 'cve' in x else None,
            'sandbox': get('sandbox', [ x, prev ]),
        },
    ),
}


def parse(soaap, analysis = 'vulnerabilities'):
    """
    Parse the results of a SOAAP analysis.
    """

    functions = {}
    calls = collections.defaultdict(int)

    (key_fn, get_details) = analyses[analysis]

    for warning in soaap[analysis]:
        fn = warning['function']
        details = get_details(warning)

        key = key_fn(fn, details)
        if key in functions:
            callee = functions[key]

        else:
            callee = functions[key] = callgraph.Function(
                fn, location = warning['location'], **details)


        for t in warning['trace']:
            details = get_details(t, details)
            key = key_fn(t['function'], details)

            if key in functions:
                caller = functions[key]

            else:
                caller = functions[key] = callgraph.Function(
                    fn = t['function'],
                    location = t['location'] if 'location' in t else t,
                    **details
                )

            callee.callers.add(caller)
            caller.callees.add(callee)
            calls[(caller, callee)] += 1

            callee = caller

    return (set(functions.values()), calls)
