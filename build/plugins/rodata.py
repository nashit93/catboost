import os

import _common as common
import _import_wrapper as iw


class Yasm(iw.CustomCommand):
    def __init__(self, path, unit):
        self._path = path
        self._flags = []

        prefix = unit.get('ASM_PREFIX')

        if prefix:
            self._flags += ['--prefix=' + prefix]

        self._incl_dirs = ['$S', '$B'] + unit.includes()
        self._pre_include = []

        flags = unit.get('YASM_FLAGS')
        if flags:
            self.parse_flags(path, unit, collections.deque(flags.split(' ')))

        if unit.enabled('darwin') or unit.enabled('ios'):
            self._platform = ['DARWIN', 'UNIX']
            self._fmt = 'macho'
        elif unit.enabled('win64') or unit.enabled('cygwin'):
            self._platform = ['WIN64']
            self._fmt = 'win'
        elif unit.enabled('win32'):
            self._platform = ['WIN32']
            self._fmt = 'win'
        else:
            self._platform = ['UNIX']
            self._fmt = 'elf'

        if 'elf' in self._fmt:
            self._flags += ['-g', 'dwarf2']

        self._fmt += unit.get('hardware_arch')
        self._type = unit.get('hardware_type')

    def parse_flags(self, path, unit, flags):
        while flags:
            flag = flags.popleft()
            if flag.startswith('-I'):
                raise Exception('Use ADDINCL macro')

            if flag.startswith('-P'):
                preinclude = flag[2:] or flags.popleft()
                self._pre_include += unit.resolve_include([(get_retargeted(path, unit)), preinclude])
                self._flags += ['-P', preinclude]
                continue

            self._flags.append(flag)

    def descr(self):
        return 'AS', self._path, 'light-green'

    def flags(self):
        return self._flags + self._platform + [self._fmt, self._type]

    def tools(self):
        return ['contrib/tools/yasm']

    def input(self):
        return common.make_tuples(self._pre_include + [self._path])

    def output(self):
        return common.make_tuples([common.tobuilddir(common.stripext(self._path)) + '.o'])

    def run(self, binary):
        return self.do_run(binary, self._path)

    def do_run(self, binary, path):
        def plt():
            for x in self._platform:
                yield '-D'
                yield x

        def incls():
            for x in self._incl_dirs:
                yield '-I'
                yield self.resolve_path(x)

        cmd = [binary, '-f', self._fmt] + list(plt()) + ['-D', '_' + self._type + '_', '-D_YASM_'] + self._flags + list(incls()) + ['-o', common.get(self.output, 0), path]
        self.call(cmd)


class ROData(Yasm):
    def __init__(self, path, unit):
        Yasm.__init__(self, path, unit)

        if unit.enabled('darwin') or (unit.enabled('windows') and unit.enabled('arch_type_32')):
            self._prefix = '_'
        else:
            self._prefix = ''

    def run(self, binary):
        in_file = self.resolve_path(common.get(self.input, 0))
        in_file_no_ext = common.stripext(in_file)
        file_name = os.path.basename(in_file_no_ext)
        tmp_file = self.resolve_path(common.get(self.output, 0) + '.asm')

        with open(tmp_file, 'w') as f:
            f.write('global ' + self._prefix + file_name + '\n')
            f.write('global ' + self._prefix + file_name + 'Size' + '\n')
            f.write('SECTION .rodata\n')
            f.write(self._prefix + file_name + ':\nincbin "' + in_file + '"\n')
            f.write(self._prefix + file_name + 'Size:\ndd ' + str(os.path.getsize(in_file)) + '\n')

        return self.do_run(binary, tmp_file)


class RODataCXX(iw.CustomCommand):
    def __init__(self, path, unit):
        self._path = path
        self._base = os.path.basename(common.stripext(self._path))

    def descr(self):
        return 'RD', self._path, 'light-green'

    def input(self):
        return common.make_tuples([self._path])

    def main_out(self):
        return common.tobuilddir(common.stripext(self._path)) + '.cpp'

    def output(self):
        return common.make_tuples([self.main_out()])

    def run(self, binary):
        with open(self.resolve_path(self.main_out()), 'w') as f:
            f.write('static_assert(sizeof(unsigned int) == 4, "ups, something gone wrong");\n\n')
            f.write('extern "C" {\n')
            f.write('    extern const unsigned char ' + self._base + '[] = {\n')

            cnt = 0

            with open(self.resolve_path(self._path), 'r') as input:
                for ch in input.read():
                    f.write('0x%02x, ' % ord(ch))

                    cnt += 1

                    if cnt % 50 == 1:
                        f.write('\n')

            f.write('    };\n')
            f.write('    extern const unsigned int ' + self._base + 'Size = sizeof(' + self._base + ');\n')
            f.write('}\n')


def ro_data(path, unit):
    if unit.enabled('ARCH_AARCH64') or unit.enabled('ARCH_ARM') or unit.enabled('ARCH_PPC64LE'):
        return RODataCXX(path, unit)

    return ROData(path, unit)


def init():
    iw.addrule('rodata', ro_data)
