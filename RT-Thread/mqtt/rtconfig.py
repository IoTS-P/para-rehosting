import os
import sys

# toolchains options
ARCH='sim'
CPU='posix'
CROSS_TOOL='gcc'
RTT_ROOT = '../rt-thread'

# cross_tool provides the cross compiler
# EXEC_PATH is the compiler execute path, for example, CodeSourcery, Keil MDK, IAR
PLATFORM    = 'gcc'
EXEC_PATH   = '/usr/bin'

if os.getenv('RTT_EXEC_PATH'):
    EXEC_PATH = os.getenv('RTT_EXEC_PATH')

BUILD = 'debug'
STM32_TYPE = 'STM32L475xx'

PREFIX = ''
CC = PREFIX + 'afl-gcc'
AS = PREFIX + 'afl-gcc'
AR = PREFIX + 'ar'
LINK = PREFIX + 'gcc'
TARGET_EXT = 'elf'
SIZE = PREFIX + 'size'
OBJDUMP = PREFIX + 'objdump'
OBJCPY = PREFIX + 'objcopy'

DEVICE = ' -march=i386 -m32 -Wno-unused -Wno-unused-parameter -Wextra -Wpedantic -Wno-sign-compare -Wno-variadic-macros -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-pedantic -Wno-unused-parameter -pthread'
CFLAGS = DEVICE + ' -std=c99 -Dgcc'
AFLAGS = ' -c' + DEVICE
LFLAGS = DEVICE + ' -march=i386 -m32 -Wno-unused -Wno-unused-parameter -Wextra -Wpedantic -Wno-sign-compare -Wno-variadic-macros -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-pedantic -Wno-unused-parameter -pthread'

CPATH = ''
LPATH = ''

CFLAGS += ' -O0 -g'

POST_ACTION = ''

def dist_handle(BSP_ROOT):
    cwd_path = os.getcwd()
    sys.path.append(os.path.join(os.path.dirname(os.path.dirname(cwd_path)), 'tools'))
    from sdk_dist import dist_do_building
    dist_do_building(BSP_ROOT)
