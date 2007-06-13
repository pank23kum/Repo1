# apport package hook for usplash
# (C) 2007 Canonical Ltd.
# Martin Pitt <martin.pitt@ubuntu.com>

# do not file bugs about crashes in libx86, since they are due to buggy BIOSes
# and we cannot do anything about them anyway

def add_info(report):
    if not report.has_key('StacktraceTop'):
        return
    top_fn = report['StacktraceTop'].splitlines()[0]
    if 'libx86.so' in top_fn or 'run_vm86' in top_fn:
        report['UnreportableReason'] = 'The crash happened in the firmware of \
the computer ("BIOS"), which cannot be influenced by the operating system.'
