#!/usr/bin/env python3
"""Fail if anything that looks like a credential is in the given files or folders.

Prints where it found one and only the first characters of the match. Run on every
release, because a key compiled into old firmware once shipped in the installers.
"""
import os
import re
import sys

PATTERNS = [
    ("OpenAI/Anthropic key", rb"sk-(?:proj-|ant-|or-|svcacct-)?[A-Za-z0-9_\-]{20,}"),
    ("Google key", rb"AIza[0-9A-Za-z_\-]{30,}"),
    ("Groq key", rb"gsk_[A-Za-z0-9]{20,}"),
    ("GitHub token", rb"gh[pousr]_[A-Za-z0-9]{30,}"),
    ("Slack token", rb"xox[abprs]-[A-Za-z0-9\-]{20,}"),
    ("AWS key", rb"AKIA[0-9A-Z]{16}"),
    # With its body: TLS libraries contain the bare header text to parse keys
    ("Private key", rb"-----BEGIN [A-Z ]*PRIVATE KEY-----\s*[A-Za-z0-9+/=]{32,}"),
    ("Bearer token", rb"Bearer [A-Za-z0-9._\-]{20,}"),
    ("App ID in a URL", rb"appid=[A-Za-z0-9\-]{6,}"),
]


def accepted_items():
    """Known and accepted for now. Each one found is reported on every run so it can't be forgotten."""
    secrets = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "sketch", "sketch_jan12a", "local_secrets.h")
    try:
        with open(secrets, "rb") as handle:
            match = re.search(rb'WOLFRAM_APPID "([A-Z0-9-]+)"', handle.read())
    except OSError:
        return {}
    return {b"appid=" + match.group(1): "Wolfram Alpha AppID from local_secrets.h"} if match else {}


ACCEPTED = accepted_items()


def files_in(path):
    if os.path.isfile(path):
        yield path
    for root, _, names in os.walk(path):
        for name in names:
            yield os.path.join(root, name)


def main():
    found = 0
    accepted = set()
    checked = 0
    for path in sys.argv[1:]:
        for name in files_in(path):
            if os.path.islink(name):
                continue
            with open(name, "rb") as handle:
                data = handle.read()
            checked += 1
            for label, pattern in PATTERNS:
                for match in re.finditer(pattern, data):
                    text = match.group(0)
                    if text in ACCEPTED:
                        accepted.add(text)
                        continue
                    found += 1
                    print(f"SECRET {label} in {name}: {text[:8].decode('latin1')}... ({len(text)} chars)")
    for text in sorted(accepted):
        print(f"warning: known credential still shipped: {ACCEPTED[text]}")
    print(f"Scanned {checked} files: {found} secret(s)")
    return 1 if found or checked == 0 else 0


if __name__ == "__main__":
    sys.exit(main())
