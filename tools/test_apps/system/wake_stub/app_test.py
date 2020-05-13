# Need Python 3 string formatting functions
import sys
import subprocess
import os
import re

from io import StringIO
import ttfw_idf


# The list of objects to check in listing
sys_obj_list = ('test_wake_stub.elf', 'esp_wake_stub_timer.c.obj',
                'esp_wake_stub_common.c.obj', 'esp_wake_stub_ext.c.obj')


class TestError(RuntimeError):
    """Test runtime error class
    """
    def __init__(self, message):
        """Constructor for core dump error
        """
        super(TestError, self).__init__(message)


class SymbolInfo(object):
    """
    Encapsulates a symbol info data from TestError
    """
    SYMB_INFO_GROUP_LEN = 6
    SYMB_INFO_ADDR = 0
    SYMB_INFO_SCOPE = 1
    SYMB_INFO_TYPE = 2
    SYMB_INFO_SEG = 3
    SYMB_INFO_LEN = 4
    SYMB_INFO_NAME = 5

    esp32_rtc_fast_mem_range1 = range(0x3ff80000, 0x3ff8ffff)
    esp32_rtc_fast_mem_range2 = range(0x400c0000, 0x400c1fff)
    esp32_rtc_slow_mem_range = range(0x50000000, 0x50001fff)
    esp32_rom_mem_range = range(0x40000000, 0x4005ffff)

    esp32s2_rtc_fast_mem_range1 = range(0x3ff9e000, 0x3ff9ffff)
    esp32s2_rtc_fast_mem_range2 = range(0x40070000, 0x40071fff)
    esp32s2_rtc_slow_mem_range = range(0x50000000, 0x50001fff)
    esp32s2_rom_mem_range = range(0x40000000, 0x4001ffff)

    def __init__(self, obj, group):
        super(SymbolInfo, self).__init__()
        if group is not None and len(group) <= self.SYMB_INFO_GROUP_LEN + 1:
            self.addr = self._get_int(group[self.SYMB_INFO_ADDR])
            self.scope = group[self.SYMB_INFO_SCOPE]
            self.type = group[self.SYMB_INFO_TYPE]
            self.segment = group[self.SYMB_INFO_SEG]
            self.len = self._get_int(group[self.SYMB_INFO_LEN])
            self.name = group[len(group) - 1]
            self.group = group
            self.object = obj

    @staticmethod
    def _get_int(str_hex):
        try:
            _int_val = int(str_hex, base=16)
        except IndexError:
            _int_val = -1
        return _int_val

    def check_symb_loc(self, target):
        result = False
        if (target in '"esp32"') or (target is None):
            if (self.addr in self.esp32_rtc_slow_mem_range or
               self.addr in self.esp32_rtc_fast_mem_range1 or
               self.addr in self.esp32_rtc_fast_mem_range2 or
               self.addr in self.esp32_rom_mem_range):
                result = True
        elif (target in '"esp32s2"'):
            print(type(target))
            if (self.addr in self.esp32s2_rtc_slow_mem_range or
               self.addr in self.esp32s2_rtc_fast_mem_range1 or
               self.addr in self.esp32s2_rtc_fast_mem_range2 or
               self.addr in self.esp32s2_rom_mem_range):
                result = True
        return result


