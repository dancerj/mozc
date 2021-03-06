# -*- coding: utf-8 -*-
# Copyright 2010-2020, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Pre-generate XML data of ibus-mozc engines.

We don't support --xml command line option in the ibus-engines-mozc command
since it could make start-up time of ibus-daemon slow when XML cache of the
daemon in ~/.cache/ibus/bus/ is not ready or is expired.
"""

from __future__ import print_function
__author__ = "yusukes"

import optparse
import os
import subprocess
import sys

# Information to generate <component> part of mozc.xml. %s will be replaced with
# a product name, 'Mozc' or 'Google Japanese Input'.
IBUS_COMPONENT_PROPS = {
    'name': 'com.google.IBus.Mozc',
    'description': '%(product_name)s Component',
    'exec': '%(ibus_mozc_path)s --ibus',
    # TODO(mazda): Generate the version number.
    'version': '0.0.0.0',
    'author': 'Google Inc.',
    'license': 'New BSD',
    'homepage': 'https://github.com/google/mozc',
    'textdomain': 'ibus-mozc',
}

# A dictionary from --branding to a product name which is embedded into the
# properties above.
PRODUCT_NAMES = {
    'Mozc': 'Mozc',
    'GoogleJapaneseInput': 'Google Japanese Input',
}

CPP_HEADER = """// Copyright 2010 Google Inc. All Rights Reserved.

#ifndef %s
#define %s
namespace {"""

CPP_FOOTER = """}  // namespace
#endif  // %s"""


def OutputXmlElement(param_dict, element_name, value):
  print('  <%s>%s</%s>' % (element_name, (value % param_dict), element_name))


def OutputXml(param_dict, component, engine_common, engines, setup_arg):
  """Outputs a XML data for ibus-daemon.

  Args:
    param_dict: A dictionary to embed options into output string.
        For example, {'product_name': 'Mozc'}.
    component: A dictionary from a property name to a property value of the
        ibus-mozc component. For example, {'name': 'com.google.IBus.Mozc'}.
    engine_common: A dictionary from a property name to a property value that
        are commonly used in all engines. For example, {'language': 'ja'}.
    engines: A dictionary from a property name to a list of property values of
        engines. For example, {'name': ['mozc-jp', 'mozc', 'mozc-dv']}.
  """
  print('<component>')
  for key in component:
    OutputXmlElement(param_dict, key, component[key])
  print('<engines>')
  for i in range(len(engines['name'])):
    print('<engine>')
    for key in engine_common:
      OutputXmlElement(param_dict, key, engine_common[key])
    if setup_arg:
      OutputXmlElement(param_dict, 'setup', ' '.join(setup_arg))
    for key in engines:
      OutputXmlElement(param_dict, key, engines[key][i])
    print('</engine>')
  print('</engines>')
  print('</component>')


def OutputCppVariable(param_dict, prefix, variable_name, value):
  print('const char k%s%s[] = "%s";' % (prefix, variable_name.capitalize(),
                                        (value % param_dict)))


def OutputCpp(param_dict, component, engine_common, engines):
  """Outputs a C++ header file for mozc/unix/ibus/main.cc.

  Args:
    param_dict: see OutputXml.
    component: ditto.
    engine_common: ditto.
    engines: ditto.
  """
  guard_name = 'MOZC_UNIX_IBUS_MAIN_H_'
  print(CPP_HEADER % (guard_name, guard_name))
  for key in component:
    OutputCppVariable(param_dict, 'Component', key, component[key])
  for key in engine_common:
    OutputCppVariable(param_dict, 'Engine', key, engine_common[key])
  for key in engines:
    print('const char* kEngine%sArray[] = {' % key.capitalize())
    for i in range(len(engines[key])):
      print('"%s",' % (engines[key][i] % param_dict))
    print('};')
  print('const size_t kEngineArrayLen = %s;' % len(engines['name']))
  print(CPP_FOOTER % guard_name)


def CheckIBusVersion(options, minimum_version):
  """Tests if ibus version is equal to or greater than the given value."""
  command_line = ['pkg-config', '--exists', 'ibus-1.0 >= %s' % minimum_version]
  return_code = subprocess.call(command_line)
  if return_code == 0:
    return True
  else:
    return False


def main():
  """The main function."""
  parser = optparse.OptionParser(usage='Usage: %prog [options]')
  parser.add_option('--output_cpp', action='store_true',
                    dest='output_cpp', default=False,
                    help='If specified, output a C++ header. Otherwise, output '
                    'XML.')
  parser.add_option('--branding', dest='branding', default=None,
                    help='GoogleJapaneseInput for the official build. '
                    'Otherwise, Mozc.')
  parser.add_option('--ibus_mozc_path', dest='ibus_mozc_path', default='',
                    help='The absolute path of ibus_mozc executable.')
  parser.add_option('--ibus_mozc_icon_path', dest='ibus_mozc_icon_path',
                    default='', help='The absolute path of ibus_mozc icon.')
  parser.add_option('--server_dir', dest='server_dir', default='',
                    help='The absolute directory path to be installed the '
                    'server executable.')
  (options, unused_args) = parser.parse_args()

  setup_arg = []
  setup_arg.append(os.path.join(options.server_dir, 'mozc_tool'))
  setup_arg.append('--mode=config_dialog')

  param_dict = {
      'product_name': PRODUCT_NAMES[options.branding],
      'ibus_mozc_path': options.ibus_mozc_path,
      'ibus_mozc_icon_path': options.ibus_mozc_icon_path,
  }

  engine_common_props = {
      'description': '%(product_name)s (Japanese Input Method)',
      'language': 'ja',
      'icon': '%(ibus_mozc_icon_path)s',
      'rank': '80',
  }

  # DO NOT change the engine name 'mozc-jp'. The names is referenced by
  # unix/ibus/mozc_engine.cc.
  engines_props = {
      'name': ['mozc-jp'],
      'longname': ['%(product_name)s'],
  }

  # IBus 1.5.11 and greater supports 'icon_prop_key'.
  # See ibus/ibus@23c45b970b195008a54884a1a9d810e7f8b22c5c
  if CheckIBusVersion(options, '1.5.11'):
    # Make sure that the property key 'InputMode' matches to the property name
    # specified to |ibus_property_new| in unix/ibus/property_handler.cc
    engine_common_props['icon_prop_key'] = 'InputMode'

  if CheckIBusVersion(options, '1.5.0'):
    engine_common_props['symbol'] = '&#x3042;'
    engines_props['layout'] = ['default']
  else:
    engines_props['layout'] = ['jp']

  if options.output_cpp:
    OutputCpp(param_dict, IBUS_COMPONENT_PROPS, engine_common_props,
              engines_props)
  else:
    OutputXml(param_dict, IBUS_COMPONENT_PROPS, engine_common_props,
              engines_props, setup_arg)
  return 0

if __name__ == '__main__':
  sys.exit(main())
