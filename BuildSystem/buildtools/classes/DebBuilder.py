import os
import sys
import subprocess
import os.path
import shutil
import fileinput
import re
from datetime import datetime, date

import Globals
from Builder import *

EX_OK = getattr(os, "EX_OK", 0)

class DebBuilder:
  do_build_ = "doBuild"
  def start(self, skip_building = 'false'):
    print('-- Starting Deb Builder')
    print('-- Skip Building Archive? ' + skip_building)

    if skip_building != 'true':
      if Globals.operating_system_ == "windows":
        self.do_build_ += '.bat'
      else:
        self.do_build_ = './' + self.do_build_ + '.sh'

      print('--> Building Casal2 Archive')
      print('-- Re-Entering the build sytem to build a release binary')
      if os.system(self.do_build_ + ' archive') != EX_OK:
        return Globals.PrintError('Failed to build Casal2 archive')

    print('-- Loading version information from git')
    p = subprocess.Popen(['git', '--no-pager', 'log', '-n', '1', '--pretty=format:%H%n%h%n%ci' ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = p.communicate()
    lines = out.decode('utf-8').split('\n')
    if len(lines) != 3:
      return Globals.PrintError('Format printed by git did not meet expectations. Expected 3 lines but got ' + str(len(lines)))

    time_pieces = lines[2].split(' ')
    temp = ' '.join(time_pieces)
    local_time = datetime.strptime(temp, '%Y-%m-%d %H:%M:%S %z')
    utc_time   = local_time.astimezone(pytz.utc)

    version = utc_time.strftime('%Y%m%d') + '.' + lines[1]

    print('-- Casal2 Revision: ' + lines[1])
    if not os.path.exists('bin/linux/deb'):
      os.mkdir('bin/linux/deb')
    folder = 'bin/linux/deb/Casal2'
    os.system('rm -rf ' + folder)
    os.makedirs(folder + '/usr/local/bin')
    os.makedirs(folder + '/usr/local/lib')
    os.system('cp Casal2/casal2 ' + folder + '/usr/local/bin')
    os.system('cp Casal2/casal2_adolc.so ' + folder + '/usr/local/lib')
    os.system('cp Casal2/casal2_betadiff.so ' + folder + '/usr/local/lib')
    os.system('cp Casal2/casal2_cppad.so ' + folder + '/usr/local/lib')
    os.system('cp Casal2/casal2_release.so ' + folder + '/usr/local/lib')
    os.system('cp Casal2/casal2_test.so ' + folder + '/usr/local/lib')
    os.makedirs(folder + '/usr/local/share/doc/casal2')
    os.makedirs(folder + '/usr/local/share/doc/ContributorsGuide')
    # os.makedirs(folder + '/usr/local/share/doc/GettingStartedGuide')
    os.makedirs(folder + '/usr/local/share/doc/README')
    os.makedirs(folder + '/usr/local/share/doc/R-library')
    os.makedirs(folder + '/usr/local/share/doc/Examples')
    os.system('cp ../Documentation/UserManual/CASAL2.pdf ' + folder + '/usr/local/share/doc/casal2/')
    os.system('cp ../Documentation/ContributorsManual/ContributorsGuide.pdf ' + folder + '/usr/local/share/doc/ContributorsGuide/')
    # os.system('cp ../Documentation/GettingStartedGuide/GettingStartedGuide.pdf ' + folder + '/usr/local/share/doc/GettingStartedGuide/')
    os.system('cp ../README.txt ' + folder + '/usr/local/share/doc/README/')
    os.system('cp -a ../Examples/. ' + folder + '/usr/local/share/doc/Examples/')
    os.system('cp ../"R-libraries"/casal_2.30.tar.gz ' + folder + '/usr/local/share/doc/R-library/casal_2.30.tar.gz')
    os.system('cp ../"R-libraries"/casal2_1.0.tar.gz ' + folder + '/usr/local/share/doc/R-library/casal2_1.0.tar.gz')
    os.makedirs(folder + '/DEBIAN')
    control_file = open(folder + '/DEBIAN/control', 'w')
    control_file.write('Package: Casal2\n')
    control_file.write('Version: 0x' + version + '\n')
    control_file.write('Section: base\n')
    control_file.write('Priority: optional\n')
    control_file.write('Architecture: amd64\n')
    control_file.write('Maintainer: NIWA <casal2@niwa.co.nz>\n')
    control_file.write('Description: Casal2 Modeling Platform\n')
    control_file.close()

    if os.system('dpkg-deb --build ' + folder) != EX_OK:
      return Globals.PrintError('Failed to build deb package')
    return True
