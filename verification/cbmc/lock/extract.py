#!/usr/bin/env python3
"""Print the C function FUNC of SRC for a CBMC harness to #include.

  extract.py SRC FUNC [--mutate OLD NEW]

1. The function text is taken verbatim from SRC (a changed body is re-verified automatically; a
   renamed function fails loudly).
2. --mutate applies one literal replacement (negative controls); it fails when OLD is absent, so a
   stale mutation cannot pass silently.
3. GLib's g_autofree / g_auto(T) / g_autoptr(T) are `__attribute__((cleanup))` declarations, which
   CBMC 6.11 silently IGNORES (checked: a cleanup function that is never called, no warning). So the
   scope-exit frees are made explicit: before every `return` and at the `}` that closes the scope of
   a declaration, a call `model_cleanup_<kind> (&var);` is inserted, in reverse declaration order,
   exactly where the compiler runs the attribute. The return value is evaluated first
   (`__typeof__ (e) model_ret = (e);`), so `return g_steal_pointer (&x);` keeps working. Nothing
   else is rewritten. break/continue out of a scope that owns an auto variable is refused.
"""
import re
import sys

DECL = re.compile(r"^\s*(g_autofree|g_auto\((\w+)\)|g_autoptr\((\w+)\))\s+[^;=(]*?(\w+)\s*(=[^;]*)?;\s*$")
TRAILING_COMMENT = re.compile(r"^(.*;|\s*[{}])(\s*/\*.*\*/)\s*$")


def find_function(lines, func):
    start = next((i for i, l in enumerate(lines) if re.match(re.escape(func) + r" \(", l)), None)
    if start is None:
        sys.exit("extract: %s not found" % func)
    b = start
    while (b > 0 and lines[b - 1].strip()
           and not re.search(r"(;|\}|\*/)\s*$", lines[b - 1])
           and not lines[b - 1].lstrip().startswith(("/*", "*", "#"))):
        b -= 1
    e = next(i for i in range(start, len(lines)) if lines[i] == "}")
    return b, e


def calls_for(scopes_vars):
    return " ".join("model_cleanup_%s (&%s);" % v for v in scopes_vars)


def explicit_cleanups(text):
    out = []
    scopes = []  # one list of (kind, var) per open brace
    pending = None  # accumulating a multi-line `return ...;`
    for raw in text.split("\n"):
        cm = TRAILING_COMMENT.match(raw)
        line, comment = (cm.group(1), cm.group(2)) if cm else (raw, "")
        if pending is not None:
            pending += "\n" + line
            if not line.rstrip().endswith(";"):
                continue
            line, pending = pending, None
        elif re.match(r"^\s*return\b", line) and not line.rstrip().endswith(";"):
            pending = line
            continue
        m = DECL.match(line)
        if m and scopes:
            kind = "autofree" if m.group(1) == "g_autofree" else (m.group(2) or m.group(3))
            scopes[-1].append((kind, m.group(4)))
            out.append(line + comment)
            continue
        r = re.match(r"^(\s*)return\b\s*(.*?)\s*;\s*$", line, re.S)
        if r:
            live = [v for sc in reversed(scopes) for v in reversed(sc)]
            if live:
                e = r.group(2)
                if e:
                    out.append("%s{ __typeof__ (%s) model_ret = (%s); %s return model_ret; }%s"
                               % (r.group(1), e, e, calls_for(live), comment))
                else:
                    out.append("%s{ %s return; }%s" % (r.group(1), calls_for(live), comment))
                continue
        if re.search(r"\breturn\b", line) and any(scopes):
            sys.exit("extract: `return` not at the start of a line while an auto variable is live: " + line)
        if re.match(r"^\s*(break|continue)\s*;", line) and any(scopes[1:]):
            sys.exit("extract: break/continue inside a scope owning an auto variable: " + line)
        res = []
        i = 0
        while i < len(line):  # brace tracking outside string and char literals
            c = line[i]
            if c in "\"'":
                j = i + 1
                while j < len(line) and line[j] != c:
                    j += 2 if line[j] == "\\" else 1
                res.append(line[i:j + 1])
                i = j + 1
                continue
            if c == "{":
                scopes.append([])
            elif c == "}":
                res.append(calls_for(reversed(scopes.pop())) + " ")
            res.append(c)
            i += 1
        out.append("".join(res) + comment)
    if scopes or pending:
        sys.exit("extract: unbalanced braces")
    return "\n".join(out)


def main():
    a = sys.argv[1:]
    src, func = a[0], a[1]
    mut = tuple(a[3:5]) if len(a) >= 5 and a[2] == "--mutate" else None
    lines = open(src).read().split("\n")
    b, e = find_function(lines, func)
    text = "\n".join(lines[b:e + 1]) + "\n"
    if mut:
        if mut[0] not in text:
            sys.exit("extract: mutation target %r not found in %s" % (mut[0], func))
        text = text.replace(mut[0], mut[1], 1)
    sys.stdout.write(explicit_cleanups(text) + "\n")


main()
