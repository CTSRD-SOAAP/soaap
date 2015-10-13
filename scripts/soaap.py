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
