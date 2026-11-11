# SPDX-License-Identifier: GPL-3.0-or-later
"""Production password verifier and live file rotation; no hardware."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
class PasswordTests(unittest.TestCase):
    def test_verifier_and_password_file_rotation(self):
        source = r'''
#include "sunshine/rkmoon_password.hpp"
#include <cassert>
#include <fstream>
#include <sys/stat.h>
int main(int argc, char **argv) {
  rkmoon_sunshine::password_auth verifier;
  assert(!verifier.authorize("Basic a3ZtOndyb25n"));
  assert(!verifier.authorize(std::string(1025, 'A')));
  assert(verifier.authorize("Basic a3ZtOmt2bQ=="));
  for (int i=0;i<10;++i) verifier.authorize("");
  assert(!verifier.authorize("Basic a3ZtOmt2bQ=="));
  rkmoon_sunshine::password_auth live;
  std::filesystem::path file=argv[1];
  { std::ofstream out(file); out << "kvm\n"; }
  chmod(file.c_str(),0600);
  assert(live.authorize_file(file,"Basic a3ZtOmt2bQ=="));
  { std::ofstream out(file); out << "other\n"; }
  assert(!live.authorize_file(file,"Basic a3ZtOmt2bQ=="));
  assert(live.authorize_file(file,"Basic a3ZtOm90aGVy"));
  chmod(file.c_str(),0644);
  assert(!live.authorize_file(file,"Basic a3ZtOm90aGVy"));
  std::filesystem::remove(file);
  std::filesystem::create_symlink("missing",file);
  assert(!live.authorize_file(file,"Basic a3ZtOmt2bQ=="));
}
'''
        compiler = shutil.which("c++")
        if not compiler: self.skipTest("C++ compiler required")
        flags = []
        openssl = Path("/opt/homebrew/opt/openssl@3")
        if openssl.exists():
            flags = ["-I" + str(openssl / "include"), "-L" + str(openssl / "lib")]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "test.cpp").write_text(source)
            subprocess.run([compiler, "-std=c++17", "-I" + str(ROOT), *flags, str(path / "test.cpp"), "-lcrypto", "-o", str(path / "test")], check=True, capture_output=True)
            subprocess.run([str(path / "test"),str(path / "password")], check=True, capture_output=True)