class SymbolList(object):
    """
    Encapsulates a symbol list data for an object
    """
    list_pattern_dict = {'OBJ_NAME': (r'([_a-zA-Z0-9]+\.c\.obj)\:\s+'
                                      r'file format elf32-xtensa-[ble]{2}'),
                         'OBJ_SYM_SKIP1': (r'([a-zA-Z0-9]+)\sl\s+d\s+([\*\.a-zA-Z0-9_]+)\s+'
                                           r'([a-zA-Z0-9]+)\s(\.xtensa\.info.*)$'),
                         'OBJ_SYM_SKIP2': (r'([a-zA-Z0-9]+)\s([g\s]{1,2})\s+([F|\s]{1})\s+'
                                           r'([\*\.a-zA-Z0-9]+)\s+([a-zA-Z0-9]+)\s(__assert_func.*)$'),
                         'OBJ_SYM_NORMAL': (r'([a-zA-Z0-9]+)\s([w|g|l\s]{1,2})\s+([O|F|d|\s]{1})\s+'
                                            r'([\*\.a-zA-Z0-9_]+)\s+([a-zA-Z0-9]+)\s([\.]*.+\.)*(\w+)$')}

    def __init__(self, bin_file):
        super(SymbolList, self).__init__()
        self.input_file = bin_file

    def _expect_re(self, data, pattern):
        """
        check if re pattern is matched in data
        :param data: data to process
        :param pattern: compiled RegEx pattern
        :return: match groups if match succeed otherwise None
        """
        ret = None
        regex = re.compile(pattern)
        match = regex.search(data)
        if match is not None:
            ret = tuple(None if x is None else x for x in match.groups())
            index = match.end()
        else:
            index = -1
        return ret, index

    def get_symbols(self, objects=None):
        try:
            dump = StringIO(subprocess.check_output(["xtensa-esp32-elf-objdump", "-t", self.input_file]).decode())
            symbol_list = []
            obj_file_name = os.path.basename(self.input_file)
            # save dump name
            dump.name = self.input_file
            # Check pattern in the listing
            data_lines = dump.getvalue().splitlines()
            for line in data_lines:
                for key, pat in self.list_pattern_dict.items():
                    group, index = self._expect_re(line, self.list_pattern_dict[key])
                    if index != -1 and group is not None:
                        if key is 'OBJ_NAME':
                            print("{%s}=%s, found in index: %d." % (key, group[0], index))
                            obj_file_name = group[0]
                            break
                        elif (key is 'OBJ_SYM_SKIP1') or (key is 'OBJ_SYM_SKIP2'):
                            print("Skip pattern %s in {%s}," % (key, obj_file_name))
                            break
                        elif key is 'OBJ_SYM_NORMAL' and obj_file_name in objects:
                            info = SymbolInfo(obj_file_name, group)
                            symbol_list.append(info)
        except TestError as e:
            print("Can not get symbols for %s\nERROR: %s" % (dump.name, e))
            sys.exit(1)
        return symbol_list


@ttfw_idf.idf_custom_test(env_tag="Example_WIFI", group="test-apps")
def test_check_wake_stub(env, extra_data):
    """
    Wake stub test application
    """
    try:
        dut1 = env.get_dut("test_wake_stub", "tools/test_apps/system/wake_stub")
        target = dut1.app.get_sdkconfig()['CONFIG_IDF_TARGET']
        print("Target: %s, %s" % (target, dut1.TARGET))
        curr_path = os.path.dirname(os.path.realpath(__file__))

        bin_file = os.path.join(dut1.app.binary_path, "test_wake_stub.elf")
        print("Check binary: %s for target %s" % (os.path.basename(bin_file), target))
        output_path = os.path.join(curr_path, "wake_stub_test.log")
        lib_path = os.path.abspath(curr_path + "/build/esp-idf/esp_system/")
        lib_file = os.path.join(lib_path, "libesp_system.a")
        sys_list = SymbolList(lib_file)
        esp_system_symbols = sys_list.get_symbols(sys_obj_list)
        print("Checked libesp-system: %s, processed %d symbols." % (lib_file, len(esp_system_symbols)))

        bin_list = SymbolList(bin_file)
        binary_symbols = bin_list.get_symbols(sys_obj_list)
        with open(output_path, "w") as f:
            for sys_symbol in esp_system_symbols:
                for bin_symbol in binary_symbols:
                    if sys_symbol.name == bin_symbol.name:
                        if bin_symbol.check_symb_loc(target):
                            print("Object {%s}, symbol {%s}, placement is correct." % (sys_symbol.object, sys_symbol.name))
                        else:
                            print("Object {%s}, symbol {%s}, placement is wrong, addr=0x%x." % (sys_symbol.object, sys_symbol.name, bin_symbol.addr))
                            raise TestError("Incorrect symbol placement %s" % sys_symbol.name)
                        for x in bin_symbol.group:
                            if x is not None:
                                    f.write(x + ' ')
                        f.write('\n')

        print("Checked binary: %s, processed %d symbols.\n" % (bin_file, len(esp_system_symbols)))
        dut1.start_app()
        dut1.expect("Wake stab enter count: 4")
        dut1.expect("Test wake stub ext0 is passed.")
        dut1.expect("Setup wake stub timer test.")
        dut1.expect("Wake stab enter count: 4")
        dut1.expect("Test wake stub timer is passed.")
        dut1.expect("All wake stub tests are passed.")
        env.close_dut(dut1.name)

    except TestError as e:
        print("An error occured during test: %s" % e)
        sys.exit(1)


if __name__ == '__main__':
    test_check_wake_stub()
