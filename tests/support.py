import glob
import platform
import os
import subprocess
import sys

#
# Standard places to find include files and libraries.
#
# Excluded from CFLAGS, LDFLAGS, etc.
#
if platform.system() == 'FreeBSD':
	std_incdirs = [ '/usr/include' ]
	std_libdirs = [ '/lib', '/usr/lib' ]
else:
	std_incdirs = [ '/usr/include', '/usr/local/include' ]
	std_libdirs = [ '/lib', '/usr/lib', '/usr/local/lib' ]


def cflags(dirs, defines = [], extra = []):
	return ' '.join([
		' '.join([ '-I %s' % d for d in dirs if d not in std_incdirs ]),
		' '.join([ '-D %s' % d for d in defines ]),
		' '.join(extra)
	])

def ldflags(dirs, libs, extras = []):
	return ' '.join([
		' '.join([ '-L %s' % d for d in dirs if d not in std_libdirs ]),
		' '.join([ '-l %s' % l for l in libs ])
	] + extras)

def cpp_out():
	""" How do we specify the output file from our platform's cpp? """
	cpp_version = run_command('cpp', [ '--version' ]).split('\n')[0]

	# Clang usage: 'cpp in -o out'; GCC usage: 'cpp in out'
	if 'clang' in cpp_version: return '-o'
	else: return ''


def find_containing_dir(filename, paths, notfound_msg):
	""" Find the first directory that contains a file. """

	for d in paths:
		if len(glob.glob(os.path.join(d, filename))) > 0:
			return d

	sys.stderr.write("No '%s' in %s\n" % (filename, paths))
	if notfound_msg: sys.stderr.write('%s\n' % notfound_msg)
	sys.stderr.flush()
	sys.exit(1)


def find_include_dir(filename, paths = [], notfound_msg = ""):
	return find_containing_dir(filename, std_incdirs + paths, notfound_msg)

def find_libdir(filename, paths = [], notfound_msg = ""):
	return find_containing_dir(filename, std_libdirs + paths, notfound_msg)


def run_command(command, args = []):
	""" Run a command line and return the output from stdout. """

	argv = [ command ] + args
	try: cmd = subprocess.Popen(argv, stdout = subprocess.PIPE)
	except OSError, why:
		sys.stderr.write('Unable to run %s: %s\n' % (command, why))
		sys.stderr.flush()
		sys.exit(1)

	cmd.wait()
	return cmd.stdout.read()



class Config:
	def __init__(self, command):
		self.command = command

	def __getitem__(self, name):
		return run_command(self.command, [ '--' + name ]).strip()

llvm_config = Config('llvm-config')

